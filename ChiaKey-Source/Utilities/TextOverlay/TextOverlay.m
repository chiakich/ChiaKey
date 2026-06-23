/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
#import <Cocoa/Cocoa.h>

int main(int argc, const char *argv[]) {
  if (argc < 3) {
    fprintf(stderr,
            "usage: TextOverlay <source> <target> <text> <x> <y> <size> "
            "[typeface]\n");
    return 1;
  }

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSApplicationLoad();

  NSString *sourceFilename = [NSString stringWithUTF8String:argv[1]];
  NSString *targetFilename = [NSString stringWithUTF8String:argv[2]];
  NSString *text = [NSString stringWithUTF8String:argv[3]];
  CGFloat originX = atof(argv[4]);
  CGFloat originY = atof(argv[5]);
  CGFloat size = atof(argv[6]);
  NSString *typeface = @"Helvetica";

  if (argc > 7) {
    typeface = [NSString stringWithUTF8String:argv[7]];
  }

  NSImage *sourceImage =
      [[[NSImage alloc] initWithContentsOfFile:sourceFilename] autorelease];
  if (!sourceImage) {
    fprintf(stderr, "cannot create image from: %s\n",
            [sourceFilename UTF8String]);
    [pool drain];
    return 1;
  }

  NSImage *targetImage =
      [[[NSImage alloc] initWithSize:[sourceImage size]] autorelease];
  [targetImage setCacheMode:NSImageCacheNever];
  [targetImage lockFocus];
  [sourceImage compositeToPoint:NSMakePoint(0.0, 0.0)
                      operation:NSCompositeCopy];

  NSFont *font = [NSFont fontWithName:typeface size:size];
  NSAttributedString *attrString = [[[NSAttributedString alloc]
      initWithString:text
          attributes:[NSDictionary
                         dictionaryWithObjectsAndKeys:font, NSFontAttributeName,
                                                      nil]] autorelease];
  [attrString drawAtPoint:NSMakePoint(originX, originY)];

  NSBitmapImageRep *bitmapImage = [[NSBitmapImageRep alloc]
      initWithFocusedViewRect:NSMakeRect(0.0, 0.0, [sourceImage size].width,
                                         [sourceImage size].height)];
  [targetImage unlockFocus];
  [targetImage recache];

  NSArray *representations = [targetImage representations];
  NSLog([representations description]);
  NSBitmapImageFileType imageType = NSPNGFileType;
  NSDictionary *imageProperties = nil;
  NSData *bitmapData;

  if ([targetFilename hasSuffix:@"jpg"] || [targetFilename hasSuffix:@"jpeg"]) {
    imageType = NSJPEGFileType;
    imageProperties =
        [NSDictionary dictionaryWithObject:[NSNumber numberWithFloat:1.0]
                                    forKey:NSImageCompressionFactor];
  } else if ([targetFilename hasSuffix:@"tif"] ||
             [targetFilename hasSuffix:@"tiff"]) {
    imageType = NSTIFFFileType;
  }

  bitmapData = [bitmapImage representationUsingType:imageType
                                         properties:imageProperties];
  [bitmapData writeToFile:targetFilename atomically:YES];
  [pool drain];
  return 0;
}
