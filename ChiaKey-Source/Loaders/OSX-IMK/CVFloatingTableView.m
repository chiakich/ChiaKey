// [AUTO_HEADER]

#import "CVFloatingTableView.h"

#import "NSColor+LFColorExtensions.h"

@implementation CVFloatingTableView

- (void)dealloc {
  [_tableBackgroundColor release];
  [super dealloc];
}
- (void)awakeFromNib {
  _tableBackgroundColor = [[NSColor clearColor] retain];
}
- (NSFocusRingType)focusRingType {
  return NSFocusRingTypeNone;
}
- (void)setTablebackgroundColor:(NSColor *)aColor {
  NSColor *tmp;
  tmp = _tableBackgroundColor;
  _tableBackgroundColor = [aColor retain];
  [tmp release];
}
- (id)_highlightColorForCell:(NSCell *)cell {
  return nil;
}
- (void)highlightSelectionInClipRect:(NSRect)clipRect {
  [_tableBackgroundColor setFill];
  [[NSBezierPath bezierPathWithRect:clipRect] fill];

  int selectedRow = [self selectedRow];
  if (selectedRow == -1) return;

  NSColor *fromColor = [NSColor highlightGradientFromColor];
  NSColor *toColor = [fromColor gradientToColor];

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
  NSGradient *g =
      [[[NSGradient alloc] initWithStartingColor:fromColor
                                     endingColor:toColor] autorelease];
  NSRect drawingRect = [self rectOfRow:selectedRow];
  [g drawInRect:drawingRect angle:90.0];
#else
#warning This is a temporary implementation
  [fromColor setFill];
  [NSBezierPath fillRect:[self rectOfRow:selectedRow]];
#endif
}

@end
