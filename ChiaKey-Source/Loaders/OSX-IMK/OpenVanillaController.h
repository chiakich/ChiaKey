//  OpenVanillaController.h

#import <Cocoa/Cocoa.h>
#import <InputMethodKit/InputMethodKit.h>

#import "OpenVanillaConfig.h"
#import "OpenVanillaLoader.h"

using namespace OpenVanilla;

@interface OpenVanillaController : IMKInputController {
  PVLoaderContext* _context;
  NSMutableString* _composingBuffer;
  BOOL _commitFromOurselves;
  BOOL _temporaryEnglishMode;
  BOOL _shiftKeyPressedForTemporaryEnglish;
  BOOL _shiftKeyTapCanceled;

  // application-specific fixes
  BOOL _doNotClearContextStateEvenWithForcedCommit;
  BOOL _updateCommitStringBeforeCommit;
}
+ (void)setActiveContext:(OpenVanillaController*)context sender:(id)sender;
// Screen rect of the line being typed on, or NSZeroRect when unavailable.
+ (NSRect)currentCaretLineRect;
#pragma mark Send string to client.
+ (void)sendComposedStringToCurrentlyActiveContext:(NSString*)text;
- (void)sendComposedStringToClient:(NSString*)text sender:(id)sender;

- (void)_resetUI;

@end
