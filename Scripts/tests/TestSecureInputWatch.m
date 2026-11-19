#import "ChiaKeySecureInputWatch.h"

static void Check(BOOL condition, NSString *message) {
  if (!condition) {
    NSLog(@"FAIL: %@", message);
    exit(1);
  }
}

static NSString *const kLoginwindowPath =
    @"/System/Library/CoreServices/loginwindow.app/Contents/MacOS/loginwindow";

static ChiaKeySecureInputSession Session(pid_t holder, BOOL locked,
                                         BOOL onConsole) {
  ChiaKeySecureInputSession session = {holder, locked, onConsole};
  return session;
}

static void TestSessionDictionaryParsing(void) {
  ChiaKeySecureInputSession empty =
      ChiaKeySecureInputSessionFromDictionary([NSDictionary dictionary]);
  Check(empty.holderPID == 0, @"no secure input key means nobody holds it");
  Check(!empty.screenLocked, @"no lock key means unlocked");
  Check(!empty.onConsole, @"no console key means not on console");

  NSDictionary *held = [NSDictionary
      dictionaryWithObjectsAndKeys:[NSNumber numberWithInt:1633],
                                   @"kCGSSessionSecureInputPID",
                                   [NSNumber numberWithBool:YES],
                                   @"kCGSSessionOnConsoleKey", nil];
  ChiaKeySecureInputSession parsed =
      ChiaKeySecureInputSessionFromDictionary(held);
  Check(parsed.holderPID == 1633, @"the holder pid is read");
  Check(parsed.onConsole, @"the console flag is read");

  NSArray *lockKeys = [NSArray arrayWithObjects:@"CGSSessionScreenIsLocked",
                                                @"kCGSSessionScreenIsLocked",
                                                nil];
  for (NSString *key in lockKeys) {
    NSDictionary *locked = [NSDictionary
        dictionaryWithObject:[NSNumber numberWithBool:YES]
                      forKey:key];
    Check(ChiaKeySecureInputSessionFromDictionary(locked).screenLocked,
          [NSString stringWithFormat:@"%@ marks the screen locked", key]);
  }

  NSDictionary *junk = [NSDictionary
      dictionaryWithObject:@"1633"
                    forKey:@"kCGSSessionSecureInputPID"];
  Check(ChiaKeySecureInputSessionFromDictionary(junk).holderPID == 0,
        @"a non-number pid is ignored");
}

static void TestStuckDetection(void) {
  Check(ChiaKeySecureInputIsStuck(Session(1633, NO, YES), kLoginwindowPath),
        @"loginwindow holding it in an unlocked session is stuck");
  Check(!ChiaKeySecureInputIsStuck(Session(1633, YES, YES), kLoginwindowPath),
        @"loginwindow holding it behind the lock screen is expected");
  Check(!ChiaKeySecureInputIsStuck(Session(1633, NO, NO), kLoginwindowPath),
        @"a session switched away from the console is not ours to judge");
  Check(!ChiaKeySecureInputIsStuck(Session(0, NO, YES), nil),
        @"nobody holding it is not stuck");
  Check(!ChiaKeySecureInputIsStuck(
            Session(5290, NO, YES),
            @"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"),
        @"an app's password field is the app's business, not a stuck session");
  Check(!ChiaKeySecureInputIsStuck(Session(1633, NO, YES), nil),
        @"an unreadable holder path is not assumed to be loginwindow");
}

static void TestWarningShowsOnlyAfterTwoLooks(void) {
  ChiaKeySecureInputWatchPhase phase = ChiaKeySecureInputWatchIdle;
  Check(ChiaKeySecureInputWatchAdvance(&phase, YES) ==
            ChiaKeySecureInputWatchScheduleConfirm,
        @"the first stuck look only schedules a confirmation");
  Check(ChiaKeySecureInputWatchAdvance(&phase, YES) ==
            ChiaKeySecureInputWatchShowWarning,
        @"the confirming look shows the warning");
  Check(ChiaKeySecureInputWatchAdvance(&phase, YES) ==
            ChiaKeySecureInputWatchNone,
        @"the warning shows once per stuck episode");

  ChiaKeySecureInputWatchRestart(&phase);
  Check(phase == ChiaKeySecureInputWatchWarned,
        @"an unlock does not re-arm a warning the user already saw");
  Check(ChiaKeySecureInputWatchAdvance(&phase, YES) ==
            ChiaKeySecureInputWatchNone,
        @"so the next unlock does not nag again");

  Check(ChiaKeySecureInputWatchAdvance(&phase, NO) ==
            ChiaKeySecureInputWatchHideWarning,
        @"the hold clearing takes the warning down");
  Check(phase == ChiaKeySecureInputWatchIdle,
        @"and starts a fresh episode");
}

