// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#import "LFUtilities.h"

@interface CVSymbolController : NSWindowController {
  BOOL _isVisible;

  IBOutlet NSPopUpButton *_popUpButton;
  IBOutlet NSView *_symbolContentView;
  NSMutableArray *_viewArray;
}
- (void)loadSymbolTable:(NSNotification *)notification;
- (void)temporaryHide;
- (void)restoreWindowStatus;
- (BOOL)isVisible;
- (void)toggleActiveView:(NSView *)view;

#pragma mark Interface Builder actions

- (IBAction)toggleSymbol:(id)sender;
- (IBAction)hide:(id)sender;
- (IBAction)show:(id)sender;

- (IBAction)toggleSymbol:(id)sender;
@end
