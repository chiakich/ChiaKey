// [AUTO_HEADER]

#import "CVSymbolController.h"

#import "CVButtonViewController.h"
#import "CVSmileyViewController.h"
#import "OpenVanillaController.h"

static const CGFloat CVSymbolWindowScreenPadding = 20.0;

@implementation CVSymbolController

- (id)init {
  self = [super init];
  if (self != nil) {
    BOOL loaded = [[NSBundle mainBundle] loadNibNamed:@"SymbolWindow" owner:self topLevelObjects:nil];
    NSAssert((loaded == YES), @"NIB did not load");
    _viewArray = [NSMutableArray new];
  }
  return self;
}
- (void)dealloc {
  [_viewArray release];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:CVLoaderUpdateCannedMessagesNotification
              object:nil];
  [super dealloc];
}
- (void)loadSymbolTable:(NSNotification *)notification {
  // NSLog(@"received %@", notification);

  if ([_viewArray count]) [_viewArray removeAllObjects];

  NSString *locale =
      [NSString stringWithUTF8String:[OpenVanillaLoader sharedLoaderService]
                                         ->locale()
                                         .c_str()];
  NSArray *array =
      [[OpenVanillaLoader sharedInstance] mergedCannedMessagesArray];
  [_popUpButton removeAllItems];
  NSDictionary *d = nil;
  NSEnumerator *enumerator = [array objectEnumerator];

  while (d = [enumerator nextObject]) {
    if ([[d valueForKey:@"IsSymbolButtonList"] isEqualToString:@"true"]) {
      CVButtonViewController *controller =
          [[CVButtonViewController alloc] initWithDictionary:d];
      [controller autorelease];
      [_viewArray addObject:controller];
    } else {
      CVSmileyViewController *controller =
          [[CVSmileyViewController alloc] initWithDictionary:d];
      [controller autorelease];
      [_viewArray addObject:controller];
    }
    NSString *name = [[d valueForKey:@"Name"]
        fallbackableLocalizedStringValueForLocale:locale];
    [_popUpButton addItemWithTitle:name];
  }
  if ([_viewArray count]) {
    id item = [_viewArray objectAtIndex:0];
    NSView *view = [item view];
    [self toggleActiveView:view];
  }
}

- (void)awakeFromNib {
  [[self window] setTitle:LFLSTR(@"Symbols")];
  [[self window] setDelegate:(id)self];

  [_popUpButton removeAllItems];
  [self toggleActiveView:nil];

  NSRect screenRect = [[NSScreen mainScreen] visibleFrame];
  NSRect windowRect = [[self window] frame];
  int x =
      screenRect.origin.x + screenRect.size.width - windowRect.size.width - 10;
  int y = screenRect.origin.y + 200;

  [[self window]
      setFrame:NSMakeRect(x, y, windowRect.size.width, windowRect.size.height)
       display:YES];

  // NSLog(@"addObserver");
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(loadSymbolTable:)
             name:CVLoaderUpdateCannedMessagesNotification
           object:nil];
}

- (void)temporaryHide {
  if ([[self window] isVisible]) {
    _frameBeforeTemporaryHide = [[self window] frame];
    _isTemporarilyHidden = YES;
  }
  [[self window] orderOut:self];
}
- (void)restoreWindowStatus {
  if (!_isVisible) return;

  if (_isTemporarilyHidden) {
    NSPoint anchorPoint = NSMakePoint(NSMidX(_frameBeforeTemporaryHide),
                                     NSMaxY(_frameBeforeTemporaryHide) - 1);
    NSRect windowRect = [self constrainedWindowFrame:_frameBeforeTemporaryHide
                                            forPoint:anchorPoint];
    [[self window] setFrame:windowRect display:NO];
    _isTemporarilyHidden = NO;
  }

  [[self window] orderFront:self];
}
- (BOOL)isVisible {
  return _isVisible;
}
- (NSRect)screenVisibleFrameForPoint:(NSPoint)point {
  NSScreen *screen = nil;
  NSEnumerator *enumerator = [[NSScreen screens] objectEnumerator];
  while (screen = [enumerator nextObject]) {
    NSRect frame = [screen frame];
    if (point.x >= NSMinX(frame) && point.x <= NSMaxX(frame) &&
        point.y >= NSMinY(frame) && point.y <= NSMaxY(frame)) {
      return [screen visibleFrame];
    }
  }
  return [[NSScreen mainScreen] visibleFrame];
}
- (NSRect)constrainedWindowFrame:(NSRect)windowRect forPoint:(NSPoint)point {
  NSRect screenFrame = [self screenVisibleFrameForPoint:point];

  if (NSMaxX(windowRect) > NSMaxX(screenFrame))
    windowRect.origin.x =
        NSMaxX(screenFrame) - windowRect.size.width - CVSymbolWindowScreenPadding;
  if (NSMinX(windowRect) < NSMinX(screenFrame))
    windowRect.origin.x = NSMinX(screenFrame) + CVSymbolWindowScreenPadding;
  if (NSMaxY(windowRect) > NSMaxY(screenFrame))
    windowRect.origin.y =
        NSMaxY(screenFrame) - windowRect.size.height - CVSymbolWindowScreenPadding;
  if (NSMinY(windowRect) < NSMinY(screenFrame))
    windowRect.origin.y = NSMinY(screenFrame) + CVSymbolWindowScreenPadding;

  return windowRect;
}
- (void)toggleActiveView:(NSView *)view {
  if ([[_symbolContentView subviews] count]) {
    NSView *lastView = [[_symbolContentView subviews] objectAtIndex:0];
    [lastView removeFromSuperview];
  }
  if (!view) return;

  NSRect viewRect = [view bounds];
  NSRect symbolFrame = [_symbolContentView frame];
  symbolFrame.size = viewRect.size;
  NSRect windowRect = [[self window] frame];
  NSPoint anchorPoint = NSMakePoint(NSMidX(windowRect), NSMaxY(windowRect) - 1);
  float currentMaxY = NSMaxY(windowRect);
  windowRect.size.height = symbolFrame.size.height + 65;
  windowRect.origin.y = currentMaxY - windowRect.size.height;
  windowRect = [self constrainedWindowFrame:windowRect forPoint:anchorPoint];
  [[self window] setFrame:windowRect display:YES animate:YES];

  [_symbolContentView setFrame:symbolFrame];
  [_symbolContentView addSubview:view];
}

#pragma mark Interface Builder actions

- (IBAction)toggleSymbol:(id)sender {
  NSInteger selectedIndex =
      [_popUpButton indexOfItem:[_popUpButton selectedItem]];
  id item = [_viewArray objectAtIndex:selectedIndex];
  NSView *view = [item view];
  [self toggleActiveView:view];
}
- (IBAction)showWindow:(id)sender {
  NSRect originalWindowRect = [[self window] frame];
  NSRect windowRect =
      [self constrainedWindowFrame:originalWindowRect
                           forPoint:originalWindowRect.origin];
  [[self window] setFrame:windowRect display:NO];
  [super showWindow:sender];
}
- (IBAction)hide:(id)sender {
  [[self window] orderOut:self];
  _isVisible = NO;
  _isTemporarilyHidden = NO;
}
- (IBAction)show:(id)sender {
  [[OpenVanillaLoader sharedInstance] mergeCannedMessagesData];
  _isTemporarilyHidden = NO;
  [self showWindow:sender];
  _isVisible = YES;
}

#pragma mark NSWindow delegate methods

- (BOOL)windowShouldClose:(id)window {
  [window orderOut:self];
  _isVisible = NO;
  _isTemporarilyHidden = NO;
  return NO;
}

@end
