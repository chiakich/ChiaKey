// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>
#import <OpenVanilla/OpenVanilla.h>
#import <PlainVanilla/PlainVanilla.h>

// temporarily so, will make into CareService.framework
#import "CVEncodingService.h"
#include "OVLoaderUserPersistence.h"
#include "SystemInfo.h"
#include "UUIDHelper.h"

using namespace OpenVanilla;
using namespace CareService;

@interface OpenVanillaLoader : NSObject {
  OVCINDatabaseService *_CINDatabaseService;
  OVSQLiteDatabaseService *_SQLiteDatabaseService;
  PVLoaderPolicy *_loaderPolicy;
  PVLoaderPolicy *_signedModulesLoaderPolicy;
  CVEncodingService *_encodingService;
  PVLoaderService *_loaderService;
  PVBundleLoadingSystem *_bundleLoadingSystem;
  PVStaticModulePackageLoadingSystem *_staticModuleLoadingSystem;
  PVCommonPackageLoadingSystem *_signedModuleLoadingSystem;
  PVLoader *_loader;

  NSString *_databaseVersion;
  BOOL _activeUserLexiconLoaded;
  BOOL _activeUserLexiconFailed;

  OVSQLiteConnection *_userPhraseDB;

  // Phrase Editor coordination (see ChiaKeyUserPhraseCoordination.h)
  BOOL _userPhraseEditingSessionActive;
  NSMutableArray *_pendingUserPhraseAdditions;

  OVLoaderUserPersistence *_userPersistence;
  NSMutableArray *_mergedCannedMessagesArray;

  // user canned messages support
  PVPropertyList *_userCannedMessagePlist;
  OVFileTimestamp *_userFreeCannedMessageFileTimestamp;

}

#pragma mark Class methods
+ (OpenVanillaLoader *)sharedInstance;
+ (PVLoader *)sharedLoader;
+ (PVLoaderService *)sharedLoaderService;
+ (NSLock *)sharedLock;
+ (void)releaseSharedObjects;
// start: runs on a background thread, so a client can ask for a controller
// before there is a loader to build one from. Blocks until boot has finished
// one way or the other, and answers whether a loader came out of it.
+ (BOOL)waitForLoaderReadyWithTimeout:(NSTimeInterval)timeout;

#pragma mark Instance methods
- (id)init;
- (void)dealloc;
- (bool)start:(NSArray *)loadPaths;
- (void)shutDown;
- (void)reload;
- (PVLoader *)loader;
- (void)noteUserExplicitlySelectedInputMethod;
- (PVLoaderService *)loaderService;
- (NSArray *)identifiersAndLocalizedNamesWithPattern:(NSString *)pattern;
- (bool)exportUserPhraseDBToFile:(NSString *)path;
- (bool)importUserPhraseDBFromFile:(NSString *)path;
- (NSString *)databaseVersion;
// NO when the installed lexicon failed to open and a fallback database is in
// use, which is the case where the previous version must be kept.
- (BOOL)activeUserLexiconLoaded;
// YES only when an installed lexicon is there and did not open, which is what
// a rollback repairs; having no installed lexicon at all is not a failure.
- (BOOL)activeUserLexiconFailed;

#pragma mark User Phrase additions
- (void)userPhraseDBAddNewRow:(NSString *)phrase;
- (void)userPhraseDBAddNewRow:(NSString *)phrase reading:(NSString *)reading;

#pragma mark Preferences app coordination
// Writes IMEStatus.plist (module list, package info, versions) so the
// Preferences app can read it without a mach service.
- (void)publishServiceStatus;
// Applies PendingModuleBlacklist.plist if present, persists it, republishes.
- (void)applyPendingModuleBlacklistAndPublish;

#pragma mark Phrase Editor coordination
- (NSString *)userDataDirectory;
- (BOOL)userPhraseEditingSessionActive;
- (void)userPhraseEditingSessionDidBegin;
- (void)userPhraseEditingSessionDidEnd;
- (void)userPhraseDBDidChangeExternally;

- (void)mergeCannedMessagesData;
- (NSArray *)mergedCannedMessagesArray;
- (void)syncUserCannedMessages;
- (const string)userFreeCannedMessagePath;

- (NSArray *)dynamicallyLoadedModulePackageInfo;
- (void)setBlackListOfPackageIdentifers:(NSArray *)inIdentifiers;
@end

extern NSString *CVLoaderUpdateCannedMessagesNotification;
