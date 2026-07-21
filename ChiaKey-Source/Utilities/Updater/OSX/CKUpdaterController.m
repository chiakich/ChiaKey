//
//  CKUpdaterController.m
//

#import "CKUpdaterController.h"

#import "ChiaKeyUpdateService.h"

static const NSInteger kCKUpdateSoakPeriodDays = 3;

// A window that covers an entire display, menu bar included, means the user is
// in full screen or presenting. Interrupting that with a modal update prompt
// is worse than waiting for the next check.
static BOOL CKFrontmostApplicationIsFullScreen(void) {
  NSRunningApplication *frontmost =
      [[NSWorkspace sharedWorkspace] frontmostApplication];
  if (!frontmost) return NO;

  pid_t frontmostPID = [frontmost processIdentifier];
  CFArrayRef windows = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
      kCGNullWindowID);
  if (!windows) return NO;

  BOOL fullScreen = NO;
  for (NSDictionary *window in (NSArray *)windows) {
    if ([[window objectForKey:(NSString *)kCGWindowOwnerPID] intValue] !=
        frontmostPID)
      continue;
    if ([[window objectForKey:(NSString *)kCGWindowLayer] intValue] != 0)
      continue;

    CGRect bounds = CGRectZero;
    if (!CGRectMakeWithDictionaryRepresentation(
            (CFDictionaryRef)[window objectForKey:(NSString *)kCGWindowBounds],
            &bounds))
      continue;

    for (NSScreen *screen in [NSScreen screens]) {
      NSSize screenSize = [screen frame].size;
      if (fabs(bounds.size.width - screenSize.width) < 1.0 &&
          fabs(bounds.size.height - screenSize.height) < 1.0) {
        fullScreen = YES;
        break;
      }
    }
    if (fullScreen) break;
  }

  CFRelease(windows);
  return fullScreen;
}

@implementation CKUpdaterController

@synthesize userRequested = _userRequested;

- (void)dealloc {
  [_service release];
  [_release release];
  [_progressWindow release];
  [super dealloc];
}

- (void)_quit {
  [NSApp terminate:nil];
}

- (void)_showErrorWithMessage:(NSString *)message {
  [NSApp activateIgnoringOtherApps:YES];

  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setAlertStyle:NSAlertStyleWarning];
  [alert setMessageText:NSLocalizedString(@"Update failed", nil)];
  [alert setInformativeText:[message length]
                                ? message
                                : NSLocalizedString(
                                      @"Please try again later, or download "
                                      @"the update manually.",
                                      nil)];
  [alert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
  [alert runModal];
  [self _quit];
}

#pragma mark Progress window

- (void)_showProgressWindow {
  NSRect frame = NSMakeRect(0.0, 0.0, 380.0, 96.0);
  _progressWindow =
      [[NSWindow alloc] initWithContentRect:frame
                                  styleMask:NSWindowStyleMaskTitled
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  [_progressWindow setTitle:NSLocalizedString(@"ChiaKey Update", nil)];
  [_progressWindow center];

  NSView *contentView = [_progressWindow contentView];

  _progressLabel = [[[NSTextField alloc]
      initWithFrame:NSMakeRect(20.0, 56.0, 340.0, 20.0)] autorelease];
  [_progressLabel setBezeled:NO];
  [_progressLabel setDrawsBackground:NO];
  [_progressLabel setEditable:NO];
  [_progressLabel setSelectable:NO];
  [_progressLabel
      setStringValue:[NSString
                         stringWithFormat:NSLocalizedString(
                                              @"Downloading ChiaKey %@…", nil),
                                          [_release tag]]];
  [contentView addSubview:_progressLabel];

  _progressIndicator = [[[NSProgressIndicator alloc]
      initWithFrame:NSMakeRect(20.0, 28.0, 340.0, 20.0)] autorelease];
  [_progressIndicator setStyle:NSProgressIndicatorStyleBar];
  [_progressIndicator setIndeterminate:YES];
  [_progressIndicator startAnimation:nil];
  [contentView addSubview:_progressIndicator];

  [NSApp activateIgnoringOtherApps:YES];
  [_progressWindow makeKeyAndOrderFront:nil];
}

- (void)_updateProgress:(double)fraction {
  if (fraction < 0.0) {
    [_progressIndicator setIndeterminate:YES];
    [_progressIndicator startAnimation:nil];
    return;
  }

  if ([_progressIndicator isIndeterminate]) {
    [_progressIndicator stopAnimation:nil];
    [_progressIndicator setIndeterminate:NO];
    [_progressIndicator setMinValue:0.0];
    [_progressIndicator setMaxValue:1.0];
  }
  [_progressIndicator setDoubleValue:fraction];
}

#pragma mark Install

- (void)_installPackageAtPath:(NSString *)path {
  [_progressLabel
      setStringValue:NSLocalizedString(
                         @"Opening the installer. Your password is required "
                         @"to finish.",
                         nil)];
  [_progressIndicator stopAnimation:nil];

  // Hand off to Installer.app rather than installing in-process: it owns the
  // authorization prompt, and the package's postinstall restarts the IME.
  BOOL opened =
      [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:path]];
  if (!opened) {
    [self _showErrorWithMessage:
              NSLocalizedString(@"Unable to open the installer.", nil)];
    return;
  }

  [self _quit];
}

