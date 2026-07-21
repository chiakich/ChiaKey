//
//  ChiaKeyUpdateService.h
//
//  Headless half of the application updater: release lookup, version policy,
//  download and package validation. Deliberately free of IBOutlets and of any
//  UI so the standalone Updater app and the Preferences pane can share it.
//
//  The IME never links this: it only decides "it is time to check" and spawns
//  the Updater app. Nothing here may run inside the input path.
//

#import <Foundation/Foundation.h>

extern NSString *const ChiaKeyUpdateErrorDomain;

@interface ChiaKeyUpdateRelease : NSObject {
  NSString *_tag;
  NSString *_releaseURL;
  NSString *_packageURL;
  NSString *_packageName;
  NSString *_packageSHA256;
  NSDate *_publishedAt;
  BOOL _prerelease;
}

@property(nonatomic, copy) NSString *tag;
@property(nonatomic, copy) NSString *releaseURL;
@property(nonatomic, copy) NSString *packageURL;
@property(nonatomic, copy) NSString *packageName;
@property(nonatomic, copy) NSString *packageSHA256;
@property(nonatomic, retain) NSDate *publishedAt;
@property(nonatomic, assign) BOOL prerelease;

@end

@interface ChiaKeyUpdateService : NSObject {
  NSURLSession *_downloadSession;
  void (^_progressHandler)(double);
  void (^_downloadCompletionHandler)(NSString *, NSError *);
  NSString *_downloadDestinationPath;
  NSString *_expectedSHA256;
  int _installLockDescriptor;
}

// Version of the installed input method, not of whatever bundle is running
// this code (the Updater lives inside the IME bundle's SharedSupport).
+ (NSString *)installedApplicationVersion;

+ (NSComparisonResult)compareVersion:(NSString *)lhs
                           toVersion:(NSString *)rhs;

- (void)fetchLatestReleaseIncludingBeta:(BOOL)includeBeta
                             completion:(void (^)(ChiaKeyUpdateRelease *release,
                                                  NSError *error))completion;

// The soak period: a release younger than this is ignored entirely, leaving
// room to pull a compromised build before clients act on it. Re-evaluated on
// every check against freshly fetched metadata, never against a cached tag.
- (BOOL)release:(ChiaKeyUpdateRelease *)release
    hasSettledForDays:(NSInteger)days;

- (BOOL)isVersionSkipped:(NSString *)tag;
- (void)skipVersion:(NSString *)tag;

// Fast User Switching leaves one IME (and one Updater) per logged-in user, but
// the package installs machine-wide behind a single admin prompt. Only the
// session holding this lock may prompt; it is released when the process exits.
- (BOOL)acquireInstallLock;

- (void)downloadPackageForRelease:(ChiaKeyUpdateRelease *)release
                         progress:(void (^)(double fraction))progress
                       completion:(void (^)(NSString *path,
                                            NSError *error))completion;

@end
