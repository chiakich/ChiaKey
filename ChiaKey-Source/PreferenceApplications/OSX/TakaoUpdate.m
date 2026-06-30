/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoUpdate.h"

#import "TakaoHelper.h"

static NSString *const ChiaKeyApplicationReleasesURL =
    @"https://api.github.com/repos/akira02/ChiaKey/releases";
static NSString *const ChiaKeyApplicationLatestReleaseURLDefaultsKey =
    @"ChiaKeyLatestApplicationReleaseURL";
static NSString *const ChiaKeyApplicationLatestPackageNameDefaultsKey =
    @"ChiaKeyLatestApplicationPackageName";
static NSString *const ChiaKeyApplicationLatestPackageSHA256DefaultsKey =
    @"ChiaKeyLatestApplicationPackageSHA256";
static NSString *const ChiaKeyApplicationLatestPackageURLDefaultsKey =
    @"ChiaKeyLatestApplicationPackageURL";
static NSString *const ChiaKeyApplicationIncludeBetaDefaultsKey =
    @"ChiaKeyApplicationIncludeBetaReleases";
static NSString *const ChiaKeyLatestApplicationDefaultsKey =
    @"ChiaKeyLatestApplicationVersion";
static NSString *const ChiaKeyLatestApplicationCheckDefaultsKey =
    @"ChiaKeyLatestApplicationCheck";

static NSString *const ChiaKeyLexiconLatestURL =
    @"https://github.com/akira02/ChiaKey-Lexicon/releases/latest";
static NSString *const ChiaKeyLatestLexiconDefaultsKey =
    @"ChiaKeyLatestLexiconVersion";
static NSString *const ChiaKeyLatestLexiconCheckDefaultsKey =
    @"ChiaKeyLatestLexiconCheck";
static NSString *const ChiaKeyLexiconAutoUpdateEnabledPreferenceKey =
    @"ShouldAutoUpdateLexicon";
static NSString *const ChiaKeySourceDatabaseArtifactKind =
    @"chiakey-source-db";
static NSString *const ChiaKeySourceDatabaseArtifactFilename =
    @"ChiaKeySource.db";

@implementation TakaoUpdate

- (void)dealloc {
  if (_task) [_task terminate];
  [_task release];
  [_availableApplicationPackageName release];
  [_availableApplicationPackageSHA256 release];
  [_availableApplicationPackageURL release];
  [_availableApplicationTag release];
  [_availableLexiconTag release];
  [super dealloc];
}

- (NSDictionary *)_jsonDictionaryFromData:(NSData *)data
                                    error:(NSError **)error {
  if (!data) return nil;

  id object = [NSJSONSerialization JSONObjectWithData:data options:0 error:error];
  if (![object isKindOfClass:[NSDictionary class]]) return nil;

  return object;
}

- (NSDictionary *)_jsonDictionaryAtPath:(NSString *)path {
  NSData *data = [NSData dataWithContentsOfFile:path];
  return [self _jsonDictionaryFromData:data error:nil];
}

- (NSDictionary *)_databaseArtifactFromManifest:(NSDictionary *)manifest {
  NSArray *artifacts = [manifest objectForKey:@"artifacts"];
  if (![artifacts isKindOfClass:[NSArray class]]) return nil;

  for (id artifact in artifacts) {
    if (![artifact isKindOfClass:[NSDictionary class]]) continue;
    if (![[artifact objectForKey:@"kind"]
            isEqualToString:ChiaKeySourceDatabaseArtifactKind])
      continue;
    if (![[artifact objectForKey:@"filename"]
            isEqualToString:ChiaKeySourceDatabaseArtifactFilename])
      continue;
    return artifact;
  }

  return nil;
}

- (NSString *)_formattedLexiconVersionFromManifest:(NSDictionary *)manifest {
  NSString *version = [manifest objectForKey:@"version"];
  if (![version isKindOfClass:[NSString class]] || ![version length])
    return nil;

  NSDictionary *databaseArtifact = [self _databaseArtifactFromManifest:manifest];
  NSString *sha256 = [databaseArtifact objectForKey:@"sha256"];
  if (![sha256 isKindOfClass:[NSString class]] || [sha256 length] < 8)
    return version;

  return [NSString stringWithFormat:@"%@ (%@)", version,
                                    [sha256 substringToIndex:8]];
}

- (NSString *)_lexiconInstallRoot {
  NSArray *paths = NSSearchPathForDirectoriesInDomains(
      NSApplicationSupportDirectory, NSUserDomainMask, YES);
  NSString *basePath =
      ([paths count] > 0) ? [paths objectAtIndex:0] : NSTemporaryDirectory();
  return [basePath stringByAppendingPathComponent:
                       @"ChiaKey/Lexicons"];
}

- (NSDictionary *)_activeLexiconManifest {
  NSString *activePath =
      [[self _lexiconInstallRoot] stringByAppendingPathComponent:@"active"];

  return [self _jsonDictionaryAtPath:[activePath stringByAppendingPathComponent:
                                                     @"lexicon-manifest.json"]];
}

- (NSString *)_runningDatabaseVersion {
  id ovService = nil;
  @try {
    ovService = [ChiaKeyServiceClient sharedClient];
    if ([ovService isAvailable]) {
      NSString *version = [ovService databaseVersion];
      if ([version length]) return version;
    }
  } @catch (NSException *e) {
    return nil;
  }

  return nil;
}

- (NSString *)_currentLexiconDisplayVersion {
  NSString *runningVersion = [self _runningDatabaseVersion];
  if ([runningVersion length]) return runningVersion;

  return [self _formattedLexiconVersionFromManifest:
                   [self _activeLexiconManifest]];
}

