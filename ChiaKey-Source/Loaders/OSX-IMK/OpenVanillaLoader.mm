// [AUTO_HEADER]

#import "OpenVanillaLoader.h"

#include <set>
#include <sstream>

#import "BPMFUserPhraseHelper.h"
#import "CVCapsLockDelayOverride.h"
#import "ChiaKeyServiceCoordination.h"
#import "ChiaKeyUserPhraseCoordination.h"
#import "LFCrossDevelopmentTools.h"
#import "LFUtilities.h"
#import "OVAFBopomofoCorrectionPackage.h"
#import "OVAFEvalPackage.h"
#import "OVIMGenericPackage.h"
#import "OVIMMandarinPackage.h"
#import "OVOFFullWidthCharacterPackage.h"
#import "OVOFHanConvertPackage.h"
#import "OpenVanillaConfig.h"
#import "Version.h"
#import "YKSignedModuleLoadingSystem.h"

NSString *CVLoaderUpdateCannedMessagesNotification =
    @"CVLoaderUpdateCannedMessagesNotification";

static const char *kChiaKeySourceDatabaseFile = "ChiaKeySource.db";
static const char *kLegacyKeyKeySourceDatabaseFile = "KeyKeySource.db";
static const char *kDefaultPrimaryInputMethod = "SmartMandarin";
static const char *kLegacyDefaultPrimaryInputMethod =
    OVIMSMARTMANDARIN_IDENTIFIER;
static const char *kUserSelectedInputMethodConfigKey =
    "UserSelectedInputMethod";
static NSString *const kChiaKeySourceDatabaseArtifactKind =
    @"chiakey-source-db";
static NSString *const kChiaKeySourceDatabaseArtifactFilename =
    @"ChiaKeySource.db";

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
  string plistPath = loaderPolicy->propertyListPathForLoader();
  PVPropertyList loaderConfig(plistPath);
  PVPlistValue *dict = loaderConfig.rootDictionary();
  PVPlistValue *existingPrimary = dict->valueForKey("PrimaryInputMethod");
  if (existingPrimary) {
    if (existingPrimary->stringValue() == kLegacyDefaultPrimaryInputMethod) {
      dict->setKeyValue("PrimaryInputMethod", kDefaultPrimaryInputMethod);
      loaderConfig.write();
    }

    return;
  }

  dict->setKeyValue("PrimaryInputMethod", kDefaultPrimaryInputMethod);
  loaderConfig.write();
}

