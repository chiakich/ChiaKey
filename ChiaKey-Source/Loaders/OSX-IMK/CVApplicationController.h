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
#import "OpenVanillaConfig.h"
#import "OpenVanillaLoader.h"
#import "OpenVanillaService.h"

@interface CVApplicationController
    : NSObject <NSApplicationDelegate, NSXPCListenerDelegate, OpenVanillaService,
                OpenVanillaXPCService> {
  CVPlainTextCandidateController *_plainTextCandidateController;
  CVVerticalCandidateController *_verticalCandidateController;
  CVHorizontalCandidateController *_horizontalCandidateController;
  CVSearchController *_searchController;
  CVSymbolController *_symbolController;
  CVToolTipController *_tooltipController;
  CVAboutController *_aboutController;
  CVInputMethodToggleWindowController *_inputMethodToggleWindowController;

  OpenVanillaLoader *_loader;

  NSXPCListener *_serviceListener;

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
@end

@interface CVApplicationController (AppDelegate)
@end
