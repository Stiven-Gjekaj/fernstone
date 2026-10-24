#include "app.h"
#include "bench.h"
#include "manifest.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "client.h"
#include "draw2d.h"
#include "fern_version.h"
#include "gfx.h"
#include "log.h"
#include "model.h"
#include "png.h"
#include "terrain.h"
#include "zip.h"

typedef enum { SCREEN_MEASURE, SCREEN_MENU, SCREEN_VERSIONS, SCREEN_GAME, SCREEN_PAUSE, SCREEN_GONE } Screen;

// The text fields of the menu.
enum { FIELD_NONE, FIELD_SERVER, FIELD_NAME, FIELD_CHAT };

typedef struct {
    char server[128];
    char name[17];
    char version[16];
    int render;           // The render distance in chunks.
    float sensitivity;
} Options;

typedef struct {
    int id;               // The pointer ID, or -1.
    float x0, y0, x, y;   // The start and the current position.
    double t0;
    int moved;
    int kind;             // 0 look, 1 stick, 2 button
} Touch;

#define MAX_TOUCHES 8

struct FernApp {
    FernAppConfig config;
    char data_dir[1024];
    int width, height;
    float scale, ui;      // ui: the size of one GUI pixel in drawable pixels.
    int gl_ready;
    Screen screen;
    Options opt;
    int focus;
    char chat_line[120];
    int chat_open;
    int debug;

    FernClient client;
    FernInput input;
    int keys[FK_COUNT];
    double now, last_frame;
    float frame_ms;

    FernTerrain *terrain;
    FernDraw2D *d2;
    GLuint tex_terrain, tex_gui, tex_items, tex_icons, tex_char, tex_white;
    int have_jar;
    char jar_error[256];
    FernVertexList models;

    Touch touch[MAX_TOUCHES];
    float stick_x, stick_y;   // The virtual stick, -1 to 1.
    int touch_attack, touch_use, touch_jump, touch_sneak;

    int import_request;
    char import_version[16];

    char shot_path[1024];     // A debug command asked for a screenshot.

    FernDevice device;        // The measured device.
    FernBench bench;

    // The probe of milestone 2.
    FernNet probe_net;
    int probing;
};

/* ---- options ---- */

static void options_path(FernApp *app, char *out, size_t cap) {
    snprintf(out, cap, "%s/options.txt", app->data_dir);
}

static void options_load(FernApp *app) {
    char path[1100], line[256];
    FILE *f;
    Options *o = &app->opt;
    snprintf(o->server, sizeof(o->server), "192.168.1.2");
    snprintf(o->name, sizeof(o->name), "%s",
             !strcmp(app->config.platform, "ios6") ? "Fern4S" : !strcmp(app->config.platform, "ios") ? "Fern15" : "FernMac");
    snprintf(o->version, sizeof(o->version), "b1.7.3");
    o->render = !strcmp(app->config.platform, "ios6") ? 4 : 8;
    o->sensitivity = 1.0f;
    options_path(app, path, sizeof(path));
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '='), *v;
        if (!eq) continue;
        *eq = 0;
        v = eq + 1;
        v[strcspn(v, "\r\n")] = 0;
        if (!strcmp(line, "server")) snprintf(o->server, sizeof(o->server), "%s", v);
        else if (!strcmp(line, "name")) snprintf(o->name, sizeof(o->name), "%s", v);
        else if (!strcmp(line, "version")) snprintf(o->version, sizeof(o->version), "%s", v);
        else if (!strcmp(line, "render")) o->render = atoi(v);
        else if (!strcmp(line, "sensitivity")) o->sensitivity = (float)atof(v);
    }
    fclose(f);
    if (o->render < 2) o->render = 2;
    if (o->render > 12) o->render = 12;
    if (!fern_version_find(o->version)) snprintf(o->version, sizeof(o->version), "b1.7.3");
}

static void options_save(FernApp *app) {
    char path[1100];
    FILE *f;
    options_path(app, path, sizeof(path));
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "server=%s\nname=%s\nversion=%s\nrender=%d\nsensitivity=%.2f\n", app->opt.server, app->opt.name,
            app->opt.version, app->opt.render, app->opt.sensitivity);
    fclose(f);
}

/* ---- the device ---- */

static void device_path(FernApp *app, char *out, size_t cap) {
    snprintf(out, cap, "%s/device.txt", app->data_dir);
}

// Read the numbers of the first launch. Give 0 if they are not there.
static int device_load(FernApp *app) {
    char path[1100];
    FILE *f;
    int ok;
    device_path(app, path, sizeof(path));
    f = fopen(path, "r");
    if (!f) return 0;
    ok = fscanf(f, "ram=%d\ngl=%d\nbench=%d", &app->device.ram_mb, &app->device.gl, &app->device.bench) == 3;
    fclose(f);
    return ok;
}

static void device_save(FernApp *app) {
    char path[1100];
    FILE *f;
    device_path(app, path, sizeof(path));
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "ram=%d\ngl=%d\nbench=%d\n", app->device.ram_mb, app->device.gl, app->device.bench);
    fclose(f);
}

// The major version of the GLES context, from GL_VERSION.
static int gl_major(void) {
    const char *v = (const char *)glGetString(GL_VERSION);
    const char *es = v ? strstr(v, "OpenGL ES ") : NULL;
    return es ? atoi(es + 10) : 2;
}

/* ---- assets ---- */

static void jar_path(FernApp *app, const char *version, char *out, size_t cap) {
    snprintf(out, cap, "%s/jars/%s.jar", app->data_dir, version);
}

static GLuint load_png(FernZip *z, const char *name, uint8_t **keep, int *w, int *h) {
    size_t len = 0;
    uint8_t *data = fern_zip_read(z, name, &len), *px;
    int pw = 0, ph = 0;
    GLuint t = 0;
    if (!data) return 0;
    px = fern_png_decode(data, len, &pw, &ph);
    free(data);
    if (!px) return 0;
    t = fern_gl_texture(px, pw, ph);
    if (keep) {
        *keep = px;
        *w = pw;
        *h = ph;
    } else {
        free(px);
    }
    return t;
}

static void unload_textures(FernApp *app) {
    GLuint *t[] = {&app->tex_terrain, &app->tex_gui, &app->tex_items, &app->tex_icons, &app->tex_char};
    unsigned i;
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
        if (*t[i]) {
            glDeleteTextures(1, t[i]);
            *t[i] = 0;
        }
}