static void TestTransientHoldNeverWarns(void) {
  ChiaKeySecureInputWatchPhase phase = ChiaKeySecureInputWatchIdle;
  ChiaKeySecureInputWatchAdvance(&phase, YES);
  Check(ChiaKeySecureInputWatchAdvance(&phase, NO) ==
            ChiaKeySecureInputWatchNone,
        @"a hold gone by the confirming look raises nothing");
  Check(phase == ChiaKeySecureInputWatchIdle, @"and leaves no suspicion");

  ChiaKeySecureInputWatchAdvance(&phase, YES);
  ChiaKeySecureInputWatchRestart(&phase);
  Check(phase == ChiaKeySecureInputWatchIdle,
        @"a lock between the two looks discards the suspicion");
  Check(ChiaKeySecureInputWatchAdvance(&phase, YES) ==
            ChiaKeySecureInputWatchScheduleConfirm,
        @"so the session after it needs its own two looks");
}

static void TestLogoutResult(void) {
  Check(ChiaKeyLogoutRequestSucceeded(0, @""), @"status 0 is success");
  Check(ChiaKeyLogoutRequestSucceeded(
            1, @"execution error: User canceled. (-128)\n"),
        @"cancelling the system dialog is not a failure");
  Check(!ChiaKeyLogoutRequestSucceeded(
            1, @"execution error: Not authorized to send Apple events to "
               @"loginwindow. (-1743)\n"),
        @"a refused Apple Event is a failure");
  Check(!ChiaKeyLogoutRequestSucceeded(1, nil), @"no output is a failure");
}

static void TestHeldByAppExcludesLoginwindowAndLockedSessions(void) {
  Check(ChiaKeySecureInputIsHeldByApp(Session(14171, NO, YES), @"/Apps/Chrome"),
        @"an app holding it counts");
  Check(!ChiaKeySecureInputIsHeldByApp(Session(0, NO, YES), nil),
        @"no holder does not count");
  Check(!ChiaKeySecureInputIsHeldByApp(Session(14171, YES, YES), @"/Apps/Chrome"),
        @"a locked screen does not count");
  Check(!ChiaKeySecureInputIsHeldByApp(Session(14171, NO, NO), @"/Apps/Chrome"),
        @"another console user does not count");
  Check(!ChiaKeySecureInputIsHeldByApp(Session(1633, NO, YES), kLoginwindowPath),
        @"loginwindow is left to the warning panel");
}

static ChiaKeySecureInputLeakAction LeakStep(ChiaKeySecureInputLeakWatch *watch,
                                             pid_t front,
                                             NSTimeInterval keyAt) {
  return ChiaKeySecureInputLeakAdvance(watch, YES, front, keyAt);
}

