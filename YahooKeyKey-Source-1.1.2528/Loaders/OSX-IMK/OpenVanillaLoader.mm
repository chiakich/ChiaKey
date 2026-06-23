// [AUTO_HEADER]

#import "OpenVanillaLoader.h"

#import <zlib.h>

#include <set>
#include <sstream>

#import "BPMFUserPhraseHelper.h"
#import "CVApplicationController.h"
#import "LFCrossDevelopmentTools.h"
#import "LFHTTPRequest.h"
#import "LFUtilities.h"
#import "OVAFBopomofoCorrectionPackage.h"
#import "OVAFEvalPackage.h"
#import "OVIMGenericPackage.h"
#import "OVIMMandarinPackage.h"
#import "OVOFFullWidthCharacterPackage.h"
#import "OVOFHanConvertPackage.h"
#import "OpenVanillaConfig.h"
#import "TrackerMaker.h"
#import "TrackerSender.h"
#import "YKSignedModuleLoadingSystem.h"

NSString *CVLoaderUpdateCannedMessagesNotification =
    @"CVLoaderUpdateCannedMessagesNotification";

static const char *kChiaKeySourceDatabaseFile = "ChiaKeySource.db";
static const char *kLegacyKeyKeySourceDatabaseFile = "KeyKeySource.db";

string FetchDatabaseVersionInfo(OVSQLiteConnection *connection,
                                const string &dbAndTableName) {
  string result;
  OVSQLiteStatement *statement = connection->prepare(
      "SELECT value FROM %s WHERE KEY = %Q", dbAndTableName.c_str(), "version");

  if (statement) {
    if (statement->step() == SQLITE_ROW) {
      result = statement->textOfColumn(0);
      while (statement->step() == SQLITE_ROW)
        ;
    }

    delete statement;
  }

  return result;
}

static bool ValidateChiaKeySourceDatabase(OVSQLiteConnection *connection,
                                         const string &databaseFile) {
  if (!connection) return false;

  const char *requiredTables[] = {
      "cooked_information",
      "prepopulated_service_data",
      "unigrams",
      "bigrams",
  };

  for (size_t index = 0;
       index < sizeof(requiredTables) / sizeof(requiredTables[0]); index++) {
    if (!connection->hasTable(requiredTables[index])) {
      NSLog(@"Rejected ChiaKeySource database %s: missing table %s",
            databaseFile.c_str(), requiredTables[index]);
      return false;
    }
  }

  return true;
}

static OVSQLiteDatabaseService *CreateValidatedChiaKeySourceDatabaseService(
    const string &databaseFile) {
  if (!OVPathHelper::PathExists(databaseFile)) return 0;

  OVSQLiteDatabaseService *service = OVSQLiteDatabaseService::Create(databaseFile);
  if (!service || !ValidateChiaKeySourceDatabase(service->connection(), databaseFile)) {
    if (service) delete service;
    return 0;
  }

  return service;
}

@interface NSData (LFHTTPFormExtensions)
+ (id)dataAsWWWURLEncodedFormFromDictionary:(NSDictionary *)formDictionary;
@end

@implementation NSData (LFHTTPFormExtensions)
+ (id)dataAsWWWURLEncodedFormFromDictionary:(NSDictionary *)formDictionary {
  NSMutableString *combinedDataString = [NSMutableString string];
  NSEnumerator *enumerator = [formDictionary keyEnumerator];
  id key;
  id value;

  if (key = [enumerator nextObject]) {
    value = [formDictionary objectForKey:key];
    [combinedDataString
        appendString:[NSString
                         stringWithFormat:
                             @"%@=%@",
                             [(NSString *)key
                                 stringByAddingPercentEscapesUsingEncoding:
                                     NSUTF8StringEncoding],
                             [value stringByAddingPercentEscapesUsingEncoding:
                                        NSUTF8StringEncoding]]];

    while ((key = [enumerator nextObject])) {
      value = [formDictionary objectForKey:key];
      [combinedDataString
          appendString:[NSString
                           stringWithFormat:
                               @"&%@=%@",
                               [(NSString *)key
                                   stringByAddingPercentEscapesUsingEncoding:
                                       NSUTF8StringEncoding],
                               [value stringByAddingPercentEscapesUsingEncoding:
                                          NSUTF8StringEncoding]]];
    }
  }

  return [combinedDataString dataUsingEncoding:NSUTF8StringEncoding
                          allowLossyConversion:NO];
}
@end

#define OVLOADER_HTTP_FIRST_FETCH_DELAY (5.0)
#define OVLOADER_HTTP_FAIL_RETRY_TIMEINTERVAL (5.0 * 60.0)  /* 5 min */
#define OVLOADER_HTTP_NEXT_FETCH_TIMEINTERVAL (30.0 * 60.0) /* 30 min */
#define OVLOADER_HTTP_CHECK_UPDATE_TIMEINTERVAL (86400.0)   /* 86400.0 */

#ifdef OVLOADER_USE_SQLITE_CRYPTO
void InitSQLiteCrypto(sqlite3 *db);
string FetchSQLiteCERODKey(const string &filename);
#endif

OpenVanillaLoader *OVLSharedInstance = nil;
NSLock *OVLSharedLock = nil;

using namespace OpenVanilla;

@implementation OpenVanillaLoader

#pragma mark Class methods

