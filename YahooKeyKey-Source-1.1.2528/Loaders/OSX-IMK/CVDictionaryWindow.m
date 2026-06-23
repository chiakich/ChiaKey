// [AUTO_HEADER]

#import "CVDictionaryWindow.h"

@implementation CVDictionaryWindow
- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSWindowStyleMask)aStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)flag {
  if (self = [super initWithContentRect:contentRect
                              styleMask:aStyle
                                backing:NSBackingStoreBuffered
                                  defer:NO]) {
    [self setLevel:CGShieldingWindowLevel() + 1];
    [self setHasShadow:YES];
  }

  return self;
}
- (BOOL)canBecomeKeyWindow {
  return YES;
}
- (BOOL)canBecomeMainWindow {
  return NO;
}
- (NSTimeInterval)animationResizeTime:(NSRect)newWindowFrame {
  NSTimeInterval interval = 0.05;
  return interval;
}
@end
