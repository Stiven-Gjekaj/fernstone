#import "FSSceneDelegate.h"
#import "FSGameViewController.h"

@implementation FSSceneDelegate {
    FSGameViewController *_game;
}

- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session
      options:(UISceneConnectionOptions *)options {
    self.window = [[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
    _game = [[FSGameViewController alloc] init];
    self.window.rootViewController = _game;
    [self.window makeKeyAndVisible];
}

- (void)sceneWillResignActive:(UIScene *)scene {
    // iOS stops the app when it draws with the GPU in the background.
    [_game setPaused:YES];
}

- (void)sceneDidBecomeActive:(UIScene *)scene {
    [_game setPaused:NO];
}

@end
