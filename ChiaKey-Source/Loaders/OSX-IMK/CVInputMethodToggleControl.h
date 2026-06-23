// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

@interface CVInputMethodToggleControl : NSControl {
  NSMutableArray *_cellArray;
  int _currentIndex;
  NSSize _autosize;
}

- (void)setInputMethodArray:(NSArray *)inputMethodArray;
- (void)setCurrentIndex:(int)currentIndex;
- (NSSize)autosize;

@end