- (void)_downloadAndInstall {
  [self _showProgressWindow];

  [_service downloadPackageForRelease:_release
      progress:^(double fraction) {
        [self _updateProgress:fraction];
      }
      completion:^(NSString *path, NSError *error) {
        if (error || ![path length]) {
          [_progressWindow orderOut:nil];
          [self _showErrorWithMessage:[error localizedDescription]];
          return;
        }
        [self _installPackageAtPath:path];
      }];
}

#pragma mark Prompt

- (void)_promptForRelease {
  [NSApp activateIgnoringOtherApps:YES];

  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:
             [NSString stringWithFormat:NSLocalizedString(
                                            @"ChiaKey %@ is available", nil),
                                        [_release tag]]];
  [alert setInformativeText:
             [NSString
                 stringWithFormat:
                     NSLocalizedString(
                         @"You are using %@. Updating restarts the input "
                         @"method, so finish what you are typing first.",
                         nil),
                     [ChiaKeyUpdateService installedApplicationVersion]]];
  [alert addButtonWithTitle:NSLocalizedString(@"Update", nil)];
  [alert addButtonWithTitle:NSLocalizedString(@"Skip This Version", nil)];
  [alert addButtonWithTitle:NSLocalizedString(@"Later", nil)];

  NSModalResponse response = [alert runModal];
  if (response == NSAlertFirstButtonReturn) {
    [self _downloadAndInstall];
    return;
  }
  if (response == NSAlertSecondButtonReturn) {
    [_service skipVersion:[_release tag]];
  }

  [self _quit];
}

#pragma mark NSApplicationDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  _service = [[ChiaKeyUpdateService alloc] init];

  BOOL includeBeta = [[[NSUserDefaults standardUserDefaults]
      objectForKey:@"ChiaKeyApplicationIncludeBetaReleases"] boolValue];

  [_service fetchLatestReleaseIncludingBeta:includeBeta
                                 completion:^(ChiaKeyUpdateRelease *release,
                                              NSError *error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      if (error || !release) {
        // An automatic run stays silent: a failed check is not the user's
        // problem, and the next one is at most a day away.
        if (_userRequested) {
          [self _showErrorWithMessage:[error localizedDescription]];
          return;
        }
        [self _quit];
        return;
      }

      // Without a known installed version there is nothing to compare against,
      // and prompting anyway would offer "updates" to people already ahead.
      NSString *installed = [ChiaKeyUpdateService installedApplicationVersion];
      if (![installed length]) {
        if (_userRequested) {
          [self _showErrorWithMessage:
                    NSLocalizedString(@"Unable to determine the installed "
                                      @"ChiaKey version.",
                                      nil)];
          return;
        }
        [self _quit];
        return;
      }
      if ([ChiaKeyUpdateService compareVersion:installed
                                     toVersion:[release tag]] !=
          NSOrderedAscending) {
        [self _quit];
        return;
      }
      if (![[release packageURL] length]) {
        [self _quit];
        return;
      }

      if (!_userRequested) {
        if ([_service isVersionSkipped:[release tag]]) {
          [self _quit];
          return;
        }
        if (![_service release:release hasSettledForDays:kCKUpdateSoakPeriodDays]) {
          [self _quit];
          return;
        }
        if (CKFrontmostApplicationIsFullScreen()) {
          [self _quit];
          return;
        }
        // Another logged-in user is already handling this update; two admin
        // prompts for the same machine-wide install helps nobody.
        if (![_service acquireInstallLock]) {
          [self _quit];
          return;
        }
      }

      _release = [release retain];
      [self _promptForRelease];
    });
  }];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
  return YES;
}

@end
