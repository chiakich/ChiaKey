// [AUTO_HEADER]

#import "CVFloatingBackground.h"

#import "NSBezierPathAdditions.h"

@implementation CVFloatingBackground

- (void)dealloc {
  [_backgroundFillColor release];
  [_borderColor release];
  [super dealloc];
}
- (void)awakeFromNib {
  _backgroundFillColor = [[NSColor blackColor] retain];
  _borderColor = [[NSColor whiteColor] retain];
}
- (NSFocusRingType)focusRingType {
  return NSFocusRingTypeNone;
}
- (void)setBackgroundFillColor:(NSColor *)aColor {
  NSColor *tmp = _backgroundFillColor;
  _backgroundFillColor = [aColor retain];
  [tmp release];
}

- (void)setBorderColor:(NSColor *)aColor {
  NSColor *tmp = _borderColor;
  _borderColor = [aColor retain];
  [tmp release];
}
- (void)drawRect:(NSRect)aRect {
  NSRect rect = [self frame];
  [[NSColor clearColor] setFill];
  [NSBezierPath fillRect:rect];

#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
  NSBezierPath *b = [NSBezierPath bezierPathWithRoundedRect:rect
                                                    xRadius:12
                                                    yRadius:12];
#else
#warning Gee, they have implemented what we'd done with LFExtensions...
  NSBezierPath *b = [NSBezierPath bezelPathWithRect:rect radius:12];
#endif
  [b setFlatness:0.1];
  [b setLineCapStyle:NSLineCapStyleButt];
  [_backgroundFillColor setFill];
  [_borderColor setStroke];
  [b setLineWidth:2.0];
  [b fill];
  [b stroke];
}
@end
