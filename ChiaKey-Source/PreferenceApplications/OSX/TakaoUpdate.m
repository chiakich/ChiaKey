/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoUpdate.h"

#include <unistd.h>

#import "ChiaKeyUpdateService.h"
#import "TakaoHelper.h"

static NSString *const ChiaKeyApplicationLatestReleaseURLDefaultsKey =
    @"ChiaKeyLatestApplicationReleaseURL";
static NSString *const ChiaKeyApplicationLatestPackageNameDefaultsKey =
    @"ChiaKeyLatestApplicationPackageName";
static NSString *const ChiaKeyApplicationLatestPackageSHA256DefaultsKey =
    @"ChiaKeyLatestApplicationPackageSHA256";
static NSString *const ChiaKeyApplicationLatestPackageURLDefaultsKey =
    @"ChiaKeyLatestApplicationPackageURL";
static NSString *const ChiaKeyLatestApplicationDefaultsKey =
    @"ChiaKeyLatestApplicationVersion";
static NSString *const ChiaKeyLatestApplicationCheckDefaultsKey =
    @"ChiaKeyLatestApplicationCheck";

static NSString *const ChiaKeyLexiconLatestURL =
    @"https://github.com/chiakich/ChiaKey-Lexicon/releases/latest";
static NSString *const ChiaKeyLexiconManifestR2URL =
    @"https://cdn.chiaki.ch/chiakey/lexicon/lexicon-manifest.json";
static NSString *const ChiaKeyLatestLexiconDefaultsKey =
    @"ChiaKeyLatestLexiconVersion";
static NSString *const ChiaKeyLatestLexiconCheckDefaultsKey =
    @"ChiaKeyLatestLexiconCheck";
static NSString *const ChiaKeyLexiconAutoUpdateEnabledPreferenceKey =
    @"ShouldAutoUpdateLexicon";
static NSString *const ChiaKeyApplicationAutoUpdateEnabledPreferenceKey =
    @"ShouldAutoUpdateApplication";
static NSString *const ChiaKeySourceDatabaseArtifactKind =
    @"chiakey-source-db";
static NSString *const ChiaKeySourceDatabaseArtifactFilename =
    @"ChiaKeySource.db";

@implementation TakaoUpdate

- (void)dealloc {
  if (_task) [_task terminate];
  [_task release];
  [_availableApplicationRelease release];
  [_availableLexiconTag release];
  [_updateService release];
  [super dealloc];
}

- (ChiaKeyUpdateService *)_updateService {
  if (!_updateService) _updateService = [[ChiaKeyUpdateService alloc] init];
  return _updateService;
}

