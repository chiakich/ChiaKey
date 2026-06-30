// [AUTO_HEADER]

#import "CVInputMethodToggleWindowController.h"

#import "CVApplicationController.h"
#import "CVFloatingBackground.h"
#import "LFUtilities.h"
#import "OpenVanillaController.h"
#import "OpenVanillaLoader.h"

@implementation CVInputMethodToggleWindowController

- (void)dealloc {
  [super dealloc];
}
- (id)init {
  self = [super init];
  if (self != nil) {
    BOOL loaded = [[NSBundle mainBundle] loadNibNamed:@"InputMethodToggleWindowController" owner:self topLevelObjects:nil];
    NSAssert((loaded == YES), @"NIB did not load");
    [[self window] orderOut:self];
  }
  return self;
}
- (void)awakeFromNib {
  CVFloatingBackground *view = [[self window] contentView];
  [view setBackgroundFillColor:[NSColor colorWithCalibratedHue:0.0
                                                    saturation:0.0
                                                    brightness:0.0
                                                         alpha:0.6]];
  [[self window] setHasShadow:NO];
}
- (void)showWindowWithInputMethods:(NSArray *)inputMethods
                     selectedIndex:(int)selectedIndex {
  NSView *contentView = [[self window] contentView];
  [_togglecontrol setInputMethodArray:inputMethods];
  [_togglecontrol setCurrentIndex:selectedIndex];

  NSSize size = [_togglecontrol autosize];
  NSRect windowFrame = [[self window] frame];
  windowFrame.size = size;
  NSRect controlFrame = windowFrame;
  windowFrame.size.height += 40;
  windowFrame.size.width += 4;
  windowFrame.origin.x = _screenFrame.origin.x +
                         (_screenFrame.size.width - windowFrame.size.width) / 2;
  windowFrame.origin.y =
      _screenFrame.origin.y +
      (_screenFrame.size.height - windowFrame.size.height) / 2;
  controlFrame.origin.x = 2;
  controlFrame.origin.y = 20;

  [[self window] setFrame:windowFrame display:YES];
  [_togglecontrol setFrame:controlFrame];
  [_togglecontrol setNeedsDisplay:YES];
  [contentView setNeedsDisplay:YES];

  [self showWindow:self];
}
- (void)_hideWindow {
  if (_shouldHideWindow) return;
  [[self window] orderOut:self];
}
- (void)_checkShouldHideWindow {
  _shouldHideWindow = NO;
  [self _hideWindow];
}
- (void)moveToNextInputMethod {
  _shouldHideWindow = YES;
  CVApplicationController *applicationController =
      (CVApplicationController *)[NSApp delegate];
  NSArray *inputMethodArray = [applicationController inputMethodsArray];

  NSString *currentSelectedInputMethod =
      [applicationController primaryInputMethod];

  int currentIndex = 0;
  NSEnumerator *e = [inputMethodArray objectEnumerator];
  NSDictionary *d = nil;
  while (d = [e nextObject]) {
    NSString *identifier = [d valueForKey:@"identifier"];
    if ([identifier isEqualToString:currentSelectedInputMethod]) {
      break;
    }
    currentIndex++;
  }
  for (NSUInteger count = 0; count < [inputMethodArray count]; count++) {
    if (currentIndex >= [inputMethodArray count] - 1)
      currentIndex = 0;
    else
      currentIndex++;

    d = [inputMethodArray objectAtIndex:currentIndex];
    NSString *identifier = [d valueForKey:@"identifier"];
    if (![OpenVanillaLoader sharedLoader]->isFailedModule(
            [identifier UTF8String]))
      break;
  }

  currentSelectedInputMethod = [d valueForKey:@"identifier"];
  [OpenVanillaLoader sharedLoader]->setPrimaryInputMethod(
      [currentSelectedInputMethod UTF8String]);
  [OpenVanillaLoader sharedLoader]->syncSandwichConfig();

  [self showWindowWithInputMethods:inputMethodArray selectedIndex:currentIndex];
}
- (void)useScreenOfPoint:(NSPoint)point {
  NSRect frame = [[NSScreen mainScreen] frame];
  NSArray *screens = [NSScreen screens];
  NSEnumerator *enumerator = [screens objectEnumerator];
  NSScreen *screen;
  while (screen = [enumerator nextObject]) {
    NSRect screenFrame = [screen frame];

    if (point.x >= NSMinX(screenFrame) && point.x <= NSMaxX(screenFrame) &&
        point.y >= NSMinY(screenFrame) && point.y <= NSMaxY(screenFrame)) {
      frame = screenFrame;
      break;
    }
  }
  _screenFrame = frame;
}

@end
