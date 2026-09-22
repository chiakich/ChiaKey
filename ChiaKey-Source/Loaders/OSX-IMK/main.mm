#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <InputMethodKit/InputMethodKit.h>
#include <string>

#import "BPMFUserPhraseHelper.h"
#import "CVApplicationController.h"
#import "ChiaKeyServiceCoordination.h"
#import "ChiaKeyUserPhraseCoordination.h"
#import "OpenVanillaConfig.h"
#import "OpenVanillaController.h"
#import "OpenVanillaLoader.h"

using namespace std;

// CLI import/export operate on the user phrase DB directly (the old XPC
// channel is gone); a running IME is coordinated via the editing-lock and
// dirty-flag files, same as the Phrase Editor.
static OVSQLiteConnection *ChiaKeyOpenUserPhraseDBForCLI() {
  NSString *dir = ChiaKeyServiceUserDataDirectory();
  ChiaKeyEnsureUserDataDirectoryPrivate();
  NSString *path =
      [dir stringByAppendingPathComponent:@"SmartMandarinUserData.db"];
  OVSQLiteConnection *db = OVSQLiteConnection::Open([path UTF8String]);
  if (!db) return 0;
  // Keep CLI access compatible with the editor and running IME, even when
  // this is the first process to create or open the user phrase database.
  db->execute("PRAGMA journal_mode=WAL");

  if (!db->hasTable("user_unigrams")) {
    db->createTable("user_unigrams", "qstring, current, probability, backoff");
    db->createIndexOnTable("user_unigrams_index", "user_unigrams", "qstring");
  }
  if (!db->hasTable("user_bigram_cache")) {
    db->createTable("user_bigram_cache",
                    "qstring, previous, current, probability");
    db->createIndexOnTable("user_bigram_cache_index", "user_bigram_cache",
                           "qstring");
  }
  if (!db->hasTable("user_candidate_override_cache")) {
    db->createTable("user_candidate_override_cache", "qstring, current");
    db->createIndexOnTable("user_candidate_override_cache_index",
                           "user_candidate_override_cache", "qstring");
  }
  return db;
}

IMKServer *OVInputMethodServer = nil;

static TISInputSourceRef ChiaKeyCreateInputSourceForID(NSString *inputSourceID) {
  NSDictionary *properties = @{
    (NSString *)kTISPropertyInputSourceID : inputSourceID,
  };
  CFArrayRef sources =
      TISCreateInputSourceList((CFDictionaryRef)properties, true);
  if (!sources || CFArrayGetCount(sources) == 0) {
    if (sources) {
      CFRelease(sources);
    }
    return nil;
  }

  TISInputSourceRef source =
      (TISInputSourceRef)CFRetain(CFArrayGetValueAtIndex(sources, 0));
  CFRelease(sources);
  return source;
}

static BOOL ChiaKeyEnableInputSourceWithID(NSString *inputSourceID) {
  TISInputSourceRef source = ChiaKeyCreateInputSourceForID(inputSourceID);
  if (!source) {
    NSLog(@"could not find input source %@", inputSourceID);
    return NO;
  }

  OSStatus enableStatus = TISEnableInputSource(source);
  CFRelease(source);
  if (enableStatus != noErr) {
    NSLog(@"failed to enable input source %@: %d", inputSourceID, enableStatus);
    return NO;
  }
  return YES;
}

static BOOL ChiaKeyDisableInputSourceWithID(NSString *inputSourceID) {
  TISInputSourceRef source = ChiaKeyCreateInputSourceForID(inputSourceID);
  if (!source) {
    // Never registered or already removed; nothing to disable.
    return YES;
  }

  OSStatus disableStatus = TISDisableInputSource(source);
  CFRelease(source);
  if (disableStatus != noErr) {
    NSLog(@"failed to disable input source %@: %d", inputSourceID,
          disableStatus);
    return NO;
  }
  return YES;
}

