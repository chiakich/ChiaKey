// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

@interface CVHorizontalCandidateCell : NSActionCell {
  NSString *_keyString;
  NSString *_candidate;

  NSMutableDictionary *_candidateAttributes;
  BOOL _isHighlighted;

  float _fontHeight;
}

- (id)initWithKeyString:(NSString *)keyString
              candidate:(NSString *)candidate
             fontHeight:(float)inFontHeight;
- (void)setHighlight:(BOOL)flag;

@end
