// [AUTO_HEADER]

#import "CVButtonViewController.h"

#import "OpenVanillaController.h"

@implementation CVButtonViewController

- (void)dealloc {
  [_view release];
  [_buttonArray release];
  [super dealloc];
}
- (id)initWithDictionary:(NSDictionary *)d {
  self = [super init];
  if (self != nil) {
    _buttonArray = [[NSMutableArray alloc] init];
    NSArray *buttons = [d valueForKey:@"Buttons"];
    int line = ceil((float)[buttons count] / 10);
    float height = line * 25;
    _view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 250, height)];
    NSEnumerator *enumerator = [buttons objectEnumerator];
    NSString *symbol;
    int i = 0;
    while (symbol = [enumerator nextObject]) {
      int row = 1 + (i / 10);
      int col = i % 10;
      int y = NSMaxY([_view frame]) - row * 25;
      int x = col * 25;
      NSRect buttonFrame = NSMakeRect(x, y, 23, 23);
      NSButton *button = [[NSButton alloc] initWithFrame:buttonFrame];
      [button autorelease];
      [button setTarget:self];
      [button setAction:@selector(sendString:)];
      [button setButtonType:NSButtonTypeMomentaryLight];
      [button setBezelStyle:NSBezelStyleTexturedSquare];
      [button setStringValue:symbol];
      [button setTitle:symbol];
      [_buttonArray addObject:button];
      [_view addSubview:button];
      i++;
    }
  }
  return self;
}
- (NSView *)view {
  return _view;
}
- (IBAction)sendString:(id)sender {
  NSButton *button = (NSButton *)sender;
  NSString *text = [button title];
  [OpenVanillaController sendComposedStringToCurrentlyActiveContext:text];
}

@end
