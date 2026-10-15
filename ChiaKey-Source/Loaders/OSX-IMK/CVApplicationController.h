// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "CVAboutController.h"
#import "CVHorizontalCandidateController.h"
#import "CVInputMethodToggleWindowController.h"
#import "CVPlainTextCandidateController.h"
#import "CVSearchController.h"
#import "CVSymbolController.h"
#import "CVToolTipController.h"
#import "CVVerticalCandidateController.h"
#import "ChiaKeyInputMethodProcessLifecycle.h"
#import "OpenVanillaLoader.h"

@interface CVApplicationController
    : NSObject <NSApplicationDelegate> {
  CVPlainTextCandidateController *_plainTextCandidateController;
  CVVerticalCandidateController *_verticalCandidateController;
  CVHorizontalCandidateController *_horizontalCandidateController;
  CVSearchController *_searchController;
  CVSymbolController *_symbolController;
  CVToolTipController *_tooltipController;
  CVAboutController *_aboutController;
  CVInputMethodToggleWindowController *_inputMethodToggleWindowController;

  OpenVanillaLoader *_loader;

  // Phrase Editor coordination (see ChiaKeyUserPhraseCoordination.h)
  NSTimer *_userPhrasePollTimer;
  BOOL _observedEditorSessionActive;
  NSDate *_lastSeenUserPhraseDirtyDate;

  // Re-arms the lexicon/application update checks while the IME process
  // stays resident, since applicationDidFinishLaunching only fires once
  // per process launch and this process can stay alive for weeks.
  NSTimer *_autoUpdateCheckTimer;

  // At most one reload runs at a time and none is dropped; a request that
  // cannot start waits for OVLBootDidFinishNotification or for the reload in
  // flight. See ChiaKeyInputMethodProcessLifecycle.h.
  ChiaKeyReloadQueue _reloadQueue;

  // <lithoglyph>
  OVSQLiteConnection *_userPhraseDB;
  // </lithoglyph>
}
- (void)setLoader:(OpenVanillaLoader *)aLoader;
- (OpenVanillaLoader *)loader;
#pragma mark User Interface Controllers
- (CVVerticalCandidateController *)verticalCandidateController;
- (CVHorizontalCandidateController *)horizontalCandidateController;
- (CVPlainTextCandidateController *)plainTextCandidateController;
- (CVSymbolController *)symbolController;
- (CVToolTipController *)tooltipController;
- (CVSearchController *)searchController;
- (CVAboutController *)aboutController;
- (CVInputMethodToggleWindowController *)inputMethodToggleWindowController;

- (IBAction)showAboutWindow:(id)sender;
- (NSArray *)inputMethodsArray;
- (NSString *)primaryInputMethod;
- (NSArray *)dynamicallyLoadedModulePackageInfo;
@end

@interface CVApplicationController (AppDelegate)
@end