+ (OpenVanillaLoader *)sharedInstance {
  if (!OVLSharedInstance) {
    OVLSharedInstance = [[OpenVanillaLoader alloc] init];
  }

  return OVLSharedInstance;
}
+ (PVLoader *)sharedLoader {
  return [[OpenVanillaLoader sharedInstance] loader];
}
+ (PVLoaderService *)sharedLoaderService {
  return [[OpenVanillaLoader sharedInstance] loaderService];
}
+ (VersionChecker *)sharedVersionChecker {
  return [[OpenVanillaLoader sharedInstance] versionChecker];
}
+ (NSLock *)sharedLock {
  if (!OVLSharedLock) {
    OVLSharedLock = [[NSLock alloc] init];
  }

  return OVLSharedLock;
}
+ (void)releaseSharedObjects {
  [OVLSharedInstance release];
  [OVLSharedLock release];
}
+ (NSString *)locale {
  // See here http://developer.apple.com/qa/qa2006/qa1391.html
  // We'll return canonical locale names, so zh-Hant and zh-Hans instead of
  // zh_TW and zh_CN

  NSArray *languages =
      [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleLanguages"];
  if ([languages count]) return [languages objectAtIndex:0];

  return @"en";
}

#pragma mark Instance methods

- (id)init {
  if (self = [super init]) {
    _loaderPolicy = 0;
    _encodingService = 0;
    _loaderService = 0;
    _bundleLoadingSystem = 0;
    _staticModuleLoadingSystem = 0;
    _signedModuleLoadingSystem = 0;
    _loader = 0;
    _CINDatabaseService = 0;
    _SQLiteDatabaseService = 0;
    _versionChecker = 0;

    _autoUpdateHTTPRequest = [LFHTTPRequest new];
    [_autoUpdateHTTPRequest setDelegate:(id)self];
    [_autoUpdateHTTPRequest
        setContentType:LFHTTPRequestWWWFormURLEncodedContentType];

    _autoUpdateSignatureHTTPRequest = [LFHTTPRequest new];
    [_autoUpdateSignatureHTTPRequest setDelegate:(id)self];
    [_autoUpdateSignatureHTTPRequest
        setContentType:LFHTTPRequestWWWFormURLEncodedContentType];

    _cannedMessagesDataHTTPRequest = [LFHTTPRequest new];
    [_cannedMessagesDataHTTPRequest setDelegate:(id)self];

    _mergedCannedMessagesArray = [NSMutableArray new];

    _userCannedMessagePlist = 0;
    _userFreeCannedMessageFileTimestamp = new OVFileTimestamp;

    return self;
  }

  return self;
}
- (void)dealloc {
  if ([_downloadTask isRunning]) {
    [_downloadTask terminate];
  }

  [_downloadTask release];

  [self shutDown];

  [_autoUpdateHTTPRequest release];
  [_autoUpdateSignatureHTTPRequest release];
  [_cannedMessagesDataHTTPRequest release];

  [_mergedCannedMessagesArray release];

  if (_userCannedMessagePlist) {
    delete _userCannedMessagePlist;
  }

  [super dealloc];
}
- (void)createDatabaseServices {
  _userPersistence->setDefaultDatabaseConnection(0, "");

  if (_CINDatabaseService) {
    delete _CINDatabaseService;
    _CINDatabaseService = 0;
  }

  if (_SQLiteDatabaseService) {
    delete _SQLiteDatabaseService;
    _SQLiteDatabaseService = 0;
  }

  string resourcePath = [[[NSBundle mainBundle] resourcePath] UTF8String];
  string cinPath = OVPathHelper::PathCat(resourcePath, "DataTables");
  string dbPath = OVPathHelper::PathCat(resourcePath, "Databases");
  string userDataPath = OVDirectoryHelper::UserApplicationSupportDataDirectory(
      _loaderPolicy->loaderName());
  string userTablePath = OVPathHelper::PathCat(userDataPath, "DataTables");
  string userLexiconPath = OVPathHelper::PathCat(
      OVPathHelper::PathCat(userDataPath, "Lexicons"), "active");
  string userChiaKeySourceDBFile =
      OVPathHelper::PathCat(userLexiconPath, kChiaKeySourceDatabaseFile);
  string legacyUserKeyKeySourceDBFile =
      OVPathHelper::PathCat(userLexiconPath, kLegacyKeyKeySourceDatabaseFile);

  NSString *libAppSupportPath = [NSSearchPathForDirectoriesInDomains(
      NSApplicationSupportDirectory, NSLocalDomainMask, YES) objectAtIndex:0];

  string libAppSupportLoaderPath = OVPathHelper::PathCat(
      [libAppSupportPath UTF8String], _loaderPolicy -> loaderName());
  string libAppSupportDBPath =
      OVPathHelper::PathCat(libAppSupportLoaderPath, "Databases");
  string supplementDBFile =
      OVPathHelper::PathCat(libAppSupportDBPath, "Supplement.db");

  cerr << supplementDBFile << endl;

  string supplementDBVersion;
  string mainDBVersion;
  string bundledChiaKeyDBFile =
      OVPathHelper::PathCat(dbPath, kChiaKeySourceDatabaseFile);
  string legacyBundledKeyKeyDBFile =
      OVPathHelper::PathCat(dbPath, kLegacyKeyKeySourceDatabaseFile);

#ifdef OPENVANILLA_CEROD_DATABASE_FILE
  string dbFile = OVPathHelper::PathCat(
      dbPath, [OPENVANILLA_CEROD_DATABASE_FILE UTF8String]);
#else
  string dbFile =
      OVPathHelper::PathCat(dbPath, _loaderPolicy->defaultDatabaseFileName());
#endif

  // NSLog(@"cin path = %s", cinPath.c_str());
  _CINDatabaseService = new OVCINDatabaseService(cinPath, "*.cin", "", 0);
  if (_CINDatabaseService) {
    if (OVDirectoryHelper::CheckDirectory(userTablePath)) {
      // NSLog(@"user cin path = %s", userTablePath.c_str());
      _CINDatabaseService->addDirectory(userTablePath, "*.cin", "", 0);
    }

    // NSLog(@"tables available = %d", _CINDatabaseService->tables().size());
  }

  // NSLog(@"db file = %s", dbFile.c_str());

  OVSQLiteConnection *dbc = 0;
  string selectedDBFile;

  _SQLiteDatabaseService =
      CreateValidatedChiaKeySourceDatabaseService(userChiaKeySourceDBFile);
  if (_SQLiteDatabaseService) {
    selectedDBFile = userChiaKeySourceDBFile;
    NSLog(@"Using external ChiaKey lexicon database: %s",
          selectedDBFile.c_str());
  } else if (OVPathHelper::PathExists(userChiaKeySourceDBFile)) {
    NSLog(@"Falling back from invalid external ChiaKey lexicon database: %s",
          userChiaKeySourceDBFile.c_str());
  }

  if (!_SQLiteDatabaseService) {
    _SQLiteDatabaseService =
        CreateValidatedChiaKeySourceDatabaseService(legacyUserKeyKeySourceDBFile);
    if (_SQLiteDatabaseService) {
      selectedDBFile = legacyUserKeyKeySourceDBFile;
      NSLog(@"Using legacy external ChiaKey lexicon database: %s",
            selectedDBFile.c_str());
    } else if (OVPathHelper::PathExists(legacyUserKeyKeySourceDBFile)) {
      NSLog(@"Falling back from invalid legacy external ChiaKey lexicon database: %s",
            legacyUserKeyKeySourceDBFile.c_str());
    }
  }

  if (!_SQLiteDatabaseService) {
    _SQLiteDatabaseService =
        CreateValidatedChiaKeySourceDatabaseService(bundledChiaKeyDBFile);
    if (_SQLiteDatabaseService) {
      selectedDBFile = bundledChiaKeyDBFile;
      NSLog(@"Using bundled ChiaKey lexicon database: %s",
            selectedDBFile.c_str());
    }
  }

  if (!_SQLiteDatabaseService) {
    _SQLiteDatabaseService =
        CreateValidatedChiaKeySourceDatabaseService(legacyBundledKeyKeyDBFile);
    if (_SQLiteDatabaseService) {
      selectedDBFile = legacyBundledKeyKeyDBFile;
      NSLog(@"Using legacy bundled ChiaKey lexicon database: %s",
            selectedDBFile.c_str());
    }
  }

  if (!_SQLiteDatabaseService && OVPathHelper::PathExists(dbFile)) {
#ifndef OVLOADER_USE_SQLITE_CRYPTO
    _SQLiteDatabaseService = CreateValidatedChiaKeySourceDatabaseService(dbFile);
    if (_SQLiteDatabaseService) selectedDBFile = dbFile;
#else
#ifdef OPENVANILLA_CEROD_DATABASE_FILE
    string openedDBFile = FetchSQLiteCERODKey(dbFile);
    dbc = OVSQLiteConnection::Open(openedDBFile);

    if (dbc && OVPathHelper::PathExists(supplementDBFile)) {
      NSLog(@"supplement database file = %s, exists: %d",
            supplementDBFile.c_str(),
            OVPathHelper::PathExists(supplementDBFile));

      string openedSupplementDBFile = FetchSQLiteCERODKey(supplementDBFile);
      int attachResult =
          dbc->execute("ATTACH %Q AS supplement",
                       openedSupplementDBFile.c_str());
      // NSLog(@"attach result: %d", attachResult);

      if (attachResult == SQLITE_OK) {
        NSLog(@"fetching attached db version info");
        supplementDBVersion =
            FetchDatabaseVersionInfo(dbc, "supplement.cooked_information");
      }
    }
#else
    dbc = OVSQLiteConnection::Open(dbFile);
    if (dbc) InitSQLiteCrypto(dbc->connection());
#endif

    if (dbc && ValidateChiaKeySourceDatabase(dbc, dbFile)) {
      _SQLiteDatabaseService =
          OVSQLiteDatabaseService::ServiceWithExistingConnection(dbc, true);
      selectedDBFile = dbFile;

      if (dbc->execute("PRAGMA synchronous = OFF") == SQLITE_OK) {
        // NSLog(@"pragma executed");
      } else {
        // NSLog(@"pragma execution failed");
      }
    } else if (dbc) {
      delete dbc;
      dbc = 0;
    }
#endif
  }

  if (_SQLiteDatabaseService) {
    mainDBVersion =
        FetchDatabaseVersionInfo(_SQLiteDatabaseService->connection(),
                                 "cooked_information");
  }

  if (supplementDBVersion.size()) {
    _versionChecker->registerComponentVersion(
        OPENVANILLA_DATABASE_COMPONENT_NAME, supplementDBVersion, true);
    NSLog(@"Registered supplement DB '%s' version '%s'",
          OPENVANILLA_DATABASE_COMPONENT_NAME, supplementDBVersion.c_str());

    [_databaseVersion autorelease];
    _databaseVersion =
        [[NSString alloc] initWithUTF8String:supplementDBVersion.c_str()];

    // see if main DB's version is newer!
    if (mainDBVersion.size()) {
      if (VersionNumber(mainDBVersion) >= VersionNumber(supplementDBVersion)) {
        NSLog(@"Detaching supplement DB because it's older");
        if (dbc) dbc->execute("DETACH supplement");
        _versionChecker->registerComponentVersion(
            OPENVANILLA_DATABASE_COMPONENT_NAME, mainDBVersion, true);
        [_databaseVersion autorelease];
        _databaseVersion =
            [[NSString alloc] initWithUTF8String:mainDBVersion.c_str()];
      }
    }
  } else if (mainDBVersion.size()) {
    _versionChecker->registerComponentVersion(
        OPENVANILLA_DATABASE_COMPONENT_NAME, mainDBVersion, true);
    NSLog(@"Registered main DB '%s' version '%s'",
          OPENVANILLA_DATABASE_COMPONENT_NAME, mainDBVersion.c_str());

    [_databaseVersion autorelease];
    _databaseVersion =
        [[NSString alloc] initWithUTF8String:mainDBVersion.c_str()];
  }

  if (!_SQLiteDatabaseService) {
    NSLog(
        @"Cannot open database file %s, use in-memory SQLite database instead",
        userChiaKeySourceDBFile.c_str());
    _SQLiteDatabaseService = OVSQLiteDatabaseService::Create();
  }
  _userPersistence->setDefaultDatabaseConnection(
      _SQLiteDatabaseService->connection(), "prepopulated_service_data");

  if (!_userCannedMessagePlist) {
    _userCannedMessagePlist = new PVPropertyList(
        OVPathHelper::PathCat(userDataPath, "UserCannedMessages.plist"));
  }
}

- (void)_firstTimeUpdateUserData {
  [self mergeCannedMessagesData];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:CVLoaderUpdateCannedMessagesNotification
                    object:self];
}

