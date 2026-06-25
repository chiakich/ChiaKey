#import <Cocoa/Cocoa.h>
#import <InputMethodKit/InputMethodKit.h>
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