- (NSString *)_currentLexiconComparableVersion {
  NSDictionary *manifest = [self _activeLexiconManifest];
  NSString *version = [manifest objectForKey:@"version"];
  if ([version isKindOfClass:[NSString class]] && [version length])
    return version;

  NSString *runningVersion = [self _runningDatabaseVersion];
  if (![runningVersion length]) return nil;

  NSRange spaceRange = [runningVersion rangeOfString:@" "];
  if (spaceRange.location != NSNotFound)
    runningVersion = [runningVersion substringToIndex:spaceRange.location];
  return runningVersion;
}

- (NSString *)_currentApplicationVersion {
  NSString *version =
      [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];
  if (![version length]) {
    version = [[NSBundle mainBundle]
        objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
  }
  return version;
}

- (NSString *)_displayString:(NSString *)string fallback:(NSString *)fallback {
  return [string length] ? string : fallback;
}

- (NSString *)_formatDate:(NSDate *)date {
  NSDateFormatter *formatter = [[[NSDateFormatter alloc] init] autorelease];
  [formatter setDateFormat:@"yyyy-MM-dd HH:mm"];
  return [formatter stringFromDate:date];
}

- (NSString *)_baseVersionString:(NSString *)version {
  if (![version length]) return nil;

  NSString *trimmed = [version
      stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
  NSCharacterSet *digits = [NSCharacterSet decimalDigitCharacterSet];
  NSUInteger firstDigit = NSNotFound;
  for (NSUInteger index = 0; index < [trimmed length]; index++) {
    unichar character = [trimmed characterAtIndex:index];
    if ([digits characterIsMember:character]) {
      firstDigit = index;
      break;
    }
  }
  if (firstDigit == NSNotFound) return trimmed;

  NSString *base = [trimmed substringFromIndex:firstDigit];
  NSRange hyphenRange = [base rangeOfString:@"-"];
  if (hyphenRange.location != NSNotFound)
    base = [base substringToIndex:hyphenRange.location];

  NSRange metadataRange = [base rangeOfString:@"+"];
  if (metadataRange.location != NSNotFound)
    base = [base substringToIndex:metadataRange.location];

  return base;
}

- (NSArray *)_numericVersionParts:(NSString *)version {
  NSString *base = [self _baseVersionString:version];
  if (![base length]) return [NSArray array];

  NSArray *rawParts = [base
      componentsSeparatedByCharactersInSet:
          [[NSCharacterSet decimalDigitCharacterSet] invertedSet]];
  NSMutableArray *parts = [NSMutableArray array];
  for (NSString *part in rawParts) {
    if (![part length]) continue;
    [parts addObject:[NSNumber numberWithInteger:[part integerValue]]];
  }
  return parts;
}

- (NSComparisonResult)_compareVersion:(NSString *)lhs toVersion:(NSString *)rhs {
  NSArray *leftParts = [self _numericVersionParts:lhs];
  NSArray *rightParts = [self _numericVersionParts:rhs];
  NSUInteger count = MAX([leftParts count], [rightParts count]);

  for (NSUInteger index = 0; index < count; index++) {
    NSInteger leftValue =
        (index < [leftParts count]) ? [[leftParts objectAtIndex:index] integerValue] : 0;
    NSInteger rightValue =
        (index < [rightParts count]) ? [[rightParts objectAtIndex:index] integerValue] : 0;
    if (leftValue < rightValue) return NSOrderedAscending;
    if (leftValue > rightValue) return NSOrderedDescending;
  }

  return NSOrderedSame;
}

- (BOOL)_isDevelopmentLexiconVersion:(NSString *)version {
  if (![version length]) return NO;
  return [[version lowercaseString] rangeOfString:@"dev"].location !=
         NSNotFound;
}

- (BOOL)_isApplicationBusy {
  return ![_applicationProgressIndicator isHidden];
}

- (BOOL)_isLexiconBusy {
  return ![_lexiconProgressIndicator isHidden];
}

- (void)_refreshApplicationInstallButton {
  BOOL hasAvailablePackage = [_availableApplicationTag length] > 0 &&
                             [_availableApplicationPackageURL length] > 0;
  [_applicationInstallButton setHidden:!hasAvailablePackage];
  [_applicationInstallButton setEnabled:hasAvailablePackage &&
                                        ![self _isApplicationBusy]];
}

- (void)_setAvailableApplicationTag:(NSString *)tag
                         packageURL:(NSString *)packageURL
                        packageName:(NSString *)packageName
                      packageSHA256:(NSString *)packageSHA256 {
  if (_availableApplicationTag != tag) {
    [_availableApplicationTag release];
    _availableApplicationTag = [tag copy];
  }
  if (_availableApplicationPackageURL != packageURL) {
    [_availableApplicationPackageURL release];
    _availableApplicationPackageURL = [packageURL copy];
  }
  if (_availableApplicationPackageName != packageName) {
    [_availableApplicationPackageName release];
    _availableApplicationPackageName = [packageName copy];
  }
  if (_availableApplicationPackageSHA256 != packageSHA256) {
    [_availableApplicationPackageSHA256 release];
    _availableApplicationPackageSHA256 = [packageSHA256 copy];
  }
  [self _refreshApplicationInstallButton];
}

- (void)_refreshLexiconInstallButton {
  BOOL hasAvailableLexicon = [_availableLexiconTag length] > 0;
  [_lexiconInstallButton setHidden:!hasAvailableLexicon];
  [_lexiconInstallButton setEnabled:hasAvailableLexicon &&
                                    ![self _isLexiconBusy]];
}

- (void)_setAvailableLexiconTag:(NSString *)tag {
  if (_availableLexiconTag != tag) {
    [_availableLexiconTag release];
    _availableLexiconTag = [tag copy];
  }
  [self _refreshLexiconInstallButton];
}

- (void)_setApplicationBusy:(BOOL)busy {
  if (busy) {
    [_applicationProgressIndicator startAnimation:self];
    [_applicationProgressIndicator setHidden:NO];
  } else {
    [_applicationProgressIndicator stopAnimation:self];
    [_applicationProgressIndicator setHidden:YES];
  }

  [_applicationCheckButton setEnabled:!busy];
  [_applicationIncludeBetaCheckBox setEnabled:!busy];
  [self _refreshApplicationInstallButton];
}

- (void)_setLexiconBusy:(BOOL)busy {
  if (busy) {
    [_lexiconProgressIndicator startAnimation:self];
    [_lexiconProgressIndicator setHidden:NO];
  } else {
    [_lexiconProgressIndicator stopAnimation:self];
    [_lexiconProgressIndicator setHidden:YES];
  }

  [_lexiconCheckButton setEnabled:!busy];
  [_lexiconAutoUpdateCheckBox setEnabled:!busy];
  [self _refreshLexiconInstallButton];
}

- (BOOL)_isAutomaticLexiconUpdateEnabled {
  NSDictionary *preferences =
      [NSDictionary dictionaryWithContentsOfFile:
                        [TakaoHelper plistFilePath:PLIST_GLOBAL_FILENAME]];
  NSString *value =
      [preferences objectForKey:ChiaKeyLexiconAutoUpdateEnabledPreferenceKey];
  if (![value length]) return YES;

  return [value isEqualToString:@"true"];
}

- (void)_setAutomaticLexiconUpdateEnabled:(BOOL)enabled {
  NSString *path = [TakaoHelper plistFilePath:PLIST_GLOBAL_FILENAME];
  NSMutableDictionary *preferences =
      [NSMutableDictionary dictionaryWithContentsOfFile:path];
  if (!preferences) preferences = [NSMutableDictionary dictionary];

  [preferences setObject:(enabled ? @"true" : @"false")
                  forKey:ChiaKeyLexiconAutoUpdateEnabledPreferenceKey];
  [preferences writeToFile:path atomically:YES];
}

- (void)_getApplicationVersionInfo {
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  NSString *currentVersion = [self _currentApplicationVersion];
  NSString *latestVersion =
      [defaults stringForKey:ChiaKeyLatestApplicationDefaultsKey];
  NSString *latestCheck =
      [defaults stringForKey:ChiaKeyLatestApplicationCheckDefaultsKey];

  [_applicationCurrentVersionTextField
      setStringValue:[self _displayString:currentVersion
                                  fallback:LFLSTR(@"Unknown")]];
  [_applicationLatestVersionTextField
      setStringValue:[self _displayString:latestVersion
                                  fallback:LFLSTR(@"Not checked yet")]];
  [_applicationLatestCheckTextField
      setStringValue:[self _displayString:latestCheck
                                  fallback:LFLSTR(@"Not checked yet")]];
}

- (void)_getLexiconVersionInfo {
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  NSString *currentVersion = [self _currentLexiconDisplayVersion];
  NSString *latestVersion =
      [defaults stringForKey:ChiaKeyLatestLexiconDefaultsKey];
  NSString *latestCheck =
      [defaults stringForKey:ChiaKeyLatestLexiconCheckDefaultsKey];

  [_lexiconCurrentVersionTextField
      setStringValue:[self _displayString:currentVersion
                                  fallback:LFLSTR(@"No lexicon installed")]];
  [_lexiconLatestVersionTextField
      setStringValue:[self _displayString:latestVersion
                                  fallback:LFLSTR(@"Not checked yet")]];
  [_lexiconLatestCheckTextField
      setStringValue:[self _displayString:latestCheck
                                  fallback:LFLSTR(@"Not checked yet")]];
}

- (void)_getVersionInfo {
  [self _getApplicationVersionInfo];
  [self _getLexiconVersionInfo];
}

- (void)awakeFromNib {
  [_applicationProgressIndicator setHidden:YES];
  [_lexiconProgressIndicator setHidden:YES];
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  [_applicationIncludeBetaCheckBox
      setIntValue:[defaults boolForKey:ChiaKeyApplicationIncludeBetaDefaultsKey]];
  [_lexiconAutoUpdateCheckBox
      setIntValue:[self _isAutomaticLexiconUpdateEnabled]];
  [self _setAvailableApplicationTag:nil
                         packageURL:nil
                        packageName:nil
                      packageSHA256:nil];
  [self _setAvailableLexiconTag:nil];
  [self _getVersionInfo];
  [self _setApplicationBusy:NO];
  [self _setLexiconBusy:NO];
}

- (NSError *)_updateErrorWithDescription:(NSString *)description
                                    code:(NSInteger)code {
  NSDictionary *userInfo =
      [NSDictionary dictionaryWithObject:description
                                  forKey:NSLocalizedDescriptionKey];
  return [NSError errorWithDomain:@"ChiaKeyUpdate"
                             code:code
                         userInfo:userInfo];
}

- (NSDictionary *)_packageAssetFromRelease:(NSDictionary *)release {
  NSArray *assets = [release objectForKey:@"assets"];
  if (![assets isKindOfClass:[NSArray class]]) return nil;

  NSDictionary *fallbackAsset = nil;
  for (id item in assets) {
    if (![item isKindOfClass:[NSDictionary class]]) continue;
    NSDictionary *asset = (NSDictionary *)item;
    NSString *name = [asset objectForKey:@"name"];
    NSString *downloadURL = [asset objectForKey:@"browser_download_url"];
    if (![name isKindOfClass:[NSString class]] ||
        ![downloadURL isKindOfClass:[NSString class]] ||
        ![downloadURL length])
      continue;
    if ([[name pathExtension] caseInsensitiveCompare:@"pkg"] != NSOrderedSame)
      continue;

    BOOL isUnsigned =
        [[name lowercaseString] rangeOfString:@"unsigned"].location !=
        NSNotFound;
    if (isUnsigned) continue;

    if (!fallbackAsset) fallbackAsset = asset;
    if ([[name lowercaseString] hasPrefix:@"chiakey-"]) return asset;
  }

  return fallbackAsset;
}

- (NSString *)_sha256FromPackageAsset:(NSDictionary *)asset {
  NSString *digest = [asset objectForKey:@"digest"];
  if (![digest isKindOfClass:[NSString class]] || ![digest length])
    digest = [asset objectForKey:@"sha256"];
  if (![digest isKindOfClass:[NSString class]]) return nil;

  NSString *lowercaseDigest = [digest lowercaseString];
  if ([lowercaseDigest hasPrefix:@"sha256:"])
    lowercaseDigest = [lowercaseDigest substringFromIndex:7];

  NSCharacterSet *nonHexCharacters =
      [[NSCharacterSet characterSetWithCharactersInString:
                           @"0123456789abcdef"] invertedSet];
  if ([lowercaseDigest length] != 64 ||
      [lowercaseDigest rangeOfCharacterFromSet:nonHexCharacters].location !=
          NSNotFound)
    return nil;

  return lowercaseDigest;
}

- (void)_latestApplicationReleaseIncludingBeta:(BOOL)includeBeta
                                    completion:
                                        (void (^)(NSString *tag,
                                                  NSString *releaseURL,
                                                  NSString *packageURL,
                                                  NSString *packageName,
                                                  NSString *packageSHA256,
                                                  BOOL prerelease,
                                                  NSError *error))completion {
  NSURL *url = [NSURL URLWithString:ChiaKeyApplicationReleasesURL];
  NSMutableURLRequest *request =
      [NSMutableURLRequest requestWithURL:url
                              cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                          timeoutInterval:20.0];
  [request setValue:@"ChiaKey Preferences" forHTTPHeaderField:@"User-Agent"];
  [request setValue:@"application/vnd.github+json" forHTTPHeaderField:@"Accept"];

  NSURLSessionDataTask *task = [[NSURLSession sharedSession]
      dataTaskWithRequest:request
        completionHandler:^(NSData *data, NSURLResponse *response,
                            NSError *requestError) {
          NSError *completionError = requestError;
          NSInteger statusCode = 0;
          if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
            statusCode = [(NSHTTPURLResponse *)response statusCode];
          }
          if (!completionError && statusCode >= 400) {
            completionError =
                [self _updateErrorWithDescription:LFLSTR(@"GitHub returned an error.")
                                             code:statusCode];
          }

          NSString *tag = nil;
          NSString *releaseURL = nil;
          NSString *packageURL = nil;
          NSString *packageName = nil;
          NSString *packageSHA256 = nil;
          BOOL prerelease = NO;
          if (!completionError) {
            NSError *jsonError = nil;
            id object = [NSJSONSerialization JSONObjectWithData:data
                                                        options:0
                                                          error:&jsonError];
            if (![object isKindOfClass:[NSArray class]]) {
              completionError = jsonError ? jsonError
                                          : [self _updateErrorWithDescription:
                                                      @"GitHub returned an unexpected response."
                                                                     code:0];
            } else {
              NSString *bestTag = nil;
              NSString *bestReleaseURL = nil;
              NSString *bestPackageURL = nil;
              NSString *bestPackageName = nil;
              NSString *bestPackageSHA256 = nil;
              BOOL bestPrerelease = NO;
              for (id item in (NSArray *)object) {
                if (![item isKindOfClass:[NSDictionary class]]) continue;
                NSDictionary *release = (NSDictionary *)item;
                if ([[release objectForKey:@"draft"] boolValue]) continue;
                BOOL itemPrerelease =
                    [[release objectForKey:@"prerelease"] boolValue];
                if (itemPrerelease && !includeBeta) continue;

                NSString *itemTag = [release objectForKey:@"tag_name"];
                if (![itemTag isKindOfClass:[NSString class]] ||
                    ![itemTag length])
                  continue;

                NSString *itemURL = [release objectForKey:@"html_url"];
                if (![bestTag length] ||
                    [self _compareVersion:bestTag toVersion:itemTag] ==
                        NSOrderedAscending) {
                  NSDictionary *packageAsset =
                      [self _packageAssetFromRelease:release];
                  NSString *itemPackageURL =
                      [packageAsset objectForKey:@"browser_download_url"];
                  NSString *itemPackageName = [packageAsset objectForKey:@"name"];
                  NSString *itemPackageSHA256 =
                      [self _sha256FromPackageAsset:packageAsset];
                  bestTag = itemTag;
                  bestReleaseURL =
                      [itemURL isKindOfClass:[NSString class]] ? itemURL : nil;
                  bestPackageURL =
                      [itemPackageURL isKindOfClass:[NSString class]]
                          ? itemPackageURL
                          : nil;
                  bestPackageName =
                      [itemPackageName isKindOfClass:[NSString class]]
                          ? itemPackageName
                          : nil;
                  bestPackageSHA256 = itemPackageSHA256;
                  bestPrerelease = itemPrerelease;
                }
              }
              tag = bestTag;
              releaseURL = bestReleaseURL;
              packageURL = bestPackageURL;
              packageName = bestPackageName;
              packageSHA256 = bestPackageSHA256;
              prerelease = bestPrerelease;
            }
          }

          if (completion)
            completion(tag, releaseURL, packageURL, packageName, packageSHA256,
                       prerelease, completionError);
        }];
  [task resume];
}

- (void)_latestLexiconReleaseTagWithCompletion:
    (void (^)(NSString *tag, NSError *error))completion {
  NSURL *url = [NSURL URLWithString:ChiaKeyLexiconLatestURL];
  NSMutableURLRequest *request =
      [NSMutableURLRequest requestWithURL:url
                              cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                          timeoutInterval:20.0];
  [request setValue:@"ChiaKey Preferences"
      forHTTPHeaderField:@"User-Agent"];
  [request setHTTPMethod:@"HEAD"];

  NSURLSessionDataTask *task = [[NSURLSession sharedSession]
      dataTaskWithRequest:request
        completionHandler:^(NSData *data, NSURLResponse *response,
                            NSError *requestError) {
          NSString *tag = nil;
          NSError *completionError = requestError;
          NSInteger statusCode = 0;

          if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
            statusCode = [(NSHTTPURLResponse *)response statusCode];
          }

          if (!completionError && statusCode >= 400) {
            completionError =
                [self _updateErrorWithDescription:LFLSTR(@"GitHub returned an error.")
                                             code:statusCode];
          }

          if (!completionError && response) {
            NSArray *pathComponents = [[[response URL] path] pathComponents];
            NSUInteger tagIndex = [pathComponents indexOfObject:@"tag"];
            if (tagIndex != NSNotFound &&
                tagIndex + 1 < [pathComponents count]) {
              tag = [pathComponents objectAtIndex:tagIndex + 1];
            }
          }

          if (completion) completion(tag, completionError);
        }];
  [task resume];
}

