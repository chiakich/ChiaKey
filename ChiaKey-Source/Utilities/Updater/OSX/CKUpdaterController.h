//
//  CKUpdaterController.h
//
//  Drives one update session end to end: ask, download, hand off to the
//  system installer. Runs in its own process so a stalled download or a
//  wedged dialog can never reach the input path.
//

#import <AppKit/AppKit.h>

@class ChiaKeyUpdateRelease;
@class ChiaKeyUpdateService;

@interface CKUpdaterController : NSObject <NSApplicationDelegate> {
  ChiaKeyUpdateService *_service;
  ChiaKeyUpdateRelease *_release;
  NSWindow *_progressWindow;
  NSProgressIndicator *_progressIndicator;
  NSTextField *_progressLabel;
  BOOL _userRequested;
}

// YES when a human asked for the check, which disables the quiet-hours and
// soak-period suppressions that only make sense for automatic runs.
@property(nonatomic, assign) BOOL userRequested;

@end
