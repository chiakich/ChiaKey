// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

@interface CVFloatingBackground : NSView {
  NSColor *_backgroundFillColor;
  NSColor *_borderColor;
}
- (void)setBackgroundFillColor:(NSColor *)aColor;
- (void)setBorderColor:(NSColor *)aColor;
@end
