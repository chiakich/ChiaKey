// [AUTO_HEADER]

#import <Foundation/Foundation.h>

@interface CVCapsLockDelayOverride : NSObject
+ (void)applyIfEnabled:(BOOL)enabled;
+ (void)reset;
@end