- (void)_addInitializedStaticMoudlePackages {
  OVModulePackage *pkg;
  OVPathInfo pathInfo = _loaderPolicy->modulePackagePathInfoFromPath("");

  pkg = new OVIMMandarinPackage;
  pkg->initialize(&pathInfo, _loaderService);
  _staticModuleLoadingSystem->addInitializedPackage("OVIMMandarinPackage", pkg);

  pkg = new OVIMGenericPackage;
  pkg->initialize(&pathInfo, _loaderService);
  _staticModuleLoadingSystem->addInitializedPackage("OVIMGenericPackage", pkg);

  pkg = new OVOFFullWidthCharacterPackage;
  pkg->initialize(&pathInfo, _loaderService);
  _staticModuleLoadingSystem->addInitializedPackage(
      "OVOFFullWidthCharacterPackage", pkg);

  pkg = new OVOFHanConvertPackage;
  pkg->initialize(&pathInfo, _loaderService);
  _staticModuleLoadingSystem->addInitializedPackage("OVOFHanConvertPackage",
                                                    pkg);

  pkg = new OVAFBopomofoCorrectionPackage;
  pkg->initialize(&pathInfo, _loaderService);
  _staticModuleLoadingSystem->addInitializedPackage(
      "OVAFBopomofoCorrectionPackage", pkg);

  pkg = new OVAFEvalPackage;
  pkg->initialize(&pathInfo, _loaderService);
  _staticModuleLoadingSystem->addInitializedPackage("OVAFEvalPackage", pkg);
}

