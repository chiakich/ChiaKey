//
//  ChiaKeyUpdateService.m
//

#import "ChiaKeyUpdateService.h"

#import <AppKit/AppKit.h>

#import <sys/file.h>
#import <sys/stat.h>
#import <unistd.h>

NSString *const ChiaKeyUpdateErrorDomain = @"ChiaKeyUpdate";

static NSString *const ChiaKeyUpdateManifestURL =
    @"https://cdn.chiaki.ch/chiakey/appcast.json";
static NSString *const ChiaKeyApplicationReleasesURL =
    @"https://api.github.com/repos/chiakich/ChiaKey/releases";
static NSString *const ChiaKeySharedDefaultsSuite = @"com.chiakey.ChiaKey";
static NSString *const ChiaKeySkippedApplicationVersionKey =
    @"ChiaKeySkippedApplicationVersion";
static NSString *const ChiaKeyIMEBundleIdentifierString =
    @"com.chiakey.inputmethod.ChiaKey";

@implementation ChiaKeyUpdateRelease

@synthesize tag = _tag;
@synthesize releaseURL = _releaseURL;
@synthesize packageURL = _packageURL;
@synthesize packageName = _packageName;
@synthesize packageSHA256 = _packageSHA256;
@synthesize publishedAt = _publishedAt;
@synthesize prerelease = _prerelease;

- (void)dealloc {
  [_tag release];
  [_releaseURL release];
  [_packageURL release];
  [_packageName release];
  [_packageSHA256 release];
  [_publishedAt release];
  [super dealloc];
}

@end

@interface ChiaKeyUpdateService () <NSURLSessionDownloadDelegate>
@end

@implementation ChiaKeyUpdateService

- (id)init {
  self = [super init];
  if (self) {
    _installLockDescriptor = -1;
  }
  return self;
}

- (void)dealloc {
  if (_installLockDescriptor >= 0) close(_installLockDescriptor);
  [_downloadSession invalidateAndCancel];
  [_downloadSession release];
  [_progressHandler release];
  [_downloadCompletionHandler release];
  [_downloadDestinationPath release];
  [_expectedSHA256 release];
  [super dealloc];
}

#pragma mark Shared preferences

- (NSUserDefaults *)_sharedDefaults {
  return [[[NSUserDefaults alloc]
      initWithSuiteName:ChiaKeySharedDefaultsSuite] autorelease];
}

- (BOOL)isVersionSkipped:(NSString *)tag {
  if (![tag length]) return NO;

  NSString *skipped =
      [[self _sharedDefaults] stringForKey:ChiaKeySkippedApplicationVersionKey];
  if (![skipped length]) return NO;

  return [[ChiaKeyUpdateService class] compareVersion:skipped
                                            toVersion:tag] != NSOrderedAscending;
}

- (void)skipVersion:(NSString *)tag {
  if (![tag length]) return;

  NSUserDefaults *defaults = [self _sharedDefaults];
  [defaults setObject:tag forKey:ChiaKeySkippedApplicationVersionKey];
  [defaults synchronize];
}

#pragma mark Cross-session locking

- (BOOL)acquireInstallLock {
  if (_installLockDescriptor >= 0) return YES;

  // /var/tmp is shared between users and survives logout, and flock releases
  // automatically if we crash, so a stale lock cannot wedge updates forever.
  const char *path = "/var/tmp/com.chiakey.ChiaKey.update.lock";
  int descriptor = open(path, O_RDONLY | O_CREAT, 0666);
  if (descriptor < 0) return YES;  // Cannot lock: do not block the update.

  // open() honours umask, so a file created by the first user could otherwise
  // be unopenable by the next one.
  fchmod(descriptor, 0666);

  if (flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
    close(descriptor);
    return NO;
  }

  _installLockDescriptor = descriptor;
  return YES;
}

#pragma mark Version policy

+ (NSString *)installedApplicationBundlePath {
  NSArray *running = [NSRunningApplication
      runningApplicationsWithBundleIdentifier:ChiaKeyIMEBundleIdentifierString];
  for (NSRunningApplication *application in running) {
    NSString *path = [[application bundleURL] path];
    if ([path length]) return path;
  }

  // Not running: the Updater lives in <IME>.app/Contents/SharedSupport, so
  // walk back up to the enclosing input method bundle.
  NSString *path = [[NSBundle mainBundle] bundlePath];
  for (NSUInteger index = 0; index < 4; index++) {
    path = [path stringByDeletingLastPathComponent];
    if ([[path pathExtension] isEqualToString:@"app"]) return path;
  }

  return nil;
}

