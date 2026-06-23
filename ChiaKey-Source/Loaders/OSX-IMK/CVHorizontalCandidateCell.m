// [AUTO_HEADER]

#import "CVHorizontalCandidateCell.h"

#import "CVHorizontalCandidateControl.h"
#import "CVTextDecoration.h"
#import "NSColor+LFColorExtensions.h"

@implementation CVHorizontalCandidateCell

- (void)dealloc {
  [_keyString release];
  [_candidate release];
  [_candidateAttributes release];
  [super dealloc];
}
- (id)initWithKeyString:(NSString *)keyString
              candidate:(NSString *)candidate
             fontHeight:(float)inFontHeight {
  self = [super init];
  if (self != nil) {
    _keyString = [keyString retain];
    _candidate = [candidate retain];

    _fontHeight = inFontHeight;

    _candidateAttributes = [NSMutableDictionary new];
    NSMutableParagraphStyle *candidateParagraphStyle =
        [[[NSMutableParagraphStyle alloc] init] autorelease];
    [candidateParagraphStyle setAlignment:NSTextAlignmentLeft];
    [candidateParagraphStyle setLineBreakMode:NSLineBreakByTruncatingMiddle];
    [_candidateAttributes setValue:[NSColor whiteColor]
                            forKey:NSForegroundColorAttributeName];
    [_candidateAttributes setValue:candidateParagraphStyle
                            forKey:NSParagraphStyleAttributeName];
    [_candidateAttributes setValue:[NSFont systemFontOfSize:_fontHeight]
                            forKey:NSFontAttributeName];
  }
  return self;
}
- (void)setHighlight:(BOOL)flag {
  _isHighlighted = flag;
}
- (NSSize)cellSize {
  float refSpacing = _fontHeight + 10.0;

  NSRect rect =
      [_candidate boundingRectWithSize:NSMakeSize(1600, refSpacing)
                               options:NSStringDrawingUsesLineFragmentOrigin
                            attributes:_candidateAttributes];
  NSSize size = rect.size;
  if (size.width > 100) size.width = 100;
  size.width += 30.0;
  size.height = refSpacing;
  return size;
}
- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
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

  NSImage *keyImage =
      [CVTextDecoration imageWithACharacterInACircle:_keyString
                                          foreground:[NSColor whiteColor]
                                          background:[NSColor blackColor]];
  NSRect imageRect = NSMakeRect(
      NSMinX(cellFrame) + 4.0,
      NSMinY(cellFrame) + (_fontHeight - 16.0) / 2 + 4.0, 16.0, 16.0);
  [keyImage drawInRect:imageRect
             fromRect:NSZeroRect
             operation:NSCompositingOperationSourceOver
              fraction:1.0];
  NSRect candidateRect =
      NSMakeRect(NSMinX(cellFrame) + 24.0, NSMinY(cellFrame),
                 cellFrame.size.width - 24.0, cellFrame.size.height);
  [_candidate drawInRect:candidateRect withAttributes:_candidateAttributes];
}

@end
