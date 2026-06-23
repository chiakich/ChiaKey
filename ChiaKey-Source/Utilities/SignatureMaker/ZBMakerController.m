//
//  ZBMakerController.m
//  SignatureMaker
//
/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#import "ZBMakerController.h"

@implementation ZBMakerController

- (void)awakeFromNib {
  [[self window] center];
  //	[[self window] setLevel:NSFloatingWindowLevel];
  [[self window] setDelegate:self];
  //	[u_dropTarget setDelegate:self];
  NSArray *types = [NSArray arrayWithObjects:NSFilenamesPboardType, nil];
  [[self window] registerForDraggedTypes:types];
}

- (void)sign:(NSString *)file {
  NSString *rubyFilePath = [[NSBundle mainBundle] pathForResource:@"digestsign"
                                                           ofType:@"rb"];
  NSString *keyFilePath =
      [[NSBundle mainBundle] pathForResource:@"DevPrivateKey" ofType:@"pem"];
  NSString *outputFilePath = [[file stringByDeletingPathExtension]
      stringByAppendingPathExtension:@"sig"];

  if (![[NSFileManager defaultManager] isWritableFileAtPath:outputFilePath]) {
    NSString *outputFileName = [outputFilePath lastPathComponent];
    NSString *desktopPath = [NSSearchPathForDirectoriesInDomains(
        NSDesktopDirectory, NSUserDomainMask, YES) objectAtIndex:0];
    outputFilePath =
        [desktopPath stringByAppendingPathComponent:outputFileName];
  }

  NSPipe *pipe = [NSPipe pipe];
  NSFileHandle *fileHandle = [pipe fileHandleForReading];

  NSTask *task = [[[NSTask alloc] init] autorelease];
  [task setLaunchPath:@"/usr/bin/ruby"];
  [task setArguments:[NSArray arrayWithObjects:rubyFilePath, file, keyFilePath,
                                               nil]];
  [task setStandardOutput:pipe];
  [task launch];
  NSData *data = [fileHandle readDataToEndOfFile];
  [data writeToFile:outputFilePath atomically:YES];
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  NSPasteboard *pb = [sender draggingPasteboard];
  if ([pb availableTypeFromArray:[NSArray
                                     arrayWithObject:NSFilenamesPboardType]]) {
    return NSDragOperationCopy;
  }
  return NSDragOperationNone;
}
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  NSPasteboard *pb = [sender draggingPasteboard];
  if ([pb availableTypeFromArray:[NSArray
                                     arrayWithObject:NSFilenamesPboardType]]) {
    NSArray *files = [pb propertyListForType:NSFilenamesPboardType];
    NSEnumerator *enumerator = [files objectEnumerator];
    NSString *file;
    while (file = [enumerator nextObject]) {
      NSFileWrapper *wrapper = [[NSFileWrapper alloc] initWithPath:file];
      if ([wrapper isRegularFile]) [self sign:file];
    }
    return YES;
  }
  return NO;
}

- (void)windowWillClose:(NSNotification *)notification {
  [NSApp terminate:self];
}
@end
