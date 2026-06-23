/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoUpdate.h"

#import "TakaoHelper.h"

static NSString *const ChiaKeyLexiconLatestURL =
    @"https://github.com/akira02/ChiaKey-Lexicon/releases/latest";
static NSString *const ChiaKeyLatestLexiconDefaultsKey =
    @"ChiaKeyLatestLexiconVersion";
static NSString *const ChiaKeyLatestLexiconCheckDefaultsKey =
    @"ChiaKeyLatestLexiconCheck";

@implementation TakaoUpdate

- (void)dealloc {
  if (_task) [_task terminate];
  [_task release];
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

- (NSString *)_lexiconInstallRoot {
  NSArray *paths = NSSearchPathForDirectoriesInDomains(
      NSApplicationSupportDirectory, NSUserDomainMask, YES);
  NSString *basePath =
      ([paths count] > 0) ? [paths objectAtIndex:0] : NSTemporaryDirectory();
  return [basePath stringByAppendingPathComponent:
                       @"ChiaKey/Lexicons"];
}

- (NSString *)_currentLexiconVersion {
  NSString *activePath =
      [[self _lexiconInstallRoot] stringByAppendingPathComponent:@"active"];

  NSDictionary *metadata = [self
      _jsonDictionaryAtPath:[activePath stringByAppendingPathComponent:
                                            @"metadata.json"]];
  NSString *version = [metadata objectForKey:@"version"];
  if ([version length]) return version;

  NSDictionary *manifest = [self
      _jsonDictionaryAtPath:[activePath stringByAppendingPathComponent:
                                            @"lexicon-manifest.json"]];
  version = [manifest objectForKey:@"version"];
  if ([version length]) return version;

  return nil;
}

- (NSString *)_displayString:(NSString *)string fallback:(NSString *)fallback {
  return [string length] ? string : fallback;
}

- (NSString *)_formatDate:(NSDate *)date {
  NSDateFormatter *formatter = [[[NSDateFormatter alloc] init] autorelease];
  [formatter setDateFormat:@"yyyy-MM-dd HH:mm"];
  return [formatter stringFromDate:date];
}

- (void)_getVersionInfo {
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  NSString *currentVersion = [self _currentLexiconVersion];
  NSString *latestVersion =
      [defaults stringForKey:ChiaKeyLatestLexiconDefaultsKey];
  NSString *latestCheck =
      [defaults stringForKey:ChiaKeyLatestLexiconCheckDefaultsKey];

  [_currentVersionTextField
      setStringValue:[self _displayString:currentVersion
                                  fallback:LFLSTR(@"No lexicon installed")]];
  [_latestVersionTextField
      setStringValue:[self _displayString:latestVersion
                                  fallback:LFLSTR(@"Not checked yet")]];
  [_latestCheckTextField
      setStringValue:[self _displayString:latestCheck
                                  fallback:LFLSTR(@"Not checked yet")]];
}

- (void)awakeFromNib {
  [_checkProgressIndicator setHidden:YES];
  [self _getVersionInfo];
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
            NSDictionary *userInfo = [NSDictionary
                dictionaryWithObject:@"GitHub returned an error."
                              forKey:NSLocalizedDescriptionKey];
            completionError = [NSError errorWithDomain:@"ChiaKeyLexiconUpdate"
                                                  code:statusCode
                                              userInfo:userInfo];
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

- (NSComparisonResult)_compareVersion:(NSString *)lhs toVersion:(NSString *)rhs {
  NSArray *leftParts = [lhs componentsSeparatedByString:@"."];
  NSArray *rightParts = [rhs componentsSeparatedByString:@"."];
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
    ovService = [NSConnection
        rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
                                            host:nil];
    if (ovService) {
      [ovService setProtocolForProxy:@protocol(OpenVanillaService)];
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

- (void)_stopChecking {
  [_checkProgressIndicator stopAnimation:self];
  [_checkProgressIndicator setHidden:YES];
}

- (void)_handleLatestLexiconTag:(NSString *)latestTag error:(NSError *)error {
  NSString *checkTime = [self _formatDate:[NSDate date]];

  if ([latestTag length]) {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:latestTag forKey:ChiaKeyLatestLexiconDefaultsKey];
    [defaults setObject:checkTime forKey:ChiaKeyLatestLexiconCheckDefaultsKey];
    [defaults synchronize];
  }

  if (![latestTag length]) {
    [self _stopChecking];
    [self _showAlertWithTitle:LFLSTR(@"Unable to check for update via the Internet.")
                      message:LFLSTR(@"Please check your Internet connection and try again.")];
    [self _getVersionInfo];
    return;
  }

  NSString *currentTag = [self _currentLexiconVersion];
  if ([currentTag length] &&
      [self _compareVersion:currentTag toVersion:latestTag] !=
          NSOrderedAscending) {
    [self _stopChecking];
    [self _showAlertWithTitle:LFLSTR(@"You are now using the newest version.")
                      message:LFLSTR(@"You need not to update your lexicon")];
    [self _getVersionInfo];
    return;
  }

  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    NSString *output = nil;
    BOOL installed = [self _installLexiconRelease:latestTag output:&output];
    NSString *installOutput = [output copy];

    dispatch_async(dispatch_get_main_queue(), ^{
      [self _stopChecking];

      if (installed) {
        BOOL reloaded = [self _reloadOpenVanillaServer];
        NSString *message = nil;
        if (reloaded) {
          message = [NSString
              stringWithFormat:
                  LFLSTR(@"Installed lexicon %@. ChiaKey has reloaded it."),
                  latestTag];
        } else {
          message = [NSString
              stringWithFormat:
                  LFLSTR(@"Installed lexicon %@. Switch away from and back to ChiaKey to reload it."),
                  latestTag];
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

#pragma mark Interface Builder actions

- (IBAction)checkUpdateNow:(id)sender {
  if (![_checkProgressIndicator isHidden]) return;

  [_checkProgressIndicator startAnimation:self];
  [_checkProgressIndicator setHidden:NO];

  [self _latestLexiconReleaseTagWithCompletion:^(NSString *latestTag,
                                                 NSError *error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self _handleLatestLexiconTag:latestTag error:error];
    });
  }];
}

@end
