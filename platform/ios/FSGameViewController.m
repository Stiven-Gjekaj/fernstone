#import <QuartzCore/QuartzCore.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import "FSGameViewController.h"
#include <EGL/egl.h>
#include "app.h"

// A text field that the user does not see. It brings up the keyboard and
// sends each key to the core.
@interface FSKeyField : UITextField
@property (nonatomic) FernApp *app;
@end

@implementation FSKeyField
- (void)deleteBackward {
    fern_app_key(self.app, FK_BACKSPACE, 1);
    fern_app_key(self.app, FK_BACKSPACE, 0);
}
@end

@interface FSGameViewController () <UITextFieldDelegate, UIDocumentPickerDelegate>
@end

@implementation FSGameViewController {
    EGLDisplay _display;
    EGLSurface _surface;
    EGLContext _context;
    CADisplayLink *_link;
    FernApp *_app;
    FSKeyField *_keys;
    NSString *_docs;
    NSString *_importVersion;
    BOOL _picking;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.multipleTouchEnabled = YES;
    self.view.backgroundColor = [UIColor blackColor];
    // Get the paths at runtime. LiveContainer gives each app its own
    // container, so a fixed path does not work.
    _docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    FernAppConfig config = {0};
    config.data_dir = _docs.fileSystemRepresentation;
    config.ram_mb = [NSProcessInfo processInfo].physicalMemory / (1024.0 * 1024.0);
    config.platform = "ios";
    config.touch = 1;
    _app = fern_app_new(&config);

    _keys = [[FSKeyField alloc] initWithFrame:CGRectMake(-100, -100, 10, 10)];
    _keys.app = _app;
    _keys.delegate = self;
    _keys.autocorrectionType = UITextAutocorrectionTypeNo;
    _keys.autocapitalizationType = UITextAutocapitalizationTypeNone;
    _keys.spellCheckingType = UITextSpellCheckingTypeNo;
    _keys.returnKeyType = UIReturnKeyDone;
    [self.view addSubview:_keys];
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    CGFloat scale = self.view.window.screen.scale ?: 3.0;
    if (!_surface) [self startGL:scale];
    EGLint w = 0, h = 0;
    eglQuerySurface(_display, _surface, EGL_WIDTH, &w);
    eglQuerySurface(_display, _surface, EGL_HEIGHT, &h);
    if (w <= 0 || h <= 0) {
        w = (EGLint)(self.view.bounds.size.width * scale);
        h = (EGLint)(self.view.bounds.size.height * scale);
    }
    fern_app_resize(_app, w, h, (float)scale);
}

- (void)startGL:(CGFloat)scale {
    static const EGLint configAttribs[] = {EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                                           EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24,
                                           EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE};
    static const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    _display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(_display, NULL, NULL);
    eglChooseConfig(_display, configAttribs, &config, 1, &count);
    self.view.layer.contentsScale = scale;
    _surface = eglCreateWindowSurface(_display, config, (__bridge EGLNativeWindowType)self.view.layer, NULL);
    _context = eglCreateContext(_display, config, EGL_NO_CONTEXT, contextAttribs);
    eglMakeCurrent(_display, _surface, _surface, _context);
    if (!_surface || !_context) NSLog(@"fernstone: EGL error 0x%x", eglGetError());

    NSString *probe = [[NSUserDefaults standardUserDefaults] stringForKey:@"FernProbe"];
    if (probe.length) fern_app_probe(_app, probe.UTF8String, 25565, "Fern15");

    _link = [CADisplayLink displayLinkWithTarget:self selector:@selector(frame:)];
    _link.preferredFrameRateRange = CAFrameRateRangeMake(30, 60, 60);
    [_link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)setPaused:(BOOL)paused {
    _link.paused = paused;
}

// A debug command file: each line in Documents/cmd.txt runs once.
- (void)readCommands {
    NSString *path = [_docs stringByAppendingPathComponent:@"cmd.txt"];
    NSString *text = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
    if (!text) return;
    [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
    for (NSString *line in [text componentsSeparatedByString:@"\n"])
        if (line.length) fern_app_command(_app, line.UTF8String);
}

- (void)frame:(CADisplayLink *)link {
    static int n;
    char version[32];
    if ((++n & 15) == 0) [self readCommands];
    eglMakeCurrent(_display, _surface, _surface, _context);
    fern_app_frame(_app, CACurrentMediaTime());
    eglSwapBuffers(_display, _surface);
    BOOL wantKeys = fern_app_wants_keyboard(_app);
    if (wantKeys && !_keys.isFirstResponder) [_keys becomeFirstResponder];
    if (!wantKeys && _keys.isFirstResponder) [_keys resignFirstResponder];
    if (!_picking && fern_app_wants_import(_app, version, sizeof(version))) [self importJar:@(version)];
}

- (void)importJar:(NSString *)version {
    UTType *jar = [UTType typeWithFilenameExtension:@"jar"] ?: UTTypeData;
    UIDocumentPickerViewController *picker =
        [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[jar, UTTypeZIP, UTTypeData] asCopy:YES];
    picker.delegate = self;
    _importVersion = version;
    _picking = YES;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController *)picker didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSString *dir = [_docs stringByAppendingPathComponent:@"jars"];
    NSString *dest = [dir stringByAppendingPathComponent:[_importVersion stringByAppendingPathExtension:@"jar"]];
    [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    [[NSFileManager defaultManager] removeItemAtPath:dest error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:urls.firstObject toURL:[NSURL fileURLWithPath:dest] error:nil];
    _picking = NO;
    fern_app_jars_changed(_app);
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)picker {
    _picking = NO;
}

- (BOOL)textField:(UITextField *)field shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)s {
    if (s.length) fern_app_text(_app, s.UTF8String);
    return NO;
}

- (BOOL)textFieldShouldReturn:(UITextField *)field {
    fern_app_key(_app, FK_ENTER, 1);
    fern_app_key(_app, FK_ENTER, 0);
    return NO;
}

- (void)send:(NSSet *)touches phase:(int)phase {
    CGFloat s = self.view.layer.contentsScale;
    for (UITouch *t in touches) {
        CGPoint p = [t locationInView:self.view];
        fern_app_pointer(_app, (int)(intptr_t)(__bridge void *)t, phase, (float)(p.x * s), (float)(p.y * s));
    }
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)e {
    [self send:touches phase:FERN_POINTER_DOWN];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)e {
    [self send:touches phase:FERN_POINTER_MOVE];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)e {
    [self send:touches phase:FERN_POINTER_UP];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)e {
    [self send:touches phase:FERN_POINTER_UP];
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

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures {
    return UIRectEdgeAll;
}

@end
