// [AUTO_HEADER]

#import "CVSendKey.h"

#import <ApplicationServices/ApplicationServices.h>
#import <ctype.h>
#import <string.h>

static BOOL CVSKKeyCodeForASCII(unsigned char c, CGKeyCode *keyCode,
                                BOOL *requiresShift) {
  *requiresShift = NO;

  switch (c) {
    case 'a':
    case 'A':
      *keyCode = 0;
      *requiresShift = isupper(c);
      return YES;
    case 's':
    case 'S':
      *keyCode = 1;
      *requiresShift = isupper(c);
      return YES;
    case 'd':
    case 'D':
      *keyCode = 2;
      *requiresShift = isupper(c);
      return YES;
    case 'f':
    case 'F':
      *keyCode = 3;
      *requiresShift = isupper(c);
      return YES;
    case 'h':
    case 'H':
      *keyCode = 4;
      *requiresShift = isupper(c);
      return YES;
    case 'g':
    case 'G':
      *keyCode = 5;
      *requiresShift = isupper(c);
      return YES;
    case 'z':
    case 'Z':
      *keyCode = 6;
      *requiresShift = isupper(c);
      return YES;
    case 'x':
    case 'X':
      *keyCode = 7;
      *requiresShift = isupper(c);
      return YES;
    case 'c':
    case 'C':
      *keyCode = 8;
      *requiresShift = isupper(c);
      return YES;
    case 'v':
    case 'V':
      *keyCode = 9;
      *requiresShift = isupper(c);
      return YES;
    case 'b':
    case 'B':
      *keyCode = 11;
      *requiresShift = isupper(c);
      return YES;
    case 'q':
    case 'Q':
      *keyCode = 12;
      *requiresShift = isupper(c);
      return YES;
    case 'w':
    case 'W':
      *keyCode = 13;
      *requiresShift = isupper(c);
      return YES;
    case 'e':
    case 'E':
      *keyCode = 14;
      *requiresShift = isupper(c);
      return YES;
    case 'r':
    case 'R':
      *keyCode = 15;
      *requiresShift = isupper(c);
      return YES;
    case 'y':
    case 'Y':
      *keyCode = 16;
      *requiresShift = isupper(c);
      return YES;
    case 't':
    case 'T':
      *keyCode = 17;
      *requiresShift = isupper(c);
      return YES;
    case 'o':
    case 'O':
      *keyCode = 31;
      *requiresShift = isupper(c);
      return YES;
    case 'u':
    case 'U':
      *keyCode = 32;
      *requiresShift = isupper(c);
      return YES;
    case 'i':
    case 'I':
      *keyCode = 34;
      *requiresShift = isupper(c);
      return YES;
    case 'p':
    case 'P':
      *keyCode = 35;
      *requiresShift = isupper(c);
      return YES;
    case 'l':
    case 'L':
      *keyCode = 37;
      *requiresShift = isupper(c);
      return YES;
    case 'j':
    case 'J':
      *keyCode = 38;
      *requiresShift = isupper(c);
      return YES;
    case 'k':
    case 'K':
      *keyCode = 40;
      *requiresShift = isupper(c);
      return YES;
    case 'n':
    case 'N':
      *keyCode = 45;
      *requiresShift = isupper(c);
      return YES;
    case 'm':
    case 'M':
      *keyCode = 46;
      *requiresShift = isupper(c);
      return YES;
    case '1':
    case '!':
      *keyCode = 18;
      *requiresShift = (c == '!');
      return YES;
    case '2':
    case '@':
      *keyCode = 19;
      *requiresShift = (c == '@');
      return YES;
    case '3':
    case '#':
      *keyCode = 20;
      *requiresShift = (c == '#');
      return YES;
    case '4':
    case '$':
      *keyCode = 21;
      *requiresShift = (c == '$');
      return YES;
    case '6':
    case '^':
      *keyCode = 22;
      *requiresShift = (c == '^');
      return YES;
    case '5':
    case '%':
      *keyCode = 23;
      *requiresShift = (c == '%');
      return YES;
    case '=':
    case '+':
      *keyCode = 24;
      *requiresShift = (c == '+');
      return YES;
    case '9':
    case '(':
      *keyCode = 25;
      *requiresShift = (c == '(');
      return YES;
    case '7':
    case '&':
      *keyCode = 26;
      *requiresShift = (c == '&');
      return YES;
    case '-':
    case '_':
      *keyCode = 27;
      *requiresShift = (c == '_');
      return YES;
    case '8':
    case '*':
      *keyCode = 28;
      *requiresShift = (c == '*');
      return YES;
    case '0':
    case ')':
      *keyCode = 29;
      *requiresShift = (c == ')');
      return YES;
    case ']':
    case '}':
      *keyCode = 30;
      *requiresShift = (c == '}');
      return YES;
    case '[':
    case '{':
      *keyCode = 33;
      *requiresShift = (c == '{');
      return YES;
    case '\'':
    case '"':
      *keyCode = 39;
      *requiresShift = (c == '"');
      return YES;
    case ';':
    case ':':
      *keyCode = 41;
      *requiresShift = (c == ':');
      return YES;
    case '\\':
    case '|':
      *keyCode = 42;
      *requiresShift = (c == '|');
      return YES;
    case ',':
    case '<':
      *keyCode = 43;
      *requiresShift = (c == '<');
      return YES;
    case '/':
    case '?':
      *keyCode = 44;
      *requiresShift = (c == '?');
      return YES;
    case '.':
    case '>':
      *keyCode = 47;
      *requiresShift = (c == '>');
      return YES;
    case ' ':
      *keyCode = 49;
      return YES;
    case '`':
    case '~':
      *keyCode = 50;
      *requiresShift = (c == '~');
      return YES;
    case '\t':
      *keyCode = kVirtualTabKey;
      return YES;
    case '\n':
    case '\r':
      *keyCode = kVirtualReturnKey;
      return YES;
    default:
      return NO;
  }
}

static void CVSKPostKeyboardEvent(CGKeyCode keyCode, BOOL keyDown,
                                  CGEventFlags flags) {
  CGEventRef event = CGEventCreateKeyboardEvent(NULL, keyCode, keyDown);
  if (!event) return;

  CGEventSetFlags(event, flags);
  CGEventPost(kCGHIDEventTap, event);
  CFRelease(event);
}

@implementation CVSendKey

static CVSendKey *_sharedSendKey = nil;

+ (CVSendKey *)sharedSendKey {
  if (_sharedSendKey == nil) _sharedSendKey = [[CVSendKey alloc] init];
  return _sharedSendKey;
}

- (void)_typeString:(NSString *)string {
  const char *s = [string UTF8String];
  NSUInteger length = strlen(s);
  for (NSUInteger i = 0; i < length; i++) {
    CGKeyCode code = 0;
    BOOL requiresShift = NO;

    if (!CVSKKeyCodeForASCII((unsigned char)s[i], &code, &requiresShift))
      continue;

    CGEventFlags flags = requiresShift ? kCGEventFlagMaskShift : 0;
    if (requiresShift) CVSKPostKeyboardEvent(kVirtualShiftKey, YES, flags);
    CVSKPostKeyboardEvent(code, YES, flags);
    CVSKPostKeyboardEvent(code, NO, flags);
    if (requiresShift) CVSKPostKeyboardEvent(kVirtualShiftKey, NO, 0);
  }
}

- (void)typeString:(NSString *)string {
  [self performSelector:@selector(_typeString:)
             withObject:string
             afterDelay:0.1];
}

@end
