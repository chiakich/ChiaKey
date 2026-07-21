// [AUTO_HEADER]

#import <AppKit/AppKit.h>

#import "CKUpdaterController.h"

int main(int argc, const char *argv[]) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  BOOL userRequested = NO;
  for (int index = 1; index < argc; index++) {
    if (strcmp(argv[index], "--user-requested") == 0) userRequested = YES;
  }

  NSApplication *application = [NSApplication sharedApplication];
  // Accessory, not regular: an automatic check that decides to stay quiet must
  // not have flashed a Dock icon on the way.
  [application setActivationPolicy:NSApplicationActivationPolicyAccessory];

  CKUpdaterController *controller = [[CKUpdaterController alloc] init];
  [controller setUserRequested:userRequested];
  [application setDelegate:controller];
  [application run];

  [controller release];
  [pool drain];
  return 0;
}