+ (NSString *)installedApplicationVersion {
  NSString *path = [self installedApplicationBundlePath];
  NSBundle *bundle = [path length] ? [NSBundle bundleWithPath:path] : nil;
  if (!bundle) return nil;

  NSString *version = [bundle objectForInfoDictionaryKey:@"CFBundleVersion"];
  if (![version length])
    version = [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];

  return version;
}

+ (NSString *)_baseVersionString:(NSString *)version {
  if (![version length]) return nil;

  NSString *trimmed = [version
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  NSCharacterSet *digits = [NSCharacterSet decimalDigitCharacterSet];
  NSUInteger firstDigit = NSNotFound;
  for (NSUInteger index = 0; index < [trimmed length]; index++) {
    if ([digits characterIsMember:[trimmed characterAtIndex:index]]) {
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

+ (NSArray *)_numericVersionParts:(NSString *)version {
  NSString *base = [self _baseVersionString:version];
  if (![base length]) return [NSArray array];

  NSArray *rawParts = [base
      componentsSeparatedByCharactersInSet:[[NSCharacterSet
                                               decimalDigitCharacterSet]
                                               invertedSet]];
  NSMutableArray *parts = [NSMutableArray array];
  for (NSString *part in rawParts) {
    if (![part length]) continue;
    [parts addObject:[NSNumber numberWithInteger:[part integerValue]]];
  }
  return parts;
}

+ (NSComparisonResult)compareVersion:(NSString *)lhs
                           toVersion:(NSString *)rhs {
  NSArray *leftParts = [self _numericVersionParts:lhs];
  NSArray *rightParts = [self _numericVersionParts:rhs];
  NSUInteger count = MAX([leftParts count], [rightParts count]);

  for (NSUInteger index = 0; index < count; index++) {
    NSInteger leftValue = (index < [leftParts count])
                              ? [[leftParts objectAtIndex:index] integerValue]
                              : 0;
    NSInteger rightValue = (index < [rightParts count])
                               ? [[rightParts objectAtIndex:index] integerValue]
                               : 0;
    if (leftValue < rightValue) return NSOrderedAscending;
    if (leftValue > rightValue) return NSOrderedDescending;
  }

  return NSOrderedSame;
}

- (BOOL)release:(ChiaKeyUpdateRelease *)release
    hasSettledForDays:(NSInteger)days {
  NSDate *publishedAt = [release publishedAt];
  // No usable timestamp means we cannot prove the soak period elapsed, so
  // treat it as too new rather than waving it through.
  if (!publishedAt) return NO;

  return [[NSDate date] timeIntervalSinceDate:publishedAt] >=
         (NSTimeInterval)days * 24.0 * 60.0 * 60.0;
}

#pragma mark Release lookup

- (NSError *)_errorWithDescription:(NSString *)description
                              code:(NSInteger)code {
  NSDictionary *userInfo =
      [NSDictionary dictionaryWithObject:(description ? description : @"")
                                  forKey:NSLocalizedDescriptionKey];
  return [NSError errorWithDomain:ChiaKeyUpdateErrorDomain
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
        ![downloadURL isKindOfClass:[NSString class]] || ![downloadURL length])
      continue;
    if ([[name pathExtension] caseInsensitiveCompare:@"pkg"] != NSOrderedSame)
      continue;
    if ([[name lowercaseString] rangeOfString:@"unsigned"].location !=
        NSNotFound)
      continue;

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
      [[NSCharacterSet characterSetWithCharactersInString:@"0123456789abcdef"]
          invertedSet];
  if ([lowercaseDigest length] != 64 ||
      [lowercaseDigest rangeOfCharacterFromSet:nonHexCharacters].location !=
          NSNotFound)
    return nil;

  return lowercaseDigest;
}

- (NSDate *)_dateFromISO8601String:(NSString *)string {
  if (![string isKindOfClass:[NSString class]] || ![string length]) return nil;

  static NSISO8601DateFormatter *formatter = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    formatter = [[NSISO8601DateFormatter alloc] init];
  });

  return [formatter dateFromString:string];
}

- (ChiaKeyUpdateRelease *)_releaseFromJSON:(NSDictionary *)json {
  NSString *tag = [json objectForKey:@"tag_name"];
  if (![tag isKindOfClass:[NSString class]] || ![tag length]) return nil;

  NSDictionary *asset = [self _packageAssetFromRelease:json];
  NSString *releaseURL = [json objectForKey:@"html_url"];
  NSString *packageURL = [asset objectForKey:@"browser_download_url"];
  NSString *packageName = [asset objectForKey:@"name"];

  ChiaKeyUpdateRelease *release =
      [[[ChiaKeyUpdateRelease alloc] init] autorelease];
  [release setTag:tag];
  [release
      setReleaseURL:([releaseURL isKindOfClass:[NSString class]] ? releaseURL
                                                                : nil)];
  [release
      setPackageURL:([packageURL isKindOfClass:[NSString class]] ? packageURL
                                                                : nil)];
  [release setPackageName:([packageName isKindOfClass:[NSString class]]
                               ? packageName
                               : nil)];
  [release setPackageSHA256:[self _sha256FromPackageAsset:asset]];
  [release setPublishedAt:[self _dateFromISO8601String:
                                   [json objectForKey:@"published_at"]]];
  [release setPrerelease:[[json objectForKey:@"prerelease"] boolValue]];

  return release;
}

- (ChiaKeyUpdateRelease *)_releaseFromManifestEntry:(NSDictionary *)entry {
  if (![entry isKindOfClass:[NSDictionary class]]) return nil;

  NSString *tag = [entry objectForKey:@"tag"];
  NSString *packageURL = [entry objectForKey:@"package_url"];
  if (![tag isKindOfClass:[NSString class]] || ![tag length]) return nil;
  if (![packageURL isKindOfClass:[NSString class]] || ![packageURL length])
    return nil;

  NSString *sha256 = [entry objectForKey:@"sha256"];
  NSString *packageName = [entry objectForKey:@"package_name"];

  ChiaKeyUpdateRelease *release =
      [[[ChiaKeyUpdateRelease alloc] init] autorelease];
  [release setTag:tag];
  [release setPackageURL:packageURL];
  [release setPackageName:([packageName isKindOfClass:[NSString class]]
                               ? packageName
                               : nil)];
  [release setPackageSHA256:([sha256 isKindOfClass:[NSString class]]
                                 ? [sha256 lowercaseString]
                                 : nil)];
  [release setPublishedAt:[self _dateFromISO8601String:
                                   [entry objectForKey:@"published_at"]]];
  [release setPrerelease:[[entry objectForKey:@"prerelease"] boolValue]];

  return release;
}

// R2 first: the GitHub API's unauthenticated 60/hour is per IP, so everyone
// behind one corporate NAT shares it, and a daily check per user is enough to
// exhaust it. GitHub stays as the fallback so a CDN outage cannot strand
// anyone on an old build.
- (void)_fetchManifestReleaseIncludingBeta:(BOOL)includeBeta
                                completion:(void (^)(ChiaKeyUpdateRelease *,
                                                     NSError *))completion {
  NSMutableURLRequest *request = [NSMutableURLRequest
      requestWithURL:[NSURL URLWithString:ChiaKeyUpdateManifestURL]
         cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
     timeoutInterval:15.0];
  [request setValue:@"ChiaKey Updater" forHTTPHeaderField:@"User-Agent"];

  NSURLSessionDataTask *task = [[NSURLSession sharedSession]
      dataTaskWithRequest:request
        completionHandler:^(NSData *data, NSURLResponse *response,
                            NSError *requestError) {
          NSInteger statusCode =
              [response isKindOfClass:[NSHTTPURLResponse class]]
                  ? [(NSHTTPURLResponse *)response statusCode]
                  : 0;
          if (requestError || statusCode >= 400 || !data) {
            completion(nil, requestError);
            return;
          }

          id object = [NSJSONSerialization JSONObjectWithData:data
                                                      options:0
                                                        error:NULL];
          if (![object isKindOfClass:[NSDictionary class]]) {
            completion(nil, nil);
            return;
          }

          NSDictionary *manifest = (NSDictionary *)object;
          ChiaKeyUpdateRelease *release = [self
              _releaseFromManifestEntry:[manifest objectForKey:(includeBeta
                                                                    ? @"beta"
                                                                    : @"stable")]];
          completion(release, nil);
        }];
  [task resume];
}

- (void)fetchLatestReleaseIncludingBeta:(BOOL)includeBeta
                             completion:(void (^)(ChiaKeyUpdateRelease *,
                                                  NSError *))completion {
  [self _fetchManifestReleaseIncludingBeta:includeBeta
                                completion:^(ChiaKeyUpdateRelease *release,
                                             NSError *error) {
    if (release) {
      if (completion) completion(release, nil);
      return;
    }
    [self _fetchGitHubReleaseIncludingBeta:includeBeta completion:completion];
  }];
}

- (void)_fetchGitHubReleaseIncludingBeta:(BOOL)includeBeta
                              completion:(void (^)(ChiaKeyUpdateRelease *,
                                                   NSError *))completion {
  NSMutableURLRequest *request = [NSMutableURLRequest
      requestWithURL:[NSURL URLWithString:ChiaKeyApplicationReleasesURL]
         cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
     timeoutInterval:20.0];
  [request setValue:@"ChiaKey Updater" forHTTPHeaderField:@"User-Agent"];
  [request setValue:@"application/vnd.github+json"
      forHTTPHeaderField:@"Accept"];

  NSURLSessionDataTask *task = [[NSURLSession sharedSession]
      dataTaskWithRequest:request
        completionHandler:^(NSData *data, NSURLResponse *response,
                            NSError *requestError) {
          NSError *error = requestError;
          NSInteger statusCode =
              [response isKindOfClass:[NSHTTPURLResponse class]]
                  ? [(NSHTTPURLResponse *)response statusCode]
                  : 0;
          if (!error && statusCode >= 400) {
            error = [self _errorWithDescription:@"GitHub returned an error."
                                           code:statusCode];
          }

          ChiaKeyUpdateRelease *best = nil;
          if (!error) {
            NSError *jsonError = nil;
            id object = [NSJSONSerialization JSONObjectWithData:data
                                                        options:0
                                                          error:&jsonError];
            if (![object isKindOfClass:[NSArray class]]) {
              error = jsonError
                          ? jsonError
                          : [self _errorWithDescription:
                                      @"GitHub returned an unexpected response."
                                                   code:0];
            } else {
              for (id item in (NSArray *)object) {
                if (![item isKindOfClass:[NSDictionary class]]) continue;
                NSDictionary *json = (NSDictionary *)item;
                if ([[json objectForKey:@"draft"] boolValue]) continue;
                if ([[json objectForKey:@"prerelease"] boolValue] &&
                    !includeBeta)
                  continue;

                ChiaKeyUpdateRelease *candidate =
                    [self _releaseFromJSON:json];
                if (!candidate) continue;
                if (best &&
                    [[self class] compareVersion:[best tag]
                                       toVersion:[candidate tag]] !=
                        NSOrderedAscending)
                  continue;
                best = candidate;
              }
            }
          }

          if (completion) completion(error ? nil : best, error);
        }];
  [task resume];
}

#pragma mark Package validation

- (BOOL)_runTool:(NSString *)launchPath
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
  } @catch (NSException *exception) {
    if (output) *output = [exception description];
    return NO;
  }

  // Read before waiting: a chatty tool can fill the pipe buffer and deadlock
  // against waitUntilExit.
  NSData *data = [[pipe fileHandleForReading] readDataToEndOfFile];
  [task waitUntilExit];

  NSString *toolOutput =
      [[[NSString alloc] initWithData:data
                             encoding:NSUTF8StringEncoding] autorelease];
  if (output) *output = toolOutput ? toolOutput : @"";

  return [task terminationStatus] == 0;
}

