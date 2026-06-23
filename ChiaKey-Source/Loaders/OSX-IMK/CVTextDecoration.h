// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>
/*!
        @class CVTextDecoration
        @abstract A helper class which generates some images
        with text and some special background. These images are
        used in the candidate window for visual effect.
*/
@interface CVTextDecoration : NSObject {
}
/*!
        @method imageWithACharacterInACircle:foreground:background:
        @abstract Generates an image in 16x16 pixel size, the background
        of the image is a circle and there is a character displayed in the
        center.
        @param character A character placed in the center of the image.
        @param foregroundColor The foreground color.
        @param backgroundColor The background color.
        @result A NSImage object.
*/
+ (NSImage *)imageWithACharacterInACircle:(NSString *)character
                               foreground:(NSColor *)foregroundColor
                               background:(NSColor *)backgroundColor;
/*!
        @method imageWithACharacterInARectangle:foreground:background:
        @abstract Generates an image in 16x16 pixel size, the background
        of the image is a rectangle and there is a character displayed in the
        center.
        @param character A character placed in the center of the image.
        @param foregroundColor The foreground color.
        @param backgroundColor The background color.
        @result A NSImage object.
*/
+ (NSImage *)imageWithACharacterInARectangle:(NSString *)character
                                  foreground:(NSColor *)foregroundColor
                                  background:(NSColor *)backgroundColor;
/*!
        @method imageWithACharacterInARoundedRectangle:foreground:background:
        @abstract Generates an image in 16x16 pixel size, the background
        of the image is a rounded corner rectangle and there is a character
        displayed in the center.
        @param character A character placed in the center of the image.
        @param foregroundColor The foreground color.
        @param backgroundColor The background color.
        @result A NSImage object.
*/
+ (NSImage *)imageWithACharacterInARoundedRectangle:(NSString *)character
                                         foreground:(NSColor *)foregroundColor
                                         background:(NSColor *)backgroundColor;
@end
