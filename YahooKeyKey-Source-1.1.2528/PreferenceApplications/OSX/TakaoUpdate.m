/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoUpdate.h"

#import "TakaoHelper.h"

@implementation TakaoUpdate

- (void)dealloc {
  if (_task) [_task terminate];
  [_task release];
  [super dealloc];
}
- (void)_getVersionInfo {
  id ovService;

  // @try {
  ovService = [NSConnection
      rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
                                          host:nil];
  // }
  // @catch(NSException *e) {
  // NSLog(@"Exceptions raise on retreiving version info");
  // [_currentVersionTextField setStringValue:@""];
  // [_latestVersionTextField setStringValue:@""];
  // [_latestCheckTextField setStringValue:@""];
  // return;
  // }

  if (ovService) {
    [ovService setProtocolForProxy:@protocol(OpenVanillaService)];
    NSString *version = [ovService version];
    if (version)
      [_currentVersionTextField setStringValue:version];
    else
      [_currentVersionTextField setStringValue:@""];

    NSString *latestVersion = [ovService latestVersion];
    if (latestVersion)
      [_latestVersionTextField setStringValue:latestVersion];
    else
      [_latestVersionTextField setStringValue:@""];

    NSString *latestCheck = [ovService latestCheck];
    if (latestCheck)
      [_latestCheckTextField setStringValue:latestCheck];
    else
      [_latestCheckTextField setStringValue:@""];
  } else {
    [_currentVersionTextField setStringValue:@""];
    [_latestVersionTextField setStringValue:@""];
    [_latestCheckTextField setStringValue:@""];
  }
}
- (void)awakeFromNib {
  [_checkProgressIndicator setHidden:YES];
  [self _getVersionInfo];
}
- (void)launchDownload:(NSString *)actionURL
          signatureURL:(NSString *)signatureURL
          changeLogURL:(NSString *)changeLogURL {
  if (_task) {
    if ([_task isRunning])
      return;
    else
      [_task release];
  }

  NSString *supportPath =
      [[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent];
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
  NSString *commandPath =
      [supportPath stringByAppendingString:
                       @"/DownloadUpdate.app/Contents/MacOS/DownloadUpdate"];
#else
  NSString *commandPath = [supportPath
      stringByAppendingString:
          @"/DownloadUpdateTiger.app/Contents/MacOS/DownloadUpdateTiger"];
#endif
  NSArray *arguments = [NSArray
      arrayWithObjects:actionURL, signatureURL,
                       ((changeLogURL && [changeLogURL length]) ? changeLogURL
                                                                : @"(null)"),
                       nil];

  _task = [NSTask launchedTaskWithLaunchPath:commandPath arguments:arguments];
  [_task retain];
  [_task launch];
  [_task waitUntilExit];
}

#pragma mark Interface Builder actions

- (IBAction)checkUpdateNow:(id)sender {
  [_checkProgressIndicator startAnimation:self];
  [_checkProgressIndicator setHidden:NO];

  NSDictionary *updateDictionary;

  id ovService = [NSConnection
      rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
                                          host:nil];
  if (ovService) {
    [ovService setProtocolForProxy:@protocol(OpenVanillaService)];
    updateDictionary = [ovService shouldUpdate];

    [_checkProgressIndicator stopAnimation:self];
    [_checkProgressIndicator setHidden:YES];
    // NSLog(@"update dict: %@", [updateDictionary description]);

    NSString *status = [updateDictionary valueForKey:@"Status"];
    if ([status isEqualToString:@"Error"]) {
      NSAlert *alert = [NSAlert
               alertWithMessageText:
                   LFLSTR(@"Unable to check for update via the Internet.")
                      defaultButton:LFLSTR(@"OK")
                    alternateButton:nil
                        otherButton:nil
          informativeTextWithFormat:
              LFLSTR(@"Please check your Internet connection and try again.")];
      [alert beginSheetModalForWindow:_window
                        modalDelegate:self
                       didEndSelector:nil
                          contextInfo:nil];
    } else if ([status isEqualToString:@"No"]) {
      NSAlert *alert =
          [NSAlert alertWithMessageText:
                       LFLSTR(@"You are now using the newest version.")
                          defaultButton:LFLSTR(@"OK")
                        alternateButton:nil
                            otherButton:nil
              informativeTextWithFormat:
                  LFLSTR(@"You need not to update your software")];
      [alert beginSheetModalForWindow:_window
                        modalDelegate:self
                       didEndSelector:nil
                          contextInfo:nil];
    } else if ([status isEqualToString:@"Yes"]) {
      NSString *actionURL = [updateDictionary objectForKey:@"ActionURL"];
      NSString *signatureURL = [updateDictionary objectForKey:@"SignatureURL"];
      NSString *changeLogURL = [updateDictionary objectForKey:@"ChangeLogURL"];
      [self launchDownload:actionURL
              signatureURL:signatureURL
              changeLogURL:changeLogURL];
    } else {
      NSAlert *alert =
          [NSAlert alertWithMessageText:
                       LFLSTR(@"Unable to check for update via the Internet.")
                          defaultButton:LFLSTR(@"OK")
                        alternateButton:nil
                            otherButton:nil
              informativeTextWithFormat:
                  LFLSTR(@"Unknown errors happened, please try again.")];
      [alert beginSheetModalForWindow:_window
                        modalDelegate:self
                       didEndSelector:nil
                          contextInfo:nil];
    }
  } else {
    [_checkProgressIndicator stopAnimation:self];
    [_checkProgressIndicator setHidden:YES];
    NSAlert *alert =
        [NSAlert alertWithMessageText:
                     LFLSTR(@"Unable to check for update via the Internet.")
                        defaultButton:LFLSTR(@"OK")
                      alternateButton:nil
                          otherButton:nil
            informativeTextWithFormat:
                LFLSTR(@"If you are nor running Chiaki KeyKey, you are not "
                       @"able to check for update.")];
    [alert runModal];
  }
  [self _getVersionInfo];
}

@end
