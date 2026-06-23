// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "CVAboutController.h"
#import "CVDictionaryController.h"
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

@interface CVApplicationController : NSObject <OpenVanillaService> {
  CVPlainTextCandidateController *_plainTextCandidateController;
  CVVerticalCandidateController *_verticalCandidateController;
  CVHorizontalCandidateController *_horizontalCandidateController;
  CVSearchController *_searchController;
  CVDictionaryController *_dictionaryController;
  CVSymbolController *_symbolController;
  CVToolTipController *_tooltipController;
  CVAboutController *_aboutController;
  CVInputMethodToggleWindowController *_inputMethodToggleWindowController;

  OpenVanillaLoader *_loader;

  NSMutableDictionary *_versionInfo;

  NSPort *_serverPort;
  NSConnection *_serverConnection;

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
- (CVDictionaryController *)dictionaryController;
- (CVSymbolController *)symbolController;
- (CVToolTipController *)tooltipController;
- (CVSearchController *)searchController;
- (CVAboutController *)aboutController;
- (CVInputMethodToggleWindowController *)inputMethodToggleWindowController;

- (IBAction)showAboutWindow:(id)sender;
- (NSArray *)inputMethodsArray;

- (NSDictionary *)shouldUpdateWithVersionInfoData:(NSData *)infoData
                         versionInfoSignatureData:(NSData *)sigData;
@end

@interface CVApplicationController (versionInfo)
- (NSString *)_plistFilepath:(NSString *)filename;
- (void)_saveVersionInfo;
- (void)_loadVersionInfo;
@end

@interface CVApplicationController (AppDelegate)
@end