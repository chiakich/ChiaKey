// [AUTO_HEADER]

#import "CVHorizontalCandidateControl.h"

#import "CVHorizontalCandidateCell.h"

@implementation CVHorizontalCandidateControl

+ (Class)cellClass {
  return [CVHorizontalCandidateCell class];
}
- (void)dealloc {
  [_candidateCellArray release];
  [super dealloc];
}
- (void)awakeFromNib {
  _candidateTextHeight = 18.0;

  if (!_candidateCellArray) _candidateCellArray = [NSMutableArray new];
}
- (NSSize)autoSize {
  return _autotSize;
}
- (void)setHighlightdeIndex:(int)index {
  if (index < [_candidateCellArray count]) {
    CVHorizontalCandidateCell *aCell =
        [_candidateCellArray objectAtIndex:index];
    if (aCell) [aCell setHighlight:YES];
    [self setNeedsDisplay:YES];
  }
}
- (void)setArray:(NSArray *)candidateArray {
  if (!_candidateCellArray) _candidateCellArray = [NSMutableArray new];

  [_candidateCellArray removeAllObjects];

  NSEnumerator *enumerator = [candidateArray objectEnumerator];
  NSDictionary *dictionary;
  int index = 0;

  float fontHeight = _candidateTextHeight;

  NSSize size = NSMakeSize(0, fontHeight + 10.0);

  while (dictionary = [enumerator nextObject]) {
    NSString *keyString = [dictionary valueForKey:@"keyString"];
    NSString *candidate = [dictionary valueForKey:@"candidate"];
    CVHorizontalCandidateCell *aCell =
        [[CVHorizontalCandidateCell alloc] initWithKeyString:keyString
                                                   candidate:candidate
                                                  fontHeight:fontHeight];
    [aCell autorelease];
    NSSize cellSize = [aCell cellSize];
    size.width += cellSize.width;
    [_candidateCellArray addObject:aCell];
    index++;
  }
  _autotSize = size;
  [self setNeedsDisplay:YES];
}

- (void)setClickable:(BOOL)flag {
  _clickable = flag;
}

- (int)clickedIndex {
  return _clickedIndex;
}

- (void)setCandidateTextHeight:(float)inTextHeight {
  _candidateTextHeight = inTextHeight;
}

- (void)drawRect:(NSRect)rect {
  NSEnumerator *enumerator = [_candidateCellArray objectEnumerator];
  CVHorizontalCandidateCell *aCell;
  NSPoint lastPoint = NSMakePoint(0, 0);
  while (aCell = [enumerator nextObject]) {
    NSSize cellSize = [aCell cellSize];
    NSRect rect = NSMakeRect(lastPoint.x, lastPoint.y + 2, cellSize.width,
                             cellSize.height - 4);
    [aCell drawInteriorWithFrame:rect inView:self];
    lastPoint.x += cellSize.width;
  }
}

- (void)mouseDown:(NSEvent *)theEvent {
  if (!_clickable) {
    _clickedIndex = -1;
    return;
  }

  NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  NSEnumerator *enumerator = [_candidateCellArray objectEnumerator];
  CVHorizontalCandidateCell *aCell;
  int i = 0;
  NSPoint lastPoint = NSMakePoint(0, 0);
  while (aCell = [enumerator nextObject]) {
    NSSize cellSize = [aCell cellSize];
    NSRect rect = NSMakeRect(lastPoint.x, lastPoint.y + 2, cellSize.width,
                             cellSize.height - 4);
    if (point.x >= NSMinX(rect) && point.x <= NSMaxX(rect) &&
        point.y >= NSMinY(rect) && point.y <= NSMaxY(rect)) {
      _clickedIndex = i;
      if ([self target] != nil &&
          [[self target] respondsToSelector:[self action]]) {
        [[self target] performSelector:[self action]];
      }
      return;
    }
    lastPoint.x += cellSize.width;
    i++;
  }
  _clickedIndex = -1;
}

@end
