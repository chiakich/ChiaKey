// [AUTO_HEADER]

#import "CVDictionaryController.h"

@implementation CVDictionaryController (Toolbar)

static NSString *sizeToolbarItemIdentifier = @"Size";
static NSString *indicatorToolbarItemIdentifier = @"Indicator";

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
  return [NSArray arrayWithObjects:sizeToolbarItemIdentifier,
                                   NSToolbarFlexibleSpaceItemIdentifier,
                                   indicatorToolbarItemIdentifier, nil];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
  return [NSArray arrayWithObjects:sizeToolbarItemIdentifier,
                                   NSToolbarFlexibleSpaceItemIdentifier,
                                   indicatorToolbarItemIdentifier, nil];
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
        itemForItemIdentifier:(NSString *)identifier
    willBeInsertedIntoToolbar:(BOOL)willBeInserted {
  NSToolbarItem *item =
      [[[NSToolbarItem alloc] initWithItemIdentifier:identifier] autorelease];
  if ([identifier isEqualToString:sizeToolbarItemIdentifier]) {
    [item setLabel:LFLSTR(sizeToolbarItemIdentifier)];
    [item setView:_changeSizeSegmentedControl];
    [item setTarget:self];
    [item setAction:@selector(changeTextSize:)];
    [item setMaxSize:[_changeSizeSegmentedControl bounds].size];
    [item setMinSize:[_changeSizeSegmentedControl bounds].size];
  } else if ([identifier isEqualToString:indicatorToolbarItemIdentifier]) {
    [item setLabel:LFLSTR(indicatorToolbarItemIdentifier)];
    [item setView:_loadingProgressIndicator];
    [item setTarget:self];
    [item setAction:nil];
    [item setMaxSize:[_loadingProgressIndicator bounds].size];
    [item setMinSize:[_loadingProgressIndicator bounds].size];
  } else {
    item = nil;
  }
  return item;
}

@end
