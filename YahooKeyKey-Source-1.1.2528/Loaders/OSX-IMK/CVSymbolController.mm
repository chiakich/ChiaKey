// [AUTO_HEADER]

#import "CVSymbolController.h"

#import "CVButtonViewController.h"
#import "CVSmileyViewController.h"
#import "OpenVanillaController.h"

@implementation CVSymbolController

- (id)init {
  self = [super init];
  if (self != nil) {
    BOOL loaded = [NSBundle loadNibNamed:@"SymbolWindow" owner:self];
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
  [[self window] orderOut:self];
}
- (void)restoreWindowStatus {
  if (_isVisible) [self showWindow:self];
}
- (BOOL)isVisible {
  return _isVisible;
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
  float currentMaxY = NSMaxY(windowRect);
  windowRect.size.height = symbolFrame.size.height + 65;
  windowRect.origin.y = currentMaxY - windowRect.size.height;
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
  NSPoint point = [[self window] frame].origin;

  NSScreen *screen = nil;
  NSEnumerator *enumerator = [[NSScreen screens] objectEnumerator];
  BOOL found = NO;
  NSRect frame;
  while (screen = [enumerator nextObject]) {
    frame = [screen frame];
    if (point.x >= NSMinX(frame) && point.x <= NSMaxX(frame) &&
        point.y >= NSMinY(frame) && point.y <= NSMaxX(frame)) {
      found = YES;
      break;
    }
  }
  if (!found) frame = [[NSScreen mainScreen] frame];

  NSRect windowRect = [[self window] frame];
  if (NSMaxX(windowRect) > NSMaxX(frame))
    windowRect.origin.x = NSMaxX(frame) - windowRect.size.width - 20;
  if (NSMinX(windowRect) < NSMinX(frame))
    windowRect.origin.x = NSMinX(frame) + 20;
  if (NSMaxY(windowRect) > NSMaxY(frame))
    windowRect.origin.y = NSMaxY(frame) - windowRect.size.height - 20;
  if (NSMinY(windowRect) < NSMinY(frame))
    windowRect.origin.y = NSMinX(frame) + 20;
  [[self window] setFrameOrigin:windowRect.origin];
  [super showWindow:sender];
}
- (IBAction)hide:(id)sender {
  [[self window] orderOut:self];
  _isVisible = NO;
}
- (IBAction)show:(id)sender {
  [[OpenVanillaLoader sharedInstance] mergeCannedMessagesData];
  [self showWindow:sender];
  _isVisible = YES;
}

#pragma mark NSWindow delegate methods

- (BOOL)windowShouldClose:(id)window {
  [window orderOut:self];
  _isVisible = NO;
  return NO;
}

@end