static void TestLeakNotifiesOnlyWhenTypingAwayFromTheOrigin(void) {
  ChiaKeySecureInputLeakWatch watch = {0, 0, NO, 0};
  pid_t chrome = 100, finder = 200, notes = 300;

  Check(LeakStep(&watch, chrome, 10) == ChiaKeySecureInputLeakContinue,
        @"the first look records the origin");
  Check(watch.originPID == chrome, @"the origin is the frontmost app");
  Check(LeakStep(&watch, chrome, 50) == ChiaKeySecureInputLeakContinue,
        @"typing in the origin is the normal password case");

  // Cmd+Tab lands in the clock after the first look away.
  Check(LeakStep(&watch, finder, 50) == ChiaKeySecureInputLeakContinue,
        @"the first look away only counts");
  Check(LeakStep(&watch, finder, 60) == ChiaKeySecureInputLeakContinue,
        @"the second look away takes the baseline, absorbing Cmd+Tab");
  Check(LeakStep(&watch, notes, 60) == ChiaKeySecureInputLeakContinue,
        @"switching with no new key stays quiet");
  Check(LeakStep(&watch, notes, 60.05) == ChiaKeySecureInputLeakContinue,
        @"clock noise is not typing");
  Check(LeakStep(&watch, notes, 62) == ChiaKeySecureInputLeakNotify,
        @"a new key after the baseline notifies");
  Check(watch.originPID == chrome, @"the origin survives further switches");

  Check(LeakStep(&watch, finder, 70) == ChiaKeySecureInputLeakContinue,
        @"the hint shows once per hold");
  Check(LeakStep(&watch, chrome, 80) == ChiaKeySecureInputLeakContinue,
        @"returning to the origin is quiet");
  LeakStep(&watch, finder, 80);
  LeakStep(&watch, finder, 80);
  Check(LeakStep(&watch, finder, 90) == ChiaKeySecureInputLeakContinue,
        @"typing after leaving again in the same hold stays quiet");
}

static void TestLeakStaysQuietWithoutTyping(void) {
  ChiaKeySecureInputLeakWatch watch = {0, 0, NO, 0};
  LeakStep(&watch, 100, 10);
  for (int i = 0; i < 20; i++) {
    Check(LeakStep(&watch, 200, 10) == ChiaKeySecureInputLeakContinue,
          @"sitting in another app without typing never notifies");
  }
}

static void TestLeakWatchEndsWithTheHold(void) {
  ChiaKeySecureInputLeakWatch watch = {100, 3, YES, 42};
  Check(ChiaKeySecureInputLeakAdvance(&watch, NO, 200, 50) ==
            ChiaKeySecureInputLeakStop,
        @"polling stops when the hold ends");
  Check(watch.originPID == 0 && watch.awayChecks == 0 && !watch.notified &&
            watch.keyBaseline == 0,
        @"the next hold starts clean");
  Check(ChiaKeySecureInputLeakAdvance(&watch, NO, 200, 50) ==
            ChiaKeySecureInputLeakStop,
        @"no hold never polls");
}

static void TestLeakWatchIgnoresUnknownFrontApp(void) {
  ChiaKeySecureInputLeakWatch watch = {0, 0, NO, 0};
  Check(LeakStep(&watch, 0, 10) == ChiaKeySecureInputLeakContinue,
        @"an unknown front app keeps watching");
  Check(watch.originPID == 0, @"an unknown front app is never the origin");
}

static void TestReturningToTheOriginRetakesTheBaseline(void) {
  ChiaKeySecureInputLeakWatch watch = {0, 0, NO, 0};
  LeakStep(&watch, 100, 10);
  LeakStep(&watch, 200, 10);
  LeakStep(&watch, 200, 10);
  LeakStep(&watch, 100, 30);
  LeakStep(&watch, 200, 30);
  Check(LeakStep(&watch, 200, 40) == ChiaKeySecureInputLeakContinue,
        @"keys typed before leaving again do not count as typing away");
  Check(LeakStep(&watch, 200, 40) == ChiaKeySecureInputLeakContinue,
        @"the new baseline holds");
}

int main(void) {
  @autoreleasepool {
    TestHeldByAppExcludesLoginwindowAndLockedSessions();
    TestLeakNotifiesOnlyWhenTypingAwayFromTheOrigin();
    TestLeakStaysQuietWithoutTyping();
    TestLeakWatchEndsWithTheHold();
    TestLeakWatchIgnoresUnknownFrontApp();
    TestReturningToTheOriginRetakesTheBaseline();
    TestSessionDictionaryParsing();
    TestStuckDetection();
    TestWarningShowsOnlyAfterTwoLooks();
    TestTransientHoldNeverWarns();
    TestLogoutResult();
    NSLog(@"secure input watch: all checks passed");
  }
  return 0;
}
