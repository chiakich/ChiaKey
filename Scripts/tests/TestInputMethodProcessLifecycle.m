#import "ChiaKeyInputMethodProcessLifecycle.h"

static void Check(BOOL condition, NSString *message) {
  if (!condition) {
    NSLog(@"FAIL: %@", message);
    exit(1);
  }
}

@interface TestLockHolder : NSObject
@end

@implementation TestLockHolder
// Takes `lock` and never gives it back, signalling `held` once it has it.
+ (void)_testHoldLock:(NSArray *)arguments {
  NSLock *lock = [arguments objectAtIndex:0];
  NSConditionLock *held = [arguments objectAtIndex:1];

  [lock lock];
  [held lock];
  [held unlockWithCondition:1];
  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate distantFuture]];
}
@end

// Every delay the schedule hands out, for a run that never succeeds.
static NSArray *RecordedDelaysForFailingCreate(int *attemptsOut) {
  NSMutableArray *delays = [NSMutableArray array];
  __block int attempts = 0;
  id created = ChiaKeyCreateWithServerLaunchRetry(
      ^id(void) {
        attempts++;
        return nil;
      },
      ^(NSTimeInterval delay) {
        [delays addObject:[NSNumber numberWithDouble:delay]];
      });

  Check(created == nil, @"a create that never succeeds answers nil");
  if (attemptsOut) *attemptsOut = attempts;
  return delays;
}

static void TestServerLaunchRetrySchedule(void) {
  int attempts = 0;
  NSArray *delays = RecordedDelaysForFailingCreate(&attempts);

  Check(attempts == kChiaKeyServerLaunchAttemptLimit,
        @"the schedule tries exactly as many times as it advertises");
  // The first attempt is immediate, so there is one fewer wait than attempt.
  Check([delays count] == (NSUInteger)(kChiaKeyServerLaunchAttemptLimit - 1),
        @"the first attempt does not wait");

  NSTimeInterval waited = 0.0;
  NSTimeInterval previous = 0.0;
  for (NSNumber *delay in delays) {
    NSTimeInterval value = [delay doubleValue];
    Check(value > 0.0, @"every retry waits");
    if (previous > 0.0)
      Check(value > previous, @"the backoff grows instead of hammering");
    previous = value;
    waited += value;
  }

  Check(fabs(waited - ChiaKeyServerLaunchRetryBudget()) < 0.0001,
        @"the advertised budget is what the schedule actually spends");
  // A launch that looks hung is as bad as one that gives up: the whole
  // schedule has to stay inside the few seconds macOS gives a starting input
  // method before the user reaches for the input menu again.
  Check(ChiaKeyServerLaunchRetryBudget() > 0.5 &&
            ChiaKeyServerLaunchRetryBudget() <= 5.0,
        @"the budget waits out a handover without looking hung");
}

static void TestServerLaunchRetrySucceedsMidSchedule(void) {
  __block int attempts = 0;
  __block int waits = 0;
  // Stands in for the previous instance releasing the connection name on the
  // third try, which is the whole reason this retry exists.
  id created = ChiaKeyCreateWithServerLaunchRetry(
      ^id(void) {
        attempts++;
        return attempts < 3 ? nil : @"server";
      },
      ^(NSTimeInterval delay) {
        (void)delay;
        waits++;
      });

  Check([created isEqual:@"server"], @"a late success is returned");
  Check(attempts == 3, @"no attempt is made after the first success");
  Check(waits == 2, @"no time is spent after the first success");
}

static void TestServerLaunchFailureMessageNamesTheHolder(void) {
  NSString *message = ChiaKeyServerLaunchFailureMessage(
      @"com.chiakey.inputmethod.ChiaKey_Connection",
      @"/Users/someone/Library/Input Methods/ChiaKey.app",
      [NSArray arrayWithObject:@"pid 4242 (/Users/someone/Library/Input "
                               @"Methods/ChiaKey.app/Contents/MacOS/ChiaKey)"]);

  Check([message rangeOfString:@"com.chiakey.inputmethod.ChiaKey_Connection"]
                .location != NSNotFound,
        @"the failure names the connection name that was refused");
  Check([message rangeOfString:@"pid 4242"].location != NSNotFound,
        @"the failure names the pid still holding it");
  Check([message rangeOfString:@"Contents/MacOS/ChiaKey"].location != NSNotFound,
        @"the failure names the executable still holding it");
  Check([message rangeOfString:@"Library/Input Methods/ChiaKey.app"].location !=
            NSNotFound,
        @"the failure names the bundle that was trying to start");

  NSString *withoutHolder = ChiaKeyServerLaunchFailureMessage(
      @"com.chiakey.inputmethod.ChiaKey_Connection", nil, [NSArray array]);
  Check([withoutHolder length] > 0,
        @"the failure is still reportable with nothing found");
  Check([withoutHolder rangeOfString:@"none found"].location != NSNotFound,
        @"the failure says so when no other instance is left");
}