// Load the textures of the version from the jar of the user.
static void load_jar(FernApp *app) {
    char path[1100];
    FernZip *z;
    uint8_t *font = NULL;
    int fw = 0, fh = 0;
    unload_textures(app);
    app->have_jar = 0;
    jar_path(app, app->opt.version, path, sizeof(path));
    z = fern_zip_open(path);
    if (!z && getenv("FERN_JAR")) z = fern_zip_open(getenv("FERN_JAR"));
    if (!z) {
        snprintf(app->jar_error, sizeof(app->jar_error), "Fernstone needs your own %s.jar.", app->opt.version);
        fern_d2_set_font(app->d2, NULL, 0, 0);
        fern_log("assets: no jar at %s", path);
        return;
    }
    app->tex_terrain = load_png(z, "terrain.png", NULL, NULL, NULL);
    app->tex_gui = load_png(z, "gui/gui.png", NULL, NULL, NULL);
    app->tex_items = load_png(z, "gui/items.png", NULL, NULL, NULL);
    app->tex_icons = load_png(z, "gui/icons.png", NULL, NULL, NULL);
    app->tex_char = load_png(z, "mob/char.png", NULL, NULL, NULL);
    {
        GLuint ft = load_png(z, "font/default.png", &font, &fw, &fh);
        if (ft) glDeleteTextures(1, &ft);
    }
    fern_zip_close(z);
    fern_d2_set_font(app->d2, font, fw, fh);
    free(font);
    app->have_jar = app->tex_terrain != 0;
    if (!app->have_jar) {
        snprintf(app->jar_error, sizeof(app->jar_error), "The jar has no terrain.png. Use the client jar of %s.",
                 app->opt.version);
        return;
    }
    fern_terrain_set_atlas(app->terrain, app->tex_terrain);
    fern_log("assets: loaded the textures of %s", app->opt.version);
}

/* ---- life cycle ---- */

FernApp *fern_app_new(const FernAppConfig *config) {
    FernApp *app = calloc(1, sizeof(*app));
    char jars[1100];
    int i;
    app->config = *config;
    snprintf(app->data_dir, sizeof(app->data_dir), "%s", config->data_dir);
    app->config.data_dir = app->data_dir;
    mkdir(app->data_dir, 0755);
    snprintf(jars, sizeof(jars), "%s/jars", app->data_dir);
    mkdir(jars, 0755);
    fern_log_open(app->data_dir);
    fern_log("fernstone %s: start on %s, %.0f MB", FERN_APP_VERSION, config->platform, config->ram_mb);
    options_load(app);
    fern_client_init(&app->client);
    fern_net_init(&app->probe_net);
    for (i = 0; i < MAX_TOUCHES; i++) app->touch[i].id = -1;
    app->scale = 1;
    app->screen = SCREEN_MENU;
    return app;
}

void fern_app_resize(FernApp *app, int width, int height, float scale) {
    float ui;
    app->width = width;
    app->height = height;
    app->scale = scale;
    // The GUI scale: Beta uses 320 x 240 GUI pixels at least.
    ui = floorf((float)height / 240.0f);
    if (ui < 1) ui = 1;
    if ((float)width / ui < 320) ui = floorf((float)width / 320.0f);
    if (ui < 1) ui = 1;
    app->ui = ui;
}

const char *fern_app_data_dir(FernApp *app) {
    return app->data_dir;
}

void fern_app_jars_changed(FernApp *app) {
    if (app->gl_ready) load_jar(app);
}

int fern_app_wants_import(FernApp *app, char *version, int cap) {
    if (!app->import_request) return 0;
    app->import_request = 0;
    snprintf(version, (size_t)cap, "%s", app->import_version);
    return 1;
}

static void connect_now(FernApp *app) {
    const FernVersion *v = fern_version_find(app->opt.version);
    char host[128];
    int port = 25565;
    char *colon;
    snprintf(host, sizeof(host), "%s", app->opt.server);
    colon = strrchr(host, ':');
    if (colon && strchr(host, ':') == colon) {
        *colon = 0;
        port = atoi(colon + 1);
    }
    options_save(app);
    fern_terrain_clear(app->terrain);
    fern_client_connect(&app->client, v ? v : fern_version_latest(), host, port, app->opt.name);
    app->screen = app->client.state == CLIENT_GONE ? SCREEN_GONE : SCREEN_GAME;
    app->focus = FIELD_NONE;
    memset(&app->input, 0, sizeof(app->input));
}

static void leave_game(FernApp *app) {
    if (app->client.state != CLIENT_GONE && app->client.state != CLIENT_IDLE)
        fern_client_disconnect(&app->client, "Quitting");
    app->client.state = CLIENT_IDLE;
    fern_terrain_clear(app->terrain);
    app->screen = SCREEN_MENU;
}

/* ---- input ---- */

