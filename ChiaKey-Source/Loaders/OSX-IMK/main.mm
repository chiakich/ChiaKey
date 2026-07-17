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
  [[NSFileManager defaultManager] createDirectoryAtPath:dir
                            withIntermediateDirectories:YES
                                             attributes:nil
                                                  error:NULL];
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

static BOOL ChiaKeyHasInputSourceWithID(NSString *inputSourceID) {
  TISInputSourceRef source = ChiaKeyCreateInputSourceForID(inputSourceID);
  if (!source) {
    return NO;
  }

  CFRelease(source);
  return YES;
}

static int ChiaKeyRegisterInputMethod() {
  NSBundle *mainBundle = [NSBundle mainBundle];
  NSURL *bundleURL = [mainBundle bundleURL];
  NSString *inputSourceID =
      [mainBundle objectForInfoDictionaryKey:@"TISInputSourceID"];
  if (![inputSourceID length]) {
    inputSourceID = [mainBundle bundleIdentifier];
  }

  if (!ChiaKeyHasInputSourceWithID(inputSourceID)) {
    OSStatus registerStatus = TISRegisterInputSource((CFURLRef)bundleURL);
    if (registerStatus != noErr) {
      NSLog(@"failed to register input source %@ at %@: %d", inputSourceID,
            bundleURL, registerStatus);
    }
  }

  if (!ChiaKeyEnableInputSourceWithID(inputSourceID)) {
    return 1;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  NSAutoreleasePool *pool = [NSAutoreleasePool new];

  if (argc > 1) {
    string cmd = argv[1];
    if (cmd == "install") {
      int status = ChiaKeyRegisterInputMethod();
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
