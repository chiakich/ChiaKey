//
//  TestEditingLockCoordination.mm
//
//  The editing lock lists every live claimant, so a session ending first must
//  not release the lock out from under one still editing.
//

#import <Foundation/Foundation.h>

#import "ChiaKeyUserPhraseCoordination.h"

static int failures = 0;

#define CHECK(cond)                                      \
  do {                                                   \
    if (!(cond)) {                                       \
      fprintf(stderr, "FAIL %d: %s\n", __LINE__, #cond); \
      failures++;                                        \
    }                                                    \
  } while (0)

static NSString *LockContents(NSString *dir) {
  return [[[NSString alloc]
      initWithData:[NSData dataWithContentsOfFile:
                               ChiaKeyUserPhraseEditingLockPath(dir)]
          encoding:NSUTF8StringEncoding] autorelease];
}

static void WriteLock(NSString *dir, NSString *contents) {
  [[contents dataUsingEncoding:NSUTF8StringEncoding]
      writeToFile:ChiaKeyUserPhraseEditingLockPath(dir)
       atomically:YES];
}

int main(int argc, char **argv) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  NSString *dir = NSTemporaryDirectory();
  NSString *me = ChiaKeyEditingLockOwnerTag();
  NSString *lockPath = ChiaKeyUserPhraseEditingLockPath(dir);
  [[NSFileManager defaultManager] removeItemAtPath:lockPath error:NULL];

  // A fresh claim lists this process and only this process.
  ChiaKeyClaimUserPhraseEditingLock(dir);
  CHECK(ChiaKeyUserPhraseEditingLockIsActive(dir));
  CHECK([LockContents(dir) isEqualToString:me]);

  // Claiming again does not duplicate the entry.
  ChiaKeyClaimUserPhraseEditingLock(dir);
  CHECK([LockContents(dir) isEqualToString:me]);

  // Another live claimant (PID 1 always exists; EPERM still counts as alive)
  // keeps the lock through this session's release.
  WriteLock(dir, [NSString stringWithFormat:@"1\n%@", me]);
  CHECK(!ChiaKeyReleaseUserPhraseEditingLockIfOwner(dir));
  CHECK(ChiaKeyUserPhraseEditingLockIsActive(dir));
  CHECK([LockContents(dir) isEqualToString:@"1"]);

  // A dead claimant is dropped, so the last live session removes the file.
  WriteLock(dir, [NSString stringWithFormat:@"999999\n%@", me]);
  CHECK(ChiaKeyReleaseUserPhraseEditingLockIfOwner(dir));
  CHECK(![[NSFileManager defaultManager] fileExistsAtPath:lockPath]);

  // A refresh puts a raced-out session back on the list.
  WriteLock(dir, @"1");
  ChiaKeyRefreshUserPhraseEditingLock(dir);
  CHECK(([LockContents(dir)
      isEqualToString:[NSString stringWithFormat:@"1\n%@", me]]));
  [[NSFileManager defaultManager] removeItemAtPath:lockPath error:NULL];

  // An old single-PID file (a pre-multi-owner build's) reads as one entry.
  WriteLock(dir, @"1");
  ChiaKeyClaimUserPhraseEditingLock(dir);
  CHECK(([LockContents(dir)
      isEqualToString:[NSString stringWithFormat:@"1\n%@", me]]));
  [[NSFileManager defaultManager] removeItemAtPath:lockPath error:NULL];

  // The owner list is rewritten under an advisory lock, so no second claimant
  // can interleave its own read-modify-write. flock() is held per open file
  // description, so a second descriptor here stands in for another process.
  NSString *guardPath = [lockPath stringByAppendingPathExtension:@"lock"];
  {
    int guard = ChiaKeyLockEditingOwnerList(dir);
    CHECK(guard >= 0);

    int contender = open([guardPath fileSystemRepresentation], O_RDONLY);
    CHECK(contender >= 0);
    CHECK(flock(contender, LOCK_EX | LOCK_NB) != 0);
    CHECK(errno == EWOULDBLOCK);
    close(contender);

    ChiaKeyUnlockEditingOwnerList(guard);
  }

  // And every operation gives it back: a leaked guard would suspend the IME's
  // writes for as long as the process lived.
  ChiaKeyClaimUserPhraseEditingLock(dir);
  ChiaKeyRefreshUserPhraseEditingLock(dir);
  CHECK(ChiaKeyReleaseUserPhraseEditingLockIfOwner(dir));
  {
    int contender = open([guardPath fileSystemRepresentation], O_RDONLY);
    CHECK(contender >= 0);
    CHECK(flock(contender, LOCK_EX | LOCK_NB) == 0);
    flock(contender, LOCK_UN);
    close(contender);
  }
  [[NSFileManager defaultManager] removeItemAtPath:lockPath error:NULL];
  [[NSFileManager defaultManager] removeItemAtPath:guardPath error:NULL];

  if (failures) {
    fprintf(stderr, "TestEditingLockCoordination: %d check(s) failed\n",
            failures);
  } else {
    printf("TestEditingLockCoordination: OK\n");
  }

  [pool drain];
  return failures ? 1 : 0;
}
