// app.h: the interface between a platform shell and the core.
// A shell makes a window and a GL context, reads the input, and runs the
// frame loop. It calls these functions. The core does all other work: the
// menus, the game, the network, and the drawing.
#ifndef FERN_APP_H
#define FERN_APP_H

typedef struct FernApp FernApp;

typedef struct {
    const char *data_dir;   // A directory that the app can write.
    double ram_mb;          // The physical memory of the device.
    const char *platform;   // "ios6", "ios", or "macos".
    int touch;              // 1: draw the touch controls.
} FernAppConfig;

// The keys that the core reads. A shell maps its key codes to these.
typedef enum {
    FK_NONE, FK_FORWARD, FK_BACK, FK_LEFT, FK_RIGHT, FK_JUMP, FK_SNEAK,
    FK_CHAT, FK_COMMAND, FK_ESCAPE, FK_ENTER, FK_BACKSPACE, FK_TAB, FK_UP, FK_DOWN,
    FK_DEBUG, FK_INVENTORY, FK_1, FK_2, FK_3, FK_4, FK_5, FK_6, FK_7, FK_8, FK_9,
    FK_COUNT
} FernKey;

enum { FERN_POINTER_DOWN, FERN_POINTER_MOVE, FERN_POINTER_UP };

FernApp *fern_app_new(const FernAppConfig *config);
// The size of the drawable in pixels, and the pixels in one point.
void fern_app_resize(FernApp *app, int width, int height, float scale);
// Run the game and draw one frame. now is a time in seconds.
void fern_app_frame(FernApp *app, double now);
void fern_app_key(FernApp *app, FernKey key, int down);
// Text that the user typed, as UTF-8.
void fern_app_text(FernApp *app, const char *utf8);
// A touch, or the mouse when the core does not capture it. x and y are in
// pixels of the drawable. id tells the touches apart.
void fern_app_pointer(FernApp *app, int id, int phase, float x, float y);
// The mouse moved while the core captures it (dx, dy in pixels).
void fern_app_look(FernApp *app, float dx, float dy);
// A mouse button while the core captures the mouse: 0 left, 1 right.
void fern_app_button(FernApp *app, int button, int down);
// The mouse wheel: +1 or -1 for each step.
void fern_app_wheel(FernApp *app, int steps);
// 1 if the shell must capture and hide the mouse (the game is in front).
int fern_app_wants_capture(FernApp *app);
// 1 if a text field has the focus, so a touch shell shows the keyboard.
int fern_app_wants_keyboard(FernApp *app);
// 1 if the user asked to import a jar. The shell shows its file picker,
// copies the file to "<data_dir>/jars/<version>.jar", and calls
// fern_app_jars_changed. The call clears the request.
int fern_app_wants_import(FernApp *app, char *version, int cap);
void fern_app_jars_changed(FernApp *app);
// The data directory that the core uses.
const char *fern_app_data_dir(FernApp *app);
// Start a connection, for a test: send the handshake and log the answer.
void fern_app_probe(FernApp *app, const char *host, int port, const char *name);
// Run a command, for a test or a debug build: "connect HOST NAME",
// "say TEXT", "walk F S", "look YAW PITCH", "attack 0|1", "use", "hold N",
// "screen NAME", "inventory", "click SLOT RIGHT", "closewin", and the test
// commands "fakemob TYPE", "fakeitem ID", "lookmob", and "shot FILE".
void fern_app_command(FernApp *app, const char *line);
void fern_app_free(FernApp *app);

#endif
