// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "CVInputMethodToggleControl.h"

@interface CVInputMethodToggleWindowController : NSWindowController {
  IBOutlet CVInputMethodToggleControl *_togglecontrol;
  BOOL _shouldHideWindow;
  NSRect _screenFrame;
}

- (void)showWindowWithInputMethods:(NSArray *)inputMethods
                     selectedIndex:(int)selectedIndex;
- (void)moveToNextInputMethod;
- (void)useScreenOfPoint:(NSPoint)point;

@end