// Self-heal a "sticky" non-default primary input method. Earlier launches (or
// builds before the lexicon database was bundled) could persist a fallback
// input method such as Cangjie when Smart Mandarin was momentarily unavailable.
// On startup, if the user has never explicitly picked an input method and the
// intended default (Smart Mandarin) is now available, restore it as primary.
// An explicit user choice is recorded via -noteUserExplicitlySelectedInputMethod
// and always takes precedence here.
static void HealDefaultPrimaryInputMethodIfNeeded(PVLoader *loader) {
  if (!loader) return;
  if (loader->configRootDictionary()->isKeyTrue(
          kUserSelectedInputMethodConfigKey))
    return;
  if (loader->primaryInputMethod() == kDefaultPrimaryInputMethod) return;
  // Only switch once the default is actually usable (e.g. its lexicon database
  // is ready); otherwise leave the current choice untouched.
  if (loader->isFailedModule(kDefaultPrimaryInputMethod)) return;

  loader->setPrimaryInputMethod(kDefaultPrimaryInputMethod);
  loader->syncSandwichConfig();
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

  for (id artifact in artifacts) {
    if (![artifact isKindOfClass:[NSDictionary class]]) continue;
    if (![[artifact objectForKey:@"kind"]
            isEqualToString:kChiaKeySourceDatabaseArtifactKind])
      continue;
    if (![[artifact objectForKey:@"filename"]
            isEqualToString:kChiaKeySourceDatabaseArtifactFilename])
      continue;
    return artifact;
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

    _userPhraseEditingSessionActive = NO;
    _pendingUserPhraseAdditions = [NSMutableArray new];

    _userCannedMessagePlist = 0;
    _userFreeCannedMessageFileTimestamp = new OVFileTimestamp;

    return self;
  }

  return self;
}
- (void)dealloc {
  [self shutDown];

  [_mergedCannedMessagesArray release];
  [_pendingUserPhraseAdditions release];

  if (_userCannedMessagePlist) {
    delete _userCannedMessagePlist;
  }

  [super dealloc];
}
- (void)createDatabaseServices {
  _userPersistence->setDefaultDatabaseConnection(0, "");
  _activeUserLexiconLoaded = NO;
  _activeUserLexiconFailed = NO;

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
    _activeUserLexiconLoaded = YES;
    NSLog(@"Using external ChiaKey lexicon database: %s",
          selectedDBFile.c_str());
  } else if (OVPathHelper::PathExists(userChiaKeySourceDBFile)) {
    _activeUserLexiconFailed = YES;
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
  // mergeCannedMessagesData posts CVLoaderUpdateCannedMessagesNotification.
  [self mergeCannedMessagesData];
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

  [self publishServiceStatus];
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

  HealDefaultPrimaryInputMethodIfNeeded(_loader);
  bool applyCapsLockDelayOverride =
      kvm.stringValueForKey("ApplyCapsLockDelayOverride") != "false";

  // NSLog(@"unlocking");
  [[OpenVanillaLoader sharedLock] unlock];
  [CVCapsLockDelayOverride applyIfEnabled:applyCapsLockDelayOverride];

  // NSLog(@"scheduling");
  [self performSelectorOnMainThread:@selector(_firstTimeUpdateUserData)
                         withObject:nil
                      waitUntilDone:NO];

  sleep(1);

  // NSLog(@"loaded: %@", [self dynamicallyLoadedModulePackageInfo]);

  // A blacklist queued while the IME was not running is persisted here and
  // takes effect at the next reload; the interactive path (Preferences with
  // the IME running) posts an explicit reload request right after.
  [self performSelectorOnMainThread:@selector(
                                        applyPendingModuleBlacklistAndPublish)
                         withObject:nil
                      waitUntilDone:NO];

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
- (void)noteUserExplicitlySelectedInputMethod {
  if (!_loader) return;
  // Record the explicit choice in the loader config so the startup self-heal
  // (HealDefaultPrimaryInputMethodIfNeeded) never overrides it afterward.
  _loader->configRootDictionary()->setKeyValue(kUserSelectedInputMethodConfigKey,
                                               "true");
  _loader->syncLoaderConfig(true);
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

- (BOOL)activeUserLexiconLoaded {
  return _activeUserLexiconLoaded;
}

- (BOOL)activeUserLexiconFailed {
  return _activeUserLexiconFailed;
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
- (NSArray *)_readingsForPhrase:(NSString *)phrase {
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
      select->bindTextToColumn(*cpi, 1);
      while (select->step() == SQLITE_ROW) {
        string b = select->textOfColumn(0);

        if (exp.match(b)) continue;

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
// Insert one phrase. reading is a composed, comma-separated Bopomofo string;
// nil/empty derives the most probable reading. The rowid is assigned by
// SQLite -- never computed positionally. Assumes _userPhraseDB is open.
- (void)_insertUserPhrase:(NSString *)phrase reading:(NSString *)reading {
  if (![phrase length]) return;
  NSString *composed =
      [reading length]
          ? reading
          : [[self _readingsForPhrase:phrase] objectAtIndex:0];
  _userPhraseDB->execute(
      "INSERT INTO user_unigrams (qstring, current, probability, backoff) "
      "VALUES (%Q, %Q, %f, %f)",
      [self _qstringFromReading:composed].c_str(), [phrase UTF8String], -1.0,
      0.0);
}

// Each pending entry is a dictionary {@"phrase", optional @"reading"} so a
// custom reading survives the replay after an editor session ends.
- (void)_queuePendingPhrase:(NSString *)phrase reading:(NSString *)reading {
  if (![phrase length]) return;
  NSMutableDictionary *entry = [NSMutableDictionary dictionary];
  [entry setObject:phrase forKey:@"phrase"];
  if ([reading length]) [entry setObject:reading forKey:@"reading"];
  @synchronized(_pendingUserPhraseAdditions) {
    [_pendingUserPhraseAdditions addObject:entry];
  }
}

- (void)userPhraseDBAddNewRow:(NSString *)phrase {
  [self userPhraseDBAddNewRow:phrase reading:nil];
}
- (void)userPhraseDBAddNewRow:(NSString *)phrase reading:(NSString *)reading {
  if (![phrase length]) return;
  // Queue additions while the Phrase Editor owns the DB; replayed on end.
  if ([self userPhraseEditingSessionActive]) {
    [self _queuePendingPhrase:phrase reading:reading];
    return;
  }
  if (![self _userPhraseDBConnection]) {
    return;
  }

  [self _insertUserPhrase:phrase reading:reading];
  _loader->forceSyncModuleConfigForNextRound("SmartMandarin");
}
#pragma mark Phrase Editor coordination

- (NSString *)userDataDirectory {
  OVPathInfo pathInfo = _loaderPolicy->modulePackagePathInfoFromPath("");
  return [NSString stringWithUTF8String:pathInfo.writablePath.c_str()];
}

- (BOOL)userPhraseEditingSessionActive {
  // The lock file is authoritative; the flag only makes notification
  // delivery take effect without waiting for the next poll.
  return _userPhraseEditingSessionActive ||
         ChiaKeyUserPhraseEditingLockIsActive([self userDataDirectory]);
}

- (void)userPhraseEditingSessionDidBegin {
  _userPhraseEditingSessionActive = YES;

  // Release our own connection so the editor session starts from a clean
  // slate; it is lazily reopened after the session.
  if (_userPhraseDB) {
    delete _userPhraseDB;
    _userPhraseDB = 0;
  }
}

- (void)userPhraseEditingSessionDidEnd {
  _userPhraseEditingSessionActive = NO;

  NSArray *pending = nil;
  @synchronized(_pendingUserPhraseAdditions) {
    if ([_pendingUserPhraseAdditions count]) {
      pending = [[_pendingUserPhraseAdditions copy] autorelease];
      [_pendingUserPhraseAdditions removeAllObjects];
    }
  }
  // Insert directly (not via userPhraseDBAddNewRow:, which would re-queue if
  // the lock still looks active) so each entry keeps its custom reading.
  if ([pending count] && [self _userPhraseDBConnection]) {
    _userPhraseDB->execute("BEGIN");
    for (NSDictionary *entry in pending) {
      [self _insertUserPhrase:[entry objectForKey:@"phrase"]
                      reading:[entry objectForKey:@"reading"]];
    }
    _userPhraseDB->execute("COMMIT");
  }

  _loader->forceSyncModuleConfigForNextRound("SmartMandarin");
}

- (void)userPhraseDBDidChangeExternally {
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

#pragma mark Preferences app coordination

- (void)publishServiceStatus {
  if (!_loader) return;

  NSString *bundleVersion = [[NSBundle mainBundle]
      objectForInfoDictionaryKey:(NSString *)kCFBundleVersionKey];

  NSMutableDictionary *status = [NSMutableDictionary dictionary];
  [status setObject:(bundleVersion ? bundleVersion : @"")
             forKey:ChiaKeyStatusVersionKey];
  [status setObject:(_databaseVersion ? _databaseVersion : @"")
             forKey:ChiaKeyStatusDatabaseVersionKey];
  [status setObject:[NSDate date] forKey:ChiaKeyStatusUpdatedAtKey];
  [status setObject:[self identifiersAndLocalizedNamesWithPattern:@"*"]
             forKey:ChiaKeyStatusModulesKey];
  [status setObject:[self dynamicallyLoadedModulePackageInfo]
             forKey:ChiaKeyStatusPackagesKey];

  ChiaKeyEnsureUserDataDirectoryPrivate();
  if ([status writeToFile:ChiaKeyServiceStatusPath() atomically:YES]) {
    ChiaKeyPostServiceNotification(ChiaKeyServiceStatusDidUpdateNotification);
  }
}

- (void)applyPendingModuleBlacklistAndPublish {
  if (!_loader) return;

  NSString *path = ChiaKeyPendingModuleBlacklistPath();
  NSArray *identifiers = [NSArray arrayWithContentsOfFile:path];
  if (identifiers) {
    [self setBlackListOfPackageIdentifers:identifiers];
    _loader->syncLoaderConfig(true);
    [[NSFileManager defaultManager] removeItemAtPath:path error:NULL];
  }
  [self publishServiceStatus];
}

@end