- (NSString *)_teamIdentifierInString:(NSString *)string {
  // Developer ID subject lines end with the team identifier in parentheses,
  // e.g. "Developer ID Installer: Someone (A1B2C3D4E5)".
  NSRegularExpression *expression = [NSRegularExpression
      regularExpressionWithPattern:@"Developer ID [A-Za-z]+: .*\\(([A-Z0-9]{10})\\)"
                           options:0
                             error:NULL];
  NSTextCheckingResult *match =
      [expression firstMatchInString:string
                             options:0
                               range:NSMakeRange(0, [string length])];
  if (!match || [match numberOfRanges] < 2) return nil;

  return [string substringWithRange:[match rangeAtIndex:1]];
}

- (NSString *)_installedApplicationTeamIdentifier {
  NSString *path = [[self class] _installedApplicationBundlePath];
  if (![path length]) return nil;

  NSString *output = nil;
  [self _runTool:@"/usr/bin/codesign"
       arguments:[NSArray arrayWithObjects:@"-dv", @"--verbose=4", path, nil]
          output:&output];
  if (![output length]) return nil;

  for (NSString *line in [output componentsSeparatedByString:@"\n"]) {
    if (![line hasPrefix:@"TeamIdentifier="]) continue;
    NSString *identifier = [line substringFromIndex:[@"TeamIdentifier=" length]];
    identifier = [identifier
        stringByTrimmingCharactersInSet:[NSCharacterSet
                                            whitespaceAndNewlineCharacterSet]];
    if ([identifier length] && ![identifier isEqualToString:@"not set"])
      return identifier;
  }

  return nil;
}