- (NSString *)_bundledLexiconInstallerPath {
  NSString *preferencesPath = [[NSBundle mainBundle] bundlePath];
  NSString *sharedSupportPath =
      [preferencesPath stringByDeletingLastPathComponent];
  NSString *contentsPath = [sharedSupportPath stringByDeletingLastPathComponent];
  NSString *resourcesPath =
      [contentsPath stringByAppendingPathComponent:@"Resources"];

  NSString *scriptPath = [resourcesPath
      stringByAppendingPathComponent:@"Scripts/install-lexicon-release.sh"];
  if ([[NSFileManager defaultManager] fileExistsAtPath:scriptPath])
    return scriptPath;

  scriptPath =
      [resourcesPath stringByAppendingPathComponent:@"install-lexicon-release.sh"];
  if ([[NSFileManager defaultManager] fileExistsAtPath:scriptPath])
    return scriptPath;

  return nil;
}

- (BOOL)_installLexiconRelease:(NSString *)tag output:(NSString **)output {
  NSString *scriptPath = [self _bundledLexiconInstallerPath];
  if (![scriptPath length]) {
    if (output) *output = LFLSTR(@"Lexicon installer was not found.");
    return NO;
  }

  if (_task) {
    if ([_task isRunning]) return NO;
    [_task release];
    _task = nil;
  }

  NSPipe *pipe = [NSPipe pipe];
  _task = [[NSTask alloc] init];
  [_task setLaunchPath:@"/bin/bash"];
  [_task setArguments:[NSArray arrayWithObjects:scriptPath, @"--tag", tag, nil]];
  [_task setStandardOutput:pipe];
  [_task setStandardError:pipe];
  [_task launch];
  [_task waitUntilExit];

  NSData *data = [[pipe fileHandleForReading] readDataToEndOfFile];
  NSString *taskOutput =
      [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]
          autorelease];
  if (output) *output = taskOutput;

  return [_task terminationStatus] == 0;
}