// The point of the whole change: teardown must give up on the lock rather than
// wait to be killed.
static void TestShutDownLockTimeoutGivesUp(void) {
  NSLock *lock = [[[NSLock alloc] init] autorelease];

  Check(ChiaKeyTakeLockBeforeTimeout(lock, kChiaKeyLoaderShutDownLockTimeout),
        @"an idle lock is taken");
  [lock unlock];

  NSConditionLock *held = [[[NSConditionLock alloc] initWithCondition:0]
      autorelease];
  // Stands in for the boot thread, which holds the shared lock for the whole
  // load.
  [NSThread detachNewThreadSelector:@selector(_testHoldLock:)
                          toTarget:[TestLockHolder class]
                        withObject:[NSArray arrayWithObjects:lock, held, nil]];
  [held lockWhenCondition:1];
  [held unlock];

  // Absolute, not relative to the constant: a teardown that waits longer than
  // this is one macOS kills, and a killed process holds the connection name
  // long enough to make the next launch the one that gives up.
  Check(kChiaKeyLoaderShutDownLockTimeout <= 2.0,
        @"teardown's patience stays well inside what macOS allows");

  NSDate *start = [NSDate date];
  Check(!ChiaKeyTakeLockBeforeTimeout(lock, kChiaKeyLoaderShutDownLockTimeout),
        @"a held lock is given up on instead of waited out");
  Check(-[start timeIntervalSinceNow] < 3.0,
        @"giving up takes about the timeout, not however long the holder runs");

  Check(!ChiaKeyTakeLockBeforeTimeout(nil, 0.1),
        @"no lock at all is the same answer as a held one");
}

static void TestReloadQueueRunsOneAtATime(void) {
  ChiaKeyReloadQueue queue;
  memset(&queue, 0, sizeof(queue));

  BOOL settle = NO;
  Check(ChiaKeyReloadQueueRequest(&queue, NO, &settle),
        @"the first request starts a reload");
  Check(!settle, @"a plain reload does not settle");

  BOOL ignored = YES;
  Check(!ChiaKeyReloadQueueRequest(&queue, NO, &ignored),
        @"a second request does not start a second reload");

  ChiaKeyReloadQueueDidFinish(&queue);
  BOOL pendingSettle = YES;
  Check(ChiaKeyReloadQueueTakePending(&queue, &pendingSettle),
        @"the request made meanwhile is not lost");
  Check(!pendingSettle, @"it keeps its own settle flag");
  Check(!ChiaKeyReloadQueueTakePending(&queue, &pendingSettle),
        @"and is handed out only once");
}

static void TestReloadQueueKeepsTheSettleFlag(void) {
  ChiaKeyReloadQueue queue;
  memset(&queue, 0, sizeof(queue));

  BOOL settle = NO;
  Check(ChiaKeyReloadQueueRequest(&queue, YES, &settle),
        @"an install's reload starts");
  Check(settle, @"and carries its settle flag");

  // A lexicon install folded into a plain reload still has to be judged
  // afterwards, or the installed database is neither retired nor rolled back.
  BOOL ignored = NO;
  Check(!ChiaKeyReloadQueueRequest(&queue, YES, &ignored),
        @"an install arriving mid-reload folds in");
  ChiaKeyReloadQueueDidFinish(&queue);

  BOOL pendingSettle = NO;
  Check(ChiaKeyReloadQueueTakePending(&queue, &pendingSettle),
        @"the folded request follows");
  Check(pendingSettle, @"with its settle flag intact");
}

static void TestReloadQueueDeferral(void) {
  ChiaKeyReloadQueue queue;
  memset(&queue, 0, sizeof(queue));

  // Boot owns the shared lock for the whole load, so a request arriving then
  // cannot start at all; it has to survive until boot reports ready.
  ChiaKeyReloadQueueDefer(&queue, NO);
  ChiaKeyReloadQueueDefer(&queue, YES);

  BOOL settle = NO;
  Check(ChiaKeyReloadQueueTakePending(&queue, &settle),
        @"a deferred request waits to be taken");
  Check(settle, @"and any one of the deferred requests wanting settle wins");

  Check(ChiaKeyReloadQueueRequest(&queue, NO, &settle),
        @"a deferred request does not block the reload it later starts");
}

int main(void) {
  @autoreleasepool {
    TestServerLaunchRetrySchedule();
    TestServerLaunchRetrySucceedsMidSchedule();
    TestServerLaunchFailureMessageNamesTheHolder();
    TestShutDownLockTimeoutGivesUp();
    TestReloadQueueRunsOneAtATime();
    TestReloadQueueKeepsTheSettleFlag();
    TestReloadQueueDeferral();
    NSLog(@"PASS: input method process lifecycle");
  }
  return 0;
}
