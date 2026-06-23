// [AUTO_HEADER]

#import "CVKeyboardHelper.h"
#import "../../Frameworks/ChiakiSupport/Headers/ChiakiTextInputSource.h"

@implementation CVKeyboardHelper

static CVKeyboardHelper *_sharedHelper = nil;

+ (CVKeyboardHelper *)sharedSendKey {
  if (_sharedHelper == nil) _sharedHelper = [[CVKeyboardHelper alloc] init];
  return _sharedHelper;
}
- (id)init {
  self = [super init];
  if (self != nil) {
    _validKeyboardLayouts = [[NSMutableArray alloc] init];
    [self loadValidKeyboardLayouts];
  }
  return self;
}
- (void)dealloc {
  [_validKeyboardLayouts release];
  [super dealloc];
}
- (void)loadValidKeyboardLayouts {
  CFArrayRef list = TISCreateInputSourceList(NULL, true);
  int i = 0;
  for (i = 0; i < CFArrayGetCount(list); i++) {
    TISInputSourceRef source =
        (TISInputSourceRef)CFArrayGetValueAtIndex(list, i);

    CFStringRef type =
        TISGetInputSourceProperty(source, kTISPropertyInputSourceType);
    if (CFStringCompare(type, kTISTypeKeyboardLayout, kCFCompareEqualTo)) {
      continue;
    }

    CFStringRef sourceId =
        TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
    NSString *sourceIdString = [NSString stringWithString:(NSString *)sourceId];
    [_validKeyboardLayouts addObject:sourceIdString];
  }
  CFRelease(list);
}
- (NSArray *)validKeyboardLayouts {
  return _validKeyboardLayouts;
}
- (BOOL)validateKeyboardLayout:(NSString *)layout {
  if (!_validKeyboardLayouts || ![_validKeyboardLayouts count]) return NO;

  NSEnumerator *e = [_validKeyboardLayouts objectEnumerator];
  NSString *currentLayout;
  while (currentLayout = [e nextObject]) {
    if ([layout isEqualToString:currentLayout]) return YES;
  }
  return NO;
}
@end
