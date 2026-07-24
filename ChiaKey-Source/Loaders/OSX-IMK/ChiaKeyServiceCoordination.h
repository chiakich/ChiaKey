//
//  ChiaKeyServiceCoordination.h
//
//  File + distributed-notification replacement for the Preferences app's
//  legacy IPC channel (see ChiaKeyUserPhraseCoordination.h for the same story
//  on the phrase editor side).
//
//  Mechanism:
//  - The IME PUBLISHES a status plist (module list, package info, versions)
//    next to its user data on startup, after every reload, and after a
//    blacklist change. The Preferences app just reads the file.
//  - The Preferences app REQUESTS actions with distributed notifications:
//    a full engine reload, or "apply the pending module blacklist" (the
//    desired blacklist is written to a pending file first, so it survives
//    the IME not running and is applied at the next startup).
//  - Liveness ("is the IME running?") is answered by NSRunningApplication,
//    not by a ping over a channel.
//

#ifndef CHIAKEY_SERVICE_COORDINATION_H_
#define CHIAKEY_SERVICE_COORDINATION_H_

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#define ChiaKeyIMEBundleIdentifier @"com.chiakey.inputmethod.ChiaKey"

// Preferences -> IME requests
#define ChiaKeyReloadRequestedNotification \
  @"org.chiakey.inputmethod.ReloadRequested"
#define ChiaKeyModuleBlacklistDidChangeNotification \
  @"org.chiakey.inputmethod.ModuleBlacklistDidChange"
// IME -> observers: the status file has been rewritten
#define ChiaKeyServiceStatusDidUpdateNotification \
  @"org.chiakey.inputmethod.ServiceStatusDidUpdate"

// Status plist keys
#define ChiaKeyStatusVersionKey @"Version"
#define ChiaKeyStatusDatabaseVersionKey @"DatabaseVersion"
#define ChiaKeyStatusUpdatedAtKey @"UpdatedAt"
// Array of [identifier, localizedName] pairs, all modules
#define ChiaKeyStatusModulesKey @"Modules"
// Array of dictionaries using the following package keys.
#define ChiaKeyStatusPackagesKey @"Packages"
#define OVServiceLoadedModulePackageIdentifierKey \
  @"OVServiceLoadedModulePackageIdentifierKey"
#define OVServiceLoadedModulePackageLocalizedNameKey \
  @"OVServiceLoadedModulePackageLocalizedNameKey"
#define OVServiceLoadedModulePackageBundlePathKey \
  @"OVServiceLoadedModulePackageBundlePathKey"
#define OVServiceLoadedModulePackageEnabledKey \
  @"OVServiceLoadedModulePackageEnabledKey"

// Same directory that holds SmartMandarinUserData.db; "ChiaKey" is the
// loader name (PVLOADERPOLICY_LOADER_NAME).
static inline NSString *ChiaKeyServiceUserDataDirectory(void) {
  return [NSHomeDirectory()
      stringByAppendingPathComponent:@"Library/Application Support/ChiaKey"];
}

// Creates the user data directory with 0700 permissions if it doesn't exist
// yet. This directory holds SmartMandarinUserData.db / UserData.db, a record
// of what the user has typed; 0700 keeps it unreadable to other local
// accounts. NSFileManager only applies `attributes:` on creation, so this
// does nothing to a directory that already exists with a different mode.
static inline void ChiaKeyEnsureUserDataDirectoryPrivate(void) {
  [[NSFileManager defaultManager]
            createDirectoryAtPath:ChiaKeyServiceUserDataDirectory()
      withIntermediateDirectories:YES
                       attributes:@{NSFilePosixPermissions : @0700}
                            error:NULL];
}

static inline NSString *ChiaKeyServiceStatusPath(void) {
  return [ChiaKeyServiceUserDataDirectory()
      stringByAppendingPathComponent:@"IMEStatus.plist"];
}

static inline NSString *ChiaKeyPendingModuleBlacklistPath(void) {
  return [ChiaKeyServiceUserDataDirectory()
      stringByAppendingPathComponent:@"PendingModuleBlacklist.plist"];
}

static inline NSDictionary *ChiaKeyReadServiceStatus(void) {
  return [NSDictionary dictionaryWithContentsOfFile:ChiaKeyServiceStatusPath()];
}

static inline BOOL ChiaKeyIMEIsRunning(void) {
  return [[NSRunningApplication
             runningApplicationsWithBundleIdentifier:ChiaKeyIMEBundleIdentifier]
             count] > 0;
}

static inline void ChiaKeyPostServiceNotification(NSString *name) {
  [[NSDistributedNotificationCenter defaultCenter]
      postNotificationName:name
                    object:nil
                  userInfo:nil
        deliverImmediately:YES];
}

#endif  // CHIAKEY_SERVICE_COORDINATION_H_