- (void)reload {
  [[OpenVanillaLoader sharedLock] lock];

  // finalize loader's inner workings
  _loaderService->logger("OpenVanilla")
      << "Preparing to reload OpenVanilla" << endl;
  _loader->prepareReload();

  // now loader's module package manager is dead, we now refresh the bundle
  // loading system
  _staticModuleLoadingSystem->flushModules();
  _bundleLoadingSystem->unloadAllUnloadables();
  _bundleLoadingSystem->reset();
  _bundleLoadingSystem->rescan(_loaderPolicy);

  _signedModuleLoadingSystem->unloadAllUnloadables();
  _signedModuleLoadingSystem->reset();
  _signedModuleLoadingSystem->rescan(_signedModulesLoaderPolicy);

  // reload the databases
  [self createDatabaseServices];
  _loaderService->setCINDatabaseService(_CINDatabaseService);
  _loaderService->setSQLiteDatabaseService(_SQLiteDatabaseService);

  [self _addInitializedStaticMoudlePackages];

  _loader->reload();

  // NSLog(@"loaded: %@", [self dynamicallyLoadedModulePackageInfo]);

  [[OpenVanillaLoader sharedLock] unlock];
}
- (bool)start:(NSArray *)loadPaths {
  NSAutoreleasePool *pool = [NSAutoreleasePool new];
  [[OpenVanillaLoader sharedLock] lock];

  if (_loader) return true;

  vector<string> cppLoadPaths;

#ifndef OVLOADER_SUPPRESS_LOADPATHS
  NSEnumerator *loadPathsEnumerator = [loadPaths objectEnumerator];
  NSString *path;
  while (path = [loadPathsEnumerator nextObject]) {
    //    for (NSString* path in loadPaths) {
    cppLoadPaths.push_back([path UTF8String]);
  }
#endif

  NSBundle *bundle = [NSBundle mainBundle];
  NSDictionary *infoDictionary = [bundle infoDictionary];
  NSString *bundleVersion = [infoDictionary objectForKey:@"CFBundleVersion"];

  _versionChecker = new VersionChecker;
  if (bundleVersion) {
    // NSLog(@"%s version %@", OPENVANILLA_LOADER_COMPONENT_NAME,
    // bundleVersion);
    _versionChecker->registerComponentVersion(OPENVANILLA_LOADER_COMPONENT_NAME,
                                              [bundleVersion UTF8String]);
  }

  _loaderPolicy = new PVLoaderPolicy(cppLoadPaths);
  string loaderUserDataPath =
      OVDirectoryHelper::UserApplicationSupportDataDirectory(
          _loaderPolicy->loaderName());

  vector<string> signedModuleLoadPaths;
  do {
    NSString *libAppSupportPath = [NSSearchPathForDirectoriesInDomains(
        NSApplicationSupportDirectory, NSLocalDomainMask, YES) objectAtIndex:0];

    string libAppSupportLoaderPath = OVPathHelper::PathCat(
        [libAppSupportPath UTF8String], _loaderPolicy -> loaderName());
    string modulePath =
        OVPathHelper::PathCat(libAppSupportLoaderPath, "SignedModules");

    //		if (OVPathHelper::PathExists(modulePath) &&
    // OVPathHelper::IsDirectory(modulePath)) { 			NSLog(@"has signed module
    // path: %s", modulePath.c_str());
    signedModuleLoadPaths.push_back(modulePath);
    //		}
  } while (0);
  _signedModulesLoaderPolicy = new PVLoaderPolicy(signedModuleLoadPaths);
  _signedModuleLoadingSystem =
      new YKSignedModuleLoadingSystem(_signedModulesLoaderPolicy);

  // create user persistence
  string userPersistenceDBPath =
      OVPathHelper::PathCat(loaderUserDataPath, "UserData.db");
  _userPersistence = new OVLoaderUserPersistence(userPersistenceDBPath);
  [self createDatabaseServices];

  // allows user modules...
  string userModulePath = OVPathHelper::PathCat(loaderUserDataPath, "Modules");
  _loaderPolicy->addModulePackageLoadPath(userModulePath);

  _encodingService = new CVEncodingService;

  string naturalLocale = [[OpenVanillaLoader locale] UTF8String];
  naturalLocale = OVLocale::POSIXLocaleID(naturalLocale);

  _loaderService = new PVLoaderService(
      naturalLocale, _CINDatabaseService, _SQLiteDatabaseService,
      0 /* uses default log emitter */, _encodingService);
  _bundleLoadingSystem = new PVBundleLoadingSystem(_loaderPolicy);

  OVPathInfo pathInfo = _loaderPolicy->modulePackagePathInfoFromPath("");

  // and we want _staticModuleLoadingSystem to own the packages we created, so
  // that we don't have to worry about them
  _staticModuleLoadingSystem =
      new PVStaticModulePackageLoadingSystem(pathInfo, true);
  [self _addInitializedStaticMoudlePackages];

  vector<PVModulePackageLoadingSystem *> loadingSystems;
  loadingSystems.push_back(_staticModuleLoadingSystem);
  loadingSystems.push_back(_bundleLoadingSystem);
  loadingSystems.push_back(_signedModuleLoadingSystem);

  _loader = new PVLoader(_loaderPolicy, _loaderService, loadingSystems);

  OVKeyValueMap kvm = _loader->configKeyValueMap();
  bool writeConfig = false;

  string platformSummary = SystemInfo::PlatformSummary();
  string loaderVersion;
  loaderVersion = [bundleVersion UTF8String];

  if (kvm.stringValueForKey("PlatformSummary") != platformSummary) {
    kvm.setKeyStringValue("PlatformSummary", platformSummary);
    writeConfig = true;
  }

  if (kvm.stringValueForKey("LoaderVersion") != loaderVersion) {
    kvm.setKeyStringValue("LoaderVersion", loaderVersion);
    writeConfig = true;
  }

  if (!kvm.hasKey("UUID")) {
    kvm.setKeyStringValue("UUID", UUIDHelper::CreateUUID());
    writeConfig = true;
  }

  if (writeConfig) _loader->syncLoaderConfig(true);

  if (!_loader->primaryInputMethod().size()) {
    _loader->setPrimaryInputMethod(OVIMTRADITIONALMANDARIN_IDENTIFIER);
    _loader->syncSandwichConfig();
  }

  // NSLog(@"unlocking");
  [[OpenVanillaLoader sharedLock] unlock];

  // NSLog(@"scheduling");
  [self performSelectorOnMainThread:@selector(_firstTimeUpdateUserData)
                         withObject:nil
                      waitUntilDone:NO];
  [self performSelectorOnMainThread:@selector(scheduleDataProvisionServices)
                         withObject:nil
                      waitUntilDone:NO];

  sleep(1);

  // NSLog(@"loaded: %@", [self dynamicallyLoadedModulePackageInfo]);

  [pool drain];
  return true;
}
- (void)shutDown {
  [[OpenVanillaLoader sharedLock] lock];

  if (_loader) {
    delete _loader;
    _loader = 0;
  }

  if (_staticModuleLoadingSystem) {
    delete _staticModuleLoadingSystem;
    _staticModuleLoadingSystem = 0;
  }

  if (_bundleLoadingSystem) {
    delete _bundleLoadingSystem;
    _bundleLoadingSystem = 0;
  }

  if (_signedModuleLoadingSystem) {
    delete _signedModuleLoadingSystem;
    _signedModuleLoadingSystem = 0;
  }

  if (_loaderService) {
    delete _loaderService;
    _loaderService = 0;
  }

  if (_CINDatabaseService) {
    delete _CINDatabaseService;
    _CINDatabaseService = 0;
  }

  if (_SQLiteDatabaseService) {
    delete _SQLiteDatabaseService;
    _SQLiteDatabaseService = 0;
  }

  if (_encodingService) {
    delete _encodingService;
    _encodingService = 0;
  }

  if (_signedModulesLoaderPolicy) {
    delete _signedModulesLoaderPolicy;
    _signedModulesLoaderPolicy = 0;
  }

  if (_loaderPolicy) {
    delete _loaderPolicy;
    _loaderPolicy = 0;
  }

  if (_versionChecker) {
    delete _versionChecker;
    _versionChecker = 0;
  }

  [[OpenVanillaLoader sharedLock] unlock];
}
- (PVLoader *)loader {
  return _loader;
}
- (PVLoaderService *)loaderService {
  return _loaderService;
}
- (NSArray *)identifiersAndLocalizedNamesWithPattern:(NSString *)pattern {
  NSMutableArray *result = [NSMutableArray array];
  vector<pair<string, string> > rsp = _loader->allModuleIdentifiersAndNames();
  auto x = [pattern UTF8String];
  OVWildcard exp((OpenVanilla::string(x)));

  for (vector<pair<string, string> >::iterator ri = rsp.begin();
       ri != rsp.end(); ++ri) {
    if (exp.match((*ri).first))
      [result
          addObject:[NSArray
                        arrayWithObjects:
                            [NSString stringWithUTF8String:(*ri).first.c_str()],
                            [NSString
                                stringWithUTF8String:(*ri).second.c_str()],
                            nil]];
  }
  return result;
}
- (bool)exportUserPhraseDBToFile:(NSString *)path {
  string ufn = [path UTF8String];
  OVPathInfo pathInfo = _loaderPolicy->modulePackagePathInfoFromPath("");
  OVSQLiteConnection *db =
      BPMFUserPhraseHelper::OpenUserPhraseDB(&pathInfo, _loaderService);
  if (!db) return false;

  bool result = BPMFUserPhraseHelper::Export(db, ufn);
  delete db;
  return result;
}
- (bool)importUserPhraseDBFromFile:(NSString *)path {
  string ufn = [path UTF8String];
  OVPathInfo pathInfo = _loaderPolicy->modulePackagePathInfoFromPath("");
  OVSQLiteConnection *db =
      BPMFUserPhraseHelper::OpenUserPhraseDB(&pathInfo, _loaderService);
  if (!db) return false;

  bool result = BPMFUserPhraseHelper::Import(db, ufn);
  delete db;

  // flush the config, thus flush its LM cache
  _loader->forceSyncModuleConfigForNextRound("SmartMandarin");
  return result;
}

