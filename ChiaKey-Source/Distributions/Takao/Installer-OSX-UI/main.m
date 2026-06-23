//
//  main.m
//  InstallerUI
//
//  Created by zonble on 2008/4/7.
//  Copyright __MyCompanyName__ 2008. All rights reserved.
//

#import <Cocoa/Cocoa.h>

int main(int argc, char *argv[]) {
  NSAutoreleasePool *pool = [NSAutoreleasePool new];
  NSString *appPath = [[NSBundle mainBundle] bundlePath];
  NSString *dmgPath = [appPath stringByDeletingLastPathComponent];
  NSString *pkgPath =
      [dmgPath stringByAppendingPathComponent:@".Package/KeyKey.pkg"];
  // NSLog(@"path: %@", pkgPath);
  if ([[NSWorkspace sharedWorkspace] openFile:pkgPath]) {
    // NSLog(@"Succesfully open");
  } else {
    NSLog(@"Failed to open");
  }
  [pool drain];
  return 0;
}
