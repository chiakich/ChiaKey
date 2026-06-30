/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoPhrases.h"

static void CKBeginAlertSheet(NSWindow *window, NSString *message,
                              NSString *informativeText, NSAlertStyle style) {
  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:message];
  [alert setInformativeText:informativeText ? informativeText : @""];
  [alert addButtonWithTitle:LFLSTR(@"OK")];
  [alert setAlertStyle:style];
  [alert beginSheetModalForWindow:window completionHandler:nil];
}

@implementation TakaoPhrases

#pragma mark Import/Export

// Export database into a text file.
- (IBAction)exportDatabase:(id)sender {
  id ovService;

  @try {
    ovService = [ChiaKeyServiceClient sharedClient];
  } @catch (NSException *e) {
    // NSLog(@"Exceptions raise on retreiving version info");
    CKBeginAlertSheet(window, LFLSTR(@"Unable to export database."),
                      LFLSTR(@"Uknow errors happend."),
                      NSAlertStyleWarning);
    return;
  }

  if (![ovService isAvailable]) {
    CKBeginAlertSheet(
        window, LFLSTR(@"Unable to export database."),
        LFLSTR(@"If you are not runnung ChiaKey, you are not "
               @"able to export your database."),
        NSAlertStyleWarning);
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
    if ([ovService isAvailable]) {
      bool rtn = [ovService exportUserPhraseDBToFile:path];
      if (rtn) {
        CKBeginAlertSheet(window, LFLSTR(@"Done!"),
                          LFLSTR(@"Your phrases are successfully exported."),
                          NSAlertStyleInformational);
      } else {
        CKBeginAlertSheet(window, LFLSTR(@"Error"),
                          LFLSTR(@"Unable to export database."),
                          NSAlertStyleWarning);
      }
    }
  } else {
    // NSLog(@"Cancel");
  }
}

// Import database from a text file.
- (IBAction)importDatabase:(id)sender {
  id ovService;
  @try {
    ovService = [ChiaKeyServiceClient sharedClient];
  } @catch (NSException *e) {
    CKBeginAlertSheet(window, LFLSTR(@"Unable to import database."),
                      LFLSTR(@"Unknown errors happened."),
                      NSAlertStyleWarning);
    return;
  }
  if (![ovService isAvailable]) {
    CKBeginAlertSheet(
        window, LFLSTR(@"Unable to import database."),
        LFLSTR(@"If you are not runnung ChiaKey, you are not "
               @"able to import your database."),
        NSAlertStyleWarning);
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
    if ([ovService isAvailable]) {
      bool rtn = [ovService importUserPhraseDBFromFile:path];
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
  } else {
    // NSLog(@"Cancel");
  }
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