- (VersionChecker *)versionChecker {
  return _versionChecker;
}

- (NSString *)databaseVersion {
  return _databaseVersion;
}

- (OVSQLiteConnection *)_userPhraseDBConnection {
  if (!_userPhraseDB) {
    OVPathInfo pathInfo = _loaderPolicy->modulePackagePathInfoFromPath("");
    _userPhraseDB =
        BPMFUserPhraseHelper::OpenUserPhraseDB(&pathInfo, _loaderService);
  }

  if (_userPhraseDB) {
  } else {
    NSLog(@"Cannot open user phrase DB");
  }

  return _userPhraseDB;
}
- (BOOL)userPhraseDBCanProvideService {
  return !![self _userPhraseDBConnection];
}
- (int)userPhraseDBNumberOfRow {
  int count = 0;

  if (![self _userPhraseDBConnection]) {
    return count;
  }

  OVSQLiteStatement *st =
      _userPhraseDB->prepare("SELECT count(*) FROM user_unigrams");
  if (st) {
    while (st->step() == SQLITE_ROW) {
      count = st->intOfColumn(0);
    }
    delete st;
  }

  return count;
}
- (NSDictionary *)userPhraseDBDictionaryAtRow:(int)row {
  NSMutableDictionary *result = [NSMutableDictionary dictionary];
  if (![self _userPhraseDBConnection]) {
    return result;
  }

  OVSQLiteStatement *select = _userPhraseDB->prepare(
      "SELECT * FROM user_unigrams WHERE rowid = %d", row + 1);
  while (select->step() == SQLITE_ROW) {
    // string qstring = select->textOfColumn(0);
    // string current = select->textOfColumn(1);
    // string probability = select->textOfColumn(2);
    // string backoff = select->textOfColumn(3);

    [result setObject:[NSString stringWithUTF8String:select->textOfColumn(1)]
               forKey:@"Text"];
    [result
        setObject:[NSString
                      stringWithUTF8String:BPMFUserPhraseHelper::BPMFString(
                                               string(select->textOfColumn(0)))
                                               .c_str()]
           forKey:@"BPMF"];
  }

  return result;
}
- (NSArray *)userPhraseDBReadingsForPhrase:(NSString *)phrase {
  NSMutableArray *results = [NSMutableArray array];
  vector<string> codepoints =
      OVUTF8Helper::SplitStringByCodePoint([phrase UTF8String]);

  OVSQLiteStatement *select = dynamic_cast<OVSQLiteDatabaseService *>(
                                  _loaderService->SQLiteDatabaseService())
                                  ->connection()
                                  ->prepare(
                                      "SELECT qstring FROM unigrams WHERE "
                                      "current = ? ORDER BY probability DESC");

  OVKeyValueDataTableInterface *tbl =
      _loaderService->SQLiteDatabaseService()->createKeyValueDataTableInterface(
          "Mandarin-bpmf-cin");

  vector<vector<string> > phraseBPMFs;
  phraseBPMFs.push_back(vector<string>());

  OVWildcard exp("*#");
  for (vector<string>::const_iterator cpi = codepoints.begin();
       cpi != codepoints.end(); ++cpi) {
    vector<string> bpmfs;
    set<string> dedup;

    if (select) {
      NSLog(@"has select, querying: %@",
            [NSString stringWithUTF8String:(*cpi).c_str()]);
      select->bindTextToColumn(*cpi, 1);
      while (select->step() == SQLITE_ROW) {
        string b = select->textOfColumn(0);

        if (exp.match(b)) continue;

        cerr << b << endl;
        dedup.insert(b);
        bpmfs.push_back(b);
      }
      select->reset();
    }

    vector<string> extBpmfs = tbl->keysForValue(*cpi);
    for (vector<string>::iterator ebi = extBpmfs.begin(); ebi != extBpmfs.end();
         ++ebi) {
      if (dedup.find(*ebi) == dedup.end()) {
        dedup.insert(*ebi);
        bpmfs.push_back(*ebi);
      }
    }

    if (!bpmfs.size()) {
      bpmfs = tbl->keysForValue("ㄅ");
    }

    vector<vector<string> > npb;
    for (vector<vector<string> >::const_iterator pbi = phraseBPMFs.begin();
         pbi != phraseBPMFs.end(); ++pbi) {
      for (vector<string>::const_iterator bi = bpmfs.begin(); bi != bpmfs.end();
           ++bi) {
        vector<string> newEntry = *pbi;
        newEntry.push_back(BPMF::FromAbsoluteOrderString(*bi).composedString());
        npb.push_back(newEntry);
      }
    }
    phraseBPMFs = npb;
  }

  for (vector<vector<string> >::const_iterator pbi = phraseBPMFs.begin();
       pbi != phraseBPMFs.end(); ++pbi) {
    [results
        addObject:[NSString stringWithUTF8String:OVStringHelper::Join(*pbi, ",")
                                                     .c_str()]];
  }

  if (select) {
    delete select;
  }

  return results;
}
- (void)userPhraseDBSave {
  if ([self _userPhraseDBConnection]) {
    _userPhraseDB->execute("VACUUM");
    delete _userPhraseDB;
    _userPhraseDB = 0;
  }
  _loader->forceSyncModuleConfigForNextRound("SmartMandarin");
}
- (string)_qstringFromReading:(NSString *)reading {
  vector<string> readings = OVStringHelper::Split([reading UTF8String], ',');
  string newReading;

  for (vector<string>::const_iterator ri = readings.begin();
       ri != readings.end(); ++ri) {
    BPMF b = BPMF::FromComposedString(*ri);
    if (b.isEmpty()) continue;

    newReading += b.absoluteOrderString();
  }
  return newReading;
}
- (void)userPhraseDBSetNewReading:(NSString *)reading forPhraseAtRow:(int)row {
  if (![self _userPhraseDBConnection]) {
    return;
  }

  _userPhraseDB->execute(
      "UPDATE user_unigrams SET qstring = %Q WHERE rowid = %d",
      [self _qstringFromReading:reading].c_str(), row + 1);
  _loader->forceSyncModuleConfigForNextRound("SmartMandarin");
}