- (BOOL)_verifySHA256OfFileAtPath:(NSString *)path
                         expected:(NSString *)expectedSHA256
                            error:(NSError **)error {
  if (![expectedSHA256 length]) {
    if (error) {
      *error = [self _errorWithDescription:
                         @"The release metadata carried no SHA-256 digest."
                                      code:0];
    }
    return NO;
  }

  NSString *output = nil;
  BOOL ran = [self _runTool:@"/usr/bin/shasum"
                  arguments:[NSArray arrayWithObjects:@"-a", @"256", path, nil]
                     output:&output];
  NSArray *components = [output
      componentsSeparatedByCharactersInSet:[NSCharacterSet
                                               whitespaceAndNewlineCharacterSet]];
  NSString *actual = [components count] ? [components objectAtIndex:0] : nil;
  if (!ran || [actual caseInsensitiveCompare:expectedSHA256] != NSOrderedSame) {
    if (error) {
      *error = [self _errorWithDescription:
                         @"Downloaded package SHA-256 did not match the "
                         @"release metadata."
                                      code:0];
    }
    return NO;
  }

  return YES;
}

- (BOOL)_validatePackageAtPath:(NSString *)path error:(NSError **)error {
  NSString *signatureOutput = nil;
  BOOL signedWithDeveloperID =
      [self _runTool:@"/usr/sbin/pkgutil"
           arguments:[NSArray arrayWithObjects:@"--check-signature", path, nil]
              output:&signatureOutput] &&
      [signatureOutput rangeOfString:@"Developer ID Installer"
                             options:NSCaseInsensitiveSearch].location !=
          NSNotFound;
  if (!signedWithDeveloperID) {
    if (error) {
      *error = [self _errorWithDescription:
                         @"Downloaded package is not signed with a Developer "
                         @"ID Installer certificate."
                                      code:0];
    }
    return NO;
  }

  // A valid Developer ID signature only proves *some* Apple developer built
  // it; pin it to the team that signed what is already installed so a
  // different account's certificate cannot ship us an update.
  NSString *expectedTeam = [self _installedApplicationTeamIdentifier];
  NSString *packageTeam = [self _teamIdentifierInString:signatureOutput];
  if ([expectedTeam length]) {
    if (![packageTeam length] ||
        ![packageTeam isEqualToString:expectedTeam]) {
      if (error) {
        *error = [self
            _errorWithDescription:
                [NSString stringWithFormat:
                              @"Downloaded package was signed by team %@, but "
                              @"the installed input method belongs to team %@.",
                              [packageTeam length] ? packageTeam : @"(unknown)",
                              expectedTeam]
                             code:0];
      }
      return NO;
    }
  }

  NSString *gatekeeperOutput = nil;
  BOOL acceptedByGatekeeper =
      [self _runTool:@"/usr/sbin/spctl"
           arguments:[NSArray arrayWithObjects:@"--assess", @"--type",
                                               @"install", @"--verbose=4", path,
                                               nil]
              output:&gatekeeperOutput] &&
      [gatekeeperOutput rangeOfString:@"Notarized Developer ID"
                              options:NSCaseInsensitiveSearch].location !=
          NSNotFound;
  if (!acceptedByGatekeeper) {
    if (error) {
      *error = [self _errorWithDescription:
                         @"Downloaded package was rejected by Gatekeeper or is "
                         @"not notarized."
                                      code:0];
    }
    return NO;
  }

  return YES;
}

