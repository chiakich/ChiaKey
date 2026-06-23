//
//  PESettingTableView.m
//  PhraseEditor
//
//  Developed by Lithoglyph Inc on 2008/11/2.
//  Copyright 2008 Yahoo! Inc. All rights reserved.
//

#import "PESettingTableView.h"

@implementation PESettingTableView

- (void)awakeFromNib {
  _array = [NSMutableArray new];
  [self setDataSource:self];
  [self setDelegate:(id)self];
}
- (void)dealloc {
  [_array release];
  [super dealloc];
}
- (void)setArray:(NSArray *)array {
  [_array removeAllObjects];
  [_array addObjectsFromArray:array];
  [self reloadData];
}
- (NSString *)currentReading {
  NSMutableString *s = [NSMutableString string];
  NSEnumerator *e = [_array objectEnumerator];
  NSString *dataDict;
  int i = 0;
  while (dataDict = [e nextObject]) {
    NSString *reading = [dataDict valueForKey:@"CurrentBPMF"];
    [s appendString:reading];
    if (i < ([_array count] - 1)) [s appendString:@","];
    i++;
  }
  return s;
}

#pragma mark NSTableView data source and delegate methods.

- (int)numberOfRowsInTableView:(NSTableView *)aTableView {
  return (int)[_array count];
}

- (id)tableView:(NSTableView *)aTableView
    objectValueForTableColumn:(NSTableColumn *)aTableColumn
                          row:(int)rowIndex {
  NSString *identifier = [aTableColumn identifier];
  if ([identifier isEqualToString:@"character"]) {
    return [[_array objectAtIndex:rowIndex] valueForKey:@"Text"];
  }
  return nil;
}

- (void)tableView:(NSTableView *)aTableView
    willDisplayCell:(id)aCell
     forTableColumn:(NSTableColumn *)aTableColumn
                row:(int)rowIndex {
  NSString *identifier = [aTableColumn identifier];
  if ([identifier isEqualToString:@"reading"]) {
    NSDictionary *dataDict = [_array objectAtIndex:rowIndex];
    NSArray *readings = [dataDict valueForKey:@"BPMF"];
    NSString *crurrentReading = [dataDict valueForKey:@"CurrentBPMF"];

    int index = [readings indexOfObject:crurrentReading];
    if (index == NSNotFound) {
      index = 0;
    }

    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Menu"];
    NSEnumerator *e = [readings objectEnumerator];
    NSString *s;

    while (s = [e nextObject]) {
      NSMenuItem *menuItem = [[NSMenuItem new] autorelease];
      [menuItem setTitle:s];
      [menu addItem:menuItem];
    }
    [aCell setMenu:menu];

    if ([readings count] > 1) {
      [aCell setObjectValue:[NSNumber numberWithInt:index]];
    }
  }
}

- (void)tableView:(NSTableView *)aTableView
    setObjectValue:(id)anObject
    forTableColumn:(NSTableColumn *)aTableColumn
               row:(int)rowIndex {
  NSString *identifier = [aTableColumn identifier];

  if ([identifier isEqualToString:@"reading"]) {
    NSDictionary *dataDict = [_array objectAtIndex:rowIndex];
    NSString *s =
        [[dataDict valueForKey:@"BPMF"] objectAtIndex:[anObject intValue]];
    [dataDict setValue:s forKey:@"CurrentBPMF"];
  }
}

@end
