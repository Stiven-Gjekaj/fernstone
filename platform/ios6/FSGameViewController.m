#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <QuartzCore/QuartzCore.h>
#import "FSGameViewController.h"
#include <sys/sysctl.h>
#include "app.h"

// The files of the app: the jars, the options, and the log. An AFC tool
// can reach this directory.
static NSString *const kDataDir = @"/var/mobile/Media/Fernstone";

@interface FSGLView : UIView
@end

@implementation FSGLView
+ (Class)layerClass {
    return [CAEAGLLayer class];
}
@end

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

@interface FSGameViewController () <UITextFieldDelegate>
@end

@implementation FSGameViewController {
    EAGLContext *_context;
    GLuint _framebuffer;
    GLuint _colorbuffer;
    GLuint _depthbuffer;
    GLint _width;
    GLint _height;
    CADisplayLink *_link;
    BOOL _started;
    FernApp *_app;
    FSKeyField *_keys;
    NSMutableArray *_touches;
}

- (void)loadView {
    FSGLView *view = [[FSGLView alloc] initWithFrame:[UIScreen mainScreen].bounds];
    view.multipleTouchEnabled = YES;
    view.contentScaleFactor = [UIScreen mainScreen].scale;
    CAEAGLLayer *layer = (CAEAGLLayer *)view.layer;
    layer.opaque = YES;
    layer.drawableProperties = @{kEAGLDrawablePropertyRetainedBacking: @NO,
                                 kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8};
    self.view = view;
    _touches = [NSMutableArray array];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    [EAGLContext setCurrentContext:_context];
    glGenFramebuffers(1, &_framebuffer);
    glGenRenderbuffers(1, &_colorbuffer);
    glGenRenderbuffers(1, &_depthbuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colorbuffer);

    [[NSFileManager defaultManager] createDirectoryAtPath:kDataDir withIntermediateDirectories:YES attributes:nil error:nil];
    FernAppConfig config = {0};
    config.data_dir = kDataDir.fileSystemRepresentation;
    config.ram_mb = [NSProcessInfo processInfo].physicalMemory / (1024.0 * 1024.0);
    config.platform = "ios6";
    config.touch = 1;
    _app = fern_app_new(&config);

    _keys = [[FSKeyField alloc] initWithFrame:CGRectMake(-100, -100, 10, 10)];
    _keys.app = _app;
    _keys.delegate = self;
    _keys.autocorrectionType = UITextAutocorrectionTypeNo;
    _keys.autocapitalizationType = UITextAutocapitalizationTypeNone;
    _keys.returnKeyType = UIReturnKeyDone;
    _keys.text = @" ";
    [self.view addSubview:_keys];
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    [EAGLContext setCurrentContext:_context];
    glBindRenderbuffer(GL_RENDERBUFFER, _colorbuffer);
    [_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.view.layer];
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &_width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &_height);
    glBindRenderbuffer(GL_RENDERBUFFER, _depthbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, _width, _height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthbuffer);
    fern_app_resize(_app, _width, _height, (float)[UIScreen mainScreen].scale);
    if (_width > _height && !_started) [self start];
}

- (void)start {
    _started = YES;
    _link = [CADisplayLink displayLinkWithTarget:self selector:@selector(frame:)];
    [_link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)setPaused:(BOOL)paused {
    _link.paused = paused;
}

// A debug command file: each line in cmd.txt runs once, then the file is
// removed. scripts/tap.sh writes it.
- (void)readCommands {
    NSString *path = [kDataDir stringByAppendingPathComponent:@"cmd.txt"];
    NSString *text = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
    if (!text) return;
    [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
    for (NSString *line in [text componentsSeparatedByString:@"\n"])
        if (line.length) fern_app_command(_app, line.UTF8String);
}

- (void)frame:(CADisplayLink *)link {
    static int n;
    if ((++n & 15) == 0) [self readCommands];
    [EAGLContext setCurrentContext:_context];
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    fern_app_frame(_app, CACurrentMediaTime());
    // The depth buffer does not need to go to the screen.
    glBindRenderbuffer(GL_RENDERBUFFER, _colorbuffer);
    [_context presentRenderbuffer:GL_RENDERBUFFER];
    BOOL wantKeys = fern_app_wants_keyboard(_app);
    if (wantKeys && !_keys.isFirstResponder) [_keys becomeFirstResponder];
    if (!wantKeys && _keys.isFirstResponder) [_keys resignFirstResponder];
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

- (int)touchID:(UITouch *)t {
    NSUInteger i = [_touches indexOfObjectIdenticalTo:t];
    if (i == NSNotFound) {
        [_touches addObject:t];
        i = _touches.count - 1;
    }
    return (int)(uintptr_t)(__bridge void *)t;
}

- (void)send:(NSSet *)touches phase:(int)phase {
    CGFloat s = self.view.contentScaleFactor;
    for (UITouch *t in touches) {
        CGPoint p = [t locationInView:self.view];
        fern_app_pointer(_app, [self touchID:t], phase, (float)(p.x * s), (float)(p.y * s));
        if (phase == FERN_POINTER_UP) [_touches removeObjectIdenticalTo:t];
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

- (BOOL)shouldAutorotate {
    return YES;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}

@end
