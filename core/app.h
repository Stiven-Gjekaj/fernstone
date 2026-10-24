// app.h: the interface between a platform shell and the core.
// A shell makes a window and a GL context, reads the input, and runs the
// frame loop. It calls these functions. The core does all other work.
#ifndef FERN_APP_H
#define FERN_APP_H

typedef struct FernApp FernApp;

typedef struct {
    const char *data_dir;   // A directory that the app can write.
    double ram_mb;          // The physical memory of the device.
    const char *platform;   // "ios6", "ios", or "macos".
} FernAppConfig;

FernApp *fern_app_new(const FernAppConfig *config);
// The size of the drawable in pixels, and the pixels in one point.
void fern_app_resize(FernApp *app, int width, int height, float scale);
// Run the game and draw one frame. now is a time in seconds.
void fern_app_frame(FernApp *app, double now);
// Start a connection, for a test: send the handshake and log the answer.
void fern_app_probe(FernApp *app, const char *host, int port, const char *name);
void fern_app_free(FernApp *app);

#endif
