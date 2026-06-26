// [AUTO_HEADER]

#import "OpenVanillaLoader.h"

#include <set>
#include <sstream>

#import "BPMFUserPhraseHelper.h"
#import "LFCrossDevelopmentTools.h"
#import "LFUtilities.h"
#import "OVAFBopomofoCorrectionPackage.h"
#import "OVAFEvalPackage.h"
#import "OVIMGenericPackage.h"
#import "OVIMMandarinPackage.h"
#import "OVOFFullWidthCharacterPackage.h"
#import "OVOFHanConvertPackage.h"
#import "OpenVanillaConfig.h"
#import "OpenVanillaService.h"
#import "Version.h"
#import "YKSignedModuleLoadingSystem.h"

NSString *CVLoaderUpdateCannedMessagesNotification =
    @"CVLoaderUpdateCannedMessagesNotification";

static const char *kChiaKeySourceDatabaseFile = "ChiaKeySource.db";
static const char *kLegacyKeyKeySourceDatabaseFile = "KeyKeySource.db";
static const char *kDefaultPrimaryInputMethod = OVIMSMARTMANDARIN_IDENTIFIER;
static NSString *const kChiaKeySourceDatabaseArtifactKind =
    @"chiakey-source-db";
static NSString *const kLegacyKeyKeySourceDatabaseArtifactKind =
    @"keykey-source-db";

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

static void EnsureInitialPrimaryInputMethod(PVLoaderPolicy *loaderPolicy) {
  PVPropertyList loaderConfig(loaderPolicy->propertyListPathForLoader());
  PVPlistValue *dict = loaderConfig.rootDictionary();
  if (dict->valueForKey("PrimaryInputMethod")) return;

  dict->setKeyValue("PrimaryInputMethod", kDefaultPrimaryInputMethod);
  loaderConfig.write();
}

static NSDictionary *JSONDictionaryAtPath(NSString *path) {
  NSData *data = [NSData dataWithContentsOfFile:path];
  if (!data) return nil;

  id object = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
  if (![object isKindOfClass:[NSDictionary class]]) return nil;

  return object;
}

static NSDictionary *DatabaseArtifactFromManifest(NSDictionary *manifest) {
  NSArray *artifacts = [manifest objectForKey:@"artifacts"];
  if (![artifacts isKindOfClass:[NSArray class]]) return nil;

  NSArray *preferredKinds = [NSArray
      arrayWithObjects:kChiaKeySourceDatabaseArtifactKind,
                       kLegacyKeyKeySourceDatabaseArtifactKind, nil];

  for (NSString *preferredKind in preferredKinds) {
    for (id artifact in artifacts) {
      if (![artifact isKindOfClass:[NSDictionary class]]) continue;
      if ([[artifact objectForKey:@"kind"] isEqualToString:preferredKind])
        return artifact;
    }
  }

  return nil;
}

static NSString *FormattedLexiconVersionFromManifestAtPath(NSString *path) {
  NSDictionary *manifest = JSONDictionaryAtPath(path);
  NSString *version = [manifest objectForKey:@"version"];
  if (![version isKindOfClass:[NSString class]] || ![version length])
    return nil;

  NSDictionary *databaseArtifact = DatabaseArtifactFromManifest(manifest);
  NSString *sha256 = [databaseArtifact objectForKey:@"sha256"];
  if (![sha256 isKindOfClass:[NSString class]] || [sha256 length] < 8)
    return nil;

  return [NSString stringWithFormat:@"%@ (%@)", version,
                                    [sha256 substringToIndex:8]];
}

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

    _mergedCannedMessagesArray = [NSMutableArray new];

    _userCannedMessagePlist = 0;
    _userFreeCannedMessageFileTimestamp = new OVFileTimestamp;

    return self;
  }

  return self;
}
- (void)dealloc {
  [self shutDown];

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

  NSString *mainDBDisplayVersion = nil;
  if (selectedDBFile == userChiaKeySourceDBFile) {
    string manifestFile =
        OVPathHelper::PathCat(userLexiconPath, "lexicon-manifest.json");
    mainDBDisplayVersion = FormattedLexiconVersionFromManifestAtPath(
        [NSString stringWithUTF8String:manifestFile.c_str()]);
  } else if (selectedDBFile == bundledChiaKeyDBFile ||
             selectedDBFile == legacyBundledKeyKeyDBFile) {
    string manifestFile =
        OVPathHelper::PathCat(dbPath, "lexicon-manifest.json");
    mainDBDisplayVersion = FormattedLexiconVersionFromManifestAtPath(
        [NSString stringWithUTF8String:manifestFile.c_str()]);
  }
  if (![mainDBDisplayVersion length] && mainDBVersion.size()) {
    mainDBDisplayVersion =
        [NSString stringWithUTF8String:mainDBVersion.c_str()];
  }

  if (supplementDBVersion.size()) {
    NSLog(@"Registered supplement DB version '%s'",
          supplementDBVersion.c_str());

    [_databaseVersion autorelease];
    _databaseVersion =
        [[NSString alloc] initWithUTF8String:supplementDBVersion.c_str()];

    // see if main DB's version is newer!
    if (mainDBVersion.size()) {
      if (VersionNumber(mainDBVersion) >= VersionNumber(supplementDBVersion)) {
        NSLog(@"Detaching supplement DB because it's older");
        if (dbc) dbc->execute("DETACH supplement");
        [_databaseVersion autorelease];
        _databaseVersion = [mainDBDisplayVersion retain];
      }
    }
  } else if ([mainDBDisplayVersion length]) {
    NSLog(@"Registered main DB version '%@'", mainDBDisplayVersion);

    [_databaseVersion autorelease];
    _databaseVersion = [mainDBDisplayVersion retain];
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

  _loaderPolicy = new PVLoaderPolicy(cppLoadPaths);
  string loaderUserDataPath =
      OVDirectoryHelper::UserApplicationSupportDataDirectory(
          _loaderPolicy->loaderName());
  if (!OVDirectoryHelper::CheckDirectory(loaderUserDataPath)) {
    NSLog(@"Cannot create user data directory: %s",
          loaderUserDataPath.c_str());
  }

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

  EnsureInitialPrimaryInputMethod(_loaderPolicy);
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
    _loader->setPrimaryInputMethod(kDefaultPrimaryInputMethod);
    _loader->syncSandwichConfig();
  }

  // NSLog(@"unlocking");
  [[OpenVanillaLoader sharedLock] unlock];

  // NSLog(@"scheduling");
  [self performSelectorOnMainThread:@selector(_firstTimeUpdateUserData)
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
        propertyListWithData:cmData
                      options:NSPropertyListMutableContainersAndLeaves
                       format:NULL
                        error:nil];
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