static void type_into(FernApp *app, const char *text) {
    char *field = NULL;
    size_t cap = 0, n;
    switch (app->focus) {
    case FIELD_SERVER: field = app->opt.server; cap = sizeof(app->opt.server); break;
    case FIELD_NAME: field = app->opt.name; cap = sizeof(app->opt.name); break;
    case FIELD_CHAT: field = app->chat_line; cap = sizeof(app->chat_line); break;
    default: return;
    }
    n = strlen(field);
    for (; *text && n + 1 < cap; text++) {
        unsigned char c = (unsigned char)*text;
        if (c < 32 || c == 127) continue;
        if (app->focus == FIELD_NAME && !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'))
            continue;
        field[n++] = (char)c;
    }
    field[n] = 0;
}

static void backspace(FernApp *app) {
    char *field = app->focus == FIELD_SERVER ? app->opt.server : app->focus == FIELD_NAME ? app->opt.name
                : app->focus == FIELD_CHAT ? app->chat_line : NULL;
    size_t n;
    if (!field) return;
    n = strlen(field);
    // Remove one UTF-8 character.
    while (n > 0) {
        n--;
        if (((unsigned char)field[n] & 0xC0) != 0x80) break;
    }
    field[n] = 0;
}

static void open_chat(FernApp *app, const char *start) {
    app->chat_open = 1;
    app->focus = FIELD_CHAT;
    snprintf(app->chat_line, sizeof(app->chat_line), "%s", start);
    memset(&app->input, 0, sizeof(app->input));
}

static void send_chat(FernApp *app) {
    if (app->chat_line[0]) fern_client_chat(&app->client, app->chat_line);
    app->chat_line[0] = 0;
    app->chat_open = 0;
    app->focus = FIELD_NONE;
}

void fern_app_key(FernApp *app, FernKey key, int down) {
    if (key <= FK_NONE || key >= FK_COUNT) return;
    app->keys[key] = down;
    if (!down) return;
    if (app->focus != FIELD_NONE) {
        if (key == FK_BACKSPACE) backspace(app);
        else if (key == FK_ENTER) {
            if (app->focus == FIELD_CHAT) send_chat(app);
            else app->focus = FIELD_NONE;
        } else if (key == FK_ESCAPE) {
            if (app->focus == FIELD_CHAT) {
                app->chat_open = 0;
                app->chat_line[0] = 0;
            }
            app->focus = FIELD_NONE;
        } else if (key == FK_TAB && app->screen == SCREEN_MENU) {
            app->focus = app->focus == FIELD_SERVER ? FIELD_NAME : FIELD_SERVER;
        }
        return;
    }
    switch (app->screen) {
    case SCREEN_MENU:
        if (key == FK_ENTER && app->have_jar) connect_now(app);
        break;
    case SCREEN_GAME:
        if (key == FK_CHAT) open_chat(app, "");
        else if (key == FK_COMMAND) open_chat(app, "/");
        else if (key == FK_ESCAPE) app->screen = SCREEN_PAUSE;
        else if (key == FK_DEBUG) app->debug = !app->debug;
        else if (key >= FK_1 && key <= FK_9) fern_client_hold(&app->client, key - FK_1);
        break;
    case SCREEN_PAUSE:
        if (key == FK_ESCAPE) app->screen = SCREEN_GAME;
        break;
    case SCREEN_GONE:
    case SCREEN_VERSIONS:
        if (key == FK_ESCAPE || key == FK_ENTER) app->screen = SCREEN_MENU;
        break;
    case SCREEN_MEASURE:
        break;
    }
}

void fern_app_text(FernApp *app, const char *utf8) {
    if (app->focus != FIELD_NONE) type_into(app, utf8);
}

void fern_app_look(FernApp *app, float dx, float dy) {
    float s = 0.15f * app->opt.sensitivity;
    if (app->screen != SCREEN_GAME || app->chat_open) return;
    app->input.yaw += dx * s;
    app->input.pitch += dy * s;
    if (app->input.pitch > 90) app->input.pitch = 90;
    if (app->input.pitch < -90) app->input.pitch = -90;
}

void fern_app_button(FernApp *app, int button, int down) {
    if (app->screen != SCREEN_GAME || app->chat_open) return;
    if (button == 0) app->input.attack = down;
    if (button == 1 && down) app->input.use = 1;
}

void fern_app_wheel(FernApp *app, int steps) {
    int h;
    if (app->screen != SCREEN_GAME) return;
    h = ((app->client.held + steps) % 9 + 9) % 9;
    fern_client_hold(&app->client, h);
}

int fern_app_wants_capture(FernApp *app) {
    return app->screen == SCREEN_GAME && !app->chat_open && !app->config.touch && app->client.health > 0;
}

int fern_app_wants_keyboard(FernApp *app) {
    return app->focus != FIELD_NONE;
}

/* ---- the GUI ---- */

// A widget in GUI pixels. The UI uses one hit test for each frame: the
// pointer that went up this frame.
typedef struct {
    int clicked;
    float x, y;
} Click;

static Click pending_click;

static int hit(FernApp *app, float x, float y, float w, float h) {
    float u = app->ui;
    if (!pending_click.clicked) return 0;
    if (pending_click.x >= x * u && pending_click.x < (x + w) * u && pending_click.y >= y * u &&
        pending_click.y < (y + h) * u) {
        pending_click.clicked = 0;
        return 1;
    }
    return 0;
}

static float gw(FernApp *app) { return app->width / app->ui; }
static float gh(FernApp *app) { return app->height / app->ui; }

static void text(FernApp *app, const char *s, float x, float y, uint32_t c) {
    fern_d2_text(app->d2, s, x * app->ui, y * app->ui, app->ui, c, 1);
}

static float text_w(FernApp *app, const char *s) {
    return fern_d2_text_width(app->d2, s, 1);
}

static void text_center(FernApp *app, const char *s, float cx, float y, uint32_t c) {
    text(app, s, cx - text_w(app, s) / 2, y, c);
}

static void rect(FernApp *app, float x, float y, float w, float h, uint32_t c) {
    fern_d2_rect(app->d2, x * app->ui, y * app->ui, w * app->ui, h * app->ui, c);
}

// A button in the style of Beta, with the texture of gui.png when the jar
// is there.
static int button(FernApp *app, const char *label, float x, float y, float w, int enabled) {
    float u = app->ui;
    int clicked = enabled && hit(app, x, y, w, 20);
    if (app->tex_gui) {
        float v = enabled ? 66.0f : 46.0f;
        fern_d2_image(app->d2, app->tex_gui, x * u, y * u, (w / 2) * u, 20 * u, 0, v / 256, (w / 2) / 256, (v + 20) / 256,
                      0xFFFFFFFF);
        fern_d2_image(app->d2, app->tex_gui, (x + w / 2) * u, y * u, (w / 2) * u, 20 * u, (200 - w / 2) / 256, v / 256,
                      200.0f / 256, (v + 20) / 256, 0xFFFFFFFF);
    } else {
        rect(app, x, y, w, 20, enabled ? FERN_RGBA(110, 110, 110, 255) : FERN_RGBA(60, 60, 60, 255));
        rect(app, x, y + 18, w, 2, FERN_RGBA(40, 40, 40, 255));
    }
    text_center(app, label, x + w / 2, y + 6, enabled ? FERN_RGBA(224, 224, 224, 255) : FERN_RGBA(160, 160, 160, 255));
    return clicked;
}

static void field(FernApp *app, const char *label, const char *value, float x, float y, float w, int id) {
    char shown[160];
    int focused = app->focus == id;
    text(app, label, x, y - 11, FERN_RGBA(160, 160, 160, 255));
    rect(app, x - 1, y - 1, w + 2, 22, focused ? FERN_RGBA(255, 255, 255, 255) : FERN_RGBA(160, 160, 160, 255));
    rect(app, x, y, w, 20, FERN_RGBA(0, 0, 0, 255));
    snprintf(shown, sizeof(shown), "%s%s", value, focused && ((int)(app->now * 2) & 1) ? "_" : "");
    text(app, shown, x + 4, y + 6, FERN_RGBA(224, 224, 224, 255));
    if (hit(app, x, y, w, 20)) app->focus = id;
}

static void background(FernApp *app) {
    float u = app->ui, x, y;
    // The dirt background of Beta, dark, when the jar is there.
    if (app->tex_terrain) {
        for (y = 0; y < gh(app); y += 32)
            for (x = 0; x < gw(app); x += 32)
                fern_d2_image(app->d2, app->tex_terrain, x * u, y * u, 32 * u, 32 * u, 2.0f / 16, 0, 3.0f / 16, 1.0f / 16,
                              FERN_RGBA(64, 64, 64, 255));
    } else {
        rect(app, 0, 0, gw(app), gh(app), FERN_RGBA(30, 26, 22, 255));
    }
}

static void draw_menu(FernApp *app) {
    float cx = gw(app) / 2, y = gh(app) / 2 - 92;
    char line[200];
    background(app);
    // The title: a stone block in the style of the icon, and the name.
    fern_d2_text(app->d2, "Fernstone", (cx - text_w(app, "Fernstone")) * app->ui, y * app->ui, app->ui * 2,
                 FERN_RGBA(255, 255, 255, 255), 1);
    text_center(app, "Alpha and Beta multiplayer", cx + text_w(app, "Fernstone") / 2 + 12, y + 18,
                FERN_RGBA(255, 255, 85, 255));
    y += 46;
    field(app, "Server address", app->opt.server, cx - 100, y, 200, FIELD_SERVER);
    y += 36;
    field(app, "Name", app->opt.name, cx - 100, y, 200, FIELD_NAME);
    y += 28;
    snprintf(line, sizeof(line), "Version: %s", app->opt.version);
    if (button(app, line, cx - 100, y, 200, 1)) {
        app->screen = SCREEN_VERSIONS;
        app->focus = FIELD_NONE;
    }
    y += 24;
    snprintf(line, sizeof(line), "Render distance: %d", app->opt.render);
    if (button(app, line, cx - 100, y, 200, 1)) {
        app->opt.render = app->opt.render >= 12 ? 2 : app->opt.render + 1;
        options_save(app);
    }
    y += 24;
    if (button(app, "Connect", cx - 100, y, 200, app->have_jar && app->opt.server[0] && app->opt.name[0])) connect_now(app);
    y += 28;
    if (!app->have_jar) {
        text_center(app, app->jar_error, cx, y, FERN_RGBA(255, 85, 85, 255));
        if (strcmp(app->config.platform, "ios6") == 0) {
            snprintf(line, sizeof(line), "Copy it to %s/jars/%s.jar", app->data_dir, app->opt.version);
            text_center(app, line, cx, y + 11, FERN_RGBA(200, 200, 200, 255));
        } else if (button(app, "Import jar", cx - 100, y + 12, 200, 1)) {
            app->import_request = 1;
            snprintf(app->import_version, sizeof(app->import_version), "%s", app->opt.version);
        }
    }
    text(app, "Fernstone " FERN_APP_VERSION, 2, gh(app) - 10, FERN_RGBA(120, 120, 120, 255));
}

static void draw_measure(FernApp *app) {
    float cx = gw(app) / 2;
    char line[128];
    // A slice of the benchmark in each frame keeps the screen live.
    fern_bench_step(&app->bench, 25);
    background(app);
    text_center(app, "Measuring this device", cx, gh(app) / 2 - 20, 0xFFFFFFFF);
    snprintf(line, sizeof(line), "Memory %d MB, OpenGL ES %d", app->device.ram_mb, app->device.gl);
    text_center(app, line, cx, gh(app) / 2 - 4, FERN_RGBA(200, 200, 200, 255));
    rect(app, cx - 100, gh(app) / 2 + 12, 200, 6, FERN_RGBA(60, 60, 60, 255));
    rect(app, cx - 100, gh(app) / 2 + 12, (float)(200 * app->bench.work_s / 2.0), 6, FERN_RGBA(120, 200, 90, 255));
    if (app->bench.done) {
        app->device.bench = app->bench.score;
        device_save(app);
        fern_log("device: %d MB, GLES %d, bench %d (%ld meshes in %.2f s)", app->device.ram_mb, app->device.gl,
                 app->device.bench, app->bench.meshes, app->bench.work_s);
        app->screen = SCREEN_MENU;
    }
}

static void draw_versions(FernApp *app) {
    float cx = gw(app) / 2, top = 31;
    const char *ids[64];
    FernTag tags[64];
    int i, n = fern_version_count(), rows = (n + 1) / 2;
    char line[128];
    background(app);
    text_center(app, "Select a version", cx, 6, FERN_RGBA(255, 255, 255, 255));
    snprintf(line, sizeof(line), "This device: %d MB, OpenGL ES %d, mesh score %d", app->device.ram_mb, app->device.gl,
             app->device.bench);
    text_center(app, line, cx, 18, FERN_RGBA(160, 160, 160, 255));
    for (i = 0; i < n && i < 64; i++) ids[i] = fern_version_at(i)->id;
    fern_manifest_tags(&app->device, ids, n, tags);
    for (i = 0; i < n && i < 64; i++) {
        const FernVersion *v = fern_version_at(i);
        float x = cx + (i < rows ? -154 : 4), y = top + (i % rows) * 22;
        const char *tag = tags[i] == TAG_RECOMMENDED ? " Recommended" : tags[i] == TAG_MAY_LAG ? " May lag" : "";
        int enabled = tags[i] != TAG_TOO_OLD_DEVICE;
        snprintf(line, sizeof(line), "%s%s%s", v->id, v->tested ? "" : "*", tag);
        if (button(app, line, x, y, 150, enabled)) {
            snprintf(app->opt.version, sizeof(app->opt.version), "%s", v->id);
            options_save(app);
            load_jar(app);
            app->screen = SCREEN_MENU;
        }
        if (tags[i] == TAG_RECOMMENDED) rect(app, x + 2, y + 2, 3, 16, FERN_RGBA(85, 255, 85, 255));
        if (tags[i] == TAG_MAY_LAG) rect(app, x + 2, y + 2, 3, 16, FERN_RGBA(255, 170, 0, 255));
        if (!strcmp(v->id, app->opt.version)) rect(app, x - 3, y + 2, 2, 16, 0xFFFFFFFF);
    }
    text_center(app, "* not tested on a server yet", cx, top + rows * 22, FERN_RGBA(140, 140, 140, 255));
    if (button(app, "Back", cx - 100, top + rows * 22 + 10, 200, 1)) app->screen = SCREEN_MENU;
}

static void draw_gone(FernApp *app) {
    float cx = gw(app) / 2;
    background(app);
    text_center(app, "Disconnected", cx, gh(app) / 2 - 30, FERN_RGBA(255, 255, 255, 255));
    text_center(app, app->client.reason, cx, gh(app) / 2 - 12, FERN_RGBA(255, 160, 160, 255));
    if (button(app, "Back to the menu", cx - 100, gh(app) / 2 + 10, 200, 1)) app->screen = SCREEN_MENU;
}

/* ---- the game view ---- */

// The color of the sky at a world time, as Beta draws it.
static float day_light(int64_t time) {
    float t = (float)((time % 24000 + 24000) % 24000) / 24000.0f - 0.25f, c;
    if (t < 0) t += 1;
    t = t + ((1.0f - (cosf(t * (float)M_PI) + 1.0f) / 2.0f) - t) / 3.0f;
    c = cosf(t * (float)M_PI * 2) * 2 + 0.5f;
    if (c < 0) c = 0;
    if (c > 1) c = 1;
    return c;
}

static int item_icon(int id) {
    // The tools: the rows of gui/items.png.
    static const struct { int16_t id; int16_t icon; } map[] = {
        {256, 82}, {257, 98}, {258, 114}, {259, 5}, {260, 10}, {261, 21}, {262, 37}, {263, 7}, {264, 55},
        {265, 23}, {266, 39}, {267, 66}, {268, 64}, {269, 80}, {270, 96}, {271, 112}, {272, 65}, {273, 81},
        {274, 97}, {275, 113}, {276, 67}, {277, 83}, {278, 99}, {279, 115}, {280, 53}, {281, 71}, {282, 72},
        {283, 68}, {284, 84}, {285, 100}, {286, 116}, {287, 8}, {288, 24}, {289, 40}, {290, 128}, {291, 129},
        {292, 130}, {293, 131}, {294, 132}, {295, 9}, {296, 25}, {297, 41}, {318, 6}, {319, 87}, {320, 88},
        {321, 26}, {322, 11}, {323, 42}, {324, 43}, {325, 74}, {326, 75}, {327, 76}, {328, 135}, {329, 104},
        {330, 44}, {331, 56}, {332, 14}, {333, 136}, {334, 103}, {335, 77}, {336, 22}, {337, 57}, {338, 27},
        {339, 58}, {340, 59}, {341, 30}, {344, 12}, {345, 54}, {347, 70}, {348, 73}, {352, 28}, {353, 13},
        {354, 29}, {355, 45}, {357, 92}, {359, 13 * 16 + 13},
    };
    unsigned i;
    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (map[i].id == id) return map[i].icon;
    return -1;
}

static void draw_item(FernApp *app, const FernItem *it, float x, float y) {
    float u = app->ui;
    char n[8];
    if (it->id <= 0) return;
    if (it->id < 256) {
        int tile = fern_block_texture(FERN_STATE(fern_block_from_wire(14, it->id), it->damage), 3);
        if (app->tex_terrain)
            fern_d2_image(app->d2, app->tex_terrain, (x + 1) * u, (y + 1) * u, 14 * u, 14 * u, (tile & 15) / 16.0f,
                          (tile >> 4) / 16.0f, ((tile & 15) + 1) / 16.0f, ((tile >> 4) + 1) / 16.0f, 0xFFFFFFFF);
    } else {
        int icon = item_icon(it->id);
        if (icon >= 0 && app->tex_items)
            fern_d2_image(app->d2, app->tex_items, x * u, y * u, 16 * u, 16 * u, (icon & 15) / 16.0f, (icon >> 4) / 16.0f,
                          ((icon & 15) + 1) / 16.0f, ((icon >> 4) + 1) / 16.0f, 0xFFFFFFFF);
        else
            rect(app, x + 3, y + 3, 10, 10, FERN_RGBA(160, 120, 200, 255));
    }
    if (it->count > 1) {
        snprintf(n, sizeof(n), "%d", it->count);
        text(app, n, x + 17 - text_w(app, n), y + 9, 0xFFFFFFFF);
    }
}

static void draw_hud(FernApp *app) {
    FernClient *c = &app->client;
    float u = app->ui, cx = gw(app) / 2, hx = cx - 91, hy = gh(app) - 22;
    int i;
    char line[256];
    // The crosshair.
    if (!app->config.touch || app->touch_attack) {
        rect(app, cx - 4.5f, gh(app) / 2 - 0.5f, 9, 1, FERN_RGBA(255, 255, 255, 200));
        rect(app, cx - 0.5f, gh(app) / 2 - 4.5f, 1, 9, FERN_RGBA(255, 255, 255, 200));
    }
    // The hotbar.
    if (app->tex_gui) {
        fern_d2_image(app->d2, app->tex_gui, hx * u, hy * u, 182 * u, 22 * u, 0, 0, 182.0f / 256, 22.0f / 256, 0xFFFFFFFF);
        fern_d2_image(app->d2, app->tex_gui, (hx - 1 + c->held * 20) * u, (hy - 1) * u, 24 * u, 24 * u, 0, 22.0f / 256,
                      24.0f / 256, 46.0f / 256, 0xFFFFFFFF);
    }
    for (i = 0; i < 9; i++) {
        draw_item(app, &c->inv[36 + i], hx + 3 + i * 20, hy + 3);
        if (app->config.touch && hit(app, hx + i * 20, hy, 20, 22)) fern_client_hold(c, i);
    }
    // The health: ten hearts from gui/icons.png.
    if (app->tex_icons) {
        for (i = 0; i < 10; i++) {
            float x = hx + i * 8, y = hy - 10;
            fern_d2_image(app->d2, app->tex_icons, x * u, y * u, 9 * u, 9 * u, 16.0f / 256, 0, 25.0f / 256, 9.0f / 256,
                          0xFFFFFFFF);
            if (c->health > i * 2 + 1)
                fern_d2_image(app->d2, app->tex_icons, x * u, y * u, 9 * u, 9 * u, 52.0f / 256, 0, 61.0f / 256, 9.0f / 256,
                              0xFFFFFFFF);
            else if (c->health == i * 2 + 1)
                fern_d2_image(app->d2, app->tex_icons, x * u, y * u, 9 * u, 9 * u, 61.0f / 256, 0, 70.0f / 256, 9.0f / 256,
                              0xFFFFFFFF);
        }
    }
    // The chat: the last lines for ten seconds, or more when it is open.
    for (i = 0; i < (app->chat_open ? 20 : 10); i++) {
        const FernChatLine *l = fern_client_chat_line(c, i);
        float age, y;
        int alpha;
        if (!l) break;
        age = (float)(app->now - l->time);
        if (!app->chat_open && age > 10) continue;
        alpha = app->chat_open ? 255 : age > 9 ? (int)(255 * (10 - age)) : 255;
        y = gh(app) - 48 - i * 9;
        rect(app, 2, y - 1, 320, 9, FERN_RGBA(0, 0, 0, alpha / 2));
        text(app, l->text, 4, y, FERN_RGBA(255, 255, 255, alpha));
    }
    if (app->chat_open) {
        rect(app, 2, gh(app) - 14, gw(app) - 4, 12, FERN_RGBA(0, 0, 0, 128));
        snprintf(line, sizeof(line), "> %s%s", app->chat_line, ((int)(app->now * 2) & 1) ? "_" : "");
        text(app, line, 4, gh(app) - 12, 0xFFFFFFFF);
    }
    // The debug screen.
    if (app->debug) {
        int meshes, drawn, queued;
        fern_terrain_stats(app->terrain, &meshes, &drawn, &queued);
        snprintf(line, sizeof(line), "Fernstone %s (%.1f ms) %s", app->opt.version, app->frame_ms, app->config.platform);
        text(app, line, 2, 2, 0xFFFFFFFF);
        snprintf(line, sizeof(line), "x %.2f y %.2f z %.2f", c->body.x, c->body.y, c->body.z);
        text(app, line, 2, 12, 0xFFFFFFFF);
        snprintf(line, sizeof(line), "columns %d, meshes %d, drawn %d, queued %d", c->world.columns, meshes, drawn, queued);
        text(app, line, 2, 22, 0xFFFFFFFF);
    }
    if (c->state == CLIENT_PLAY && c->spawned && c->health <= 0) {
        rect(app, 0, 0, gw(app), gh(app), FERN_RGBA(120, 0, 0, 120));
        fern_d2_text(app->d2, "You died!", (cx - text_w(app, "You died!")) * u, (gh(app) / 2 - 40) * u, u * 2,
                     0xFFFFFFFF, 1);
        if (button(app, "Respawn", cx - 100, gh(app) / 2, 200, 1)) fern_client_respawn(c);
        if (button(app, "Disconnect", cx - 100, gh(app) / 2 + 24, 200, 1)) leave_game(app);
    }
    if (c->state != CLIENT_PLAY || !c->spawned) {
        rect(app, 0, 0, gw(app), gh(app), FERN_RGBA(0, 0, 0, 160));
        text_center(app, c->state == CLIENT_PLAY ? "Downloading the terrain" : "Connecting to the server", cx,
                    gh(app) / 2 - 4, 0xFFFFFFFF);
    }
}

static void draw_touch_controls(FernApp *app) {
    float h = gh(app), w = gw(app);
    int i;
    // The stick: a ring where the left thumb went down.
    for (i = 0; i < MAX_TOUCHES; i++) {
        Touch *t = &app->touch[i];
        if (t->id >= 0 && t->kind == 1) {
            float bx = t->x0 / app->ui, by = t->y0 / app->ui;
            rect(app, bx - 24, by - 24, 48, 48, FERN_RGBA(255, 255, 255, 40));
            rect(app, bx - 8 + app->stick_x * 20, by - 8 + app->stick_y * 20, 16, 16, FERN_RGBA(255, 255, 255, 120));
        }
    }
    // The buttons: jump, sneak, chat, and the pause button.
    rect(app, w - 46, h - 70, 40, 40, FERN_RGBA(255, 255, 255, app->touch_jump ? 110 : 50));
    text_center(app, "Jump", w - 26, h - 54, 0xFFFFFFFF);
    rect(app, w - 90, h - 50, 40, 26, FERN_RGBA(255, 255, 255, app->touch_sneak ? 110 : 50));
    text_center(app, "Sneak", w - 70, h - 41, 0xFFFFFFFF);
    rect(app, w - 46, 4, 42, 16, FERN_RGBA(255, 255, 255, 50));
    text_center(app, "Chat", w - 25, 8, 0xFFFFFFFF);
    rect(app, 4, 4, 42, 16, FERN_RGBA(255, 255, 255, 50));
    text_center(app, "Pause", 25, 8, 0xFFFFFFFF);
}

static void draw_world(FernApp *app) {
    FernClient *c = &app->client;
    FernMat proj, view, vp;
    FernTerrainLook look;
    float day = day_light(c->world.time);
    double ex = c->body.x, ey = c->body.y + FERN_EYE_HEIGHT, ez = c->body.z;
    float sky[3] = {0.6f * day + 0.02f, 0.75f * day + 0.02f, 1.0f * day + 0.05f};
    int i;
    look.day = day * 0.85f + 0.15f;
    look.fog_end = app->opt.render * 16.0f - 4;
    look.fog_start = look.fog_end * 0.6f;
    look.fog[0] = sky[0];
    look.fog[1] = sky[1];
    look.fog[2] = sky[2];
    glClearColor(sky[0], sky[1], sky[2], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!app->have_jar || !c->spawned) return;
    proj = fern_mat_perspective(70.0f, (float)app->width / (float)app->height, 0.05f, app->opt.render * 16.0f + 32);
    // The view: pitch around x, then yaw around y. The camera looks along +z at yaw 0.
    view = fern_mat_mul(fern_mat_rotate(c->body.pitch, 1, 0, 0), fern_mat_rotate(c->body.yaw + 180.0f, 0, 1, 0));
    vp = fern_mat_mul(proj, view);
    fern_terrain_update(app->terrain, &c->world, ex, ey, ez, app->opt.render);
    fern_terrain_draw(app->terrain, &vp, ex, ey, ez, &look);

    // The other players, the mobs, and the items.
    fern_entities_step(&c->ents, 0.3);
    for (i = 0; i < FERN_ENTITY_MAX; i++) {
        FernEntity *e = &c->ents.e[i];
        int bx, by, bz;
        uint8_t light;
        float ox, oy, oz;
        if (!e->used || e->id == c->self_id) continue;
        ox = (float)(e->px - ex);
        oy = (float)(e->py - ey);
        oz = (float)(e->pz - ez);
        if (ox * ox + oz * oz > look.fog_end * look.fog_end) continue;
        // Do not draw a player that stands in the camera.
        if (e->kind == ENT_PLAYER && ox * ox + oz * oz < 0.25f && oy > -2.0f && oy < 0.5f) continue;
        bx = (int)floor(e->px);
        by = (int)floor(e->py) + 1;
        bz = (int)floor(e->pz);
        light = (uint8_t)(fern_world_sky(&c->world, bx, by, bz) << 4 | fern_world_light(&c->world, bx, by, bz));
        app->models.count = 0;
        if (e->kind == ENT_PLAYER) {
            double moved = fabs(e->x - e->px) + fabs(e->z - e->pz);
            float walk = moved > 0.01 ? (float)(app->now * 10.0) : 0;
            fern_model_player(&app->models, e->yaw, e->pitch, walk, e->swing > 0 ? e->swing : 0, e->crouch, light);
            fern_terrain_draw_quads(app->terrain, &vp, app->tex_char, ox, oy, oz, app->models.v, app->models.count, &look);
        } else if (e->kind == ENT_MOB) {
            // A colored box for each mob kind: creeper, skeleton, spider,
            // zombie, slime, pig, sheep, cow, chicken, squid, wolf.
            uint8_t r = 200, g = 200, b = 200;
            float w = 0.6f, h = 1.8f;
            switch (e->type) {
            case 50: r = 80; g = 200; b = 80; h = 1.7f; break;
            case 51: r = 220; g = 220; b = 220; break;
            case 52: r = 70; g = 50; b = 50; w = 1.4f; h = 0.9f; break;
            case 54: r = 60; g = 140; b = 90; break;
            case 55: r = 100; g = 220; b = 100; w = h = 1.0f; break;
            case 90: r = 240; g = 160; b = 160; w = 0.9f; h = 0.9f; break;
            case 91: r = 240; g = 240; b = 240; w = 0.9f; h = 1.3f; break;
            case 92: r = 90; g = 70; b = 50; w = 0.9f; h = 1.3f; break;
            case 93: r = 255; g = 255; b = 255; w = 0.4f; h = 0.7f; break;
            case 94: r = 40; g = 60; b = 120; w = h = 0.95f; break;
            case 95: r = 200; g = 200; b = 200; w = 0.6f; h = 0.8f; break;
            default: break;
            }
            fern_model_mob(&app->models, e->yaw, w, h, r, g, b, light);
            fern_terrain_draw_quads(app->terrain, &vp, app->tex_char, ox, oy, oz, app->models.v, app->models.count, &look);
        } else if (e->kind == ENT_ITEM && e->item > 0 && e->item < 256) {
            int tile = fern_block_texture(FERN_STATE(fern_block_from_wire(14, e->item), 0), 3);
            fern_model_item(&app->models, tile, (float)fmod(app->now * 60.0, 360.0), light);
            fern_terrain_draw_quads(app->terrain, &vp, app->tex_terrain, ox, oy + 0.1f, oz, app->models.v,
                                    app->models.count, &look);
        }
    }

    // The outline of the block in view: thin dark bars on the edges of the
    // face that the ray hits. The bars get lighter while the block breaks.
    if (c->target.hit) {
        float x0 = (float)(c->target.x - ex), y0 = (float)(c->target.y - ey), z0 = (float)(c->target.z - ez);
        static const int corner[6][4][3] = {
            {{0, 0, 0}, {16, 0, 0}, {16, 0, 16}, {0, 0, 16}}, {{0, 16, 16}, {16, 16, 16}, {16, 16, 0}, {0, 16, 0}},
            {{16, 0, 0}, {0, 0, 0}, {0, 16, 0}, {16, 16, 0}}, {{0, 0, 16}, {16, 0, 16}, {16, 16, 16}, {0, 16, 16}},
            {{0, 0, 0}, {0, 0, 16}, {0, 16, 16}, {0, 16, 0}}, {{16, 0, 16}, {16, 0, 0}, {16, 16, 0}, {16, 16, 16}}};
        FernVertex v[16];
        int f = c->target.face, k, e;
        uint8_t shade = c->digging ? (uint8_t)(40 + c->dig_done * 200) : 20;
        memset(v, 0, sizeof(v));
        for (e = 0; e < 4; e++) {
            const int *a0 = corner[f][e], *a1 = corner[f][(e + 1) & 3];
            // The bar goes from one corner to the next, 1/16 block wide, toward the middle.
            int in0[3], in1[3];
            for (k = 0; k < 3; k++) {
                int mid = (corner[f][0][k] + corner[f][2][k]) / 2;
                in0[k] = a0[k] + (mid > a0[k] ? 1 : mid < a0[k] ? -1 : 0);
                in1[k] = a1[k] + (mid > a1[k] ? 1 : mid < a1[k] ? -1 : 0);
            }
            for (k = 0; k < 4; k++) {
                const int *p = k == 0 ? a0 : k == 1 ? a1 : k == 2 ? in1 : in0;
                FernVertex *q = &v[e * 4 + k];
                q->x = (int16_t)p[0];
                q->y = (int16_t)p[1];
                q->z = (int16_t)p[2];
                // A plain part of the snow tile, so that the color is the shade.
                q->u = (uint16_t)(2 * 65535 / 16 + 600);
                q->v = (uint16_t)(4 * 65535 / 16 + 600);
                q->r = q->g = q->b = shade;
                q->light = 0xFF;
            }
        }
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1, -1);
        fern_terrain_draw_quads(app->terrain, &vp, app->tex_terrain, x0, y0, z0, v, 16, &look);
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
}

/* ---- touch input ---- */

static Touch *touch_of(FernApp *app, int id) {
    int i;
    for (i = 0; i < MAX_TOUCHES; i++)
        if (app->touch[i].id == id) return &app->touch[i];
    return NULL;
}

void fern_app_pointer(FernApp *app, int id, int phase, float x, float y) {
    Touch *t;
    float gx = x / app->ui, gy = y / app->ui, w = gw(app), h = gh(app);
    // On the death screen, a touch goes to the buttons.
    int game = app->screen == SCREEN_GAME && !app->chat_open && app->client.health > 0;
    if (phase == FERN_POINTER_DOWN) {
        int i;
        for (i = 0; i < MAX_TOUCHES && app->touch[i].id >= 0; i++) {}
        if (i == MAX_TOUCHES) return;
        t = &app->touch[i];
        memset(t, 0, sizeof(*t));
        t->id = id;
        t->x0 = t->x = x;
        t->y0 = t->y = y;
        t->t0 = app->now;
        t->kind = 0;
        if (game && app->config.touch) {
            if (gx > w - 46 && gy > h - 70 && gy < h - 30) {
                t->kind = 2;
                app->touch_jump = 1;
            } else if (gx > w - 90 && gx < w - 50 && gy > h - 50 && gy < h - 24) {
                t->kind = 2;
                app->touch_sneak = !app->touch_sneak;
            } else if (gx > w - 46 && gy < 20) {
                t->kind = 2;
                open_chat(app, "");
            } else if (gx < 46 && gy < 20) {
                t->kind = 2;
                app->screen = SCREEN_PAUSE;
            } else if (gy > h - 24 && fabsf(gx - w / 2) < 91) {
                t->kind = 2;
                pending_click.clicked = 1;
                pending_click.x = x;
                pending_click.y = y;
            } else if (gx < w * 0.4f) {
                t->kind = 1;
            }
        }
        return;
    }
    t = touch_of(app, id);
    if (!t) return;
    if (phase == FERN_POINTER_MOVE) {
        float dx = x - t->x, dy = y - t->y;
        t->x = x;
        t->y = y;
        if (fabsf(x - t->x0) + fabsf(y - t->y0) > 12 * app->scale) t->moved = 1;
        if (game && app->config.touch && t->kind == 0 && !app->touch_attack) {
            fern_app_look(app, dx * 2.0f / app->scale, dy * 2.0f / app->scale);
        } else if (game && t->kind == 0 && app->touch_attack) {
            fern_app_look(app, dx * 1.2f / app->scale, dy * 1.2f / app->scale);
        }
        return;
    }
    // Up.
    if (t->kind == 0) {
        if (game && app->config.touch) {
            if (!t->moved && !app->touch_attack && app->now - t->t0 < 0.3) app->input.use = 1;
        } else {
            pending_click.clicked = 1;
            pending_click.x = x;
            pending_click.y = y;
        }
        app->touch_attack = 0;
    }
    if (t->kind == 2) app->touch_jump = 0;
    t->id = -1;
}

static void update_touch(FernApp *app) {
    int i;
    app->stick_x = app->stick_y = 0;
    for (i = 0; i < MAX_TOUCHES; i++) {
        Touch *t = &app->touch[i];
        if (t->id < 0) continue;
        if (t->kind == 1) {
            float r = 20 * app->ui;
            app->stick_x = (t->x - t->x0) / r;
            app->stick_y = (t->y - t->y0) / r;
            if (app->stick_x > 1) app->stick_x = 1;
            if (app->stick_x < -1) app->stick_x = -1;
            if (app->stick_y > 1) app->stick_y = 1;
            if (app->stick_y < -1) app->stick_y = -1;
        }
        // Hold a look touch still to dig.
        if (t->kind == 0 && !t->moved && app->now - t->t0 > 0.35) app->touch_attack = 1;
    }
}

/* ---- the frame ---- */

static void update_game_input(FernApp *app) {
    FernInput *in = &app->input;
    if (app->chat_open) {
        in->forward = in->strafe = 0;
        in->jump = in->sneak = 0;
        in->attack = 0;
        return;
    }
    in->forward = (float)(app->keys[FK_FORWARD] - app->keys[FK_BACK]);
    in->strafe = (float)(app->keys[FK_LEFT] - app->keys[FK_RIGHT]);
    in->jump = app->keys[FK_JUMP];
    in->sneak = app->keys[FK_SNEAK];
    if (app->config.touch) {
        update_touch(app);
        if (app->stick_x || app->stick_y) {
            in->forward = -app->stick_y;
            in->strafe = -app->stick_x;
        }
        in->jump |= app->touch_jump;
        in->sneak |= app->touch_sneak;
        in->attack = app->touch_attack;
    }
}

static void draw_pause(FernApp *app) {
    float cx = gw(app) / 2, y = gh(app) / 2 - 40;
    rect(app, 0, 0, gw(app), gh(app), FERN_RGBA(0, 0, 0, 120));
    text_center(app, "Game menu", cx, y - 20, 0xFFFFFFFF);
    if (button(app, "Back to the game", cx - 100, y, 200, 1)) app->screen = SCREEN_GAME;
    y += 24;
    if (app->client.health <= 0 && button(app, "Respawn", cx - 100, y, 200, 1)) {
        fern_client_respawn(&app->client);
        app->screen = SCREEN_GAME;
    }
    y += 24;
    if (button(app, "Disconnect", cx - 100, y, 200, 1)) leave_game(app);
}

static void poll_probe(FernApp *app) {
    FernPacket p;
    size_t used;
    FernNetState s = fern_net_poll(&app->probe_net);
    if (s == FERN_NET_FAILED || s == FERN_NET_CLOSED) {
        fern_log("probe: %s", app->probe_net.error);
        app->probing = 0;
        return;
    }
    if (fern_packet_decode(fern_version_latest(), app->probe_net.in, app->probe_net.in_len, &p, &used) == FERN_DECODE_OK) {
        if (p.id == 0x02) fern_log("probe: the server answered the handshake: \"%s\"", p.u.str.text);
        fern_net_close(&app->probe_net);
        app->probing = 0;
    }
}

void fern_app_probe(FernApp *app, const char *host, int port, const char *name) {
    FernWriter w;
    fern_log("probe: connect to %s:%d as %s", host, port, name);
    if (!fern_net_connect(&app->probe_net, host, port)) {
        fern_log("probe: %s", app->probe_net.error);
        return;
    }
    fern_writer_init(&w);
    fern_send_handshake(&w, fern_version_latest(), name);
    fern_net_send(&app->probe_net, w.data, w.len);
    fern_writer_free(&w);
    app->probing = 1;
}

void fern_app_command(FernApp *app, const char *line) {
    char a[128], b[128];
    float x, y;
    if (sscanf(line, "connect %127s %127s", a, b) == 2) {
        snprintf(app->opt.server, sizeof(app->opt.server), "%s", a);
        snprintf(app->opt.name, sizeof(app->opt.name), "%s", b);
        if (app->have_jar) connect_now(app);
    } else if (!strncmp(line, "say ", 4)) {
        fern_client_chat(&app->client, line + 4);
    } else if (sscanf(line, "walk %f %f", &x, &y) == 2) {
        app->keys[FK_FORWARD] = x > 0;
        app->keys[FK_BACK] = x < 0;
        app->keys[FK_LEFT] = y > 0;
        app->keys[FK_RIGHT] = y < 0;
    } else if (sscanf(line, "look %f %f", &x, &y) == 2) {
        app->input.yaw = x;
        app->input.pitch = y;
    } else if (sscanf(line, "attack %f", &x) == 1) {
        app->input.attack = x != 0;
    } else if (!strcmp(line, "use")) {
        app->input.use = 1;
    } else if (!strcmp(line, "jump")) {
        app->keys[FK_JUMP] = 1;
    } else if (sscanf(line, "hold %f", &x) == 1) {
        fern_client_hold(&app->client, (int)x);
    } else if (sscanf(line, "shot %1023s", app->shot_path) == 1) {
        // The next frame saves itself to the file.
    } else if (!strcmp(line, "debug")) {
        app->debug = !app->debug;
    } else if (sscanf(line, "screen %127s", a) == 1) {
        if (!strcmp(a, "versions")) app->screen = SCREEN_VERSIONS;
        else if (!strcmp(a, "menu")) app->screen = SCREEN_MENU;
    }
}

void fern_app_frame(FernApp *app, double now) {
    if (!app->gl_ready) {
        app->terrain = fern_terrain_new();
        app->d2 = fern_d2_new();
        app->gl_ready = 1;
        load_jar(app);
        app->last_frame = now;
        // The first launch measures the device for the version browser.
        if (!device_load(app) || getenv("FERN_MEASURE")) {
            app->device.ram_mb = fern_device_ram(app->config.ram_mb);
            app->device.gl = gl_major();
            fern_bench_start(&app->bench);
            app->screen = SCREEN_MEASURE;
        }
    }
    app->frame_ms = (float)((now - app->last_frame) * 1000.0);
    app->last_frame = now;
    app->now = now;
    if (app->probing) poll_probe(app);
    glViewport(0, 0, app->width, app->height);

    if (app->screen == SCREEN_GAME || app->screen == SCREEN_PAUSE) {
        if (app->screen == SCREEN_GAME) update_game_input(app);
        fern_client_update(&app->client, &app->input, now);
        if (app->client.ticks > 0) app->input.use = 0;
        if (app->client.state == CLIENT_GONE) app->screen = SCREEN_GONE;
        app->input.yaw = app->screen == SCREEN_GAME ? app->input.yaw : app->input.yaw;
        draw_world(app);
    } else {
        glClearColor(0.1f, 0.1f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    // Keep the yaw of the player when the server moves it.
    if (app->client.spawned && app->client.ticks <= 1) {
        app->input.yaw = app->client.body.yaw;
        app->input.pitch = app->client.body.pitch;
    }

    fern_d2_begin(app->d2, app->width, app->height);
    switch (app->screen) {
    case SCREEN_MEASURE: draw_measure(app); break;
    case SCREEN_MENU: draw_menu(app); break;
    case SCREEN_VERSIONS: draw_versions(app); break;
    case SCREEN_GAME:
        draw_hud(app);
        if (app->config.touch && !app->chat_open) draw_touch_controls(app);
        break;
    case SCREEN_PAUSE:
        draw_hud(app);
        draw_pause(app);
        break;
    case SCREEN_GONE: draw_gone(app); break;
    }
    fern_d2_flush(app->d2);
    pending_click.clicked = 0;
    if (app->shot_path[0]) {
        uint8_t *px = malloc((size_t)app->width * app->height * 4), *flip = malloc((size_t)app->width * app->height * 4);
        int y;
        glReadPixels(0, 0, app->width, app->height, GL_RGBA, GL_UNSIGNED_BYTE, px);
        for (y = 0; y < app->height; y++)
            memcpy(flip + (size_t)y * app->width * 4, px + (size_t)(app->height - 1 - y) * app->width * 4, (size_t)app->width * 4);
        fern_log("shot: %s %s", app->shot_path, fern_png_write(app->shot_path, flip, app->width, app->height) ? "saved" : "failed");
        free(px);
        free(flip);
        app->shot_path[0] = 0;
    }
    // A tap on the empty part of a menu takes the focus away.
}

void fern_app_free(FernApp *app) {
    leave_game(app);
    fern_client_free(&app->client);
    free(app->models.v);
    if (app->terrain) fern_terrain_free(app->terrain);
    free(app);
}
