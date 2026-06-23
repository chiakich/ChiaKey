// [AUTO_HEADER]

#import "NSStringExtension.h"

@implementation NSString (SplitBySpaceWithQuote)
+ (NSString *)stringByColor:(NSColor *)c {
  NSColor *convertedColor =
      [c colorUsingColorSpace:[NSColorSpace genericRGBColorSpace]];
  if (!convertedColor) convertedColor = c;
  return [NSString stringWithFormat:@"%f %f %f", [convertedColor redComponent],
                                    [convertedColor greenComponent],
                                    [convertedColor blueComponent]];
}
- (NSColor *)colorByString {
  NSArray *s = [self splitBySpaceWithQuote];
  if ([s count] < 3) return nil;

  return [NSColor colorWithCalibratedRed:[[s objectAtIndex:0] floatValue]
                                   green:[[s objectAtIndex:1] floatValue]
                                    blue:[[s objectAtIndex:2] floatValue]
                                   alpha:1.0];
}
- (NSString *)stringByQuoting {
  NSUInteger l = [self length];
  UniChar *s = (UniChar *)calloc(1, l * sizeof(UniChar));
  [self getCharacters:s];
  UniChar *buf = (UniChar *)calloc(1, (l * 2 + 2) * sizeof(UniChar));
  NSUInteger i, p = 0;

  buf[p++] = '\"';
  for (i = 0; i < l; i++) {
    if (s[i] == '\"') buf[p++] = '\\';
    buf[p++] = s[i];
  }
  buf[p++] = '\"';
  NSString *r = [NSString stringWithCharacters:buf length:p];
  free(s);
  free(buf);
  return r;
}
- (NSString *)stringByChomping {
  NSUInteger l = [self length];
  if (!l) return @"";

  NSInteger i;
  for (i = (NSInteger)l - 1; i >= 0; i--) {
    if ([self characterAtIndex:i] != '\n') break;
  }

  if (i == -1) return @"";

  NSRange r = (NSRange){0, static_cast<NSUInteger>(i + 1)};
  return [self substringWithRange:r];
}
- (NSArray *)splitBySpaceWithQuote {
  NSMutableArray *ma = [NSMutableArray array];
  NSUInteger p = 0, l = [self length];
  int q = 0;
  UniChar *s = (UniChar *)calloc(1, l * sizeof(UniChar));
  [self getCharacters:s];
  UniChar *buf = (UniChar *)calloc(1, l * sizeof(UniChar));
  NSUInteger bufp = 0;
  int emptystr = 0;

  while (p < l) {
    // skip spaces
    if (s[p] == 32) p++;

    // first non-space char
    if (s[p] == '\"') {
      q = 1;
      p++;
    }

    bufp = 0;
    emptystr = 0;
    while (p < l) {
      if (!q && (s[p] == 32 || s[p] == '\"')) break;
      if (q && s[p] == '\"') {
        q = 0;
        p++;
        if (!bufp) emptystr = 1;  // make an emptystr regardless
        break;
      }
      if (s[p] == '\\' && p + 1 < l && s[p + 1] == '\"') {
        p += 2;
        buf[bufp++] = '\"';
      } else
        buf[bufp++] = s[p++];
    }

    if (bufp || emptystr) {
      [ma addObject:[NSString stringWithCharacters:buf length:bufp]];
    }
  }

  free(s);
  free(buf);
  return ma;
}
@end