- (BOOL)_reloadOpenVanillaServer {
  id ovService = nil;
  @try {
    ovService = [ChiaKeyServiceClient sharedClient];
    if ([ovService isAvailable]) {
      [ovService reloadOpenVanilla];
      return YES;
    }
  } @catch (NSException *e) {
    return NO;
  }
  return NO;
}

- (void)_showAlertWithTitle:(NSString *)title message:(NSString *)message {
  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:title ? title : @""];
  [alert setInformativeText:message ? message : @""];
  [alert addButtonWithTitle:LFLSTR(@"OK")];
  [alert beginSheetModalForWindow:_window completionHandler:nil];
}

- (void)_handleLatestApplicationTag:(NSString *)latestTag
                         releaseURL:(NSString *)releaseURL
                         packageURL:(NSString *)packageURL
                        packageName:(NSString *)packageName
                      packageSHA256:(NSString *)packageSHA256
                          prerelease:(BOOL)prerelease
                              error:(NSError *)error
                         showAlerts:(BOOL)showAlerts {
  NSString *checkTime = [self _formatDate:[NSDate date]];

  if ([latestTag length]) {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:latestTag forKey:ChiaKeyLatestApplicationDefaultsKey];
    [defaults setObject:checkTime
                 forKey:ChiaKeyLatestApplicationCheckDefaultsKey];
    if ([releaseURL length]) {
      [defaults setObject:releaseURL
                   forKey:ChiaKeyApplicationLatestReleaseURLDefaultsKey];
    }
    if ([packageURL length]) {
      [defaults setObject:packageURL
                   forKey:ChiaKeyApplicationLatestPackageURLDefaultsKey];
    } else {
      [defaults removeObjectForKey:ChiaKeyApplicationLatestPackageURLDefaultsKey];
    }
    if ([packageName length]) {
      [defaults setObject:packageName
                   forKey:ChiaKeyApplicationLatestPackageNameDefaultsKey];
    } else {
      [defaults removeObjectForKey:ChiaKeyApplicationLatestPackageNameDefaultsKey];
    }
    if ([packageSHA256 length]) {
      [defaults setObject:packageSHA256
                   forKey:ChiaKeyApplicationLatestPackageSHA256DefaultsKey];
    } else {
      [defaults
          removeObjectForKey:ChiaKeyApplicationLatestPackageSHA256DefaultsKey];
    }
    [defaults synchronize];
  }

  [self _setApplicationBusy:NO];

  if (![latestTag length]) {
    [self _setAvailableApplicationTag:nil
                           packageURL:nil
                          packageName:nil
                        packageSHA256:nil];
    if (showAlerts) {
      [self _showAlertWithTitle:LFLSTR(@"Unable to check for update via the Internet.")
                        message:LFLSTR(@"Please check your Internet connection and try again.")];
    }
    [self _getVersionInfo];
    return;
  }

  NSString *currentVersion = [self _currentApplicationVersion];
  if ([currentVersion length] &&
      [self _compareVersion:currentVersion toVersion:latestTag] !=
          NSOrderedAscending) {
    [self _setAvailableApplicationTag:nil
                           packageURL:nil
                          packageName:nil
                        packageSHA256:nil];
    if (showAlerts) {
      [self _showAlertWithTitle:LFLSTR(@"You are now using the newest version.")
                        message:LFLSTR(@"You need not to update your software")];
    }
    [self _getVersionInfo];
    return;
  }

  [self _setAvailableApplicationTag:latestTag
                         packageURL:packageURL
                        packageName:packageName
                      packageSHA256:packageSHA256];
  [_applicationLatestVersionTextField setStringValue:latestTag];
  [_applicationLatestCheckTextField setStringValue:checkTime];

  if (!showAlerts) return;

  NSString *message = [NSString
      stringWithFormat:LFLSTR(@"Latest input method version: %@"), latestTag];
  if (prerelease) {
    message = [message stringByAppendingFormat:@"\n%@",
                                           LFLSTR(@"This is a beta release.")];
  }
  if (![packageURL length]) {
    message = [message stringByAppendingFormat:@"\n%@",
                                           LFLSTR(@"Input method package was not found in the release.")];
  }
  if ([releaseURL length]) {
    message = [message stringByAppendingFormat:@"\n%@", releaseURL];
  }

  [self _showAlertWithTitle:LFLSTR(@"A newer version is available.")
                    message:message];
  [self _getVersionInfo];
}

