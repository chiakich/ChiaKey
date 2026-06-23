//  OpenVanillaController.h

#import <Cocoa/Cocoa.h>
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
#import <InputMethodKit/InputMethodKit.h>
#else
#import "InputMethodKitTiger.h"
#endif

#import "OpenVanillaConfig.h"
#import "OpenVanillaLoader.h"

using namespace OpenVanilla;

@interface OpenVanillaController : IMKInputController {
  PVLoaderContext* _context;
  NSMutableString* _composingBuffer;
  BOOL _commitFromOurselves;

  // application-specific fixes
  BOOL _doNotClearContextStateEvenWithForcedCommit;
  BOOL _updateCommitStringBeforeCommit;
}
+ (void)setActiveContext:(OpenVanillaController*)context sender:(id)sender;
#pragma mark Send string to client.
+ (void)sendComposedStringToCurrentlyActiveContext:(NSString*)text;
- (void)sendComposedStringToClient:(NSString*)text sender:(id)sender;

- (void)_resetUI;

@end
