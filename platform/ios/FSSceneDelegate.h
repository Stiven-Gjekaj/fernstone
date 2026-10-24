#import <UIKit/UIKit.h>

// iOS 27 needs the scene lifecycle. The scene owns the window.
@interface FSSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (strong, nonatomic) UIWindow *window;
@end