- (ChiaKeyUpdateRelease *)_availableRelease {
  return (ChiaKeyUpdateRelease *)_availableApplicationRelease;
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
  // The status file may be stale when the IME is not running; only trust it
  // as the "running" version while the process exists.
  if (!ChiaKeyIMEIsRunning()) return nil;

  NSString *version = [ChiaKeyReadServiceStatus()
      objectForKey:ChiaKeyStatusDatabaseVersionKey];
  return [version length] ? version : nil;
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

- (NSComparisonResult)_compareVersion:(NSString *)lhs toVersion:(NSString *)rhs {
  return [ChiaKeyUpdateService compareVersion:lhs toVersion:rhs];
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
  ChiaKeyUpdateRelease *release = [self _availableRelease];
  BOOL hasAvailablePackage =
      [[release tag] length] > 0 && [[release packageURL] length] > 0;
  [_applicationInstallButton setHidden:!hasAvailablePackage];
  [_applicationInstallButton setEnabled:hasAvailablePackage &&
                                        ![self _isApplicationBusy]];
}

- (void)_setAvailableRelease:(ChiaKeyUpdateRelease *)release {
  if (_availableApplicationRelease != release) {
    [_availableApplicationRelease release];
    _availableApplicationRelease = [release retain];
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
  [_applicationAutoUpdateCheckBox setEnabled:!busy];
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

// These live in the shared ChiaKey plist rather than NSUserDefaults because
// the IME reads them from its own process to decide whether to check at all.
- (BOOL)_isGlobalPreferenceEnabled:(NSString *)key {
  NSDictionary *preferences =
      [NSDictionary dictionaryWithContentsOfFile:
                        [TakaoHelper plistFilePath:PLIST_GLOBAL_FILENAME]];
  NSString *value = [preferences objectForKey:key];
  if (![value length]) return YES;

  return [value isEqualToString:@"true"];
}

- (void)_setGlobalPreference:(NSString *)key enabled:(BOOL)enabled {
  NSString *path = [TakaoHelper plistFilePath:PLIST_GLOBAL_FILENAME];
  NSMutableDictionary *preferences =
      [NSMutableDictionary dictionaryWithContentsOfFile:path];
  if (!preferences) preferences = [NSMutableDictionary dictionary];

  [preferences setObject:(enabled ? @"true" : @"false") forKey:key];
  [preferences writeToFile:path atomically:YES];
}

- (BOOL)_isAutomaticLexiconUpdateEnabled {
  return [self
      _isGlobalPreferenceEnabled:ChiaKeyLexiconAutoUpdateEnabledPreferenceKey];
}

- (void)_setAutomaticLexiconUpdateEnabled:(BOOL)enabled {
  [self _setGlobalPreference:ChiaKeyLexiconAutoUpdateEnabledPreferenceKey
                     enabled:enabled];
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
  [_applicationIncludeBetaCheckBox setIntValue:[self _includeBetaReleases]];
  [_applicationAutoUpdateCheckBox
      setIntValue:[self _isGlobalPreferenceEnabled:
                            ChiaKeyApplicationAutoUpdateEnabledPreferenceKey]];
  [_lexiconAutoUpdateCheckBox
      setIntValue:[self _isAutomaticLexiconUpdateEnabled]];
  [self _setAvailableRelease:nil];
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

// R2 mirror first: the GitHub API's unauthenticated 60/hour limit is per IP,
// shared by everyone behind one NAT. GitHub stays as the fallback so a CDN
// outage or regional block cannot strand anyone on stale lexicon info.
- (void)_latestLexiconReleaseTagWithCompletion:
    (void (^)(NSString *tag, NSError *error))completion {
  [self _latestLexiconTagFromR2ManifestWithCompletion:^(NSString *tag,
                                                        NSError *error) {
    if ([tag length]) {
      if (completion) completion(tag, nil);
      return;
    }
    [self _latestLexiconTagFromGitHubWithCompletion:completion];
  }];
}

- (void)_latestLexiconTagFromR2ManifestWithCompletion:
    (void (^)(NSString *tag, NSError *error))completion {
  NSURL *url = [NSURL URLWithString:ChiaKeyLexiconManifestR2URL];
  NSMutableURLRequest *request =
      [NSMutableURLRequest requestWithURL:url
                              cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                          timeoutInterval:15.0];
  [request setValue:@"ChiaKey Preferences" forHTTPHeaderField:@"User-Agent"];

  NSURLSessionDataTask *task = [[NSURLSession sharedSession]
      dataTaskWithRequest:request
        completionHandler:^(NSData *data, NSURLResponse *response,
                            NSError *requestError) {
          NSInteger statusCode = 0;
          if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
            statusCode = [(NSHTTPURLResponse *)response statusCode];
          }

          NSString *tag = nil;
          if (!requestError && statusCode < 400 && [data length]) {
            id manifest = [NSJSONSerialization JSONObjectWithData:data
                                                            options:0
                                                              error:NULL];
            if ([manifest isKindOfClass:[NSDictionary class]]) {
              id version = [(NSDictionary *)manifest objectForKey:@"version"];
              if ([version isKindOfClass:[NSString class]]) tag = version;
            }
          }

          if (completion) completion(tag, requestError);
        }];
  [task resume];
}

- (void)_latestLexiconTagFromGitHubWithCompletion:
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

- (NSString *)_bundledScriptNamed:(NSString *)name {
  NSString *preferencesPath = [[NSBundle mainBundle] bundlePath];
  NSString *sharedSupportPath =
      [preferencesPath stringByDeletingLastPathComponent];
  NSString *contentsPath = [sharedSupportPath stringByDeletingLastPathComponent];
  NSString *resourcesPath =
      [contentsPath stringByAppendingPathComponent:@"Resources"];

  NSString *scriptPath = [[resourcesPath
      stringByAppendingPathComponent:@"Scripts"]
      stringByAppendingPathComponent:name];
  if ([[NSFileManager defaultManager] fileExistsAtPath:scriptPath])
    return scriptPath;

  scriptPath = [resourcesPath stringByAppendingPathComponent:name];
  if ([[NSFileManager defaultManager] fileExistsAtPath:scriptPath])
    return scriptPath;

  return nil;
}

- (NSString *)_bundledLexiconInstallerPath {
  return [self _bundledScriptNamed:@"install-lexicon-release.sh"];
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
  if (!ChiaKeyIMEIsRunning()) return NO;
  ChiaKeyPostServiceNotification(ChiaKeyReloadRequestedNotification);
  return YES;
}

- (void)_showAlertWithTitle:(NSString *)title message:(NSString *)message {
  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:title ? title : @""];
  [alert setInformativeText:message ? message : @""];
  [alert addButtonWithTitle:LFLSTR(@"OK")];
  [alert beginSheetModalForWindow:_window completionHandler:nil];
}

- (void)_storeOrClear:(NSString *)value forKey:(NSString *)key {
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  if ([value length]) {
    [defaults setObject:value forKey:key];
  } else {
    [defaults removeObjectForKey:key];
  }
}

- (void)_handleLatestRelease:(ChiaKeyUpdateRelease *)release
                       error:(NSError *)error
                  showAlerts:(BOOL)showAlerts {
  NSString *checkTime = [self _formatDate:[NSDate date]];
  NSString *latestTag = [release tag];

  if ([latestTag length]) {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:latestTag forKey:ChiaKeyLatestApplicationDefaultsKey];
    [defaults setObject:checkTime
                 forKey:ChiaKeyLatestApplicationCheckDefaultsKey];
    if ([[release releaseURL] length]) {
      [defaults setObject:[release releaseURL]
                   forKey:ChiaKeyApplicationLatestReleaseURLDefaultsKey];
    }
    [self _storeOrClear:[release packageURL]
                 forKey:ChiaKeyApplicationLatestPackageURLDefaultsKey];
    [self _storeOrClear:[release packageName]
                 forKey:ChiaKeyApplicationLatestPackageNameDefaultsKey];
    [self _storeOrClear:[release packageSHA256]
                 forKey:ChiaKeyApplicationLatestPackageSHA256DefaultsKey];
    [defaults synchronize];
  }

  [self _setApplicationBusy:NO];

  if (![latestTag length]) {
    [self _setAvailableRelease:nil];
    if (showAlerts) {
      [self _showAlertWithTitle:LFLSTR(@"Unable to check for update via the Internet.")
                        message:[self _displayString:[error localizedDescription]
                                            fallback:LFLSTR(@"Please check your Internet connection and try again.")]];
    }
    [self _getVersionInfo];
    return;
  }

  NSString *currentVersion = [self _currentApplicationVersion];
  if ([currentVersion length] &&
      [self _compareVersion:currentVersion toVersion:latestTag] !=
          NSOrderedAscending) {
    [self _setAvailableRelease:nil];
    if (showAlerts) {
      [self _showAlertWithTitle:LFLSTR(@"You are now using the newest version.")
                        message:LFLSTR(@"You need not to update your software")];
    }
    [self _getVersionInfo];
    return;
  }

  [self _setAvailableRelease:release];
  [_applicationLatestVersionTextField setStringValue:latestTag];
  [_applicationLatestCheckTextField setStringValue:checkTime];

  if (!showAlerts) return;

  NSString *message = [NSString
      stringWithFormat:LFLSTR(@"Latest input method version: %@"), latestTag];
  if ([release prerelease]) {
    message = [message stringByAppendingFormat:@"\n%@",
                                           LFLSTR(@"This is a beta release.")];
  }
  if (![[release packageURL] length]) {
    message = [message stringByAppendingFormat:@"\n%@",
                                           LFLSTR(@"Input method package was not found in the release.")];
  }
  if ([[release releaseURL] length]) {
    message = [message stringByAppendingFormat:@"\n%@", [release releaseURL]];
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

- (void)_checkApplicationUpdateShowingAlerts:(BOOL)showAlerts {
  if ([self _isApplicationBusy]) return;

  [self _setAvailableRelease:nil];
  [self _setApplicationBusy:YES];
  BOOL includeBeta = [_applicationIncludeBetaCheckBox intValue] == 1;

  [[self _updateService]
      fetchLatestReleaseIncludingBeta:includeBeta
                           completion:^(ChiaKeyUpdateRelease *release,
                                        NSError *error) {
                             dispatch_async(dispatch_get_main_queue(), ^{
                               [self _handleLatestRelease:release
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

- (void)_launchUninstallerPurging:(BOOL)purge {
  NSString *scriptPath = [self _bundledScriptNamed:@"uninstall.sh"];
  if (![scriptPath length]) {
    [self _showAlertWithTitle:LFLSTR(@"Unable to uninstall ChiaKey.")
                      message:LFLSTR(@"The uninstaller script was not found.")];
    return;
  }

  // The script deletes the bundle it ships in, so run a copy from a
  // temporary location and let it wait for this app to exit.
  NSString *tempPath = [NSTemporaryDirectory()
      stringByAppendingPathComponent:
          [NSString stringWithFormat:@"ChiaKey-uninstall-%d.sh",
                                     (int)getpid()]];
  NSFileManager *fileManager = [NSFileManager defaultManager];
  [fileManager removeItemAtPath:tempPath error:NULL];
  if (![fileManager copyItemAtPath:scriptPath toPath:tempPath error:NULL]) {
    [self _showAlertWithTitle:LFLSTR(@"Unable to uninstall ChiaKey.")
                      message:LFLSTR(@"The uninstaller script was not found.")];
    return;
  }

  NSMutableArray *arguments = [NSMutableArray
      arrayWithObjects:tempPath, @"--wait-pid",
                       [NSString stringWithFormat:@"%d", (int)getpid()],
                       @"--show-completion-alert", nil];
  if (purge) [arguments addObject:@"--purge"];

  NSTask *task = [[[NSTask alloc] init] autorelease];
  [task setLaunchPath:@"/bin/bash"];
  [task setArguments:arguments];
  @try {
    [task launch];
  } @catch (NSException *exception) {
    [self _showAlertWithTitle:LFLSTR(@"Unable to uninstall ChiaKey.")
                      message:[exception reason]];
    return;
  }

  [NSApp terminate:self];
}

- (IBAction)uninstallChiaKey:(id)sender {
  NSButton *purgeCheckbox = [[[NSButton alloc]
      initWithFrame:NSMakeRect(0, 0, 300, 18)] autorelease];
  [purgeCheckbox setButtonType:NSButtonTypeSwitch];
  [purgeCheckbox setTitle:LFLSTR(@"Also delete user phrases and settings")];
  [purgeCheckbox sizeToFit];

  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setAlertStyle:NSAlertStyleWarning];
  [alert setMessageText:LFLSTR(@"Uninstall ChiaKey?")];
  [alert setInformativeText:
             LFLSTR(@"ChiaKey will be removed from Input Sources and deleted. "
                    @"This window will close. Log out and back in to finish "
                    @"the removal.")];
  [alert setAccessoryView:purgeCheckbox];
  [alert addButtonWithTitle:LFLSTR(@"Uninstall")];
  [alert addButtonWithTitle:LFLSTR(@"Cancel")];

  [alert beginSheetModalForWindow:_window
                completionHandler:^(NSModalResponse response) {
                  if (response != NSAlertFirstButtonReturn) return;
                  BOOL purge =
                      [purgeCheckbox state] == NSControlStateValueOn;
                  [self _launchUninstallerPurging:purge];
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

  ChiaKeyUpdateRelease *release =
      [[[self _availableRelease] retain] autorelease];
  if (![[release packageURL] length]) {
    [self _showAlertWithTitle:LFLSTR(@"No input method update selected")
                      message:LFLSTR(@"Please check for input method updates first.")];
    return;
  }

  [self _setApplicationBusy:YES];
  [[self _updateService] downloadPackageForRelease:release
      progress:nil
      completion:^(NSString *path, NSError *error) {
        if (error || ![path length]) {
          [self _setApplicationBusy:NO];
          [self _showAlertWithTitle:LFLSTR(@"Unable to download input method update.")
                            message:[self _displayString:[error localizedDescription]
                                                fallback:LFLSTR(@"Please check your Internet connection and try again.")]];
          return;
        }

        // Install to the domain the current install lives in — same target
        // logic as the background updater — rather than handing the package to
        // Installer.app, whose per-user-only GUI would leave a legacy
        // system-domain install behind as a duplicate.
        dispatch_async(
            dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
              NSError *installError = nil;
              BOOL installed =
                  [[self _updateService] installDownloadedPackageAtPath:path
                                                                  error:&installError];
              NSInteger code = [installError code];
              dispatch_async(dispatch_get_main_queue(), ^{
                [self _setApplicationBusy:NO];
                if (installed) {
                  [self _setAvailableRelease:nil];
                  [self _showAlertWithTitle:LFLSTR(@"Input method updated")
                                    message:LFLSTR(@"ChiaKey has been updated.")];
                  [self _getVersionInfo];
                  return;
                }
                // -128 is a user-cancelled admin prompt: no error to report.
                if (code == -128) return;
                [self _showAlertWithTitle:LFLSTR(@"Input method update failed")
                                  message:LFLSTR(@"Unknown errors happened, please try again.")];
              });
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

- (IBAction)toggleAutomaticApplicationUpdates:(id)sender {
  [self _setGlobalPreference:ChiaKeyApplicationAutoUpdateEnabledPreferenceKey
                     enabled:([_applicationAutoUpdateCheckBox intValue] == 1)];
}

// Through the shared suite: the background Updater reads this too, and a
// value written to this app's own defaults would never reach it.
- (NSUserDefaults *)_updateSharedDefaults {
  return [[[NSUserDefaults alloc]
      initWithSuiteName:ChiaKeyUpdateSharedDefaultsSuiteName] autorelease];
}

- (BOOL)_includeBetaReleases {
  NSUserDefaults *shared = [self _updateSharedDefaults];
  id value = [shared objectForKey:ChiaKeyIncludeBetaReleasesDefaultsKey];
  if (value) return [value boolValue];

  // The toggle used to live in this app's own defaults; carry an opt-in over.
  BOOL legacy = [[NSUserDefaults standardUserDefaults]
      boolForKey:ChiaKeyIncludeBetaReleasesDefaultsKey];
  if (legacy) {
    [shared setBool:YES forKey:ChiaKeyIncludeBetaReleasesDefaultsKey];
    [shared synchronize];
  }
  return legacy;
}

- (IBAction)toggleIncludeBetaReleases:(id)sender {
  NSUserDefaults *defaults = [self _updateSharedDefaults];
  [defaults setBool:([_applicationIncludeBetaCheckBox intValue] == 1)
             forKey:ChiaKeyIncludeBetaReleasesDefaultsKey];
  [defaults synchronize];
  [self _setAvailableRelease:nil];
  _didAutoCheckOnShow = NO;
}

- (IBAction)toggleAutomaticLexiconUpdates:(id)sender {
  [self _setAutomaticLexiconUpdateEnabled:
            ([_lexiconAutoUpdateCheckBox intValue] == 1)];
}

@end
