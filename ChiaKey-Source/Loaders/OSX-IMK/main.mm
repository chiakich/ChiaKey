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

static BOOL ChiaKeyInputSourceIsEnabled(NSString *inputSourceID) {
  TISInputSourceRef source = ChiaKeyCreateInputSourceForID(inputSourceID);
  if (!source) {
    return NO;
  }

  CFBooleanRef enabled = (CFBooleanRef)TISGetInputSourceProperty(
      source, kTISPropertyInputSourceIsEnabled);
  BOOL isEnabled = enabled && CFBooleanGetValue(enabled);
  CFRelease(source);
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

// The enable turns into a system consent dialog; the flag only flips once the
// user approves, and the notification that refreshes our cached view needs a
// running run loop, so spin the loop instead of sleeping.
static const NSTimeInterval kChiaKeyEnableApprovalTimeout = 600;

static BOOL ChiaKeyWaitForInputSourceEnabled(NSString *inputSourceID,
                                             NSTimeInterval timeout) {
  NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
  while (!ChiaKeyInputSourceIsEnabled(inputSourceID)) {
    if ([deadline timeIntervalSinceNow] <= 0) return NO;
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

  // TIS watches the Input Methods folders itself, so a bundle that already
  // answers to our ID needs no re-registration; doing it anyway only makes
  // every running app rebuild its input source cache. Register when nothing
  // answers yet: a first install, or a bundle in a folder TIS does not scan.
  TISInputSourceRef existing = ChiaKeyCreateInputSourceForID(inputSourceID);
  if (existing) {
    CFRelease(existing);
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

  // The enabled state lives in the user's preferences, keyed by input source
  // ID, so it survives an update untouched.
  if (ChiaKeyInputSourceIsEnabled(inputSourceID)) {
    printf("already-enabled\n");
    return 0;
  }

  if (!ChiaKeyEnableInputSourceWithID(inputSourceID)) {
    return 1;
  }

  if (!waitForApproval) {
    printf("enable-requested\n");
    return 0;
  }

  // Anything that asks the user to log out has to come after this returns: a
  // logout before the consent dialog is answered drops the enable.
  if (!ChiaKeyWaitForInputSourceEnabled(inputSourceID,
                                        kChiaKeyEnableApprovalTimeout)) {
    NSLog(@"input source %@ was not enabled within %.0f seconds", inputSourceID,
          kChiaKeyEnableApprovalTimeout);
    printf("enable-timeout\n");
    return 0;
  }

  printf("newly-enabled\n");
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

    if (cmd == "uninstall") {
      // Used by Scripts/uninstall.sh to take the input source out of the
      // system list before the bundle is deleted.
      int status = ChiaKeyDisableInputSourceWithID(ChiaKeyInputSourceID()) ? 0 : 1;
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