- (void)userPhraseDBDeleteRow:(int)row {
  if (![self _userPhraseDBConnection]) {
    return;
  }

  _userPhraseDB->execute("BEGIN");
  _userPhraseDB->execute("CREATE TEMP TABLE uu_temp(a, b, c, d)");
  _userPhraseDB->execute("INSERT INTO uu_temp SELECT * from user_unigrams");
  _userPhraseDB->execute("DELETE FROM uu_temp WHERE rowid = %d", row + 1);
  _userPhraseDB->execute("DELETE FROM user_unigrams");
  _userPhraseDB->execute("INSERT INTO user_unigrams SELECT * from uu_temp");
  _userPhraseDB->execute("DROP TABLE uu_temp");
  _userPhraseDB->execute("END");
  _loader->forceSyncModuleConfigForNextRound("SmartMandarin");
}
- (void)userPhraseDBAddNewRow:(NSString *)phrase {
  if (![self _userPhraseDBConnection]) {
    return;
  }

  NSString *reading =
      [[self userPhraseDBReadingsForPhrase:phrase] objectAtIndex:0];
  _userPhraseDB->execute(
      "INSERT INTO user_unigrams (qstring, current, probability, backoff) "
      "VALUES (%Q, %Q, %f, %f)",
      [self _qstringFromReading:reading].c_str(), [phrase UTF8String], -1.0,
      0.0);

  _loader->forceSyncModuleConfigForNextRound("SmartMandarin");
}
- (void)userPhraseDBAddNewRows:(NSArray *)array {
  if (![self _userPhraseDBConnection]) {
    return;
  }

  // in theory we need to lock the loader (stop user action) otherwise Manjusri
  // would not be able to write in the cache, but let's not do that for now

  _userPhraseDB->execute("BEGIN");

  NSString *phrase;
  NSEnumerator *enumerator = [array objectEnumerator];
  while (phrase = [enumerator nextObject]) {
    // NSLog(@"before looking for reading");
    NSString *reading =
        [[self userPhraseDBReadingsForPhrase:phrase] objectAtIndex:0];
    // NSLog(@"before insert");
    _userPhraseDB->execute(
        "INSERT INTO user_unigrams (qstring, current, probability, backoff) "
        "VALUES (%Q, %Q, %f, %f)",
        [self _qstringFromReading:reading].c_str(), [phrase UTF8String], -1.0,
        0.0);
  }

  _userPhraseDB->execute("COMMIT");

  _loader->forceSyncModuleConfigForNextRound("SmartMandarin");
}

- (void)userPhraseDBSetPhrase:(NSString *)phrase atRow:(int)row {
  if (![self _userPhraseDBConnection]) {
    return;
  }

  NSString *reading =
      [[self userPhraseDBReadingsForPhrase:phrase] objectAtIndex:0];
  _userPhraseDB->execute(
      "UPDATE user_unigrams SET qstring = %Q, current = %Q WHERE rowid = %d",
      [self _qstringFromReading:reading].c_str(), [phrase UTF8String], row + 1);
  _loader->forceSyncModuleConfigForNextRound("SmartMandarin");
}

- (NSURL *)serverEndpointWithDefaultURLString:(NSString *)str
                            overrideConfigKey:(NSString *)overrideKey {
  OVKeyValueMap kvm = _loader->configKeyValueMap();
  NSString *urlStr = str;

  if (kvm.hasKey([overrideKey UTF8String])) {
    urlStr = [NSString
        stringWithUTF8String:kvm.stringValueForKey([overrideKey UTF8String])
                                 .c_str()];
  }

  return [NSURL URLWithString:urlStr];
}

- (BOOL)_tellIfLastCheckTimeElapsedAndUpdateWithKeyName:(NSString *)key {
  // NSLog(@"_tellIfLastCheckTimeElapsedAndUpdateWithKeyName:%@", key);

  OVKeyValueMap kvm = _loader->configKeyValueMap();

  NSTimeInterval now = [[NSDate date] timeIntervalSince1970];
  NSString *timeStr = [NSString stringWithFormat:@"%f", now];
  if (!kvm.hasKey([key UTF8String])) {
    kvm.setKeyStringValue([key UTF8String], [timeStr UTF8String]);
    _loader->syncLoaderConfig(true);
    return YES;
  }

  NSString *lastChecked = [NSString
      stringWithUTF8String:kvm.stringValueForKey([key UTF8String]).c_str()];

  // NSLog(@"last checked: %@, now: %@, diff: %f", lastChecked, timeStr, now -
  // [lastChecked doubleValue]);

  if (now - [lastChecked doubleValue] >=
      OVLOADER_HTTP_CHECK_UPDATE_TIMEINTERVAL) {
    kvm.setKeyStringValue([key UTF8String], [timeStr UTF8String]);
    _loader->syncLoaderConfig(true);
    return YES;
  }

  return NO;
}

- (void)_handleAutoUpdateTimer:(NSTimer *)timer {
  // NSLog(@"%s", __PRETTY_FUNCTION__);

  if (![self _tellIfLastCheckTimeElapsedAndUpdateWithKeyName:
                 @"AutoUpdateLastCheckedTime"]) {
    [NSTimer
        scheduledTimerWithTimeInterval:OVLOADER_HTTP_NEXT_FETCH_TIMEINTERVAL
                                target:self
                              selector:@selector(_handleAutoUpdateTimer:)
                              userInfo:nil
                               repeats:NO];
    return;
  }

  NSURL *reqURL = [self serverEndpointWithDefaultURLString:VERSION_INFO_URL
                                         overrideConfigKey:@"VersionInfoURL"];

  stringstream sst;
  sst << *(_loader->configRootDictionary()) << endl;

  // this time we remember to POST the entire plist to the server...
  NSDictionary *userInfo = [NSDictionary
      dictionaryWithObjectsAndKeys:[NSString
                                       stringWithUTF8String:sst.str().c_str()],
                                   @"userinfo", nil];
  NSData *postData = [NSData dataAsWWWURLEncodedFormFromDictionary:userInfo];

  // NSLog(@"Posting POST on URL: %@", reqURL);
  [_autoUpdateHTTPRequest performMethod:LFHTTPRequestPOSTMethod
                                  onURL:reqURL
                               withData:postData];

  // we also send the tracker beacon; hateful, but that's life
  Takao::TrackerMaker tm;
  NSString *startURL =
      [NSString stringWithUTF8String:tm.loaderStartURLString().c_str()];

  // remember we must do this on the main thread because the HTTP request will
  // be autoreleased later when it's done (or when it fails)
  [[TrackerSender sharedTrackerSender] sendTrackerWithURLString:startURL];
}

- (void)_handleCannedMessagesTimer:(NSTimer *)timer {
  if (![self _tellIfLastCheckTimeElapsedAndUpdateWithKeyName:
                 @"CannedMessagesLastCheckedTime"]) {
    [NSTimer
        scheduledTimerWithTimeInterval:OVLOADER_HTTP_NEXT_FETCH_TIMEINTERVAL
                                target:self
                              selector:@selector(_handleCannedMessagesTimer:)
                              userInfo:nil
                               repeats:NO];
    return;
  }

  NSURL *reqURL =
      [self serverEndpointWithDefaultURLString:TAKAO_CANNED_MESSAGES_URL
                             overrideConfigKey:@"CannedMessagesEndpointURL"];
  [_cannedMessagesDataHTTPRequest performMethod:LFHTTPRequestGETMethod
                                          onURL:reqURL
                                       withData:nil];
}

