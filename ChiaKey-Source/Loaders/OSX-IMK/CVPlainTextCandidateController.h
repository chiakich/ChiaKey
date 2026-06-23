// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>
#import <PlainVanilla/PlainVanilla.h>

#import "CVBubbleTextView.h"
#import "CVBubbleTextWindow.h"

using namespace OpenVanilla;

@interface CVPlainTextCandidateController : NSWindowController {
}
- (void)updateContent:(PVPlainTextCandidatePanel*)panel
              atPoint:(NSPoint)position;
@end