#pragma mark Download

- (NSString *)_safePackageFileNameForRelease:(ChiaKeyUpdateRelease *)release {
  NSString *fileName = [[release packageName] length]
                           ? [release packageName]
                           : [NSString stringWithFormat:@"ChiaKey-%@.pkg",
                                                        [release tag]];
  NSCharacterSet *unsafeCharacters =
      [NSCharacterSet characterSetWithCharactersInString:@"/:"];
  fileName = [[fileName componentsSeparatedByCharactersInSet:unsafeCharacters]
      componentsJoinedByString:@"-"];
  if ([[fileName pathExtension] caseInsensitiveCompare:@"pkg"] !=
      NSOrderedSame)
    fileName = [fileName stringByAppendingPathExtension:@"pkg"];

  return fileName;
}

- (void)_finishDownloadWithPath:(NSString *)path error:(NSError *)error {
  void (^completion)(NSString *, NSError *) =
      [[_downloadCompletionHandler retain] autorelease];

  [_progressHandler release];
  _progressHandler = nil;
  [_downloadCompletionHandler release];
  _downloadCompletionHandler = nil;

  if (completion) {
    dispatch_async(dispatch_get_main_queue(), ^{
      completion(path, error);
    });
  }
}

- (void)downloadPackageForRelease:(ChiaKeyUpdateRelease *)release
                         progress:(void (^)(double))progress
                       completion:(void (^)(NSString *, NSError *))completion {
  NSURL *url = [NSURL URLWithString:[release packageURL]];
  if (!url) {
    if (completion) {
      completion(nil, [self _errorWithDescription:@"Invalid package URL."
                                             code:0]);
    }
    return;
  }

  NSString *directory =
      [NSTemporaryDirectory() stringByAppendingPathComponent:@"ChiaKeyUpdates"];

  [_progressHandler release];
  _progressHandler = [progress copy];
  [_downloadCompletionHandler release];
  _downloadCompletionHandler = [completion copy];
  [_downloadDestinationPath release];
  _downloadDestinationPath =
      [[directory stringByAppendingPathComponent:
                      [self _safePackageFileNameForRelease:release]] copy];
  [_expectedSHA256 release];
  _expectedSHA256 = [[release packageSHA256] copy];

  if (!_downloadSession) {
    NSURLSessionConfiguration *configuration =
        [NSURLSessionConfiguration ephemeralSessionConfiguration];
    _downloadSession =
        [[NSURLSession sessionWithConfiguration:configuration
                                       delegate:self
                                  delegateQueue:nil] retain];
  }

  [[_downloadSession downloadTaskWithURL:url] resume];
}