// Ask the list the input menu is built from, not the source's own flag.
// A filtered TISCreateInputSourceList hands back a source whether or not it is
// enabled, and kTISPropertyInputSourceIsEnabled then lies about input *modes*:
// it reads YES for our mode while the mode is absent from the enabled list and
// the input menu does not offer it. Believing that flag made "install" report
// already-enabled and skip the enable entirely, which is exactly the state a
// user with no ChiaKey in their menu needs us to repair. Enumerating the
// enabled-only list (no filter dictionary, includeAllInstalled NO) agrees with
// the menu and with AppleEnabledInputSources, so enumerate and match the ID.
static BOOL ChiaKeyInputSourceIsEnabled(NSString *inputSourceID) {
  CFArrayRef sources = TISCreateInputSourceList(NULL, false);
  if (!sources) return NO;

  BOOL isEnabled = NO;
  for (CFIndex i = 0; i < CFArrayGetCount(sources); i++) {
    TISInputSourceRef source =
        (TISInputSourceRef)CFArrayGetValueAtIndex(sources, i);
    NSString *sourceID =
        (NSString *)TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
    if ([sourceID isKindOfClass:[NSString class]] &&
        [sourceID isEqualToString:inputSourceID]) {
      isEnabled = YES;
      break;
    }
  }
  CFRelease(sources);
  return isEnabled;
}

static NSString *ChiaKeyInputSourceID() {
  NSBundle *mainBundle = [NSBundle mainBundle];
  NSString *inputSourceID =
      [mainBundle objectForInfoDictionaryKey:@"TISInputSourceID"];
  if (![inputSourceID length]) {
    inputSourceID = [mainBundle bundleIdentifier];
  }
  return inputSourceID;
}

// The parent identifies the bundle; the visible mode is the selectable source.
// Read the mode from the plist so the dev bundle can use its own identity.
static NSString *ChiaKeyInputModeID() {
  NSDictionary *modes = [[NSBundle mainBundle]
      objectForInfoDictionaryKey:@"ComponentInputModeDict"];
  NSString *modeID =
      [[modes objectForKey:@"tsVisibleInputModeOrderedArrayKey"] firstObject];
  return modeID ?: ChiaKeyInputSourceID();
}

// The bundle a registered input source actually lives in. TIS exposes no
// bundle URL, but it reports the icon as a URL relative to that bundle, so the
// base URL is the bundle. Nil when it cannot be determined -- the caller then
// re-registers, which is the safe answer.
static NSString *ChiaKeyRegisteredBundlePath(TISInputSourceRef source) {
  NSURL *iconURL =
      (NSURL *)TISGetInputSourceProperty(source, kTISPropertyIconImageURL);
  if (![iconURL isKindOfClass:[NSURL class]]) return nil;

  NSURL *bundleURL = [iconURL baseURL];
  if (!bundleURL) {
    // Absolute icon URL: walk up to the enclosing .app instead.
    bundleURL = [iconURL URLByDeletingLastPathComponent];
    while ([[bundleURL path] length] > 1 &&
           ![[bundleURL pathExtension] isEqualToString:@"app"]) {
      bundleURL = [bundleURL URLByDeletingLastPathComponent];
    }
    if (![[bundleURL pathExtension] isEqualToString:@"app"]) return nil;
  }

  NSString *path = [[bundleURL path] stringByStandardizingPath];
  return [path length] ? path : nil;
}

// The enable turns into a system consent dialog; the flag only flips once the
// user approves. Measured on a real first install, an approval lands in about
// three seconds and the enabled list reflects it at once, so this budget buys
// nothing for the case that works -- it is spent entirely on someone who is
// not answering the dialog, staring at an installer that shows only "Running
// package scripts...". Half a minute is long enough to notice the dialog even
// when it opens behind the installer window, and short enough that ignoring it
// is not the ordeal that a 600 second wait made of it. The conclusion pane
// covers whatever is left unenabled.
//
// It is a budget for the whole registration rather than per input source: the
// parent and the mode are enabled in turn, and two separate timeouts would
// leave the installer sitting there for twice as long.
static const NSTimeInterval kChiaKeyEnableApprovalTimeout = 30;