- (void)_launchDownload:(NSString *)actionURL
           signatureURL:(NSString *)signatureURL
           changeLogURL:(NSString *)changeLogURL {
  // NSLog(@"launch download");

  if (_downloadTask) {
    if ([_downloadTask isRunning]) {
      return;
    }

    [_downloadTask release];
    _downloadTask = nil;
  }

  NSString *supportPath = [[[NSBundle mainBundle] bundlePath]
      stringByAppendingString:@"/Contents/SharedSupport"];
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
  NSString *commandPath =
      [supportPath stringByAppendingString:
                       @"/DownloadUpdate.app/Contents/MacOS/DownloadUpdate"];
#else
  NSString *commandPath = [supportPath
      stringByAppendingString:
          @"/DownloadUpdateTiger.app/Contents/MacOS/DownloadUpdateTiger"];
#endif

  NSArray *arguments = [NSArray
      arrayWithObjects:actionURL, signatureURL,
                       ((changeLogURL && [changeLogURL length]) ? changeLogURL
                                                                : @"(null)"),
                       nil];
  _downloadTask = [[NSTask launchedTaskWithLaunchPath:commandPath
                                            arguments:arguments] retain];
}

- (BOOL)_validateServerData:(NSData *)data {
  id plist =
      [NSPropertyListSerialization propertyListFromData:data
                                       mutabilityOption:NSPropertyListImmutable
                                                 format:NULL
                                       errorDescription:NULL];

  return [plist isKindOfClass:[NSDictionary class]];
}
- (void)httpRequestDidComplete:(LFHTTPRequest *)request {
  NSData *receivedData = [request receivedData];

  if (request != _autoUpdateSignatureHTTPRequest &&
      LFIsRunningUnderOSX10_4Only()) {
    // we need to see if it's gzipped data, especially if we run under OS X 10.4
    unsigned char *byteData = (unsigned char *)[receivedData bytes];
    if ([receivedData length] > 2) {
      if (byteData[0] == 0x1f && byteData[1] == 0x8b) {
        // int uncompress (Bytef *dest, uLongf *destLen, const Bytef *source,
        // uLong sourceLen); we assume a deflate rate of x50 (2%)
        uLongf destSize = (uLongf)([receivedData length] * 50);
        NSMutableData *decompressedData =
            [NSMutableData dataWithLength:destSize];
        z_stream zst;
        bzero(&zst, sizeof(zst));
        zst.next_in = byteData;
        zst.avail_in = [receivedData length];
        zst.total_in = 0;
        zst.next_out = (Bytef *)[decompressedData mutableBytes];
        zst.avail_out = destSize;
        zst.total_out = 0;
        zst.zalloc = Z_NULL;
        zst.zfree = Z_NULL;
        zst.opaque = Z_NULL;

        // 47 guesses the format
        int result = inflateInit2(&zst, 47);
        if (result != Z_OK) {
          NSLog(@"inflateInit2 error");
        } else {
          result = inflate(&zst, Z_NO_FLUSH);
          NSLog(@"zlib result code: %d, returned dest size: %d", result,
                zst.total_out);
          if (result != Z_STREAM_END) {
            NSLog(@"decompress error");
          } else {
            [decompressedData setLength:zst.total_out];
            // NSLog(@"real data size: %d (check: %d)", zst.total_out,
            // [decompressedData length]);
            receivedData = decompressedData;
          }
        }

      } else {
        // NSLog(@"normal data");
      }
    }
  }

  // NSLog(@"Received data, HTTP req obj %p", request);

  if (request == _autoUpdateHTTPRequest) {
    // got the auto update data, send the signature request
    NSURL *reqURL =
        [self serverEndpointWithDefaultURLString:VERSION_INFO_SIGNATURE_URL
                               overrideConfigKey:@"VersionInfoSignatureURL"];

    [_autoUpdateSignatureHTTPRequest setSessionInfo:receivedData];
    [_autoUpdateSignatureHTTPRequest performMethod:LFHTTPRequestGETMethod
                                             onURL:reqURL
                                          withData:nil];
  } else if (request == _autoUpdateSignatureHTTPRequest) {
    // now we have complete signature data
    NSDictionary *updateDictionary =
        [(CVApplicationController *)[NSApp delegate]
            shouldUpdateWithVersionInfoData:[request sessionInfo]
                   versionInfoSignatureData:receivedData];
    NSString *status = [updateDictionary valueForKey:@"Status"];
    if ([status isEqualToString:@"Yes"]) {
      NSString *actionURL = [updateDictionary objectForKey:@"ActionURL"];
      NSString *signatureURL = [updateDictionary objectForKey:@"SignatureURL"];
      NSString *changeLogURL = [updateDictionary objectForKey:@"ChangeLogURL"];
      [self _launchDownload:actionURL
               signatureURL:signatureURL
               changeLogURL:changeLogURL];
    }

    [NSTimer
        scheduledTimerWithTimeInterval:OVLOADER_HTTP_NEXT_FETCH_TIMEINTERVAL
                                target:self
                              selector:@selector(_handleAutoUpdateTimer:)
                              userInfo:nil
                               repeats:NO];
  } else if (request == _cannedMessagesDataHTTPRequest) {
    char buf[1];
    memset(buf, 0, 1);
    NSMutableData *nullTerminatedData = [receivedData mutableCopy];
    [nullTerminatedData appendBytes:buf length:1];

    if ([self _validateServerData:nullTerminatedData]) {
      string rcvStr = (const char *)[nullTerminatedData bytes];
      _userPersistence->populateIfValueDifferentUserDB("canned_messages",
                                                       rcvStr);
      [self mergeCannedMessagesData];
    }

    [NSTimer
        scheduledTimerWithTimeInterval:OVLOADER_HTTP_NEXT_FETCH_TIMEINTERVAL
                                target:self
                              selector:@selector(_handleCannedMessagesTimer:)
                              userInfo:nil
                               repeats:NO];
  }
}
- (void)httpRequest:(LFHTTPRequest *)request
    didFailWithError:(NSString *)error {
  // NSLog(@"HTTP request failed, request object: %p, now rescheduling",
  // request);

  if (request == _autoUpdateHTTPRequest ||
      request == _autoUpdateSignatureHTTPRequest) {
    [NSTimer
        scheduledTimerWithTimeInterval:OVLOADER_HTTP_FAIL_RETRY_TIMEINTERVAL
                                target:self
                              selector:@selector(_handleAutoUpdateTimer:)
                              userInfo:nil
                               repeats:NO];
  } else if (request == _cannedMessagesDataHTTPRequest) {
    [NSTimer
        scheduledTimerWithTimeInterval:OVLOADER_HTTP_FAIL_RETRY_TIMEINTERVAL
                                target:self
                              selector:@selector(_handleCannedMessagesTimer:)
                              userInfo:nil
                               repeats:NO];
  }
}

- (void)scheduleDataProvisionServices {
  NSLog(@"Skipping legacy online update and data provision services");
}

