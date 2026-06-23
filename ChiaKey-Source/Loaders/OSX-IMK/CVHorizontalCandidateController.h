// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>
#import <PlainVanilla/PlainVanilla.h>

#import "CVFloatingBackground.h"
#import "CVHorizontalCandidateControl.h"
#import "CVSendKey.h"
#import "CVTextDecoration.h"

using namespace OpenVanilla;

@interface CVHorizontalCandidateController : NSWindowController {
  IBOutlet CVHorizontalCandidateControl *_candidateControl;
  IBOutlet CVFloatingBackground *_background;
  IBOutlet NSButton *_previousButton;
  IBOutlet NSButton *_nextButton;
  IBOutlet NSTextField *_pageTextField;
  IBOutlet NSTextField *_promptTextField;
  float _width;
  float _fontHeight;

  NSColor *_backgroundColor;
  NSColor *_foregroundColor;
  NSColor *_highlightTextColor;
  PVHorizontalCandidatePanel *_panel;

  BOOL _allowClick;
  BOOL _sending;
}

- (void)setFontHeight:(float)newHeight;

- (void)updateContent:(PVHorizontalCandidatePanel *)panel
              atPoint:(NSPoint)position;
- (void)hide;

- (IBAction)sendKey:(id)sender;
- (IBAction)gotoNextPage:(id)sender;
- (IBAction)gotoPreviousPage:(id)sender;

- (void)setCandidateTextHeight:(float)inTextHeight;
@end
