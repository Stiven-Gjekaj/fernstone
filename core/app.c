#include "app.h"
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "net.h"
#include "packet.h"
#include "tri.h"

struct FernApp {
    FernAppConfig config;
    int width, height;
    float scale;
    int gl_ready;
    FernNet net;
    FernWriter out;
    int probing;
};

FernApp *fern_app_new(const FernAppConfig *config) {
    FernApp *app = calloc(1, sizeof(*app));
    app->config = *config;
    fern_net_init(&app->net);
    fern_writer_init(&app->out);
    fern_log_open(config->data_dir);
    fern_log("fernstone: start on %s, %.0f MB", config->platform, config->ram_mb);
    return app;
}

void fern_app_resize(FernApp *app, int width, int height, float scale) {
    app->width = width;
    app->height = height;
    app->scale = scale;
}

void fern_app_probe(FernApp *app, const char *host, int port, const char *name) {
    const FernVersion *v = fern_version_latest();
    fern_log("probe: connect to %s:%d as %s", host, port, name);
    if (!fern_net_connect(&app->net, host, port)) {
        fern_log("probe: %s", app->net.error);
        return;
    }
    fern_writer_reset(&app->out);
    fern_send_handshake(&app->out, v, name);
    fern_net_send(&app->net, app->out.data, app->out.len);
    app->probing = 1;
}

static void poll_probe(FernApp *app) {
    FernPacket p;
    size_t used;
    FernNetState s = fern_net_poll(&app->net);
    if (s == FERN_NET_FAILED || s == FERN_NET_CLOSED) {
        fern_log("probe: %s", app->net.error);
        app->probing = 0;
        return;
    }
    if (fern_packet_decode(fern_version_latest(), app->net.in, app->net.in_len, &p, &used) == FERN_DECODE_OK) {
        if (p.id == 0x02) fern_log("probe: the server answered the handshake: \"%s\"", p.u.str.text);
        fern_net_consume(&app->net, used);
        fern_net_close(&app->net);
        app->probing = 0;
    }
}

void fern_app_frame(FernApp *app, double now) {
    (void)now;
    if (!app->gl_ready) {
        if (!fern_tri_init()) fern_log("fernstone: the shader does not compile");
        app->gl_ready = 1;
    }
    if (app->probing) poll_probe(app);
    fern_tri_draw(app->width, app->height);
}

void fern_app_free(FernApp *app) {
    fern_net_close(&app->net);
    fern_writer_free(&app->out);
    free(app);
}
