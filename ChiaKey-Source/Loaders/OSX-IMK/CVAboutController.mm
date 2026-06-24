// [AUTO_HEADER]

#import "CVAboutController.h"

#import "CVApplicationController.h"

static void CVApplyAboutTextStyle(NSView *view) {
  if ([view isKindOfClass:[NSTextField class]]) {
    NSTextField *textField = (NSTextField *)view;
    [textField setTextColor:[NSColor whiteColor]];
    [textField setBackgroundColor:[NSColor clearColor]];
    [textField setDrawsBackground:NO];
  }

  for (NSView *subview in [view subviews]) {
    CVApplyAboutTextStyle(subview);
  }
}

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
  [[self window] setBackgroundColor:[NSColor colorWithCalibratedWhite:0.05
                                                                alpha:1.0]];
  CVApplyAboutTextStyle([[self window] contentView]);
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
    frame.size.height += 110;
    if (!_wordCountController) {
      _wordCountController = [[TakaoWordCount alloc] init];
    }
    [_wordCountController update];
    [[self window] setFrame:frame display:NO];
    NSView *view = [_wordCountController view];
    [view setFrame:NSMakeRect((defaultWindowSize.width - 260) / 2,
                              defaultWindowSize.height + 5, 260, 100)];
    CVApplyAboutTextStyle(view);
    [[[self window] contentView] addSubview:view];
  } else {
    if (_wordCountController && [[_wordCountController view] superview]) {
      [[_wordCountController view] removeFromSuperview];
      NSRect frame = [[self window] frame];
      frame.size = defaultWindowSize;
      [[self window] setFrame:frame display:YES];
    }
  }
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
                         @"https://github.com/akira02/ChiaKey/issues"]];
  [[self window] orderOut:self];
}

@end
