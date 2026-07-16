//
//  ChiaKeyUserPhraseCoordination.h
//
//  Shared protocol between the Phrase Editor and the running input method
//  for coordinating access to the user phrase database
//  (SmartMandarinUserData.db). No mach-service registration required.
//
//  Three cooperating signals, all next to the user DB:
//  - Editing-lock file (SmartMandarinUserData.editing): authoritative "an
//    editor session is active" marker. The editor creates it on window open,
//    refreshes its mtime periodically, removes it on exit. While a fresh
//    lock exists the IME must not write to the user DB (auto-learn and
//    chiakey:// additions are queued or skipped). A lock older than
//    ChiaKeyPhraseEditorSessionTimeout is stale (editor crashed) and must be
//    ignored, so writes are never suspended forever.
//  - Dirty-flag file (SmartMandarinUserData.dirty): its mtime advances on
//    every editor commit. The IME reloads its LM caches when it sees a newer
//    mtime than the last load. This is the lossless fallback for missed
//    notifications.
//  - NSDistributedNotificationCenter notifications (below): best-effort,
//    fire-and-forget hints that make the above take effect immediately
//    instead of at the next poll.
//
//  Note for C++ call sites that cannot import Foundation (e.g. Manjusri's
//  LanguageModel): the lock file name and the staleness rule above are the
//  contract; check the file with stat(2) directly.
//

#ifndef CHIAKEY_USER_PHRASE_COORDINATION_H_
#define CHIAKEY_USER_PHRASE_COORDINATION_H_

#import <Foundation/Foundation.h>

#define ChiaKeyPhraseEditorDidBeginEditingNotification \
  @"org.chiakey.inputmethod.PhraseEditorDidBeginEditing"
#define ChiaKeyPhraseEditorDidEndEditingNotification \
  @"org.chiakey.inputmethod.PhraseEditorDidEndEditing"
#define ChiaKeyUserPhraseDidChangeNotification \
  @"org.chiakey.inputmethod.UserPhraseDidChange"

// A lock whose mtime is older than this is treated as left over from a
// crashed editor. Keep in sync with kEditingLockTimeout in LanguageModel.h.
#define ChiaKeyPhraseEditorSessionTimeout (30.0 * 60.0)

#define ChiaKeyUserPhraseEditingLockFilename @"SmartMandarinUserData.editing"
#define ChiaKeyUserPhraseDirtyFlagFilename @"SmartMandarinUserData.dirty"

static inline NSString *ChiaKeyUserPhraseEditingLockPath(
    NSString *userDataDirectory) {
  return [userDataDirectory
      stringByAppendingPathComponent:ChiaKeyUserPhraseEditingLockFilename];
}

static inline NSString *ChiaKeyUserPhraseDirtyFlagPath(
    NSString *userDataDirectory) {
  return [userDataDirectory
      stringByAppendingPathComponent:ChiaKeyUserPhraseDirtyFlagFilename];
}

static inline void ChiaKeyTouchCoordinationFile(NSString *path) {
  NSFileManager *fm = [NSFileManager defaultManager];
  if (![fm fileExistsAtPath:path]) {
    [fm createFileAtPath:path contents:[NSData data] attributes:nil];
  } else {
    [fm setAttributes:[NSDictionary
                          dictionaryWithObject:[NSDate date]
                                        forKey:NSFileModificationDate]
         ofItemAtPath:path
                error:NULL];
  }
}

static inline NSDate *ChiaKeyCoordinationFileDate(NSString *path) {
  return [[[NSFileManager defaultManager] attributesOfItemAtPath:path
                                                            error:NULL]
      objectForKey:NSFileModificationDate];
}

static inline BOOL ChiaKeyUserPhraseEditingLockIsActive(
    NSString *userDataDirectory) {
  NSDate *date = ChiaKeyCoordinationFileDate(
      ChiaKeyUserPhraseEditingLockPath(userDataDirectory));
  if (!date) return NO;
  return [[NSDate date] timeIntervalSinceDate:date] <
         ChiaKeyPhraseEditorSessionTimeout;
}

static inline void ChiaKeyPostUserPhraseNotification(NSString *name) {
  [[NSDistributedNotificationCenter defaultCenter]
      postNotificationName:name
                    object:nil
                  userInfo:nil
        deliverImmediately:YES];
}

#endif  // CHIAKEY_USER_PHRASE_COORDINATION_H_
