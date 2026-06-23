// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

@interface CVBubbleTextView : NSView {
  NSMutableAttributedString *_attributedString;
  NSBezierPath *_outerBezelPath;
  NSBezierPath *_innerBezelPath;
  NSColor *_backgroundToColor;
  NSColor *_backgroundFromColor;
  NSColor *_textColor;
  NSFont *_font;
  NSRect _boundingRect;
  NSRect _innerBound;
  float _padding;
  float _radius;
}

- (void)setText:(NSString *)newText;
- (void)setBackgroundColor:(NSColor *)color;
- (void)setTextColor:(NSColor *)color;
- (void)setFont:(NSFont *)font;
- (NSRect)boundingRect;
@end