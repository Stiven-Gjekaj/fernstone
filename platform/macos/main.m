// The macOS shell: an AppKit window with an EGL surface on ANGLE (Metal).
// It reads the keyboard and the mouse and runs the frame loop. The core
// does all other work.
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#include <EGL/egl.h>
#include "app.h"

@interface FSView : NSView
@property (nonatomic) FernApp *app;
@end

@implementation FSView {
    EGLDisplay _display;
    EGLSurface _surface;
    EGLContext _context;
    NSTimer *_timer;
    BOOL _captured;
    NSFileHandle *_commands;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)wantsUpdateLayer {
    return YES;
}

- (CALayer *)makeBackingLayer {
    return [CAMetalLayer layer];
}

- (void)startWithApp:(FernApp *)app {
    static const EGLint configAttribs[] = {EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                                           EGL_DEPTH_SIZE, 24, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE};
    static const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    self.app = app;
    self.wantsLayer = YES;
    self.layer.contentsScale = self.window.backingScaleFactor;
    _display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(_display, NULL, NULL);
    eglChooseConfig(_display, configAttribs, &config, 1, &count);
    _surface = eglCreateWindowSurface(_display, config, (__bridge EGLNativeWindowType)self.layer, NULL);
    _context = eglCreateContext(_display, config, EGL_NO_CONTEXT, contextAttribs);
    eglMakeCurrent(_display, _surface, _surface, _context);
    eglSwapInterval(_display, 1);
    _timer = [NSTimer timerWithTimeInterval:1.0 / 60.0 target:self selector:@selector(frame:) userInfo:nil repeats:YES];
    [[NSRunLoop mainRunLoop] addTimer:_timer forMode:NSRunLoopCommonModes];
    // A test can send commands through a file: FERN_COMMANDS names it.
    const char *cmd = getenv("FERN_COMMANDS");
    if (cmd) {
        _commands = [NSFileHandle fileHandleForReadingAtPath:@(cmd)];
    }
}

- (void)readCommands {
    NSData *data;
    if (!_commands) return;
    data = [_commands availableData];
    if (data.length) {
        NSString *s = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        for (NSString *line in [s componentsSeparatedByString:@"\n"])
            if (line.length) fern_app_command(self.app, line.UTF8String);
    }
}

- (void)frame:(NSTimer *)timer {
    CGFloat scale = self.window.backingScaleFactor;
    NSSize size = self.bounds.size;
    BOOL want;
    [self readCommands];
    eglMakeCurrent(_display, _surface, _surface, _context);
    fern_app_resize(self.app, (int)(size.width * scale), (int)(size.height * scale), (float)scale);
    fern_app_frame(self.app, CACurrentMediaTime());
    eglSwapBuffers(_display, _surface);
    char version[32];
    if (fern_app_wants_import(self.app, version, sizeof(version))) [self importJar:@(version)];
    want = fern_app_wants_capture(self.app) && self.window.isKeyWindow;
    if (want != _captured) {
        _captured = want;
        CGAssociateMouseAndMouseCursorPosition(!want);
        if (want) [NSCursor hide]; else [NSCursor unhide];
    }
}

- (void)importJar:(NSString *)version {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.message = [NSString stringWithFormat:@"Select your own %@ client jar.", version];
    panel.allowedFileTypes = @[@"jar"];
    if ([panel runModal] != NSModalResponseOK) return;
    NSString *docs = @(fern_app_data_dir(self.app));
    NSString *dest = [[docs stringByAppendingPathComponent:@"jars"] stringByAppendingPathComponent:
                      [version stringByAppendingPathExtension:@"jar"]];
    [[NSFileManager defaultManager] removeItemAtPath:dest error:nil];
    [[NSFileManager defaultManager] copyItemAtPath:panel.URL.path toPath:dest error:nil];
    fern_app_jars_changed(self.app);
}

static FernKey key_of(NSEvent *e) {
    switch (e.keyCode) {
    case 13: return FK_FORWARD;   // W
    case 1: return FK_BACK;       // S
    case 0: return FK_LEFT;       // A
    case 2: return FK_RIGHT;      // D
    case 49: return FK_JUMP;      // space
    case 17: return FK_CHAT;      // T
    case 44: return FK_COMMAND;   // slash
    case 53: return FK_ESCAPE;
    case 36: case 76: return FK_ENTER;
    case 51: return FK_BACKSPACE;
    case 48: return FK_TAB;
    case 126: return FK_UP;
    case 125: return FK_DOWN;
    case 99: return FK_DEBUG;     // F3
    case 14: return FK_INVENTORY; // E
    case 18: return FK_1; case 19: return FK_2; case 20: return FK_3; case 21: return FK_4;
    case 23: return FK_5; case 22: return FK_6; case 26: return FK_7; case 28: return FK_8; case 25: return FK_9;
    default: return FK_NONE;
    }
}

