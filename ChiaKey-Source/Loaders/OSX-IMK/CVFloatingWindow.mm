// [AUTO_HEADER]

#import "CVFloatingWindow.h"

#import <LFExtensions/LFCrossDevelopmentTools.h>

@implementation CVFloatingWindow
- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSWindowStyleMask)aStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)flag {
  if (self = [super initWithContentRect:contentRect
                              styleMask:NSWindowStyleMaskBorderless
                                backing:NSBackingStoreBuffered
                                  defer:NO]) {
    [self setBackgroundColor:[NSColor clearColor]];
    [self setOpaque:NO];

    if (LFIsRunningUnderOSX10_5AndAbove()) {
      [self setLevel:CGShieldingWindowLevel() + 1];
    } else {
      [self setLevel:NSScreenSaverWindowLevel];
    }

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