- (void)_handleLatestLexiconTag:(NSString *)latestTag
                           error:(NSError *)error
                      showAlerts:(BOOL)showAlerts {
  NSString *checkTime = [self _formatDate:[NSDate date]];

  if ([latestTag length]) {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:latestTag forKey:ChiaKeyLatestLexiconDefaultsKey];
    [defaults setObject:checkTime forKey:ChiaKeyLatestLexiconCheckDefaultsKey];
    [defaults synchronize];
  }

  [self _setLexiconBusy:NO];

  if (![latestTag length]) {
    [self _setAvailableLexiconTag:nil];
    if (showAlerts) {
      [self _showAlertWithTitle:LFLSTR(@"Unable to check for update via the Internet.")
                        message:LFLSTR(@"Please check your Internet connection and try again.")];
    }
    [self _getVersionInfo];
    return;
  }

  NSString *currentTag = [self _currentLexiconComparableVersion];
  if ([self _isDevelopmentLexiconVersion:currentTag] ||
      ([currentTag length] &&
      [self _compareVersion:currentTag toVersion:latestTag] !=
          NSOrderedAscending)) {
    [self _setAvailableLexiconTag:nil];
    if (showAlerts) {
      [self _showAlertWithTitle:LFLSTR(@"You are now using the newest version.")
                        message:LFLSTR(@"You need not to update your lexicon")];
    }
    [self _getVersionInfo];
    return;
  }

  [self _setAvailableLexiconTag:latestTag];
  [_lexiconLatestVersionTextField setStringValue:latestTag];
  [_lexiconLatestCheckTextField setStringValue:checkTime];
  if (!showAlerts) return;

  [self _showAlertWithTitle:LFLSTR(@"A newer lexicon is available.")
                    message:[NSString
                                stringWithFormat:
                                    LFLSTR(@"Latest lexicon version: %@"),
                                    latestTag]];
}

