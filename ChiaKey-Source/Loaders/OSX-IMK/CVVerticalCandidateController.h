// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>
#import <PlainVanilla/PlainVanilla.h>

#import "CVFloatingBackground.h"
#import "CVFloatingTableView.h"
#import "CVSendKey.h"
#import "CVTextDecoration.h"

using namespace OpenVanilla;

@interface CVPageButton : NSButton
@end

@interface CVVerticalCandidateController : NSWindowController {
  IBOutlet id _scrollView;
  IBOutlet CVFloatingTableView *_tableView;
  IBOutlet NSTextField *_pageIndicatorTextField;
  IBOutlet NSTextField *_promptTextField;
  IBOutlet NSButton *_previousButton;
  IBOutlet NSButton *_nextButton;

  NSColor *_backgroundColor;
  NSColor *_foregroundColor;
  NSColor *_highlightTextColor;
  NSMutableArray *_candidateArray;
  PVVerticalCandidatePanel *_panel;
  float _width;
  float _fontHeight;

  BOOL _allowClick;
  BOOL _sending;

  float _candidateTextHeight;
}

- (void)setFontHeight:(float)newHeight;
- (void)updateContent:(PVVerticalCandidatePanel *)panel
              atPoint:(NSPoint)position;
- (void)hide;

- (void)setCandidateTextHeight:(float)inTextHeight;

#pragma mark Interface Builder actions

/*!
        @method	sendKey:
        @abstract Raises a keyboard event and send a key code as
        simulating a key is pressed down.
        @param sender The Sender Object.
*/
- (IBAction)sendKey:(id)sender;
- (IBAction)updateSelectedCandidate:(id)sender;
- (IBAction)gotoNextPage:(id)sender;
- (IBAction)gotoPreviousPage:(id)sender;
@end