// Once the consent dialog has been answered the remaining enables need nobody,
// but the enabled list still takes a moment to catch up. A source whose wait
// starts with the shared budget almost spent would be declared timed out for
// being merely slow, so never hand one less than this.
static const NSTimeInterval kChiaKeyEnableSettleGrace = 5;

static NSDate *ChiaKeyEnableDeadlineForSource(NSDate *approvalDeadline) {
  NSDate *grace = [NSDate dateWithTimeIntervalSinceNow:kChiaKeyEnableSettleGrace];
  return ([approvalDeadline compare:grace] == NSOrderedDescending)
             ? approvalDeadline
             : grace;
}

static BOOL ChiaKeyWaitForInputSourceEnabled(NSString *inputSourceID,
                                             NSDate *deadline) {
  while (!ChiaKeyInputSourceIsEnabled(inputSourceID)) {
    if ([deadline timeIntervalSinceNow] <= 0) return NO;
    // The enabled list is rebuilt off a notification, so spin the run loop
    // instead of sleeping through it.
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.5, false);
  }
  return YES;
}

// Prints one status token per line for the install scripts:
//   registered | registration-skipped
//   already-enabled | newly-enabled | enable-requested | enable-timeout
//
// waitForApproval blocks on the system consent dialog, which only the GUI
// installer wants: it holds its "Log Out" button back until the input source
// is really enabled. The CLI and updater paths must not wait -- nobody is
// looking at that dialog, and the updater would sit there for the timeout.
static int ChiaKeyRegisterInputMethod(BOOL waitForApproval) {
  NSURL *bundleURL = [[NSBundle mainBundle] bundleURL];
  NSString *inputSourceID = ChiaKeyInputSourceID();
  NSString *modeID = ChiaKeyInputModeID();

  // TIS watches the Input Methods folders itself, so a bundle that already
  // answers to our ID needs no re-registration; doing it anyway only makes
  // every running app rebuild its input source cache. Register when nothing
  // answers yet -- a first install, or a bundle in a folder TIS does not scan
  // -- and when the ID resolves to a *different* bundle, which is what an
  // install that moves the app (say /Library to ~/Library) leaves behind.
  // Looking up the mode also forces registration when upgrading a legacy
  // installation that only registered the parent ID.
  TISInputSourceRef existing = ChiaKeyCreateInputSourceForID(modeID);
  NSString *registeredPath = existing ? ChiaKeyRegisteredBundlePath(existing) : nil;
  NSString *ourPath = [[bundleURL path] stringByStandardizingPath];
  BOOL registeredHere =
      registeredPath && [registeredPath isEqualToString:ourPath];
  if (existing) {
    if (!registeredHere) {
      NSLog(@"input source %@ resolves to %@, re-registering %@", inputSourceID,
            registeredPath ? registeredPath : @"an unknown bundle", ourPath);
    }
    CFRelease(existing);
  }

  if (registeredHere) {
    printf("registration-skipped\n");
  } else {
    OSStatus registerStatus = TISRegisterInputSource((CFURLRef)bundleURL);
    if (registerStatus != noErr) {
      NSLog(@"failed to register input source %@ at %@: %d", inputSourceID,
            bundleURL, registerStatus);
      return 1;
    }
    printf("registered\n");
  }

  // TIS requires the parent to be enabled before enabling an input mode.
  // Preserve an existing enable, but do not mistake an enabled legacy parent
  // for an enabled mode after an upgrade.
  BOOL requestedEnable = NO;
  BOOL enablePending = NO;
  NSDate *approvalDeadline =
      [NSDate dateWithTimeIntervalSinceNow:kChiaKeyEnableApprovalTimeout];
  for (NSString *sourceID in @[inputSourceID, modeID]) {
    if (ChiaKeyInputSourceIsEnabled(sourceID)) continue;
    if (!ChiaKeyEnableInputSourceWithID(sourceID)) {
      // TIS refuses a mode whose parent is still waiting on consent. On the
      // non-waiting path that is the expected outcome rather than a failure:
      // the mode's default-state flag and the next install call pick it up.
      if (enablePending) break;
      return 1;
    }
    requestedEnable = YES;

    if (waitForApproval) {
      if (!ChiaKeyWaitForInputSourceEnabled(sourceID,
                                            ChiaKeyEnableDeadlineForSource(
                                                approvalDeadline))) {
        NSLog(@"input source %@ was not enabled within the approval budget "
              @"(%.0f seconds from the registration, never less than %.0f "
              @"seconds for this source)", sourceID,
              kChiaKeyEnableApprovalTimeout, kChiaKeyEnableSettleGrace);
        printf("enable-timeout\n");
        return 0;
      }
    } else if (!ChiaKeyInputSourceIsEnabled(sourceID)) {
      // CLI/updater must not wait for consent, but it must still ask for the
      // mode: returning here left TISEnableInputSource uncalled for it, so a
      // command-line install enabled the parent and nothing else.
      enablePending = YES;
    }
  }

  if (enablePending) {
    printf("enable-requested\n");
    return 0;
  }

  printf(requestedEnable ? "newly-enabled\n" : "already-enabled\n");
  return 0;
}

