// [AUTO_HEADER]

#import "CVAboutController.h"

#import "CVApplicationController.h"
#import "OpenVanillaLoader.h"
#import "Version.h"

@implementation CVAboutController

- (void)dealloc {
  [_wordCountController release];
  [super dealloc];
}

- (id)init {
  self = [super init];
  if (self != nil) {
    BOOL loaded = [[NSBundle mainBundle] loadNibNamed:@"AboutWindow" owner:self topLevelObjects:nil];
    NSAssert((loaded == YES), @"NIB did not load");
    _wordCountController = nil;
  }
  return self;
}

- (void)awakeFromNib {
  [[self window] setLevel:NSFloatingWindowLevel];
  [[self window] setBackgroundColor:[NSColor whiteColor]];
  defaultWindowSize = [[self window] frame].size;
}

- (void)_updateContent {
  NSArray *info = [(CVApplicationController *)[NSApp delegate]
      dynamicallyLoadedModulePackageInfo];
  BOOL useWordCount = NO;
  if ([info count]) {
    NSEnumerator *e = [info objectEnumerator];
    NSDictionary *d = nil;
    while (d = [e nextObject]) {
      if ([[d valueForKey:OVServiceLoadedModulePackageIdentifierKey]
              isEqualToString:@"YKAFWordCount"]) {
        useWordCount =
            [[d valueForKey:OVServiceLoadedModulePackageEnabledKey] boolValue];
        break;
      }
    }
  }
  if (useWordCount) {
    NSRect frame = [[self window] frame];
    frame.size = defaultWindowSize;
    frame.size.height += 60;
    if (!_wordCountController) {
      _wordCountController = [[TakaoWordCount alloc] init];
    }
    [_wordCountController update];
    [[self window] setFrame:frame display:NO];
    NSView *view = [_wordCountController view];
    [view setFrame:NSMakeRect(20, 50, defaultWindowSize.width - 40, 100)];
    [[[self window] contentView] addSubview:view];
  } else {
    if (_wordCountController && [[_wordCountController view] superview]) {
      [[_wordCountController view] removeFromSuperview];
      NSRect frame = [[self window] frame];
      frame.size = defaultWindowSize;
      [[self window] setFrame:frame display:YES];
    }
  }

  NSString *name =
      [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleDisplayName"];
  if (!name)
    name = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
  NSString *version =
      [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];

#if (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5)
  bool tiger = true;
#else
  bool tiger = false;
#endif

  VersionNumber verNum([version UTF8String]);

  const char *vn =
      [[[OpenVanillaLoader sharedInstance] databaseVersion] UTF8String];
  VersionNumber dbNum;

  if (vn) {
    dbNum = VersionNumber(vn);
  }

  if (!tiger) {
    version =
        [NSString stringWithFormat:@"%d.%d\n(build %d, database %d)",
                                   verNum.majorVersion(), verNum.minorVersion(),
                                   verNum.buildNumber(), dbNum.buildNumber()];
  } else {
    version = [NSString
        stringWithFormat:@"%d.%d\n(build %d for OS X Tiger, database %d)",
                         verNum.majorVersion(), verNum.minorVersion(),
                         verNum.buildNumber(), dbNum.buildNumber()];
  }

  NSMutableString *copyright = [[[[NSBundle mainBundle]
      objectForInfoDictionaryKey:@"NSHumanReadableCopyright"] mutableCopy]
      autorelease];

  // insert a \n between "Taiwan All Rights Reserved"...
  NSRange range = [copyright rangeOfString:@"Taiwan All"];
  if (range.location != NSNotFound) {
    [copyright replaceCharactersInRange:range withString:@"Taiwan\nAll"];
  }

  [_aboutTextField
      setStringValue:[NSString stringWithFormat:@"%@ %@\n%@", name, version,
                                                copyright]];
  init = YES;
}

#pragma mark Interface Builder actions

- (IBAction)showWindow:(id)sender {
  if (![[self window] isVisible]) {
    [self _updateContent];
    [[self window] center];
  }
  [[self window] orderFront:self];
}
- (IBAction)launchCustomerCare:(id)sender {
  [[NSWorkspace sharedWorkspace]
      openURL:[NSURL URLWithString:
                         @"http://tw.help.cc.yahoo.com/feedback.html?id=3430"]];
  [[self window] orderOut:self];
}

@end
