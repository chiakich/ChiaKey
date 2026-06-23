/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoGeneric.h"

@implementation TakaoGeneric

- (void)dealloc {
  [_modules release];
  [super dealloc];
}

- (void)setModules:(NSArray *)modules;
{
  if (![modules count]) {
    return;
  }

  if (!_modules) {
    _modules = [[NSMutableArray alloc] init];
  } else {
    [_modules removeAllObjects];
  }

  NSEnumerator *moduleEnumeartor = [modules objectEnumerator];
  NSArray *itemArray;
  while (itemArray = [moduleEnumeartor nextObject]) {
    TakaoGenericSettings *genericSettingController = [TakaoGenericSettings new];
    NSString *identifier = [itemArray objectAtIndex:0];
    NSString *localizedName = [itemArray objectAtIndex:1];
    [genericSettingController loadSettingsWithName:identifier
                                     localizedName:localizedName];
    NSDictionary *dictionary =
        [NSDictionary dictionaryWithObjectsAndKeys:localizedName, @"name",
                                                   genericSettingController,
                                                   @"controller", nil];
    [genericSettingController release];
    [_modules addObject:dictionary];
  }

  [_genericModuleListTableView setDataSource:self];
  [_genericModuleListTableView setDelegate:(id)self];
  [_genericModuleListTableView reloadData];

  id controller = [[_modules objectAtIndex:0] valueForKey:@"controller"];
  if (controller) {
    [_genericSettingView addSubview:[controller view]];
    [_genericModuleListTableView
            selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
        byExtendingSelection:NO];
    [_genericModuleListTableView setAllowsEmptySelection:NO];
  }
}

#pragma mark NSTableView data source methods

- (int)numberOfRowsInTableView:(NSTableView *)aTableView {
  return [_modules count];
}
- (id)tableView:(NSTableView *)aTableView
    objectValueForTableColumn:(NSTableColumn *)aTableColumn
                          row:(int)rowIndex {
  if ([[aTableColumn identifier] isEqualToString:@"modules"])
    return [[_modules objectAtIndex:rowIndex] valueForKey:@"name"];
  return nil;
}
- (void)tableViewSelectionDidChange:(NSNotification *)aNotification {
  int index = [_genericModuleListTableView selectedRow];
  if ([[_genericSettingView subviews] objectAtIndex:0])
    [[[_genericSettingView subviews] objectAtIndex:0] removeFromSuperview];
  if ([_modules objectAtIndex:index]) {
    id controller = [[_modules objectAtIndex:index] valueForKey:@"controller"];
    [_genericSettingView addSubview:[controller view]];
  }
}

@end