int main(int argc, char *argv[]) {
  NSAutoreleasePool *pool = [NSAutoreleasePool new];

  if (argc > 1) {
    string cmd = argv[1];
    if (cmd == "install") {
      BOOL waitForApproval =
          (argc > 2) && (string(argv[2]) == "--wait-for-approval");
      int status = ChiaKeyRegisterInputMethod(waitForApproval);
      [pool drain];
      return status;
    }

    // Diagnostic probe for the install scripts and for tracking down "the
    // input source is not in my menu" reports: answers for one ID with the
    // same enabled-list check the install path uses. Status only, no output.
    if (cmd == "check-enabled") {
      NSString *sourceID = (argc > 2)
                               ? [NSString stringWithUTF8String:argv[2]]
                               : ChiaKeyInputModeID();
      int status = ChiaKeyInputSourceIsEnabled(sourceID) ? 0 : 1;
      [pool drain];
      return status;
    }

    if (cmd == "uninstall") {
      // Used by Scripts/uninstall.sh to take the input source out of the
      // system list before the bundle is deleted.
      BOOL modeDisabled = ChiaKeyDisableInputSourceWithID(ChiaKeyInputModeID());
      BOOL parentDisabled = ChiaKeyDisableInputSourceWithID(ChiaKeyInputSourceID());
      int status = (modeDisabled && parentDisabled) ? 0 : 1;
      [pool drain];
      return status;
    }

    NSApplicationLoad();
    [NSRunLoop currentRunLoop];

    string arg = (argc > 2) ? argv[2] : "";
    int status = 0;

    if (cmd == "reload") {
      if (ChiaKeyIMEIsRunning()) {
        ChiaKeyPostServiceNotification(ChiaKeyReloadRequestedNotification);
        NSLog(@"reload requested");
      } else {
        NSLog(@"reload: ChiaKey is not running");
        status = 1;
      }
    } else if (cmd == "modulelist") {
      NSArray *idsAndNames =
          [ChiaKeyReadServiceStatus() objectForKey:ChiaKeyStatusModulesKey];
      if ([idsAndNames count]) {
        NSEnumerator *ianEnum = [idsAndNames objectEnumerator];
        id item;
        while (item = [ianEnum nextObject]) {
          NSLog(@"module: %@ (%@)", [item objectAtIndex:0],
                [item objectAtIndex:1]);
        }
      } else {
        NSLog(@"modulelist: no published status; run the input method first");
        status = 1;
      }
    } else if (cmd == "import") {
      NSString *dir = ChiaKeyServiceUserDataDirectory();
      ChiaKeyClaimUserPhraseEditingLock(dir);
      ChiaKeyPostUserPhraseNotification(
          ChiaKeyPhraseEditorDidBeginEditingNotification);

      OVSQLiteConnection *db = ChiaKeyOpenUserPhraseDBForCLI();
      bool ok = db && Manjusri::BPMFUserPhraseHelper::Import(db, arg);
      if (db) delete db;

      ChiaKeyTouchCoordinationFile(ChiaKeyUserPhraseDirtyFlagPath(dir));
      if (ChiaKeyReleaseUserPhraseEditingLockIfOwner(dir)) {
        ChiaKeyPostUserPhraseNotification(
            ChiaKeyPhraseEditorDidEndEditingNotification);
      }

      if (ok) {
        NSLog(@"import succeeded, file: %s", arg.c_str());
      } else {
        NSLog(@"import failed");
        status = 1;
      }
    } else if (cmd == "export") {
      OVSQLiteConnection *db = ChiaKeyOpenUserPhraseDBForCLI();
      bool ok = db && Manjusri::BPMFUserPhraseHelper::Export(db, arg);
      if (db) delete db;

      if (ok) {
        NSLog(@"export succeeded, file: %s", arg.c_str());
      } else {
        NSLog(@"export failed");
        status = 1;
      }
    } else {
      NSLog(@"unknown command.");
      status = 1;
    }

    // Give the distributed notifications a moment to flush.
    [[NSRunLoop currentRunLoop]
           runMode:NSDefaultRunLoopMode
        beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];

    [pool drain];
    return status;
  }

  OVInputMethodServer =
      [[IMKServer alloc] initWithName:OPENVANILLA_CONNECTION_NAME
                     bundleIdentifier:[[NSBundle mainBundle] bundleIdentifier]];

  if (!OVInputMethodServer) {
    NSLog(@"input method server init failed!");
    return 1;
  }

  [NSApplication sharedApplication];
  NSBundle *mainBundle = [NSBundle mainBundle];
  BOOL usesProgrammaticDelegate =
      [[mainBundle objectForInfoDictionaryKey:@"LSBackgroundOnly"] boolValue] ||
      [[mainBundle objectForInfoDictionaryKey:@"LSUIElement"] boolValue];
  CVApplicationController *applicationController = nil;
  CVApplicationController *applicationDelegate = nil;
  if (usesProgrammaticDelegate) {
    applicationController = [[CVApplicationController alloc] init];
    [NSApp setDelegate:applicationController];
    applicationDelegate = applicationController;
  } else {
    BOOL result = [[NSBundle mainBundle] loadNibNamed:@"MainMenu"
                                                owner:NSApp
                                      topLevelObjects:nil];
    //	NSLog(@"nib loading result: %d", result);
    applicationDelegate = (CVApplicationController *)[NSApp delegate];
  }

  NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
  NSString *modulePath =
      [resourcePath stringByAppendingPathComponent:@"Modules"];
  NSArray *loadPaths = [NSArray arrayWithObjects:modulePath, nil];

  OpenVanillaLoader *ovl = [OpenVanillaLoader sharedInstance];
  [applicationDelegate setLoader:ovl];
  [NSThread detachNewThreadSelector:@selector(start:)
                           toTarget:ovl
                         withObject:loadPaths];

  [[NSApplication sharedApplication] run];

  // [OpenVanillaController cleanUpAutoUpdate];
  [ovl shutDown];
  [OpenVanillaLoader releaseSharedObjects];
  [NSApp setDelegate:nil];
  [applicationController release];
  [OVInputMethodServer release];
  [pool drain];
  return 0;
}
