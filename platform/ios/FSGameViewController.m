#import <QuartzCore/QuartzCore.h>
#import "FSGameViewController.h"
#include <EGL/egl.h>
#include "app.h"

@implementation FSGameViewController {
    EGLDisplay _display;
    EGLSurface _surface;
    EGLContext _context;
    CADisplayLink *_link;
    FernApp *_app;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.multipleTouchEnabled = YES;
    self.view.backgroundColor = [UIColor blackColor];
    // Get the paths at runtime. LiveContainer gives each app its own
    // container, so a fixed path does not work.
    NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    FernAppConfig config = {0};
    config.data_dir = docs.fileSystemRepresentation;
    config.ram_mb = [NSProcessInfo processInfo].physicalMemory / (1024.0 * 1024.0);
    config.platform = "ios";
    _app = fern_app_new(&config);
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    if (!_surface) [self startGL];
    CGFloat scale = self.view.window.screen.scale ?: [UIScreen mainScreen].scale;
    self.view.layer.contentsScale = scale;
    EGLint w = 0, h = 0;
    eglQuerySurface(_display, _surface, EGL_WIDTH, &w);
    eglQuerySurface(_display, _surface, EGL_HEIGHT, &h);
    if (w <= 0 || h <= 0) {
        w = (EGLint)(self.view.bounds.size.width * scale);
        h = (EGLint)(self.view.bounds.size.height * scale);
    }
    fern_app_resize(_app, w, h, (float)scale);
}

- (void)startGL {
    static const EGLint configAttribs[] = {EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                                           EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24,
                                           EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE};
    static const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    _display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(_display, NULL, NULL);
    eglChooseConfig(_display, configAttribs, &config, 1, &count);
    self.view.layer.contentsScale = [UIScreen mainScreen].scale;
    _surface = eglCreateWindowSurface(_display, config, (__bridge EGLNativeWindowType)self.view.layer, NULL);
    _context = eglCreateContext(_display, config, EGL_NO_CONTEXT, contextAttribs);
    eglMakeCurrent(_display, _surface, _surface, _context);
    if (!_surface || !_context) NSLog(@"fernstone: EGL error 0x%x", eglGetError());

    NSString *probe = [[NSUserDefaults standardUserDefaults] stringForKey:@"FernProbe"];
    if (probe.length) fern_app_probe(_app, probe.UTF8String, 25565, "Fern15");

    _link = [CADisplayLink displayLinkWithTarget:self selector:@selector(frame:)];
    [_link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)setPaused:(BOOL)paused {
    _link.paused = paused;
}

- (void)frame:(CADisplayLink *)link {
    eglMakeCurrent(_display, _surface, _surface, _context);
    fern_app_frame(_app, CACurrentMediaTime());
    eglSwapBuffers(_display, _surface);
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}

- (BOOL)prefersHomeIndicatorAutoHidden {
    return YES;
}

@end