- (void)keyDown:(NSEvent *)e {
    FernKey k = key_of(e);
    BOOL typing = fern_app_wants_keyboard(self.app);
    if (typing && k != FK_ENTER && k != FK_ESCAPE && k != FK_BACKSPACE && k != FK_TAB) {
        fern_app_text(self.app, e.characters.UTF8String);
        return;
    }
    if (!e.isARepeat || k == FK_BACKSPACE) fern_app_key(self.app, k, 1);
}

- (void)keyUp:(NSEvent *)e {
    fern_app_key(self.app, key_of(e), 0);
}

- (void)flagsChanged:(NSEvent *)e {
    fern_app_key(self.app, FK_SNEAK, (e.modifierFlags & NSEventModifierFlagShift) != 0);
}

- (NSPoint)pixel:(NSEvent *)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    CGFloat s = self.window.backingScaleFactor;
    return NSMakePoint(p.x * s, (self.bounds.size.height - p.y) * s);
}

- (void)mouseDown:(NSEvent *)e {
    if (_captured) { fern_app_button(self.app, 0, 1); return; }
    NSPoint p = [self pixel:e];
    fern_app_pointer(self.app, 0, FERN_POINTER_DOWN, p.x, p.y);
}

- (void)mouseUp:(NSEvent *)e {
    if (_captured) { fern_app_button(self.app, 0, 0); return; }
    NSPoint p = [self pixel:e];
    fern_app_pointer(self.app, 0, FERN_POINTER_UP, p.x, p.y);
}

// The right button: in the game it places, and in a window it is the
// right click (pointer 1).
- (void)rightMouseDown:(NSEvent *)e {
    if (_captured) { fern_app_button(self.app, 1, 1); return; }
    NSPoint p = [self pixel:e];
    fern_app_pointer(self.app, 1, FERN_POINTER_DOWN, p.x, p.y);
}

- (void)rightMouseUp:(NSEvent *)e {
    if (_captured) { fern_app_button(self.app, 1, 0); return; }
    NSPoint p = [self pixel:e];
    fern_app_pointer(self.app, 1, FERN_POINTER_UP, p.x, p.y);
}

- (void)mouseMoved:(NSEvent *)e {
    if (_captured) {
        fern_app_look(self.app, (float)e.deltaX, (float)e.deltaY);
        return;
    }
    // The pointer of a window: the item on the cursor follows it.
    NSPoint p = [self pixel:e];
    fern_app_pointer(self.app, 0, FERN_POINTER_MOVE, p.x, p.y);
}

- (void)mouseDragged:(NSEvent *)e {
    [self mouseMoved:e];
}

- (void)rightMouseDragged:(NSEvent *)e {
    [self mouseMoved:e];
}

- (void)scrollWheel:(NSEvent *)e {
    if (e.scrollingDeltaY > 0.5) fern_app_wheel(self.app, -1);
    else if (e.scrollingDeltaY < -0.5) fern_app_wheel(self.app, 1);
}

@end

@interface FSDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

@implementation FSDelegate {
    NSWindow *_window;
    FSView *_view;
    FernApp *_app;
}

- (void)applicationDidFinishLaunching:(NSNotification *)n {
    NSString *support = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES).firstObject;
    NSString *dir = [support stringByAppendingPathComponent:@"Fernstone"];
    const char *override = getenv("FERN_DATA");
    if (override) dir = @(override);
    [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    FernAppConfig config = {0};
    config.data_dir = dir.fileSystemRepresentation;
    config.ram_mb = [NSProcessInfo processInfo].physicalMemory / (1024.0 * 1024.0);
    config.platform = "macos";
    _app = fern_app_new(&config);

    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1280, 720)
                                          styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                    NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    _window.title = @"Fernstone";
    _window.delegate = self;
    _window.acceptsMouseMovedEvents = YES;
    _view = [[FSView alloc] initWithFrame:_window.contentView.bounds];
    _view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _window.contentView = _view;
    [_window center];
    [_window makeKeyAndOrderFront:nil];
    [_window makeFirstResponder:_view];
    [_view startWithApp:_app];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app {
    return YES;
}

- (void)applicationWillTerminate:(NSNotification *)n {
    fern_app_free(_app);
}

@end

int main(int argc, const char **argv) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        FSDelegate *delegate = [[FSDelegate alloc] init];
        app.delegate = delegate;
        app.activationPolicy = NSApplicationActivationPolicyRegular;
        [app run];
    }
    return 0;
}
