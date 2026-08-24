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

// NSImageView animates its GIF off an NSTimer that AppKit does not stop when
// the window stops being visible, and this process outlives every window it
// opens: one look at the About box kept the mascot decoding until logout.
static void CVSetAboutImageAnimation(NSView *view, BOOL animates) {
  if ([view isKindOfClass:[NSImageView class]]) {
    [(NSImageView *)view setAnimates:animates];
  }

  for (NSView *subview in [view subviews]) {
    CVSetAboutImageAnimation(subview, animates);
  }
}

@implementation CVAboutController

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
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

// The nib carries the localized product name; the version is appended from the
// bundle so it cannot drift out of date. Written to be idempotent in case the
// nib is ever loaded more than once.
- (void)_applyVersionToTitle {
  if (![_aboutTextField respondsToSelector:@selector(setStringValue:)]) return;

  NSString *version =
      [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];
  if (![version length]) {
    version = [[NSBundle mainBundle]
        objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
  }
  if (![version length]) return;

  NSString *name = [_aboutTextField stringValue];
  NSRange existing = [name rangeOfString:@" v"];
  if (existing.location != NSNotFound)
    name = [name substringToIndex:existing.location];

  [_aboutTextField
      setStringValue:[NSString stringWithFormat:@"%@ v%@", name, version]];
}

// Turn the "chiaki.ch" substring of the credit line into a clickable link.
// The rest of the label keeps the nib's localized author name untouched.
- (void)_linkifyAuthorLabel {
  if (![_authorLabel respondsToSelector:@selector(setAttributedStringValue:)])
    return;

  NSString *text = [_authorLabel stringValue];
  NSRange range = [text rangeOfString:@"chiaki.ch"];
  if (range.location == NSNotFound) return;

  NSMutableAttributedString *attributed =
      [[[NSMutableAttributedString alloc] initWithString:text] autorelease];
  [attributed addAttribute:NSForegroundColorAttributeName
                     value:[NSColor whiteColor]
                     range:NSMakeRange(0, [text length])];
  [attributed addAttribute:NSLinkAttributeName
                     value:@"https://chiaki.ch"
                     range:range];
  // The default link color is dark blue, unreadable on the near-black
  // background, so give it a light tint and an underline.
  [attributed addAttribute:NSForegroundColorAttributeName
                     value:[NSColor colorWithCalibratedRed:0.4
                                                     green:0.7
                                                      blue:1.0
                                                     alpha:1.0]
                     range:range];
  [attributed addAttribute:NSUnderlineStyleAttributeName
                     value:[NSNumber numberWithInt:NSUnderlineStyleSingle]
                     range:range];

  [_authorLabel setAllowsEditingTextAttributes:YES];
  [_authorLabel setSelectable:YES];
  [_authorLabel setAttributedStringValue:attributed];
}

// Occlusion rather than just close: a window left open behind another one, on
// another Space or minimized is equally invisible and equally not worth
// animating.
- (void)_syncMascotAnimation {
  BOOL visible = [[self window] isVisible] &&
                 ([[self window] occlusionState] &
                  NSWindowOcclusionStateVisible) != 0;
  CVSetAboutImageAnimation([[self window] contentView], visible);
}

- (void)_windowOcclusionStateChanged:(NSNotification *)notification {
  [self _syncMascotAnimation];
}

- (void)_windowWillClose:(NSNotification *)notification {
  // -isVisible is still YES at this point, so settle it outright.
  CVSetAboutImageAnimation([[self window] contentView], NO);
}

- (void)awakeFromNib {
  [self _applyVersionToTitle];
  [self _linkifyAuthorLabel];
  [[self window] setLevel:NSFloatingWindowLevel];
  [[self window] setBackgroundColor:[NSColor colorWithCalibratedWhite:0.05
                                                                alpha:1.0]];
  CVApplyAboutTextStyle([[self window] contentView]);
  defaultWindowSize = [[self window] frame].size;

  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(_windowOcclusionStateChanged:)
                 name:NSWindowDidChangeOcclusionStateNotification
               object:[self window]];
  [center addObserver:self
             selector:@selector(_windowWillClose:)
                 name:NSWindowWillCloseNotification
               object:[self window]];
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
  [self _syncMascotAnimation];
}
- (IBAction)launchCustomerCare:(id)sender {
  [[NSWorkspace sharedWorkspace]
      openURL:[NSURL URLWithString:
                         @"https://github.com/chiakich/ChiaKey/issues"]];
  [[self window] orderOut:self];
  [self _syncMascotAnimation];
}

@end
