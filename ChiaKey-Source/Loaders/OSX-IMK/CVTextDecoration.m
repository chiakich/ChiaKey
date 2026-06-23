// [AUTO_HEADER]
#import "CVTextDecoration.h"

#import "NSBezierPathAdditions.h"

@implementation CVTextDecoration
+ (NSImage *)imageWithACharacterInACircle:(NSString *)character
                               foreground:(NSColor *)foregroundColor
                               background:(NSColor *)backgroundColor {
  NSImage *image =
      [[[NSImage alloc] initWithSize:NSMakeSize(16, 16)] autorelease];
  [image lockFocus];
  [backgroundColor setFill];
  NSBezierPath *circle =
      [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(0, 0, 16, 16)];
  [circle fill];
  NSFont *font = [NSFont fontWithName:@"Helvetica" size:12];
  NSMutableParagraphStyle *paragraphStyle =
      [[NSMutableParagraphStyle new] autorelease];
  [paragraphStyle setAlignment:NSTextAlignmentCenter];
  NSDictionary *attributes = [NSDictionary
      dictionaryWithObjectsAndKeys:font, NSFontAttributeName, foregroundColor,
                                   NSForegroundColorAttributeName,
                                   paragraphStyle,
                                   NSParagraphStyleAttributeName, nil];
  [character drawInRect:NSMakeRect(0, 0.5, 16, 16) withAttributes:attributes];
  [image unlockFocus];
  return image;
}
+ (NSImage *)imageWithACharacterInARectangle:(NSString *)character
                                  foreground:(NSColor *)foregroundColor
                                  background:(NSColor *)backgroundColor {
  NSImage *image =
      [[[NSImage alloc] initWithSize:NSMakeSize(16, 16)] autorelease];
  [image lockFocus];
  [backgroundColor setFill];
  NSBezierPath *rectangle =
      [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(0, 0, 16, 16)];
  [rectangle fill];
  NSFont *font = [NSFont fontWithName:@"Helvetica" size:12];
  NSMutableParagraphStyle *paragraphStyle =
      [[NSMutableParagraphStyle new] autorelease];
  [paragraphStyle setAlignment:NSTextAlignmentCenter];
  NSDictionary *attributes = [NSDictionary
      dictionaryWithObjectsAndKeys:font, NSFontAttributeName, foregroundColor,
                                   NSForegroundColorAttributeName,
                                   paragraphStyle,
                                   NSParagraphStyleAttributeName, nil];
  [character drawInRect:NSMakeRect(0, 0.5, 16, 16) withAttributes:attributes];
  [image unlockFocus];
  return image;
}
+ (NSImage *)imageWithACharacterInARoundedRectangle:(NSString *)character
                                         foreground:(NSColor *)foregroundColor
                                         background:(NSColor *)backgroundColor {
  NSImage *image =
      [[[NSImage alloc] initWithSize:NSMakeSize(16, 16)] autorelease];
  [image lockFocus];
  [backgroundColor setFill];

#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
  NSBezierPath *rectangle =
      [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(0, 0, 16, 16)
                                      xRadius:4
                                      yRadius:4];
#else
#warning Gee, they have implemented what we'd done with LFExtensions... here again
  NSBezierPath *rectangle =
      [NSBezierPath bezelPathWithRect:NSMakeRect(0, 0, 16, 16) radius:4];
#endif

  [rectangle fill];
  NSFont *font = [NSFont fontWithName:@"Helvetica" size:12];
  NSMutableParagraphStyle *paragraphStyle =
      [[NSMutableParagraphStyle new] autorelease];
  [paragraphStyle setAlignment:NSTextAlignmentCenter];
  NSDictionary *attributes = [NSDictionary
      dictionaryWithObjectsAndKeys:font, NSFontAttributeName, foregroundColor,
                                   NSForegroundColorAttributeName,
                                   paragraphStyle,
                                   NSParagraphStyleAttributeName, nil];
  [character drawInRect:NSMakeRect(0, 0.5, 16, 16) withAttributes:attributes];
  [image unlockFocus];
  return image;
}

@end