- (NSString *)_safePackageFileName:(NSString *)packageName
                        fallbackTag:(NSString *)tag {
  NSString *fileName = [packageName length]
                           ? packageName
                           : [NSString stringWithFormat:@"ChiaKey-%@.pkg",
                                                        [self _displayString:tag
                                                                    fallback:@"Update"]];
  NSCharacterSet *unsafeCharacters =
      [NSCharacterSet characterSetWithCharactersInString:@"/:"];
  fileName = [[fileName componentsSeparatedByCharactersInSet:unsafeCharacters]
      componentsJoinedByString:@"-"];
  if ([[fileName pathExtension] caseInsensitiveCompare:@"pkg"] !=
      NSOrderedSame)
    fileName = [fileName stringByAppendingPathExtension:@"pkg"];
  return fileName;
}

- (BOOL)_runValidationTool:(NSString *)launchPath
                 arguments:(NSArray *)arguments
                    output:(NSString **)output {
  NSTask *task = [[[NSTask alloc] init] autorelease];
  NSPipe *pipe = [NSPipe pipe];
  [task setLaunchPath:launchPath];
  [task setArguments:arguments];
  [task setStandardOutput:pipe];
  [task setStandardError:pipe];

  @try {
    [task launch];
    [task waitUntilExit];
  } @catch (NSException *exception) {
    if (output) *output = [exception description];
    return NO;
  }

  NSData *data = [[pipe fileHandleForReading] readDataToEndOfFile];
  NSString *toolOutput =
      [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]
          autorelease];
  if (output) *output = toolOutput ? toolOutput : @"";

  return [task terminationStatus] == 0;
}

