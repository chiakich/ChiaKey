// [AUTO_HEADER]

#import "CVInputMethodToggleControl.h"

#import "CVInputMethodToggleCell.h"

@implementation CVInputMethodToggleControl

- (void)dealloc {
  [_cellArray release];
  [super dealloc];
}

- (void)awakeFromNib {
  _cellArray = [NSMutableArray new];
}

- (void)drawRect:(NSRect)rect {
  // Drawing code here.
  NSRect selfBounds = [self bounds];
  float height = selfBounds.size.height;
  float width = selfBounds.size.width;
  NSEnumerator *e = [_cellArray objectEnumerator];
  NSCell *cell = nil;
  int count = 0;
  while (cell = [e nextObject]) {
    float y = height - ((count + 1) * 30.0);
    NSRect cellFrame = NSMakeRect(0, y, width, 30.0);
    [cell setHighlighted:(count == _currentIndex)];
    [cell drawWithFrame:cellFrame inView:self];
    count++;
  }
}

- (void)setInputMethodArray:(NSArray *)inputMethodArray {
  [_cellArray removeAllObjects];

  NSMutableDictionary *attributes = [NSMutableDictionary dictionary];
  NSFont *font = [NSFont boldSystemFontOfSize:20.0];
  [attributes setValue:font forKey:NSFontAttributeName];
  NSMutableParagraphStyle *style =
      [[[NSMutableParagraphStyle alloc] init] autorelease];
  [style setAlignment:NSTextAlignmentCenter];
  [style setLineBreakMode:NSLineBreakByTruncatingTail];
  [attributes setValue:style forKey:NSParagraphStyleAttributeName];
  [attributes setValue:[NSColor whiteColor]
                forKey:NSForegroundColorAttributeName];

  NSEnumerator *e = [inputMethodArray objectEnumerator];
  NSDictionary *d = nil;
  float width = 200.0;
  float height = 30.0 * [inputMethodArray count];

  while (d = [e nextObject]) {
    NSString *name = [d valueForKey:@"localizedName"];
    CVInputMethodToggleCell *cell =
        [[CVInputMethodToggleCell alloc] initTextCell:name];
    [cell autorelease];
    [cell setAttributedStringValue:[[[NSAttributedString alloc]
                                       initWithString:name
                                           attributes:attributes] autorelease]];
    [cell setFont:font];
    NSSize size =
        [name boundingRectWithSize:NSMakeSize(1600, 20)
                           options:NSStringDrawingUsesLineFragmentOrigin
                        attributes:attributes]
            .size;
    float w = size.width + 20;
    if (w > width) width = w;
    [_cellArray addObject:cell];
  }

  _autosize = NSMakeSize(width, height);
}
- (void)setCurrentIndex:(int)currentIndex {
  _currentIndex = currentIndex;
}
- (NSSize)autosize {
  return _autosize;
}

@end
