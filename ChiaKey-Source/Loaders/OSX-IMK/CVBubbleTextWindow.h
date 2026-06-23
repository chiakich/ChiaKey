// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

@interface CVBubbleTextWindow : NSWindow {
  NSPoint _initialLocation;
  BOOL _useWindowAnimation;

  IBOutlet id _textView;
}
- (void)setText:(NSString *)newText;
- (void)setBubbleBackgroundColor:(NSColor *)color;
- (void)setTextColor:(NSColor *)color;
- (void)setFont:(NSFont *)font;
- (void)setUseWindowAnimation:(BOOL)animation;
- (void)updateWindowSize;
@end