- (BOOL)_verifySHA256OfFileAtPath:(NSString *)path
                          expected:(NSString *)expectedSHA256
                             error:(NSError **)error {
  if (![expectedSHA256 length]) return YES;

  NSString *shaOutput = nil;
  BOOL hasSHAOutput =
      [self _runValidationTool:@"/usr/bin/shasum"
                     arguments:[NSArray arrayWithObjects:@"-a",
                                                          @"256",
                                                          path,
                                                          nil]
                        output:&shaOutput];
  NSArray *components =
      [shaOutput componentsSeparatedByCharactersInSet:
                     [NSCharacterSet whitespaceAndNewlineCharacterSet]];
  NSString *actualSHA256 = [components count] ? [components objectAtIndex:0] : nil;
  if (!hasSHAOutput ||
      [actualSHA256 caseInsensitiveCompare:expectedSHA256] != NSOrderedSame) {
    if (error) {
      *error = [self _updateErrorWithDescription:
                         LFLSTR(@"Downloaded package SHA-256 did not match "
                                @"the release metadata.")
                                             code:0];
    }
    return NO;
  }

  return YES;
}

- (BOOL)_validateApplicationPackageAtPath:(NSString *)path
                                    error:(NSError **)error {
  NSString *signatureOutput = nil;
  BOOL hasDeveloperIDSignature =
      [self _runValidationTool:@"/usr/sbin/pkgutil"
                     arguments:[NSArray arrayWithObjects:@"--check-signature",
                                                          path, nil]
                        output:&signatureOutput] &&
      [signatureOutput rangeOfString:@"Developer ID Installer"
                              options:NSCaseInsensitiveSearch].location !=
          NSNotFound;
  if (!hasDeveloperIDSignature) {
    if (error) {
      *error = [self _updateErrorWithDescription:
                         LFLSTR(@"Downloaded package is not signed with a "
                                @"Developer ID Installer certificate.")
                                             code:0];
    }
    return NO;
  }

  NSString *gatekeeperOutput = nil;
  BOOL acceptedByGatekeeper =
      [self _runValidationTool:@"/usr/sbin/spctl"
                     arguments:[NSArray arrayWithObjects:@"--assess",
                                                          @"--type",
                                                          @"install",
                                                          @"--verbose=4",
                                                          path,
                                                          nil]
                        output:&gatekeeperOutput] &&
      [gatekeeperOutput rangeOfString:@"Notarized Developer ID"
                              options:NSCaseInsensitiveSearch].location !=
          NSNotFound;
  if (!acceptedByGatekeeper) {
    if (error) {
      *error = [self _updateErrorWithDescription:
                         LFLSTR(@"Downloaded package was rejected by Gatekeeper "
                                @"or is not notarized.")
                                             code:0];
    }
    return NO;
  }

  return YES;
}

- (void)_downloadApplicationPackageFromURL:(NSString *)packageURL
                               packageName:(NSString *)packageName
                            expectedSHA256:(NSString *)expectedSHA256
                                completion:
                                    (void (^)(NSString *path,
                                              NSError *error))completion {
  NSURL *url = [NSURL URLWithString:packageURL];
  if (!url) {
    if (completion) {
      completion(nil,
                 [self _updateErrorWithDescription:LFLSTR(@"Invalid package URL.")
                                              code:0]);
    }
    return;
  }

  NSString *directory =
      [NSTemporaryDirectory() stringByAppendingPathComponent:@"ChiaKeyUpdates"];
  NSString *fileName =
      [self _safePackageFileName:packageName fallbackTag:_availableApplicationTag];
  NSString *destinationPath = [directory stringByAppendingPathComponent:fileName];
  NSURL *destinationURL = [NSURL fileURLWithPath:destinationPath];

  NSURLSessionDownloadTask *task = [[NSURLSession sharedSession]
      downloadTaskWithURL:url
        completionHandler:^(NSURL *location, NSURLResponse *response,
                            NSError *requestError) {
          NSError *completionError = requestError;
          NSInteger statusCode = 0;
          if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
            statusCode = [(NSHTTPURLResponse *)response statusCode];
          }
          if (!completionError && statusCode >= 400) {
            completionError =
                [self _updateErrorWithDescription:LFLSTR(@"GitHub returned an error.")
                                             code:statusCode];
          }

          if (!completionError && !location) {
            completionError =
                [self _updateErrorWithDescription:LFLSTR(@"Downloaded package was not found.")
                                             code:0];
          }

          if (!completionError) {
            NSFileManager *fileManager = [NSFileManager defaultManager];
            [fileManager createDirectoryAtPath:directory
                   withIntermediateDirectories:YES
                                    attributes:nil
                                         error:&completionError];
            if (!completionError) {
              [fileManager removeItemAtURL:destinationURL error:nil];
              [fileManager moveItemAtURL:location
                                   toURL:destinationURL
                                   error:&completionError];
            }
            if (!completionError) {
              [self _verifySHA256OfFileAtPath:destinationPath
                                      expected:expectedSHA256
                                         error:&completionError];
            }
            if (!completionError) {
              [self _validateApplicationPackageAtPath:destinationPath
                                                error:&completionError];
            }
            if (completionError) {
              [fileManager removeItemAtURL:destinationURL error:nil];
            }
          }

          if (completion)
            completion(completionError ? nil : destinationPath,
                       completionError);
        }];
  [task resume];
}

