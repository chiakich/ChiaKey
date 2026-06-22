#import <Cocoa/Cocoa.h>
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
#import <InputMethodKit/InputMethodKit.h>
#else
#import "InputMethodKitTiger.h"
#endif
#include <string>

#import "CVApplicationController.h"
#import "OpenVanillaConfig.h"
#import "OpenVanillaController.h"
#import "OpenVanillaLoader.h"

using namespace std;

IMKServer *OVInputMethodServer = nil;

int main(int argc, char *argv[]) {
  NSAutoreleasePool *pool = [NSAutoreleasePool new];

  if (argc > 1) {
    NSApplicationLoad();
    [NSRunLoop currentRunLoop];

    id ovService = [[NSConnection
        rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
                                            host:nil] retain];
    if (ovService) {
      // NSLog(@"OpenVanilla DO service obtained: %@",
      // OPENVANILLA_DO_CONNECTION_NAME);

      [ovService setProtocolForProxy:@protocol(OpenVanillaService)];

      string cmd = argv[1];
      string arg = (argc > 2) ? argv[2] : "";

      if (cmd == "reload") {
        NSLog(@"Invoking reload");
        [ovService reloadOpenVanilla];
      } else if (cmd == "modulelist") {
        NSArray *idsAndNames =
            [ovService identifiersAndLocalizedNamesWithPattern:@"*"];

        if (idsAndNames) {
          NSEnumerator *ianEnum = [idsAndNames objectEnumerator];
          id item;
          while (item = [ianEnum nextObject]) {
            NSLog(@"module: %@ (%@)", [item objectAtIndex:0],
                  [item objectAtIndex:1]);
          }
        } else {
          NSLog(@"modulelist failed");
        }
      } else if (cmd == "import") {
        if ([ovService importUserPhraseDBFromFile:
                           [NSString stringWithUTF8String:arg.c_str()]])
          NSLog(@"import succeeded, file: %s", arg.c_str());
        else
          NSLog(@"import failed");
      } else if (cmd == "export") {
        if ([ovService
                exportUserPhraseDBToFile:[NSString
                                             stringWithUTF8String:arg.c_str()]])
          NSLog(@"export succeeded, file: %s", arg.c_str());
        else
          NSLog(@"export failed");
      }
      // remark this in production
      //            else if (cmd == "test") {
      //                if ([ovService userPhraseDBCanProvideService]) {
      //                    int row = [ovService userPhraseDBNumberOfRow];
      //                    NSLog(@"number of user phrase db entries: %d", row);
      //
      //                    for (int i = 0 ; i < row ; i++) {
      //                        NSDictionary *entry = [ovService
      //                        userPhraseDBDictionaryAtRow:i]; NSLog(@"entry %d
      //                        : %@", i, entry);
      //                    }
      //
      //                    NSArray *x = [ovService
      //                    userPhraseDBReadingsForPhrase:@"一個輸入法"]; id n,
      //                    e = [x objectEnumerator]; while (n = [e nextObject])
      //                    {
      //                        NSLog(@"possible sounds: %@", n);
      //                    }
      //                }
      //            }
      else {
        NSLog(@"unknown comomand.");
      }

      [[NSRunLoop currentRunLoop]
             runMode:NSDefaultRunLoopMode
          beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];

      return 0;
    }

    // ignore other strange arguments...
  }

  OVInputMethodServer =
      [[IMKServer alloc] initWithName:OPENVANILLA_CONNECTION_NAME
                     bundleIdentifier:[[NSBundle mainBundle] bundleIdentifier]];

  if (!OVInputMethodServer) {
    NSLog(@"input method server init failed!");
    return 1;
  }

  BOOL result = [NSBundle loadNibNamed:@"MainMenu"
                                 owner:[NSApplication sharedApplication]];
  //	NSLog(@"nib loading result: %d", result);

  NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
  NSString *modulePath =
      [resourcePath stringByAppendingPathComponent:@"Modules"];
  NSArray *loadPaths = [NSArray arrayWithObjects:modulePath, nil];

  // migrates Search.plist -> OneKey.plist
  NSString *libPath = [NSSearchPathForDirectoriesInDomains(
      NSLibraryDirectory, NSUserDomainMask, YES) objectAtIndex:0];
  NSString *prefPath = [libPath stringByAppendingPathComponent:@"Preferences"];
  NSFileManager *mgr = [NSFileManager defaultManager];
  NSString *oldSearchPlist = [prefPath
      stringByAppendingPathComponent:@"com.chiaki.KeyKey.Search.plist"];
  NSString *oneKeyPlist = [prefPath
      stringByAppendingPathComponent:@"com.chiaki.KeyKey.OneKey.plist"];

  if ([mgr fileExistsAtPath:oldSearchPlist]) {
    if (![mgr fileExistsAtPath:oneKeyPlist]) {
      [mgr movePath:oldSearchPlist toPath:oneKeyPlist handler:nil];
    } else {
      [mgr removeFileAtPath:oldSearchPlist handler:nil];
    }
  }

  OpenVanillaLoader *ovl = [OpenVanillaLoader sharedInstance];
  [[NSApp delegate] setLoader:ovl];
  [NSThread detachNewThreadSelector:@selector(start:)
                           toTarget:ovl
                         withObject:loadPaths];

  [[NSApplication sharedApplication] run];

  // [OpenVanillaController cleanUpAutoUpdate];
  [ovl shutDown];
  [OpenVanillaLoader releaseSharedObjects];
  [OVInputMethodServer release];
  [pool drain];
  return 0;
}
