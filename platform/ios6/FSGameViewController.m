#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <QuartzCore/QuartzCore.h>
#import "FSGameViewController.h"
#include "tri.h"

@interface FSGLView : UIView
@end

@implementation FSGLView
+ (Class)layerClass {
    return [CAEAGLLayer class];
}
@end

@implementation FSGameViewController {
    EAGLContext *_context;
    GLuint _framebuffer;
    GLuint _colorbuffer;
    GLint _width;
    GLint _height;
    CADisplayLink *_link;
    BOOL _started;
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
}

- (void)viewDidLoad {
    [super viewDidLoad];
    _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    [EAGLContext setCurrentContext:_context];
    glGenFramebuffers(1, &_framebuffer);
    glGenRenderbuffers(1, &_colorbuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colorbuffer);
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    [EAGLContext setCurrentContext:_context];
    glBindRenderbuffer(GL_RENDERBUFFER, _colorbuffer);
    [_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.view.layer];
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &_width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &_height);
    if (_width > _height && !_started) [self start];
}

- (void)start {
    _started = YES;
    if (!fern_tri_init()) NSLog(@"fernstone: the shader does not compile");
    _link = [CADisplayLink displayLinkWithTarget:self selector:@selector(frame:)];
    [_link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)setPaused:(BOOL)paused {
    _link.paused = paused;
}

- (void)frame:(CADisplayLink *)link {
    [EAGLContext setCurrentContext:_context];
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    fern_tri_draw(_width, _height);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorbuffer);
    [_context presentRenderbuffer:GL_RENDERBUFFER];
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
