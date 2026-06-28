// [AUTO_HEADER]

#import "CVCapsLockDelayOverride.h"

@implementation CVCapsLockDelayOverride

+ (void)_runHIDUtilWithPropertyJSON:(NSString *)propertyJSON
                         actionName:(NSString *)actionName {
  @try {
    NSTask *task = [[[NSTask alloc] init] autorelease];
    [task setLaunchPath:@"/usr/bin/hidutil"];
    [task setArguments:[NSArray arrayWithObjects:
                                    @"property",
                                    @"--set",
                                    propertyJSON,
                                    nil]];
    [task launch];
    [task waitUntilExit];

    if ([task terminationStatus] != 0) {
      NSLog(@"hidutil Caps Lock delay override %@ exited with status %d",
            actionName, [task terminationStatus]);
    }
  } @catch (NSException *exception) {
    NSLog(@"Unable to %@ Caps Lock delay override: %@", actionName, exception);
  }
}

+ (void)applyIfEnabled:(BOOL)enabled {
  if (!enabled) return;

  [self _runHIDUtilWithPropertyJSON:@"{\"CapsLockDelayOverride\":0}"
                         actionName:@"apply"];
}

+ (void)reset {
  [self _runHIDUtilWithPropertyJSON:@"{\"CapsLockDelayOverride\":null}"
                         actionName:@"reset"];
}

@end
