// [AUTO_HEADER]

#import "CVVerticalCandidateController.h"

#import "NSColor+LFColorExtensions.h"

@implementation CVPageButton

- (BOOL)isFlipped {
  return NO;
}
- (void)drawRect:(NSRect)aRect {
  NSRect selfRect = [self frame];
  NSRect rect = NSMakeRect(0, 0, selfRect.size.width, selfRect.size.height);
  if ([self image]) {
    NSImage *i = nil;
    if ([[self cell] isHighlighted]) {
      if ([self alternateImage])
        i = [self alternateImage];
      else
        i = [self image];
    } else {
      i = [self image];
    }
    NSSize size = [i size];
    float x = (rect.size.width - size.width) / 2;
    float y = (rect.size.height - size.height) / 2;
    [i drawAtPoint:NSMakePoint(x, y)
          fromRect:NSZeroRect
         operation:NSCompositingOperationSourceOver
          fraction:1.0];
  }
}
@end

@implementation CVVerticalCandidateController

- (void)dealloc {
  [_candidateArray release];
  [_backgroundColor release];
  [_foregroundColor release];
  [_highlightTextColor release];
  [super dealloc];
}
- (id)init {
  self = [super init];
  if (self != nil) {
    BOOL loaded = [[NSBundle mainBundle] loadNibNamed:@"VerticalCandidateWindow" owner:self topLevelObjects:nil];
    NSAssert((loaded == YES), @"NIB did not load");

    _candidateTextHeight = 18.0;
  }
  return self;
}
- (NSImage *)imagePrev:(NSColor *)aColor {
  NSImage *imagePrev;
  imagePrev = [[[NSImage alloc] initWithSize:NSMakeSize(6, 6)] autorelease];
  [imagePrev lockFocus];
  NSBezierPath *bezierPath = [NSBezierPath bezierPath];
  [bezierPath moveToPoint:NSMakePoint(0, 0)];
  [bezierPath lineToPoint:NSMakePoint(6, 0)];
  [bezierPath lineToPoint:NSMakePoint(3, 6)];
  [bezierPath closePath];
  [aColor setFill];
  [bezierPath fill];
  [imagePrev unlockFocus];
  return imagePrev;
}
- (NSImage *)imageNext:(NSColor *)aColor {
  NSImage *imageNext;
  imageNext = [[[NSImage alloc] initWithSize:NSMakeSize(6, 6)] autorelease];
  [imageNext lockFocus];
  NSBezierPath *bezierPath = [NSBezierPath bezierPath];
  [bezierPath moveToPoint:NSMakePoint(0, 6)];
  [bezierPath lineToPoint:NSMakePoint(6, 6)];
  [bezierPath lineToPoint:NSMakePoint(3, 0)];
  [bezierPath closePath];
  [aColor setFill];
  [bezierPath fill];
  [imageNext unlockFocus];
  return imageNext;
}
- (void)awakeFromNib {
  _backgroundColor = [[NSColor blackColor] retain];
  _foregroundColor = [[NSColor whiteColor] retain];
  _highlightTextColor = [[NSColor whiteColor] retain];

  [[[self window] contentView] setBackgroundFillColor:_backgroundColor];
  [[[self window] contentView] setBorderColor:_foregroundColor];
  [_tableView setTablebackgroundColor:_backgroundColor];

  _candidateArray = [[NSMutableArray alloc] init];
  [_scrollView setBorderType:NSNoBorder];
  [_tableView setDelegate:(id)self];
  [_tableView setDataSource:(id)self];

  [_pageIndicatorTextField setStringValue:@""];
  [_pageIndicatorTextField setTextColor:_foregroundColor];
  [_promptTextField setStringValue:@""];
  [_promptTextField setTextColor:_foregroundColor];

  [_tableView setBackgroundColor:[NSColor clearColor]];
  [_tableView setAction:@selector(sendKey:)];

  [_previousButton setHidden:YES];
  [_nextButton setHidden:YES];

  _sending = NO;
}
- (void)setFontHeight:(float)newHeight {
  _fontHeight = newHeight;
  if (_fontHeight < 20) _fontHeight = 20;
}
- (void)updateDisplay:(PVVerticalCandidatePanel *)panel
              atPoint:(NSPoint)position {
  // hide if it's invisible--before update
  if (!panel->isVisible()) {
    [[self window] orderOut:self];
    return;
  }

  _sending = NO;

  NSPoint newPosition = position;

  if (_candidateArray)
    [_candidateArray removeAllObjects];
  else
    _candidateArray = [NSMutableArray new];

  [[[self window] contentView] setBackgroundFillColor:_backgroundColor];
  [[[self window] contentView] setBorderColor:_foregroundColor];
  [_tableView setTablebackgroundColor:_backgroundColor];

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
      [NSString stringWithFormat:@"%zu/%zu", currentPage, pageCount];
  if (pageCount > 1) {
    [_pageIndicatorTextField setStringValue:pageString];
    [_pageIndicatorTextField setHidden:NO];
    [_previousButton setHidden:NO];
    [_nextButton setHidden:NO];
  } else {
    [_pageIndicatorTextField setStringValue:@""];
    [_pageIndicatorTextField setHidden:YES];
    [_previousButton setHidden:YES];
    [_nextButton setHidden:YES];
  }

  _width = 0;

  NSString *prompt = [NSString stringWithUTF8String:panel->prompt().c_str()];
  [_promptTextField setStringValue:prompt];
  float promptHeight = [_promptTextField frame].size.height;
  NSRect newPromptRect = [[_promptTextField attributedStringValue]
      boundingRectWithSize:NSMakeSize(1600, promptHeight)
                   options:NSStringDrawingUsesLineFragmentOrigin];
  _width = newPromptRect.size.width + 10;

  OVCandidateList *list = panel->candidateList();

  float fontSize = _candidateTextHeight;

  NSFont *defaultFont = [NSFont systemFontOfSize:fontSize];
  NSDictionary *attributes =
      [NSDictionary dictionaryWithObject:defaultFont
                                  forKey:NSFontAttributeName];

  [_tableView setRowHeight:fontSize + 6.0];
  for (index = 0; index < count; index++) {
    string candidate = list->candidateAtIndex(fromIndex + index);
    string keyString = panel->candidateKeyAtIndex(index).receivedString();

    NSAttributedString *c = [[NSAttributedString alloc]
        initWithString:[NSString stringWithUTF8String:candidate.c_str()]
            attributes:attributes];
    NSString *k = [NSString stringWithUTF8String:keyString.c_str()];
    NSMutableDictionary *d = [NSMutableDictionary dictionary];
    [d setValue:c forKey:@"candidate"];
    NSImage *i = nil;
    if (index == highlightedIndex && panel->isInControl()) {
      i = [CVTextDecoration imageWithACharacterInACircle:k
                                              foreground:_highlightTextColor
                                              background:_backgroundColor];
    } else {
      i = [CVTextDecoration imageWithACharacterInACircle:k
                                              foreground:_foregroundColor
                                              background:_backgroundColor];
    }
    [d setValue:i forKey:@"key"];

    NSRect rect =
        [c boundingRectWithSize:NSMakeSize(1600, [_tableView rowHeight])
                        options:NSStringDrawingUsesLineFragmentOrigin];
    float currentWidth = rect.size.width;
    if (currentWidth >= _width) _width = currentWidth;

    [_candidateArray addObject:d];
  }
  [_tableView reloadData];

  NSRect tableFrame = [_tableView frame];
  tableFrame.size.width = 50 + _width;
  tableFrame.size.height =
      ([_tableView rowHeight] + 2) * panel->candidatesPerPage();
  if (!tableFrame.size.height) tableFrame.size.height = 200;
  [_tableView setFrame:tableFrame];
  [_tableView sizeToFit];

  NSRect windowFrame = [[self window] frame];
  windowFrame.size.width = tableFrame.size.width;
  windowFrame.size.height = tableFrame.size.height + 40;
  windowFrame.origin.x = newPosition.x;

  NSRect frame = [[NSScreen mainScreen] visibleFrame];
  NSArray *screens = [NSScreen screens];
  NSUInteger i, c = [screens count];

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

  [[self window] setFrame:windowFrame display:YES];

  if (panel->isInControl()) {
    _allowClick = YES;
    [_tableView setAllowsEmptySelection:NO];
    [_tableView selectRowIndexes:[NSIndexSet
                                     indexSetWithIndex:(int)highlightedIndex]
            byExtendingSelection:NO];
  } else {
    [_tableView setAllowsEmptySelection:YES];
    [_tableView deselectAll:self];
    _allowClick = NO;
  }

  // show if it's visible--after update
  if (panel->isVisible()) [[self window] makeKeyAndOrderFront:self];
}
- (void)hide {
  [[self window] orderOut:self];
  ;
}

