/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
//
//  main.m
//  DownloadUpdate
//

#import <Cocoa/Cocoa.h>

#import "DownloadController.h"

int main(int argc, char *argv[]) {
  NSAutoreleasePool *pool = [NSAutoreleasePool new];
  if (argc < 3) {
    printf(
        "Usage: DownloadUpdate <URL of target file> <URL of the signature "
        "file>\n");
    [pool drain];
    return 1;
  }

  id ovService = [NSConnection
      rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
                                          host:nil];
  if (ovService) {
    [ovService setProtocolForProxy:@protocol(OpenVanillaService)];
    NSString *actionURLString = [NSString stringWithUTF8String:argv[1]];
    NSString *signatureURLString = [NSString stringWithUTF8String:argv[2]];
    NSString *actionFilename = [[actionURLString pathComponents] lastObject];
    NSString *actionFilePath = [[ovService randomTemporaryFilenameWithFullpath]
        stringByAppendingString:actionFilename];
    NSString *signatureFilePath =
        [[ovService randomTemporaryFilenameWithFullpath]
            stringByAppendingString:@".sig"];
    NSString *changelogURLString = @"";
    if (argc > 3) {
      changelogURLString = [NSString stringWithUTF8String:argv[3]];
    }
    [[DownloadManager sharedManager] setDownloadURLString:actionURLString
                                       signatureURLString:signatureURLString
                                           actionFilePath:actionFilePath
                                        signatureFilePath:signatureFilePath
                                       changelogURLString:changelogURLString];

  } else {
    [pool drain];
    return 1;
  }
  NSApplicationMain(argc, (const char **)argv);
  [pool drain];
  return 0;
}
