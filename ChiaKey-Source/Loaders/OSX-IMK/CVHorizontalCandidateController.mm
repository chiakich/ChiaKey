// [AUTO_HEADER]

#import "CVHorizontalCandidateController.h"

#import "NSColor+LFColorExtensions.h"

@implementation CVHorizontalCandidateController

- (void)dealloc {
  [_backgroundColor release];
  [_foregroundColor release];
  [_highlightTextColor release];
  [super dealloc];
}
- (id)init {
  self = [super init];
  if (self != nil) {
    BOOL loaded = [[NSBundle mainBundle] loadNibNamed:@"HorizontalCandidateWindow" owner:self topLevelObjects:nil];
    NSAssert((loaded == YES), @"NIB did not load");
  }
  return self;
}
- (NSImage *)imagePrev:(NSColor *)aColor {
  NSImage *imagePrev;
  imagePrev = [[[NSImage alloc] initWithSize:NSMakeSize(6, 6)] autorelease];
  [imagePrev lockFocus];
  NSBezierPath *b = [NSBezierPath bezierPath];
  [b moveToPoint:NSMakePoint(6, 0)];
  [b lineToPoint:NSMakePoint(0, 3)];
  [b lineToPoint:NSMakePoint(6, 6)];
  [b closePath];
  [aColor setFill];
  [b fill];
  [imagePrev unlockFocus];
  return imagePrev;
}
- (NSImage *)imageNext:(NSColor *)aColor {
  NSImage *imageNext;
  imageNext = [[[NSImage alloc] initWithSize:NSMakeSize(6, 6)] autorelease];
  [imageNext lockFocus];
  NSBezierPath *b = [NSBezierPath bezierPath];
  [b moveToPoint:NSMakePoint(0, 0)];
  [b lineToPoint:NSMakePoint(6, 3)];
  [b lineToPoint:NSMakePoint(0, 6)];
  [b closePath];
  [aColor setFill];
  [b fill];
  [imageNext unlockFocus];
  return imageNext;
}
- (void)awakeFromNib {
  _backgroundColor = [[NSColor blackColor] retain];
  _foregroundColor = [[NSColor whiteColor] retain];
  _highlightTextColor = [[NSColor whiteColor] retain];

  [_background setBackgroundFillColor:_backgroundColor];
  [_background setBorderColor:_foregroundColor];

  [_pageTextField setStringValue:@""];
  [_pageTextField setHidden:YES];
  [_pageTextField setTextColor:_foregroundColor];

  [_previousButton setHidden:YES];
  [_nextButton setHidden:YES];

  [_candidateControl setTarget:self];
  [_candidateControl setAction:@selector(sendKey:)];
  _sending = NO;
}
- (void)setFontHeight:(float)newHeight {
  _fontHeight = newHeight;
  if (_fontHeight < 20) _fontHeight = 20;
}
- (void)updateDisplay:(PVHorizontalCandidatePanel *)panel
              atPoint:(NSPoint)position {
  // hide if it's invisible--before update
  if (!panel->isVisible()) {
    [[self window] orderOut:self];
    return;
  }

  _sending = NO;

  NSPoint newPosition = position;

  NSColor *highlightColor = [NSColor highlightGradientFromColor];
  [_previousButton setImage:[self imagePrev:_foregroundColor]];
  [_previousButton setAlternateImage:[self imagePrev:highlightColor]];
  [_nextButton setImage:[self imageNext:_foregroundColor]];
  [_nextButton setAlternateImage:[self imageNext:highlightColor]];

  // update the content
  size_t fromIndex = panel->currentPage() * panel->candidatesPerPage();
  size_t index;
  size_t count = panel->currentPageCandidateCount();
  size_t highlightedIndex = panel->currentHightlightIndex();
  size_t currentPage = panel->currentPage() + 1;
  size_t pageCount = panel->pageCount();
  NSString *pageString =
      [NSString stringWithFormat:@"%d/%d", currentPage, pageCount];
  NSString *prompt = [NSString stringWithUTF8String:panel->prompt().c_str()];

  OVCandidateList *list = panel->candidateList();

  NSMutableArray *array = [NSMutableArray array];

  for (index = 0; index < count; index++) {
    string candidate = list->candidateAtIndex(fromIndex + index);
    string keyString = panel->candidateKeyAtIndex(index).receivedString();
    NSDictionary *dictionary = [NSDictionary
        dictionaryWithObjectsAndKeys:
            [NSString stringWithUTF8String:candidate.c_str()], @"candidate",
            [NSString stringWithUTF8String:keyString.c_str()], @"keyString",
            nil];
    [array addObject:dictionary];
  }
  [_candidateControl setArray:array];

  NSSize candidateSize = [_candidateControl autoSize];
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height = candidateSize.height;
  windowFrame.size.width = candidateSize.width + 40;

  NSRect candidateFrame = [_candidateControl frame];
  candidateFrame.origin = NSMakePoint(20, 0);
  candidateFrame.size = candidateSize;
  [_candidateControl setFrame:candidateFrame];

  NSRect goPrevFrame = [_previousButton frame];
  goPrevFrame.origin = NSMakePoint(2, 4);
  NSRect goNextFrame = [_nextButton frame];
  goNextFrame.origin = NSMakePoint(NSMaxX(candidateFrame), 4);

  if (pageCount > 1) {
    [_previousButton setHidden:NO];
    [_nextButton setHidden:NO];
    [_pageTextField setHidden:NO];
    windowFrame.size.width = candidateSize.width + 80;
    goNextFrame.origin = NSMakePoint(NSMaxX(candidateFrame) + 40, 4);
    [_pageTextField setStringValue:pageString];
    [_pageTextField
        setFrame:NSMakeRect(NSMaxX(candidateFrame) + 10, 4, 30, 18)];
  } else {
    [_previousButton setHidden:YES];
    [_nextButton setHidden:YES];
    [_pageTextField setHidden:YES];
  }

  if ([prompt length]) {
    windowFrame.size.height = candidateSize.height + 10;
    if (windowFrame.size.width < 150) windowFrame.size.width = 150;
    [_promptTextField setHidden:NO];
    [_promptTextField setStringValue:prompt];
    NSRect promptFrame = [_promptTextField frame];
    promptFrame.size.width = 80;
    promptFrame.size.height = 10;
    promptFrame.origin = NSMakePoint(10, NSMaxY(candidateFrame));
    [_promptTextField setFrame:promptFrame];
  } else {
    [_promptTextField setHidden:YES];
  }

  [_previousButton setFrame:goPrevFrame];
  [_nextButton setFrame:goNextFrame];
  [_background
      setFrame:NSMakeRect(0, 0, windowFrame.size.width, candidateSize.height)];

  NSRect frame = [[NSScreen mainScreen] visibleFrame];
  NSArray *screens = [NSScreen screens];
  int i, c = [screens count];
  if ([screens count] > 1) {
    for (i = 0; i < c; i++) {
      NSScreen *screen = [screens objectAtIndex:i];
      NSRect screenFrame = [screen frame];

      if (newPosition.x >= NSMinX(screenFrame) &&
          newPosition.x <= NSMaxX(screenFrame)) {
        frame = [screen visibleFrame];
        break;
      }
    }
  }

  if (newPosition.y < NSMinY(frame))
    newPosition.y = NSMinY(frame);
  else if (newPosition.y - windowFrame.size.height < NSMinY(frame))
    newPosition.y = newPosition.y + _fontHeight;
  //	else if (newPosition.y + windowFrame.size.height > NSMaxY(frame))
  else if (newPosition.y > NSMaxY(frame))
    newPosition.y = NSMaxY(frame) - windowFrame.size.height;
  else
    newPosition.y = newPosition.y - windowFrame.size.height;

  if (newPosition.x < NSMinX(frame))
    newPosition.x = NSMinX(frame);
  else if (newPosition.x + windowFrame.size.width > NSMaxX(frame))
    newPosition.x = NSMaxX(frame) - windowFrame.size.width;

  windowFrame.origin = newPosition;

  if (panel->isInControl()) {
    [_candidateControl setClickable:YES];
    [_candidateControl setHighlightdeIndex:highlightedIndex];
  } else {
    [_candidateControl setClickable:NO];
  }

  [[self window] setFrame:windowFrame display:YES];

  if (panel->isVisible()) [[self window] makeKeyAndOrderFront:self];
}
- (void)updateContent:(PVHorizontalCandidatePanel *)panel
              atPoint:(NSPoint)position;
{
  [self updateDisplay:panel atPoint:position];
  _panel = panel;
}
- (void)hide {
  [[self window] orderOut:self];
}
- (IBAction)sendKey:(id)sender {
  if (_sending) return;
  int selectedItem = [_candidateControl clickedIndex];
  string keyString = _panel->candidateKeyAtIndex(selectedItem).receivedString();
  [[CVSendKey sharedSendKey]
      typeString:[NSString stringWithUTF8String:keyString.c_str()]];
}
- (IBAction)gotoNextPage:(id)sender {
  _panel->goToNextPage();
  NSPoint p = [[self window] frame].origin;
  p.y = NSMaxY([[self window] frame]);
  [self updateDisplay:_panel atPoint:p];
}
- (IBAction)gotoPreviousPage:(id)sender {
  _panel->goToPreviousPage();
  NSPoint p = [[self window] frame].origin;
  p.y = NSMaxY([[self window] frame]);
  [self updateDisplay:_panel atPoint:p];
}

- (void)setCandidateTextHeight:(float)inTextHeight {
  [_candidateControl setCandidateTextHeight:inTextHeight];
}

@end
