//
//  ChiaKeyInputMethodProcessLifecycle.h
//
//  The rules that keep this process from spending the handful of input-method
//  restarts macOS allows per login session. Once that budget is gone the whole
//  session refuses to select this input method again -- it greys out in the
//  input menu until the user logs out -- so every needless "launch, exit
//  immediately" and every teardown that has to be killed costs the user real
//  availability (see chiakich/ChiaKey#23).
//
//  Three places spend the budget, and each has its rule here:
//  - LAUNCH. IMKServer refuses the connection name while the previous
//    instance still owns it, and a process that gives up there has launched
//    and exited for nothing. ChiaKeyCreateWithServerLaunchRetry waits out that
//    handover instead, and the failure message names whoever still holds it.
//  - TEARDOWN. Waiting for the shared lock at exit hangs whenever boot or a
//    reload holds it, and the kill that follows holds the connection name even
//    longer -- which is what makes the NEXT launch exit immediately.
//    ChiaKeyTakeLockBeforeTimeout gives up instead.
//  - RELOAD. A reload takes seconds, and doing it on the thread IMK talks to
//    makes the input method look hung. ChiaKeyReloadQueue is the bookkeeping
//    for running it elsewhere without losing or overlapping requests.
//
//  Header-only and free of C++ and of the loader itself, so the policy can be
//  tested on its own (Scripts/test-input-method-process-lifecycle.sh).
//

#ifndef CHIAKEY_INPUT_METHOD_PROCESS_LIFECYCLE_H_
#define CHIAKEY_INPUT_METHOD_PROCESS_LIFECYCLE_H_

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#include <unistd.h>

#pragma mark Launch: waiting out the previous instance

// Small on purpose: a launch that looks hung is worse than one that gives up,
// and the handover this waits for takes milliseconds once the old instance
// starts exiting. Four retries over the budget below.
#define kChiaKeyServerLaunchAttemptLimit 5
#define kChiaKeyServerLaunchFirstRetryDelay 0.2

// Delay before attempt `attempt` (0-based), doubling each time.
static inline NSTimeInterval ChiaKeyServerLaunchRetryDelay(int attempt) {
  if (attempt <= 0) return 0.0;
  NSTimeInterval delay = kChiaKeyServerLaunchFirstRetryDelay;
  for (int i = 1; i < attempt; i++) delay *= 2.0;
  return delay;
}

// Everything the schedule can spend waiting, worst case.
static inline NSTimeInterval ChiaKeyServerLaunchRetryBudget(void) {
  NSTimeInterval total = 0.0;
  for (int i = 0; i < kChiaKeyServerLaunchAttemptLimit; i++)
    total += ChiaKeyServerLaunchRetryDelay(i);
  return total;
}

// Runs `create` until it answers non-nil or the schedule runs out. `wait` is
// the sleep, injected so a test can run the schedule without real time; pass
// nil for the real one.
static inline id ChiaKeyCreateWithServerLaunchRetry(
    id (^create)(void), void (^wait)(NSTimeInterval)) {
  for (int attempt = 0; attempt < kChiaKeyServerLaunchAttemptLimit; attempt++) {
    NSTimeInterval delay = ChiaKeyServerLaunchRetryDelay(attempt);
    if (delay > 0.0) {
      if (wait) {
        wait(delay);
      } else {
        usleep((useconds_t)(delay * 1000000.0));
      }
    }

    id created = create();
    if (created) return created;
  }

  return nil;
}

// "pid 123 (/path/to/ChiaKey.app)" for every other process running this same
// input method bundle -- the only candidates for holding our connection name,
// since the name is derived from the bundle. Empty when none is left, which
// points the blame somewhere else entirely.
static inline NSArray *ChiaKeyOtherRunningInstanceDescriptions(void) {
  NSMutableArray *descriptions = [NSMutableArray array];
  NSString *bundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];
  if (![bundleIdentifier length]) return descriptions;

  pid_t ownPID = [[NSProcessInfo processInfo] processIdentifier];
  NSArray *instances = [NSRunningApplication
      runningApplicationsWithBundleIdentifier:bundleIdentifier];
  for (NSRunningApplication *instance in instances) {
    if ([instance processIdentifier] == ownPID) continue;
    NSString *path = [[instance executableURL] path];
    [descriptions
        addObject:[NSString stringWithFormat:@"pid %d (%@)",
                                             (int)[instance processIdentifier],
                                             path ? path : @"unknown path"]];
  }

  return descriptions;
}

