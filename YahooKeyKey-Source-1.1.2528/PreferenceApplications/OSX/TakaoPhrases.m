/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoPhrases.h"

#define OPENVANILLA_DOTMAC_ID @"YHKK"
#define OPENVANILLA_APPLICATION_NAME @"ChiaKey"

#ifdef __x86_64__
#warning Not working under x86_64
#define kDMiDiskService nil
#define DMMemberAccount NSObject
#define DMiDiskSession NSObject
#endif

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

- (void)dealloc {
  [_transactionTimer release];
  [super dealloc];
}
- (void)awakeFromNib {
  [_progressIndicator setIndeterminate:YES];
}

#pragma mark Import/Export

// Export database into a text file.
- (IBAction)exportDatabase:(id)sender {
  id ovService;

  @try {
    ovService = [NSConnection
        rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
                                            host:nil];
  } @catch (NSException *e) {
    // NSLog(@"Exceptions raise on retreiving version info");
    CKBeginAlertSheet(window, LFLSTR(@"Unable to export database."),
                      LFLSTR(@"Uknow errors happend."),
                      NSAlertStyleWarning);
    return;
  }

  if (!ovService) {
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
    if (ovService) {
      [ovService setProtocolForProxy:@protocol(OpenVanillaService)];
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
    ovService = [NSConnection
        rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
                                            host:nil];
  } @catch (NSException *e) {
    CKBeginAlertSheet(window, LFLSTR(@"Unable to import database."),
                      LFLSTR(@"Unknown errors happened."),
                      NSAlertStyleWarning);
    return;
  }
  if (!ovService) {
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
    if (ovService) {
      [ovService setProtocolForProxy:@protocol(OpenVanillaService)];
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

#pragma mark -
#pragma mark Import to/Export from DotMac

//- (void)alertDidEnd:(NSAlert *)alert returnCode:(int)returnCode
// contextInfo:(void *)contextInfo
//{
////	NSLog(@"alertDidEnd:%d", returnCode);
//	if (returnCode == NSAlertDefaultReturn) {
//		// The apple script to launch the MobileMe preference pane.
//		NSString *source = @"tell application \"System
// Preferences\"\nactivate\nset the current pane to pane id
//\"com.apple.preference.internet\"\nend tell"; 		NSAppleScript
//*script =
//[[NSAppleScript alloc] initWithSource:source]; 		[script
//autorelease]; 		[script executeAndReturnError:nil];
//	}
//	else if (returnCode == NSAlertAlternateReturn) {
//		// The API to launch the web browser to open the MobileMe
//website to register a new account. 		[DMMemberAccount
// signUpNewMemberWithApplicationID: OPENVANILLA_DOTMAC_ID];
//	}
//}

//- (DMMemberAccount *)loginDotMac
//{
//	DMMemberAccount *myMemberAccount = [DMMemberAccount
// accountFromPreferencesWithApplicationID: OPENVANILLA_DOTMAC_ID];
//	[myMemberAccount setApplicationName: OPENVANILLA_APPLICATION_NAME];
//	if ([myMemberAccount validateCredentials] != kDMSuccess) {
//		NSAlert *alert = [NSAlert alertWithMessageText:LFLSTR(@"Unable
//to login MobileMe.") defaultButton:LFLSTR(@"Open MobileMe Preference")
// alternateButton:LFLSTR(@"Register a New Account")
// otherButton:LFLSTR(@"Cancel") informativeTextWithFormat:LFLSTR(@"You may not
// set your account information in the System Preferences, or your MobileMe
// account is expired. Do you want to loign MobileMe by opening the MobileMe
// preference pane, or register a new account online?")]; 		[alert
// beginSheetModalForWindow:window modalDelegate:self
// didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
// contextInfo:nil]; 		return nil;
//	}
//	return myMemberAccount;
//}
//
//- (void)_lockDotMac
//{
//	[_exportDotMacButton setEnabled: NO];
//	[_importDotMacButton setEnabled: NO];
//}
//- (void)_unlockDotMac
//{
//	[_exportDotMacButton setEnabled: YES];
//	[_importDotMacButton setEnabled: YES];
//}

- (void)transactionTimer:(NSTimer *)theTimer {
  //	DMTransaction *downloadTransaction = [theTimer userInfo];
  //	[_progressIndicator setIndeterminate:NO];
  //	[_progressIndicator setMaxValue:(double)[downloadTransaction
  // contentLength]];
  //	[_progressIndicator setDoubleValue:(double)[downloadTransaction
  // bytesTransferred]];
  //
  //	if ([downloadTransaction isFinished])
  //		[theTimer invalidate];
}

- (IBAction)exportDatabaseToDotMac:(id)sender {
  //	id ovService;
  //	@try {
  //		ovService = [NSConnection
  // rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
  // host:nil]; 		if (ovService) { [ovService
  // setProtocolForProxy:@protocol(OpenVanillaService)];
  //		}
  //		else {
  //			NSAlert *alert = [NSAlert
  //alertWithMessageText:LFLSTR(@"Unable to export database.")
  // defaultButton:LFLSTR(@"OK") alternateButton:nil otherButton:nil
  // informativeTextWithFormat:LFLSTR(@"If you are not runnung ChiaKey,
  // you are not able to export your database.")]; 			[alert
  // beginSheetModalForWindow:window modalDelegate:self didEndSelector:nil
  // contextInfo:nil]; 			return;
  //		}
  //
  //	}
  //	@catch(NSException *e) {
  //		NSAlert *alert = [NSAlert alertWithMessageText:LFLSTR(@"Unable
  //to export database.") defaultButton:LFLSTR(@"OK") alternateButton:nil
  // otherButton:nil informativeTextWithFormat:LFLSTR(@"Unknown errors
  // happened.")]; 		[alert beginSheetModalForWindow:window
  // modalDelegate:self didEndSelector:nil contextInfo:nil]; 		return;
  //	}
  //
  //	NSString *path = [ovService randomTemporaryFilenameWithFullpath];
  //	if (![ovService exportUserPhraseDBToFile:path]) {
  //		NSAlert *alert = [NSAlert alertWithMessageText:LFLSTR(@"Unable
  //to export database") defaultButton:LFLSTR(@"OK") alternateButton:nil
  // otherButton:nil informativeTextWithFormat:LFLSTR(@"Unknown error
  // happend.")]; 		[alert beginSheetModalForWindow:window
  // modalDelegate:self didEndSelector:nil contextInfo:nil]; 		return;
  //	}
  //
  //	DMMemberAccount *myMemberAccount = [self loginDotMac];
  //	if (!myMemberAccount)
  //		return;
  //	[myMemberAccount setIsSynchronous:YES];
  //
  //	DMTransaction *serviceTransaction = [myMemberAccount
  // servicesAvailableForAccount];
  //
  //	if ([serviceTransaction isSuccessful]) {
  //		NSArray *services = [serviceTransaction result];
  //		if ([services containsObject:kDMiDiskService] == NO) {
  //			NSAlert *alert = [NSAlert
  //alertWithMessageText:LFLSTR(@"iDisk service is not available.")
  // defaultButton:LFLSTR(@"OK") alternateButton:nil otherButton:nil
  // informativeTextWithFormat:LFLSTR(@"You are not able to use the iDisk
  // service, please check the status of your MobileMe account and try
  // again.")]; 			[alert beginSheetModalForWindow:window
  // modalDelegate:self didEndSelector:nil contextInfo:nil];
  // return;
  //		}
  //		DMiDiskSession *mySession = [DMiDiskSession
  // iDiskSessionWithAccount:myMemberAccount]; 		[mySession
  // setDelegate:(id)self]; 		NSData *messageData = [NSData
  //dataWithContentsOfFile:path]; 		_transactionType = tUpload; 		DMTransaction
  //*uploadTransaction = [mySession putData:messageData
  // toPath:@"/Documents/ChiaKey Phrases.txt"]; 		if
  // (uploadTransaction != nil) {
  //			[[NSApplication sharedApplication]
  //beginSheet:_progressWindow modalForWindow:window modalDelegate:self
  // didEndSelector:nil contextInfo:nil];
  //			[_progressIndicator startAnimation:self];
  //
  //			if (_transactionTimer)
  //				_transactionTimer = nil;
  //			_transactionTimer = [NSTimer
  //scheduledTimerWithTimeInterval:0.1 target:self
  // selector:@selector(transactionTimer:) userInfo:uploadTransaction
  // repeats:YES];
  //		}
  //	} else {
  //		// handle error
  //	}
}

- (IBAction)importDatabaseFromDotMac:(id)sender {
  //	id ovService;
  //	@try {
  //		ovService = [NSConnection
  // rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
  // host:nil]; 		if (!ovService) { 			NSAlert *alert =
  // [NSAlert alertWithMessageText:LFLSTR(@"Unable to import database.")
  // defaultButton:LFLSTR(@"OK") alternateButton:nil otherButton:nil
  // informativeTextWithFormat:LFLSTR(@"If you are not runnung ChiaKey,
  // you are not able to import your database.")]; 			[alert
  // beginSheetModalForWindow:window modalDelegate:self didEndSelector:nil
  // contextInfo:nil]; 			return;
  //		}
  //	}
  //	@catch(NSException *e) {
  //		NSAlert *alert = [NSAlert alertWithMessageText:LFLSTR(@"Unable
  //to import database.") defaultButton:LFLSTR(@"OK") alternateButton:nil
  // otherButton:nil informativeTextWithFormat:LFLSTR(@"Unnwn errors
  // happened.")]; 		[alert beginSheetModalForWindow:window
  // modalDelegate:self didEndSelector:nil contextInfo:nil]; 		return;
  //	}
  //
  //	DMMemberAccount *myMemberAccount = [self loginDotMac];
  //	if (!myMemberAccount)
  //		return;
  //	[myMemberAccount setIsSynchronous:YES];
  //
  //	DMTransaction *serviceTransaction = [myMemberAccount
  // servicesAvailableForAccount];
  //
  //	if ([serviceTransaction isSuccessful]) {
  //		NSArray *services = [serviceTransaction result];
  //		if ([services containsObject:kDMiDiskService] == NO) {
  //			NSAlert *alert = [NSAlert
  //alertWithMessageText:LFLSTR(@"iDisk service is not available.")
  // defaultButton:LFLSTR(@"OK") alternateButton:nil otherButton:nil
  // informativeTextWithFormat:LFLSTR(@"You are not able to use the iDisk
  // service, please check the status of your MobileMe account and try
  // again.")]; 			[alert beginSheetModalForWindow:window
  // modalDelegate:self didEndSelector:nil contextInfo:nil];
  // return;
  //		}
  //		DMiDiskSession *mySession = [DMiDiskSession
  // iDiskSessionWithAccount:myMemberAccount]; 		[mySession
  // setDelegate:(id)self]; 		_transactionType = tDownload; 		DMTransaction
  //*downloadTransaction = [mySession getDataAtPath:@"/Documents/ChiaKey
  // Phrases.txt"]; 		if (downloadTransaction != nil) {
  //			[[NSApplication sharedApplication]
  //beginSheet:_progressWindow modalForWindow:window modalDelegate:self
  // didEndSelector:nil contextInfo:nil];
  //			[_progressIndicator startAnimation:self];
  //
  //			if (_transactionTimer)
  //				_transactionTimer = nil;
  //			_transactionTimer = [NSTimer
  //scheduledTimerWithTimeInterval:0.1 target:self
  // selector:@selector(transactionTimer:) userInfo:downloadTransaction
  // repeats:YES];
  //		}
  //	} else {
  //		// handle error
  //	}
}

#pragma mark DotMac transaction delegate methods

//- (void)transactionSuccessful: (DMTransaction *)theTransaction {
//	[[NSApplication sharedApplication] endSheet:_progressWindow];
//	[_progressWindow orderOut:self];
//	[_progressIndicator stopAnimation:self];
//    NSData *result = [theTransaction result];
//	// NSLog(@"done");
//
//	switch (_transactionType) {
//		case tDownload:
//			if ([result length]) {
//				id ovService = [NSConnection
// rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
// host:nil]; 				if (ovService) {
// [ovService setProtocolForProxy:@protocol(OpenVanillaService)];
// NSString *path = [ovService randomTemporaryFilenameWithFullpath];
//[result writeToFile:path
// atomically:YES]; 					NSAlert *alert = nil;
// if ([ovService importUserPhraseDBFromFile:path])
// alert = [NSAlert alertWithMessageText:LFLSTR(@"Done!")
// defaultButton:LFLSTR(@"OK") alternateButton:nil otherButton:nil
// informativeTextWithFormat:LFLSTR(@"Successfully imported the backup of your
// customized phrases database from your iDisk.")];
// else 						alert = [NSAlert
// alertWithMessageText:LFLSTR(@"Failed!") defaultButton:LFLSTR(@"OK")
// alternateButton:nil otherButton:nil informativeTextWithFormat:LFLSTR(@"Unable
// to import the downloaded file, your backup on iDisk may be corrupt.")];
// [alert beginSheetModalForWindow:window modalDelegate:self didEndSelector:nil
// contextInfo:nil];
//				}
//			}
//			else {
//				NSAlert *alert = [NSAlert
// alertWithMessageText:LFLSTR(@"The downloaded file is blank.")
// defaultButton:LFLSTR(@"OK") alternateButton:nil otherButton:nil
// informativeTextWithFormat:@""]; 				[alert
// beginSheetModalForWindow:window modalDelegate:self didEndSelector:nil
// contextInfo:nil];
//			}
//			break;
//		case tUpload:
//			{
//				NSAlert *alert = [NSAlert
// alertWithMessageText:LFLSTR(@"Done!") defaultButton:LFLSTR(@"OK")
// alternateButton:nil otherButton:nil
// informativeTextWithFormat:LFLSTR(@"Successfully upload your phrases database
// to iDisk.")]; 				[alert beginSheetModalForWindow:window
// modalDelegate:self didEndSelector:nil contextInfo:nil];
//			}
//			break;
//		default:
//			break;
//	}
//	_transactionType = 0;
//}
//
//- (void)transactionHadError: (DMTransaction *)theTransaction {
//	[[NSApplication sharedApplication] endSheet:_progressWindow];
//	[_progressWindow orderOut:self];
//	[_progressIndicator stopAnimation:self];
//
//    int errorType = [theTransaction errorType];
//	NSString *error = LFLSTR(@"Error");
//	NSString *errorDescription;
//	switch(errorType) {
//		case kDMNetworkError:
//			errorDescription = LFLSTR(@"Unable to interact with
//MobileMe because of a networking error."); 			break;
// case kDMResourceNotFound: 			errorDescription = LFLSTR(@"ChiaKey user
//phrases database was not found on your iDisk."); 			break;
// case kDMServiceBusy: 			errorDescription = LFLSTR(@"The iDisk servie is now
//busy, please wait for a while and try again."); 			break;
// case kDMInvalidParameter: 			errorDescription = LFLSTR(@"A user-provided
//parameter was invalid."); 			break; default:
// errorDescription = LFLSTR(@"Unknown error happend.");
// break;
//	}
//	NSAlert *alert = [NSAlert alertWithMessageText:error
// defaultButton:LFLSTR(@"OK") alternateButton:nil otherButton:nil
// informativeTextWithFormat:errorDescription]; 	[alert
// beginSheetModalForWindow:window modalDelegate:self didEndSelector:nil
// contextInfo:nil]; 	_transactionType = 0;
//}
//
//- (void)transactionAborted: (DMTransaction *)theTransaction {
//	[[NSApplication sharedApplication] endSheet:_progressWindow];
//	[_progressWindow orderOut:self];
//	[_progressIndicator stopAnimation:self];
//	_transactionType = 0;
//}

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
