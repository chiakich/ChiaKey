/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoPhrases.h"

// Import/export talk to the user phrase DB directly, coordinated with the
// running IME via the editing-lock/dirty-flag protocol -- no XPC.
#import "ChiaKeyServiceCoordination.h"
#import "PEUserPhraseStore.h"

// Yahoo! KeyKey, the input method ChiaKey descends from. Its user phrase
// database is encrypted with a key we do not have, so the only way in is to
// ask the old input method itself to export a plain-text MJSR file -- the
// same format ChiaKey imports. That requires the old binary to still run
// (Rosetta 2 on Apple Silicon) and, because the CLI talks to the running
// input method over a distributed object, to have it up.
static NSString *const kLegacyDataDirectoryName = @"Yahoo! KeyKey";
static NSString *const kLegacyBundleName = @"Yahoo! KeyKey.app";
static NSString *const kLegacyExecutableName = @"Yahoo! KeyKey";
static NSString *const kLegacyBundleIdentifier =
    @"com.yahoo.inputmethod.KeyKey";

static void CKBeginAlertSheet(NSWindow *window, NSString *message,
                              NSString *informativeText, NSAlertStyle style) {
  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:message];
  [alert setInformativeText:informativeText ? informativeText : @""];
  [alert addButtonWithTitle:LFLSTR(@"OK")];
  [alert setAlertStyle:style];
  [alert beginSheetModalForWindow:window completionHandler:nil];
}

#pragma mark Legacy (Yahoo! KeyKey) discovery

static NSString *CKLegacyDataDirectory(void) {
  return [[NSHomeDirectory()
      stringByAppendingPathComponent:@"Library/Application Support"]
      stringByAppendingPathComponent:kLegacyDataDirectoryName];
}

// The legacy install has data only if its user phrase DB is there; the
// directory alone can be left behind by an uninstall.
static BOOL CKLegacyDataExists(void) {
  NSString *db = [CKLegacyDataDirectory()
      stringByAppendingPathComponent:@"SmartMandarinUserData.db"];
  return [[NSFileManager defaultManager] fileExistsAtPath:db];
}

