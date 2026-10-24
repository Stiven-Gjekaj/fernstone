#import <UIKit/UIKit.h>

// The view controller makes an EGL context with ANGLE on Metal, reads the
// touches, and runs the frame loop. The core does all other work.
@interface FSGameViewController : UIViewController
- (void)setPaused:(BOOL)paused;
@end