#pragma mark NSURLSessionDownloadDelegate

- (void)URLSession:(NSURLSession *)session
                 downloadTask:(NSURLSessionDownloadTask *)downloadTask
                 didWriteData:(int64_t)bytesWritten
            totalBytesWritten:(int64_t)totalBytesWritten
    totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite {
  if (!_progressHandler) return;

  double fraction =
      (totalBytesExpectedToWrite > 0)
          ? (double)totalBytesWritten / (double)totalBytesExpectedToWrite
          : -1.0;
  void (^handler)(double) = [[_progressHandler retain] autorelease];
  dispatch_async(dispatch_get_main_queue(), ^{
    handler(fraction);
  });
}

- (void)URLSession:(NSURLSession *)session
                 downloadTask:(NSURLSessionDownloadTask *)downloadTask
    didFinishDownloadingToURL:(NSURL *)location {
  NSInteger statusCode =
      [[downloadTask response] isKindOfClass:[NSHTTPURLResponse class]]
          ? [(NSHTTPURLResponse *)[downloadTask response] statusCode]
          : 0;
  if (statusCode >= 400) {
    [self _finishDownloadWithPath:nil
                            error:[self _errorWithDescription:
                                            @"GitHub returned an error."
                                                         code:statusCode]];
    return;
  }

  NSFileManager *fileManager = [NSFileManager defaultManager];
  NSURL *destinationURL = [NSURL fileURLWithPath:_downloadDestinationPath];
  NSError *error = nil;

  [fileManager createDirectoryAtPath:[_downloadDestinationPath
                                         stringByDeletingLastPathComponent]
         withIntermediateDirectories:YES
                          attributes:nil
                               error:&error];
  if (!error) {
    [fileManager removeItemAtURL:destinationURL error:nil];
    [fileManager moveItemAtURL:location toURL:destinationURL error:&error];
  }
  if (!error) {
    [self _verifySHA256OfFileAtPath:_downloadDestinationPath
                           expected:_expectedSHA256
                              error:&error];
  }
  if (!error) {
    [self _validatePackageAtPath:_downloadDestinationPath error:&error];
  }
  if (error) {
    [fileManager removeItemAtURL:destinationURL error:nil];
    [self _finishDownloadWithPath:nil error:error];
    return;
  }

  [self _finishDownloadWithPath:_downloadDestinationPath error:nil];
}

- (void)URLSession:(NSURLSession *)session
                    task:(NSURLSessionTask *)task
    didCompleteWithError:(NSError *)error {
  // Success is reported by didFinishDownloadingToURL, which runs first.
  if (error) [self _finishDownloadWithPath:nil error:error];
}

@end
