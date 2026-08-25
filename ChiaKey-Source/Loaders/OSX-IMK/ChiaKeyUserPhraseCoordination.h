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

#include <errno.h>
#include <signal.h>
#include <unistd.h>

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

// The lock carries the PID of every live claimant, one per line, so that
// overlapping editing sessions (e.g. Phrase Editor open while Preferences
// runs an import) do not tear down each other's lock: each session adds
// itself on claim and removes only itself on release; the file goes away
// with the last live claimant. A dead PID (crashed session) is dropped
// whenever the list is rewritten. The IME only ever looks at existence +
// mtime, never at the contents, so older builds that wrote a single PID
// interoperate: their file reads as a one-entry list, and they refuse to
// remove a multi-entry file they do not recognise as their own.

static inline NSString *ChiaKeyEditingLockOwnerTag(void) {
  return [NSString stringWithFormat:@"%d", (int)getpid()];
}

static inline NSArray *ChiaKeyLiveEditingLockOwners(NSString *path) {
  NSString *contents = [[[NSString alloc]
      initWithData:[NSData dataWithContentsOfFile:path]
          encoding:NSUTF8StringEncoding] autorelease];
  NSMutableArray *live = [NSMutableArray array];
  for (NSString *line in
       [contents componentsSeparatedByCharactersInSet:
                     [NSCharacterSet newlineCharacterSet]]) {
    int pid = [line intValue];
    if (pid <= 0) continue;
    // Gone only when the kernel says so; EPERM still means alive.
    if (kill((pid_t)pid, 0) != 0 && errno == ESRCH) continue;
    NSString *tag = [NSString stringWithFormat:@"%d", pid];
    if (![live containsObject:tag]) [live addObject:tag];
  }
  return live;
}

static inline void ChiaKeyWriteEditingLockOwners(NSString *path,
                                                 NSArray *owners) {
  [[[owners componentsJoinedByString:@"\n"]
      dataUsingEncoding:NSUTF8StringEncoding] writeToFile:path
                                               atomically:YES];
}

static inline void ChiaKeyClaimUserPhraseEditingLock(
    NSString *userDataDirectory) {
  NSString *path = ChiaKeyUserPhraseEditingLockPath(userDataDirectory);
  NSMutableArray *owners = [NSMutableArray array];
  if (ChiaKeyUserPhraseEditingLockIsActive(userDataDirectory)) {
    [owners addObjectsFromArray:ChiaKeyLiveEditingLockOwners(path)];
  }
  NSString *me = ChiaKeyEditingLockOwnerTag();
  if (![owners containsObject:me]) [owners addObject:me];
  ChiaKeyWriteEditingLockOwners(path, owners);
}

static inline void ChiaKeyRefreshUserPhraseEditingLock(
    NSString *userDataDirectory) {
  NSString *path = ChiaKeyUserPhraseEditingLockPath(userDataDirectory);
  if ([[NSFileManager defaultManager] fileExistsAtPath:path]) {
    // Re-list this session if a write raced it out; also advances the mtime.
    NSArray *owners = ChiaKeyLiveEditingLockOwners(path);
    NSString *me = ChiaKeyEditingLockOwnerTag();
    if (![owners containsObject:me]) {
      ChiaKeyWriteEditingLockOwners(
          path, [owners arrayByAddingObject:me]);
    } else {
      ChiaKeyTouchCoordinationFile(path);
    }
  } else {
    // Someone removed it (e.g. another session's end); reclaim.
    ChiaKeyWriteEditingLockOwners(
        path, [NSArray arrayWithObject:ChiaKeyEditingLockOwnerTag()]);
  }
}

// Removes this session from the lock. Returns YES when no other live session
// remains and the file was removed; NO while another claimant still holds it.
static inline BOOL ChiaKeyReleaseUserPhraseEditingLockIfOwner(
    NSString *userDataDirectory) {
  NSString *path = ChiaKeyUserPhraseEditingLockPath(userDataDirectory);
  NSMutableArray *owners =
      [[ChiaKeyLiveEditingLockOwners(path) mutableCopy] autorelease];
  [owners removeObject:ChiaKeyEditingLockOwnerTag()];
  if ([owners count]) {
    // Another live session is still editing: hand the lock over to it.
    ChiaKeyWriteEditingLockOwners(path, owners);
    return NO;
  }
  [[NSFileManager defaultManager] removeItemAtPath:path error:NULL];
  return YES;
}

static inline void ChiaKeyPostUserPhraseNotification(NSString *name) {
  [[NSDistributedNotificationCenter defaultCenter]
      postNotificationName:name
                    object:nil
                  userInfo:nil
        deliverImmediately:YES];
}

#endif  // CHIAKEY_USER_PHRASE_COORDINATION_H_
