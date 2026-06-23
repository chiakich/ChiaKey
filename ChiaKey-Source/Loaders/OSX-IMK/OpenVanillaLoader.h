// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>
#import <OpenVanilla/OpenVanilla.h>
#import <PlainVanilla/PlainVanilla.h>

// temporarily so, will make into CareService.framework
#import "CVEncodingService.h"
#include "OVLoaderUserPersistence.h"
#include "SystemInfo.h"
#include "UUIDHelper.h"
#include "Version.h"

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
  VersionChecker *_versionChecker;

  NSString *_databaseVersion;

  OVSQLiteConnection *_userPhraseDB;

  id _cannedMessagesDataHTTPRequest;

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
+ (VersionChecker *)sharedVersionChecker;
+ (NSLock *)sharedLock;
+ (void)releaseSharedObjects;

#pragma mark Instance methods
- (id)init;
- (void)dealloc;
- (bool)start:(NSArray *)loadPaths;
- (void)shutDown;
- (void)reload;
- (PVLoader *)loader;
- (PVLoaderService *)loaderService;
- (NSArray *)identifiersAndLocalizedNamesWithPattern:(NSString *)pattern;
- (bool)exportUserPhraseDBToFile:(NSString *)path;
- (bool)importUserPhraseDBFromFile:(NSString *)path;
- (VersionChecker *)versionChecker;
- (NSString *)databaseVersion;

#pragma mark User Phrase DO methods
- (BOOL)userPhraseDBCanProvideService;
- (int)userPhraseDBNumberOfRow;
- (NSDictionary *)userPhraseDBDictionaryAtRow:(int)row;
- (NSArray *)userPhraseDBReadingsForPhrase:(NSString *)phrase;
- (void)userPhraseDBSave;
- (void)userPhraseDBSetNewReading:(NSString *)reading forPhraseAtRow:(int)row;
- (void)userPhraseDBDeleteRow:(int)row;
- (void)userPhraseDBAddNewRow:(NSString *)phrase;
- (void)userPhraseDBAddNewRows:(NSArray *)array;
- (void)userPhraseDBSetPhrase:(NSString *)phrase atRow:(int)row;

#pragma mark Auto-update and server-side data provision
- (void)scheduleDataProvisionServices;

- (void)mergeCannedMessagesData;
- (NSArray *)mergedCannedMessagesArray;
- (void)syncUserCannedMessages;
- (const string)userFreeCannedMessagePath;

- (NSArray *)dynamicallyLoadedModulePackageInfo;
- (void)setBlackListOfPackageIdentifers:(NSArray *)inIdentifiers;
@end

extern NSString *CVLoaderUpdateCannedMessagesNotification;