#pragma mark Interface Builder actions

- (IBAction)sendKey:(id)sender {
  if (_sending) return;
  NSInteger selectedItem = [_tableView selectedRow];
  string keyString = _panel->candidateKeyAtIndex(selectedItem).receivedString();
  [[CVSendKey sharedSendKey]
      typeString:[NSString stringWithUTF8String:keyString.c_str()]];
}

- (IBAction)updateSelectedCandidate:(id)sender {
  NSInteger selectedItem = [_tableView selectedRow];
  _panel->setHighlightIndex((size_t)selectedItem);
  NSPoint p = [[self window] frame].origin;
  p.y = p.y + [[self window] frame].size.height;
  [self updateDisplay:_panel atPoint:p];
}

- (void)updateContent:(PVVerticalCandidatePanel *)panel
              atPoint:(NSPoint)position;
{
  [self updateDisplay:panel atPoint:position];
  _panel = panel;
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
  _candidateTextHeight = inTextHeight;
}

#pragma mark TableView delegate

- (BOOL)selectionShouldChangeInTableView:(NSTableView *)aTableView {
  return _allowClick;
}
- (void)tableView:(NSTableView *)aTableView
    willDisplayCell:(id)aCell
     forTableColumn:(NSTableColumn *)aTableColumn
                row:(int)rowIndex {
  NSInteger selectedRow = [_tableView selectedRow];
  if ([[aTableColumn identifier] isEqualToString:@"candidate"]) {
    if (rowIndex == selectedRow)
      [aCell setTextColor:_highlightTextColor];
    else
      [aCell setTextColor:_foregroundColor];
  }
}
- (id)tableView:(NSTableView *)aTableView
    objectValueForTableColumn:(NSTableColumn *)aTableColumn
                          row:(int)rowIndex {
  if ([[aTableColumn identifier] isEqualToString:@"key"])
    return [[_candidateArray objectAtIndex:rowIndex] objectForKey:@"key"];
  else if ([[aTableColumn identifier] isEqualToString:@"candidate"])
    return [[_candidateArray objectAtIndex:rowIndex] objectForKey:@"candidate"];
  return nil;
}
- (int)numberOfRowsInTableView:(NSTableView *)aTableView {
  return (int)[_candidateArray count];
}

@end
