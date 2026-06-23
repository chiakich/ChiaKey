// [AUTO_HEADER]

#import "CVInputMethodToggleCell.h"

#import "CVInputMethodToggleControl.h"
#import "NSColor+LFColorExtensions.h"

@implementation CVInputMethodToggleCell

- (void)setHighlighted:(BOOL)flag {
  _isHighlighted = flag;
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
  if (_isHighlighted) {
    NSColor *fromColor = [NSColor highlightGradientFromColor];
    NSColor *toColor = [fromColor gradientToColor];
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4

    NSGradient *g =
        [[[NSGradient alloc] initWithStartingColor:fromColor
                                       endingColor:toColor] autorelease];
    [g drawInRect:cellFrame angle:270.0];
#else
#warning This is a temporary implementation
    [fromColor setFill];
    [NSBezierPath fillRect:cellFrame];
#endif
  }
  [super drawWithFrame:cellFrame inView:controlView];
}

@end
