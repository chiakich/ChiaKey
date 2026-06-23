// [AUTO_HEADER]

#import "CVSmileyViewController.h"

#import "OpenVanillaController.h"
#import "OpenVanillaLoader.h"

@implementation NSString (FallbackableLocalizedStringValue)
- (NSString *)fallbackableLocalizedStringValueForLocale:(NSString *)locale {
  return self;
}
@end

@implementation NSDictionary (FallbackableLocalizedStringValue)
- (NSString *)fallbackableLocalizedStringValueForLocale:(NSString *)locale {
  NSString *v = [self objectForKey:locale];
  if (v) return v;

  v = [self objectForKey:@"en"];
  if (v) return v;

  NSArray *keys = [self allKeys];
  if ([keys count]) return [self objectForKey:[keys objectAtIndex:0]];

  return @"";
}
@end

@implementation CVSmileyViewController

- (void)dealloc {
  [_array release];
  [super dealloc];
}
- (id)initWithDictionary:(NSDictionary *)d {
  self = [super init];
  if (self != nil) {
    _array = [[d valueForKey:@"Messages"] retain];
    BOOL loaded = [[NSBundle mainBundle] loadNibNamed:@"SmileyView" owner:self topLevelObjects:nil];
    NSAssert((loaded == YES), @"NIB did not load");
  }
  return self;
}
- (void)awakeFromNib {
  [_tableView setTarget:self];
  [_tableView setDoubleAction:@selector(sendStringFromTableView:)];
  [_sendButton setTitle:LFLSTR(@"Send")];
  [_sendButton setStringValue:LFLSTR(@"Send")];
}
- (NSView *)view {
  return _view;
}
- (void)sendStringOfRowIndex:(int)rowIndex {
  id obj = [_array objectAtIndex:rowIndex];
  NSString *text = nil;
  if ([obj isKindOfClass:[NSString class]]) {
    text = obj;
  } else if ([obj isKindOfClass:[NSDictionary class]]) {
    if ([obj valueForKey:@"Text"]) {
      text = [obj valueForKey:@"Text"];
    } else {
      NSString *locale =
          [NSString stringWithUTF8String:[OpenVanillaLoader sharedLoaderService]
                                             ->locale()
                                             .c_str()];
      id nameDict = [obj valueForKey:@"Name"];
      text = [nameDict fallbackableLocalizedStringValueForLocale:locale];
    }
  }
  if (text) {
    [OpenVanillaController sendComposedStringToCurrentlyActiveContext:text];
  }
}
- (IBAction)sendString:(id)sender {
  int selection = [_tableView selectedRow];
  if (selection > -1 && selection != NSNotFound) {
    [self sendStringOfRowIndex:selection];
  }
}
- (IBAction)sendStringFromTableView:(id)sender {
  int selection = [_tableView clickedRow];
  if (selection > -1 && selection != NSNotFound) {
    [self sendStringOfRowIndex:selection];
  }
}

#pragma mark NSTableView data source and delegate methods.

- (int)numberOfRowsInTableView:(NSTableView *)aTableView {
  return [_array count];
}
- (id)tableView:(NSTableView *)aTableView
    objectValueForTableColumn:(NSTableColumn *)aTableColumn
                          row:(int)rowIndex {
  id obj = [_array objectAtIndex:rowIndex];
  if ([obj isKindOfClass:[NSString class]]) return obj;

  if ([obj isKindOfClass:[NSDictionary class]]) {
    NSString *locale =
        [NSString stringWithUTF8String:[OpenVanillaLoader sharedLoaderService]
                                           ->locale()
                                           .c_str()];
    id nameDict = [obj valueForKey:@"Name"];
    return [nameDict fallbackableLocalizedStringValueForLocale:locale];
  }
  return nil;
}

@end
