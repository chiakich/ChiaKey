// [AUTO_HEADER]

#import "NSColor+LFColorExtensions.h"
#import "NSStringExtension.h"
#import "OpenVanillaLoader.h"

@implementation NSColor (LFColorExtensions)

+ (NSColor *)highlightGradientFromColor {
  PVPlistValue *dict = [OpenVanillaLoader sharedLoader]->configRootDictionary();
  if (dict) {
    PVPlistValue *colorValue = dict->valueForKey("HighlightColor");
    if (colorValue) {
      string colorString = colorValue->stringValue();
      NSString *color = [NSString stringWithUTF8String:colorString.c_str()];
      if ([color isEqualToString:@"Default"] ||
          [color isEqualToString:@"Purple"]) {
        return [NSColor purpleColor];
      } else if ([color isEqualToString:@"Green"]) {
        return [NSColor colorWithCalibratedRed:0.23
                                         green:0.68
                                          blue:0.12
                                         alpha:1.00];
      } else if ([color isEqualToString:@"Yellow"]) {
        return [NSColor yellowColor];
      } else if ([color isEqualToString:@"Red"]) {
        return [NSColor colorWithCalibratedRed:0.75
                                         green:0.00
                                          blue:0.16
                                         alpha:1.00];
      } else {
        NSColor *customColor = [color colorByString];
        if (customColor) return customColor;
      }
    }
  }
  return [NSColor purpleColor];
}
+ (NSColor *)highlightGradientToColor;
{
  NSColor *color = [NSColor highlightGradientFromColor];
  return [color gradientToColor];
}
- (NSColor *)gradientToColor {
  NSColor *fromColor =
      [self colorUsingColorSpace:[NSColorSpace genericRGBColorSpace]];
  if (!fromColor) fromColor = self;

  float hue = [fromColor hueComponent];
  float saturation = [fromColor saturationComponent];
  float brightness = [fromColor brightnessComponent] / 3.0;
  NSColor *toColor = [NSColor colorWithCalibratedHue:hue
                                          saturation:saturation
                                          brightness:brightness
                                               alpha:1.0];
  return toColor;
}

@end