// Input methods live in either domain; a per-user install wins because that
// is the copy the running input method would have been launched from.
static NSString *CKLegacyExecutablePath(void) {
  NSArray *roots = @[
    [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Input Methods"],
    @"/Library/Input Methods",
  ];
  for (NSString *root in roots) {
    NSString *executable =
        [[[root stringByAppendingPathComponent:kLegacyBundleName]
            stringByAppendingPathComponent:@"Contents/MacOS"]
            stringByAppendingPathComponent:kLegacyExecutableName];
    if ([[NSFileManager defaultManager] isExecutableFileAtPath:executable]) {
      return executable;
    }
  }
  return nil;
}

static BOOL CKLegacyIsRunning(void) {
  return [[NSRunningApplication
             runningApplicationsWithBundleIdentifier:kLegacyBundleIdentifier]
             count] > 0;
}

// Runs `Yahoo! KeyKey export <path>`. The old CLI forwards the request to the
// running input method and exits 0 either way, so success is judged by the
// file it was supposed to write, not by the exit status.
static BOOL CKRunLegacyExport(NSString *executable, NSString *outputPath) {
  [[NSFileManager defaultManager] removeItemAtPath:outputPath error:NULL];

  NSTask *task = [[[NSTask alloc] init] autorelease];
  [task setLaunchPath:executable];
  [task setArguments:@[ @"export", outputPath ]];
  [task setStandardOutput:[NSFileHandle fileHandleWithNullDevice]];
  [task setStandardError:[NSFileHandle fileHandleWithNullDevice]];

  @try {
    [task launch];
    [task waitUntilExit];
  } @catch (NSException *exception) {
    // Thrown when the binary cannot be executed at all -- on Apple Silicon
    // that means the Intel slice found no Rosetta 2 to run under.
    return NO;
  }

  NSString *contents = [NSString stringWithContentsOfFile:outputPath
                                                 encoding:NSUTF8StringEncoding
                                                    error:NULL];
  return contents &&
         [contents rangeOfString:@"MJSR version 1.0.0"].location != NSNotFound;
}

// Copies files the legacy install kept in plain text and ChiaKey reads in the
// same format. Never overwrites: whatever the user has now wins.
static NSUInteger CKCopyLegacyPlainFiles(void) {
  NSFileManager *manager = [NSFileManager defaultManager];
  NSString *source = CKLegacyDataDirectory();
  NSString *target = ChiaKeyServiceUserDataDirectory();
  ChiaKeyEnsureUserDataDirectoryPrivate();

  NSUInteger copied = 0;
  NSString *cannedName = @"UserCannedMessages.txt";
  NSString *cannedTarget = [target stringByAppendingPathComponent:cannedName];
  if (![manager fileExistsAtPath:cannedTarget] &&
      [manager copyItemAtPath:[source stringByAppendingPathComponent:cannedName]
                       toPath:cannedTarget
                        error:NULL]) {
    copied++;
  }

  NSString *tablesSource =
      [source stringByAppendingPathComponent:@"DataTables"];
  NSString *tablesTarget =
      [target stringByAppendingPathComponent:@"DataTables"];
  for (NSString *name in [manager contentsOfDirectoryAtPath:tablesSource
                                                      error:NULL]) {
    if (![[name pathExtension] isEqualToString:@"cin"]) continue;
    [manager createDirectoryAtPath:tablesTarget
        withIntermediateDirectories:YES
                         attributes:nil
                              error:NULL];
    NSString *destination = [tablesTarget stringByAppendingPathComponent:name];
    if ([manager fileExistsAtPath:destination]) continue;
    if ([manager
            copyItemAtPath:[tablesSource stringByAppendingPathComponent:name]
                    toPath:destination
                     error:NULL]) {
      copied++;
    }
  }
  return copied;
}

@implementation TakaoPhrases

#pragma mark Import/Export

// Export database into a text file.
- (IBAction)exportDatabase:(id)sender {
  PEUserPhraseStore *store = [PEUserPhraseStore sharedStore];
  if (![store isAvailable]) {
    CKBeginAlertSheet(window, LFLSTR(@"Unable to export database."),
                      LFLSTR(@"Uknow errors happend."), NSAlertStyleWarning);
    return;
  }

  NSSavePanel *panel = [NSSavePanel savePanel];
  [panel setAllowedFileTypes:[NSArray arrayWithObjects:@"txt", nil]];
  [panel setExtensionHidden:NO];
  [panel setCanCreateDirectories:NO];
  [panel setNameFieldLabel:LFLSTR(@"Export As:")];
  [panel setTitle:LFLSTR(@"Export Database")];
  [panel setMessage:LFLSTR(@"Exporting your own customized phrases database.")];
  [panel setPrompt:LFLSTR(@"Export")];
  if ([panel runModal] == NSModalResponseOK) {
    NSString *path = [[panel URL] path];
    if ([store exportUserPhraseDBToFile:path]) {
      CKBeginAlertSheet(window, LFLSTR(@"Done!"),
                        LFLSTR(@"Your phrases are successfully exported."),
                        NSAlertStyleInformational);
    } else {
      CKBeginAlertSheet(window, LFLSTR(@"Error"),
                        LFLSTR(@"Unable to export database."),
                        NSAlertStyleWarning);
    }
  }
}

// Import database from a text file.
- (IBAction)importDatabase:(id)sender {
  PEUserPhraseStore *store = [PEUserPhraseStore sharedStore];
  if (![store isAvailable]) {
    CKBeginAlertSheet(window, LFLSTR(@"Unable to import database."),
                      LFLSTR(@"Unknown errors happened."), NSAlertStyleWarning);
    return;
  }

  NSOpenPanel *panel = [NSOpenPanel openPanel];
  [panel setAllowedFileTypes:[NSArray arrayWithObjects:@"txt", nil]];
  [panel setExtensionHidden:NO];
  [panel setCanCreateDirectories:NO];
  [panel setTitle:LFLSTR(@"Import Database")];
  [panel setMessage:LFLSTR(@"Import customized phrases to your own database.")];
  [panel setPrompt:LFLSTR(@"Choose")];
  if ([panel runModal] == NSModalResponseOK) {
    NSString *path = [[panel URL] path];
    // Take the editing lock for the duration of the write so the running
    // IME suspends its own writes; ending the session makes it reload.
    [store beginEditingSession];
    BOOL rtn = [store importUserPhraseDBFromFile:path];
    [store endEditingSession];
    if (rtn) {
      CKBeginAlertSheet(window, LFLSTR(@"Done!"),
                        LFLSTR(@"Your phrases are successfully imported."),
                        NSAlertStyleInformational);
    } else {
      CKBeginAlertSheet(window, LFLSTR(@"Error"),
                        LFLSTR(@"Unable to import database."),
                        NSAlertStyleWarning);
    }
  }
}

#pragma mark Legacy import

// Import everything a Yahoo! KeyKey install left behind: the phrases and
// learning caches (by way of the old input method's own export), plus the
// files it kept in plain text.
- (IBAction)importLegacyDatabase:(id)sender {
  if (!CKLegacyDataExists()) {
    CKBeginAlertSheet(
        window, LFLSTR(@"No legacy data found."),
        LFLSTR(@"ChiaKey could not find a Yahoo! KeyKey user database in "
               @"~/Library/Application Support/Yahoo! KeyKey."),
        NSAlertStyleInformational);
    return;
  }

  PEUserPhraseStore *store = [PEUserPhraseStore sharedStore];
  if (![store isAvailable]) {
    CKBeginAlertSheet(window, LFLSTR(@"Unable to import database."),
                      LFLSTR(@"Unknown errors happened."), NSAlertStyleWarning);
    return;
  }

  NSString *executable = CKLegacyExecutablePath();
  if (!executable) {
    // The data is here but the program that can decrypt it is not; take what
    // is readable without it and say why the rest was left behind.
    NSUInteger copied = CKCopyLegacyPlainFiles();
    CKBeginAlertSheet(
        window, LFLSTR(@"Yahoo! KeyKey is not installed."),
        [NSString
            stringWithFormat:
                LFLSTR(@"Its user phrase database is encrypted and only that "
                       @"input method can read it, so only %lu plain-text "
                       @"file(s) were imported. Reinstall Yahoo! KeyKey and "
                       @"try again to bring the phrases over."),
                (unsigned long)copied],
        NSAlertStyleWarning);
    return;
  }

  NSString *exportPath = [NSTemporaryDirectory()
      stringByAppendingPathComponent:@"ChiaKeyLegacyImport.txt"];

  // The old CLI is only a messenger: it needs the old input method running to
  // answer. If it is not up, start it and give it a few seconds -- the export
  // itself is the readiness check.
  BOOL exported = CKRunLegacyExport(executable, exportPath);
  if (!exported) {
    if (!CKLegacyIsRunning()) {
      [[NSWorkspace sharedWorkspace]
          openURL:[NSURL
                      fileURLWithPath:[[[executable
                                          stringByDeletingLastPathComponent]
                                          stringByDeletingLastPathComponent]
                                          stringByDeletingLastPathComponent]]];
    }
    for (int attempt = 0; attempt < 16 && !exported; attempt++) {
      [NSThread sleepForTimeInterval:0.5];
      exported = CKRunLegacyExport(executable, exportPath);
    }
  }

  if (!exported) {
    CKBeginAlertSheet(
        window, LFLSTR(@"Could not read the legacy database."),
        LFLSTR(@"ChiaKey asked Yahoo! KeyKey to export its phrases, but it did "
               @"not answer. Switch to Yahoo! KeyKey, type a character to wake "
               @"it up, then try again. On Apple Silicon this also needs "
               @"Rosetta 2, because Yahoo! KeyKey has no Apple Silicon "
               @"version."),
        NSAlertStyleWarning);
    return;
  }

  NSUInteger before = [store numberOfPhrasesMatchingFilter:nil];
  [store beginEditingSession];
  BOOL imported = [store importLegacyUserPhraseDBFromFile:exportPath];
  [store endEditingSession];
  [[NSFileManager defaultManager] removeItemAtPath:exportPath error:NULL];

  if (!imported) {
    CKBeginAlertSheet(window, LFLSTR(@"Unable to import database."),
                      LFLSTR(@"Unknown errors happened."), NSAlertStyleWarning);
    return;
  }

  [store invalidateCachedCounts];
  NSUInteger added = [store numberOfPhrasesMatchingFilter:nil] - before;
  NSUInteger copied = CKCopyLegacyPlainFiles();
  CKBeginAlertSheet(
      window, LFLSTR(@"Done!"),
      [NSString stringWithFormat:LFLSTR(@"Imported %lu phrase(s) and %lu "
                                        @"other file(s) from Yahoo! KeyKey."),
                                 (unsigned long)added, (unsigned long)copied],
      NSAlertStyleInformational);
}

- (IBAction)launchEditor:(id)sender {
  NSString *sharedSupprtPath =
      [[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent];

#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
  NSString *phraseEditorPath =
      [sharedSupprtPath stringByAppendingPathComponent:@"PhraseEditor.app"];
#else
  NSString *phraseEditorPath = [sharedSupprtPath
      stringByAppendingPathComponent:@"PhraseEditorTiger.app"];
#endif

  NSURL *phraseEditorURL = [NSURL fileURLWithPath:phraseEditorPath];
  if (![[NSWorkspace sharedWorkspace] openURL:phraseEditorURL]) {
    if (@available(macOS 10.15, *)) {
      NSURL *applicationURL = [[NSWorkspace sharedWorkspace]
          URLForApplicationWithBundleIdentifier:
              @"com.chiakey.inputmethod.ChiaKey.PhraseEditor"];
      if (applicationURL) {
        [[NSWorkspace sharedWorkspace]
            openApplicationAtURL:applicationURL
                    configuration:[NSWorkspaceOpenConfiguration configuration]
                completionHandler:nil];
      }
    }
    usleep(700);
    [NSApp terminate:self];
  } else {
    usleep(700);
    [NSApp terminate:self];
  }
}
@end
