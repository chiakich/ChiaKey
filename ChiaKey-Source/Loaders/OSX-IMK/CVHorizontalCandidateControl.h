// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

@interface CVHorizontalCandidateControl : NSControl {
  NSMutableArray *_candidateCellArray;
  NSSize _autotSize;

  BOOL _clickable;
  int _clickedIndex;

  float _candidateTextHeight;
}
- (NSSize)autoSize;
- (void)setArray:(NSArray *)candidateArray;
- (void)setHighlightdeIndex:(int)index;
- (void)setClickable:(BOOL)flag;
- (int)clickedIndex;

- (void)setCandidateTextHeight:(float)inTextHeight;
@end
