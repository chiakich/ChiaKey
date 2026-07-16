// [AUTO_HEADER]

#import "CVFloatingPanelWindow.h"

@implementation CVFloatingPanelWindow
- (void)configurePanel {
  // Without this, clicking the panel makes the loader the active app, which
  // fires deactivateServer: on the text client and clears the active context
  // before the click's action runs.
  [self setStyleMask:[self styleMask] | NSWindowStyleMaskNonactivatingPanel];

  // The loader never becomes active, so a normal-level panel would be buried by
  // the window being typed into.
  [self setLevel:NSFloatingWindowLevel];
  [self setHasShadow:YES];
}
- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSWindowStyleMask)aStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)flag {
  if (self = [super initWithContentRect:contentRect
                              styleMask:aStyle
                                backing:NSBackingStoreBuffered
                                  defer:NO]) {
    [self configurePanel];
  }

  return self;
}
// Nib-loaded windows are decoded via initWithCoder:, bypassing the designated
// initializer above.
- (void)awakeFromNib {
  [self configurePanel];
}
// Taking key status would resign it from the text client and fire
// deactivateServer:, and temporaryHide/restoreWindowStatus would then bounce
// focus back and forth. The panel is click-only; nothing in it takes keyboard
// input, so it never needs to be key.
- (BOOL)canBecomeKeyWindow {
  return NO;
}
- (BOOL)canBecomeMainWindow {
  return NO;
}
- (NSTimeInterval)animationResizeTime:(NSRect)newWindowFrame {
  NSTimeInterval interval = 0.05;
  return interval;
}
@end