- (void)_checkApplicationUpdateShowingAlerts:(BOOL)showAlerts {
  if ([self _isApplicationBusy]) return;

  [self _setAvailableApplicationTag:nil
                         packageURL:nil
                        packageName:nil
                      packageSHA256:nil];
  [self _setApplicationBusy:YES];
  BOOL includeBeta = [_applicationIncludeBetaCheckBox intValue] == 1;

  [self _latestApplicationReleaseIncludingBeta:includeBeta
                                    completion:^(NSString *latestTag,
                                                 NSString *releaseURL,
                                                 NSString *packageURL,
                                                 NSString *packageName,
                                                 NSString *packageSHA256,
                                                 BOOL prerelease,
                                                 NSError *error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self _handleLatestApplicationTag:latestTag
                             releaseURL:releaseURL
                             packageURL:packageURL
                            packageName:packageName
                          packageSHA256:packageSHA256
                              prerelease:prerelease
                                  error:error
                             showAlerts:showAlerts];
    });
  }];
}

- (void)_checkLexiconUpdateShowingAlerts:(BOOL)showAlerts {
  if ([self _isLexiconBusy]) return;

  [self _setAvailableLexiconTag:nil];
  [self _setLexiconBusy:YES];

  [self _latestLexiconReleaseTagWithCompletion:^(NSString *latestTag,
                                                 NSError *error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self _handleLatestLexiconTag:latestTag
                               error:error
                          showAlerts:showAlerts];
    });
  }];
}

- (void)updatePaneDidBecomeActive {
  if (_didAutoCheckOnShow) return;
  _didAutoCheckOnShow = YES;
  [self _checkApplicationUpdateShowingAlerts:NO];
  [self _checkLexiconUpdateShowingAlerts:NO];
}

#pragma mark Interface Builder actions

- (IBAction)checkApplicationUpdateNow:(id)sender {
  [self _checkApplicationUpdateShowingAlerts:YES];
}

- (IBAction)checkLexiconUpdateNow:(id)sender {
  [self _checkLexiconUpdateShowingAlerts:YES];
}

- (IBAction)installApplicationUpdate:(id)sender {
  if ([self _isApplicationBusy]) return;

  NSString *packageURL = [[_availableApplicationPackageURL copy] autorelease];
  NSString *packageName = [[_availableApplicationPackageName copy] autorelease];
  NSString *packageSHA256 =
      [[_availableApplicationPackageSHA256 copy] autorelease];
  if (![packageURL length]) {
    [self _showAlertWithTitle:LFLSTR(@"No input method update selected")
                      message:LFLSTR(@"Please check for input method updates first.")];
    return;
  }

  [self _setApplicationBusy:YES];
  [self _downloadApplicationPackageFromURL:packageURL
                               packageName:packageName
                            expectedSHA256:packageSHA256
                                completion:^(NSString *path, NSError *error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self _setApplicationBusy:NO];

      if (error || ![path length]) {
        [self _showAlertWithTitle:LFLSTR(@"Unable to download input method update.")
                          message:[self _displayString:[error localizedDescription]
                                              fallback:LFLSTR(@"Please check your Internet connection and try again.")]];
        return;
      }

      BOOL opened =
          [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:path]];
      if (!opened) {
        [self _showAlertWithTitle:LFLSTR(@"Unable to open installer.")
                          message:path];
      }
    });
  }];
}

- (IBAction)installLexiconUpdate:(id)sender {
  if ([self _isLexiconBusy]) return;

  NSString *tag = [[_availableLexiconTag copy] autorelease];
  if (![tag length]) {
    [self _showAlertWithTitle:LFLSTR(@"No lexicon update selected")
                      message:LFLSTR(@"Please check for lexicon updates first.")];
    return;
  }

  [self _setLexiconBusy:YES];

  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    NSString *output = nil;
    BOOL installed = [self _installLexiconRelease:tag output:&output];
    NSString *installOutput = [output copy];

    dispatch_async(dispatch_get_main_queue(), ^{
      [self _setLexiconBusy:NO];

      if (installed) {
        [self _setAvailableLexiconTag:nil];
        BOOL reloaded = [self _reloadOpenVanillaServer];
        NSString *message = nil;
        if (reloaded) {
          message = [NSString
              stringWithFormat:
                  LFLSTR(@"Installed lexicon %@. ChiaKey has reloaded it."),
                  tag];
        } else {
          message = [NSString
              stringWithFormat:
                  LFLSTR(@"Installed lexicon %@. Switch away from and back to ChiaKey to reload it."),
                  tag];
        }
        [self _showAlertWithTitle:LFLSTR(@"Lexicon updated")
                          message:message];
      } else {
        [self _showAlertWithTitle:LFLSTR(@"Lexicon update failed")
                          message:[self _displayString:installOutput
                                              fallback:LFLSTR(@"Unknown errors happened, please try again.")]];
      }

      [self _getVersionInfo];
      [installOutput release];
    });
  });
}

- (IBAction)toggleIncludeBetaReleases:(id)sender {
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:([_applicationIncludeBetaCheckBox intValue] == 1)
             forKey:ChiaKeyApplicationIncludeBetaDefaultsKey];
  [defaults synchronize];
  [self _setAvailableApplicationTag:nil
                         packageURL:nil
                        packageName:nil
                      packageSHA256:nil];
  _didAutoCheckOnShow = NO;
}

- (IBAction)toggleAutomaticLexiconUpdates:(id)sender {
  [self _setAutomaticLexiconUpdateEnabled:
            ([_lexiconAutoUpdateCheckBox intValue] == 1)];
}

@end