// The one log line a launch that never got the connection name leaves behind.
// Written to be pasteable into a bug report on its own: who held the name, and
// which bundle was trying to take it.
static inline NSString *ChiaKeyServerLaunchFailureMessage(
    NSString *connectionName, NSString *ownBundlePath, NSArray *otherInstances) {
  NSString *holders =
      [otherInstances count]
          ? [otherInstances componentsJoinedByString:@", "]
          : @"none found (the name may be held by a process that has already "
             "left the application list)";

  return [NSString
      stringWithFormat:@"ChiaKey: IMKServer could not take the connection name "
                       @"%@ after %.1fs; this bundle: %@; other instances of "
                       @"it: %@. The input method cannot start; log out and "
                       @"back in if it stays greyed out in the input menu.",
                       connectionName, ChiaKeyServerLaunchRetryBudget(),
                       ownBundlePath ? ownBundlePath : @"unknown",
                       holders];
}

#pragma mark Teardown: never waiting forever for the loader

// Long enough to win a lock someone is only passing through (a context being
// built), far too short to sit out a boot or a reload -- which is the point.
#define kChiaKeyLoaderShutDownLockTimeout 0.5

// NO when the lock is still held at the deadline, and the caller must then do
// without it rather than wait.
static inline BOOL ChiaKeyTakeLockBeforeTimeout(NSLock *lock,
                                                NSTimeInterval timeout) {
  if (!lock) return NO;
  return [lock lockBeforeDate:[NSDate dateWithTimeIntervalSinceNow:timeout]];
}

#pragma mark Reload: one at a time, none lost

// A reload runs elsewhere than the thread that asked for it, so requests can
// arrive while one is in flight (the Preferences app, a lexicon install and
// the hourly check all ask). At most one may run -- they all take the loader
// apart -- and none may be dropped, since each one answers for something that
// changed on disk. `settle` travels with the request because only an install
// has a lexicon state to judge afterwards.
typedef struct {
  BOOL running;
  BOOL pending;
  BOOL pendingSettle;
} ChiaKeyReloadQueue;

// YES when the caller should start a reload now, with *settleNow. NO when the
// request was folded into the one already running and will follow it.
static inline BOOL ChiaKeyReloadQueueRequest(ChiaKeyReloadQueue *queue,
                                             BOOL settle, BOOL *settleNow) {
  if (!queue) return NO;

  if (queue->running) {
    queue->pending = YES;
    if (settle) queue->pendingSettle = YES;
    return NO;
  }

  queue->running = YES;
  if (settleNow) *settleNow = settle;
  return YES;
}

// Remembers a request the caller could not start at all (loader boot still
// owns the lock); ChiaKeyReloadQueueTakePending hands it back later.
static inline void ChiaKeyReloadQueueDefer(ChiaKeyReloadQueue *queue,
                                           BOOL settle) {
  if (!queue) return;
  queue->pending = YES;
  if (settle) queue->pendingSettle = YES;
}

static inline void ChiaKeyReloadQueueDidFinish(ChiaKeyReloadQueue *queue) {
  if (queue) queue->running = NO;
}

// YES when a request is waiting, with *settle; it is cleared, so a caller that
// does not act on it loses it.
static inline BOOL ChiaKeyReloadQueueTakePending(ChiaKeyReloadQueue *queue,
                                                 BOOL *settle) {
  if (!queue || !queue->pending) return NO;

  queue->pending = NO;
  if (settle) *settle = queue->pendingSettle;
  queue->pendingSettle = NO;
  return YES;
}

#endif  // CHIAKEY_INPUT_METHOD_PROCESS_LIFECYCLE_H_