- (void)mergeCannedMessagesData {
  @synchronized(self) {
    [_mergedCannedMessagesArray removeAllObjects];

    string cannedMsgs =
        _userPersistence->fetchLatestValueByKeyAndPopulateUserDB(
            "canned_messages");
    PVPlistValue emptyDictionary(PVPlistValue::Dictionary);
    PVPlistValue *parsed =
        PVPropertyList::ParsePlistFromString(cannedMsgs.c_str());
    if (!parsed) parsed = &emptyDictionary;

    string localDT = OVDateTimeHelper::LocalDateTimeString();

    PVPlistValue msgArray(PVPlistValue::Array);
    PVPlistValue *pvs[2];
    pvs[0] = parsed;
    pvs[1] = _userCannedMessagePlist->rootDictionary();

    for (size_t pi = 0; pi < 2; pi++) {
      if (!pvs[pi]) continue;
      PVPlistValue *msgs = pvs[pi]->valueForKey("CannedMessages");
      if (msgs) {
        for (size_t i = 0; i < msgs->arraySize(); i++) {
          PVPlistValue *category = msgs->arrayElementAtIndex(i);

          string notBefore = category->stringValueForKey("NotBefore");
          if (notBefore.length() && localDT < notBefore) {
            continue;
          }

          string notAfter = category->stringValueForKey("NotAfter");
          if (notAfter.length() && localDT > notAfter) {
            continue;
          }

          msgArray.addArrayElement(category);
        }
      }
    }

    vector<string> userMessages;
    ifstream ifs;
    ifs.open([self userFreeCannedMessagePath].c_str(), ifstream::in);

    if (ifs.good()) {
      // ignore the first line
      string emptyLine;
      getline(ifs, emptyLine);
    }

    while (ifs.good()) {
      string line;
      getline(ifs, line);
      if (line.length()) {
        userMessages.push_back(line);
      }
    }
    ifs.close();

    if (userMessages.size()) {
      PVPlistValue userCategory(PVPlistValue::Dictionary);
      userCategory.setKeyValue("Name", [LFLSTR(@"User Defined") UTF8String]);
      PVPlistValue messages(PVPlistValue::Array);
      for (vector<string>::iterator umi = userMessages.begin();
           umi != userMessages.end(); ++umi) {
        PVPlistValue msg(*umi);
        messages.addArrayElement(&msg);
      }
      userCategory.setKeyValue("Messages", &messages);
      msgArray.addArrayElement(&userCategory);
    }

    PVPlistValue newData(PVPlistValue::Dictionary);
    newData.setKeyValue("CannedMessages", &msgArray);

    stringstream sst;
    sst << newData;

    const string &s = sst.str();
    const char *ndc = s.c_str();

    NSData *cmData = [NSData dataWithBytesNoCopy:(void *)ndc
                                          length:s.length()
                                    freeWhenDone:NO];
    id cmPlist = [NSPropertyListSerialization
        propertyListFromData:cmData
            mutabilityOption:NSPropertyListMutableContainersAndLeaves
                      format:NULL
            errorDescription:NULL];
    if (cmPlist) {
      NSArray *a = [cmPlist objectForKey:@"CannedMessages"];
      if (a) {
        if ([a isKindOfClass:[NSArray class]]) {
          [_mergedCannedMessagesArray addObjectsFromArray:a];
        }
      }
    }
  }

  [[NSNotificationCenter defaultCenter]
      postNotificationName:CVLoaderUpdateCannedMessagesNotification
                    object:self];
}

- (NSArray *)mergedCannedMessagesArray;
{
  @synchronized(self) {
    return _mergedCannedMessagesArray;
  }
}

- (void)syncUserCannedMessages {
  string path = [self userFreeCannedMessagePath];

  if (!OVPathHelper::PathExists(path)) {
    // populate the file with UTF-8 BOM
    FILE *stream = OVFileHelper::OpenStream(path, "w");
    if (stream) {
      NSString *BOMLine = LFLSTR(@"BOM-LINE");
      NSString *exampleLine = LFLSTR(@"EXAMPLE-LINE");

      fputs([BOMLine UTF8String], stream);
      fputs("\n", stream);
      fputs([exampleLine UTF8String], stream);
      fputs("\n", stream);

      fclose(stream);
    }
  }

  bool shouldMerge = false;
  OVFileTimestamp newTS = OVPathHelper::TimestampForPath(path);

  if (newTS > *_userFreeCannedMessageFileTimestamp) {
    *_userFreeCannedMessageFileTimestamp = newTS;
    shouldMerge = true;
  }

  if (_userCannedMessagePlist->shouldReadSync()) {
    _userCannedMessagePlist->readSync();
    shouldMerge = true;
  }

  if (shouldMerge) {
    [self mergeCannedMessagesData];
  }
}

- (const string)userFreeCannedMessagePath {
  string appDataDir = OVDirectoryHelper::UserApplicationSupportDataDirectory(
      _loaderPolicy->loaderName());
  OVDirectoryHelper::CheckDirectory(appDataDir);
  return OVPathHelper::PathCat(appDataDir, "UserCannedMessages.txt");
}

- (NSArray *)dynamicallyLoadedModulePackageInfo {
  NSMutableArray *result = [NSMutableArray array];

  set<string> excluded;
  vector<string> excludedList = _loader->excludedModulePackages();
  for (vector<string>::const_iterator ei = excludedList.begin();
       ei != excludedList.end(); ++ei) {
    excluded.insert(*ei);
  }

  vector<string> pkgNames;

  pkgNames = _signedModuleLoadingSystem->availablePackages();
  for (vector<string>::const_iterator pi = pkgNames.begin();
       pi != pkgNames.end(); ++pi) {
    string localizedName = (dynamic_cast<YKSignedModuleLoadingSystem *>(
                                _signedModuleLoadingSystem))
                               ->localizedNameForPackage(*pi, _loaderService);
    OVPathInfo info =
        _signedModuleLoadingSystem->pathInfoForPackage(*pi, _loaderPolicy);

    [result
        addObject:[NSDictionary
                      dictionaryWithObjectsAndKeys:
                          [NSString stringWithUTF8String:(*pi).c_str()],
                          OVServiceLoadedModulePackageIdentifierKey,
                          [NSString stringWithUTF8String:localizedName.c_str()],
                          OVServiceLoadedModulePackageLocalizedNameKey,
                          [NSString
                              stringWithUTF8String:info.loadedPath.c_str()],
                          OVServiceLoadedModulePackageBundlePathKey,
                          ((excluded.find(*pi) == excluded.end())
                               ? (id)kCFBooleanTrue
                               : (id)kCFBooleanFalse),
                          OVServiceLoadedModulePackageEnabledKey, nil]];
  }

  pkgNames = _bundleLoadingSystem->availablePackages();
  for (vector<string>::const_iterator pi = pkgNames.begin();
       pi != pkgNames.end(); ++pi) {
    string localizedName = *pi;
    OVPathInfo info =
        _bundleLoadingSystem->pathInfoForPackage(*pi, _loaderPolicy);

    [result
        addObject:[NSDictionary
                      dictionaryWithObjectsAndKeys:
                          [NSString stringWithUTF8String:(*pi).c_str()],
                          OVServiceLoadedModulePackageIdentifierKey,
                          [NSString stringWithUTF8String:localizedName.c_str()],
                          OVServiceLoadedModulePackageLocalizedNameKey,
                          [NSString
                              stringWithUTF8String:info.loadedPath.c_str()],
                          OVServiceLoadedModulePackageBundlePathKey,
                          ((excluded.find(*pi) == excluded.end())
                               ? (id)kCFBooleanTrue
                               : (id)kCFBooleanFalse),
                          OVServiceLoadedModulePackageEnabledKey, nil]];
  }

  return result;
}

- (void)setBlackListOfPackageIdentifers:(NSArray *)inIdentifiers {
  vector<string> list;
  NSEnumerator *ie = [inIdentifiers objectEnumerator];
  while (NSString *i = [ie nextObject]) {
    list.push_back([i UTF8String]);
  }

  _loader->setExcludedModulePackages(list);
}

@end
