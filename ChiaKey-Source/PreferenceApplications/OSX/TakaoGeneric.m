/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoGeneric.h"

#import "TakaoCINTable.h"
#import "TakaoSettings.h"

// Height of the add/remove bar we tuck under the table list. The pane comes
// out of the nib with the list occupying the full height, so we shorten it
// here rather than keeping two localized copies of MainMenu.xib in sync.
static const CGFloat kButtonBarHeight = 28.0;

@interface TakaoGeneric (Private)
- (void)_buildButtonBar;
- (void)_confirmImportOfFileAtPath:(NSString *)path;
- (void)_showSettingsForRow:(NSInteger)row;
- (void)_updateButtonState;
- (BOOL)_requestReloadAndReturnIMEIsRunning;
- (void)_showAlertWithTitle:(NSString *)title message:(NSString *)message;
- (NSWindow *)_window;
@end

@implementation TakaoGeneric

- (void)dealloc {
  [_modules release];
  [_emptyStateTextField release];
  [super dealloc];
}

- (void)awakeFromNib {
  // Nib loading can send this more than once; the button bar resizes the
  // list, so doing it twice would keep shrinking it.
  if (_emptyStateTextField) return;

  [self _buildButtonBar];

  // The nib's header view is layer-backed and does not repaint as rows scroll
  // underneath it, so it turns transparent once the list is long enough to
  // scroll. A one-column list sitting under a pane already titled "Custom"
  // does not need a column title anyway.
  [_genericModuleListTableView setHeaderView:nil];

  [_genericModuleListTableView setDataSource:self];
  [_genericModuleListTableView setDelegate:self];
  // With rows present there is always a selection, so clicking blank space
  // below the list cannot land the pane in a "nothing selected" state that
  // has nothing sensible to show.
  [_genericModuleListTableView setAllowsEmptySelection:NO];
  // The nib silences the focus ring on the enclosing scroll view but not
  // on the table itself, so focusing the list drew a ring around the whole
  // sidebar. The selection highlight already shows where focus is.
  [_genericModuleListTableView setFocusRingType:NSFocusRingTypeNone];
  [_genericModuleListTableView
      registerForDraggedTypes:[NSArray
                                  arrayWithObject:NSPasteboardTypeFileURL]];

  _emptyStateTextField =
      [[NSTextField alloc] initWithFrame:NSMakeRect(20, 0, 276, 120)];
  [_emptyStateTextField setEditable:NO];
  [_emptyStateTextField setSelectable:NO];
  [_emptyStateTextField setBezeled:NO];
  [_emptyStateTextField setDrawsBackground:NO];
  [_emptyStateTextField setAlignment:NSTextAlignmentCenter];
  [_emptyStateTextField setTextColor:[NSColor secondaryLabelColor]];
  [[_emptyStateTextField cell] setWraps:YES];
  [_emptyStateTextField
      setStringValue:LFLSTR(@"No input tables have been imported.\n\nClick + "
                            @"or drag a .cin table file here to import one. "
                            @"Imported tables appear in the input menu "
                            @"alongside the built-in input methods.")];

  NSRect settingFrame = [_genericSettingView bounds];
  NSRect labelFrame = [_emptyStateTextField frame];
  labelFrame.origin.x = NSMidX(settingFrame) - labelFrame.size.width / 2.0;
  labelFrame.origin.y = NSMidY(settingFrame) - labelFrame.size.height / 2.0;
  [_emptyStateTextField setFrame:labelFrame];

  [self reloadTables];
}

- (void)_buildButtonBar {
  NSScrollView *scrollView = [_genericModuleListTableView enclosingScrollView];
  NSView *container = [scrollView superview];
  if (!scrollView || !container) return;

  NSRect frame = [scrollView frame];
  frame.origin.y += kButtonBarHeight;
  frame.size.height -= kButtonBarHeight;
  [scrollView setFrame:frame];

  CGFloat width = 30.0;
  NSRect importFrame =
      NSMakeRect(NSMinX(frame), 0, width, kButtonBarHeight - 2.0);
  NSRect removeFrame = importFrame;
  removeFrame.origin.x += width - 1.0;

  _importButton = [[[NSButton alloc] initWithFrame:importFrame] autorelease];
  [_importButton setBezelStyle:NSBezelStyleSmallSquare];
  [_importButton setTitle:@"+"];
  [_importButton setTarget:self];
  [_importButton setAction:@selector(importTable:)];
  [_importButton setToolTip:LFLSTR(@"Import a .cin table file")];
  [container addSubview:_importButton];

  _removeButton = [[[NSButton alloc] initWithFrame:removeFrame] autorelease];
  [_removeButton setBezelStyle:NSBezelStyleSmallSquare];
  [_removeButton setTitle:@"−"];
  [_removeButton setTarget:self];
  [_removeButton setAction:@selector(removeSelectedTable:)];
  [_removeButton setToolTip:LFLSTR(@"Remove the selected table")];
  [container addSubview:_removeButton];
}

#pragma mark Contents

- (void)reloadTables {
  [self reloadTablesSelectingFileName:nil];
}

- (void)reloadTablesSelectingFileName:(NSString *)fileNameToSelect {
  if (!_modules)
    _modules = [[NSMutableArray alloc] init];
  else
    [_modules removeAllObjects];

  NSEnumerator *enumerator = [[TakaoCINTable installedTables] objectEnumerator];
  NSDictionary *table = nil;
  while (table = [enumerator nextObject]) {
    TakaoGenericSettings *controller = [[TakaoGenericSettings alloc] init];
    [controller
        loadSettingsWithName:[table objectForKey:TakaoCINTableIdentifierKey]
               localizedName:[table objectForKey:TakaoCINTableDisplayNameKey]];

    NSMutableDictionary *entry =
        [NSMutableDictionary dictionaryWithDictionary:table];
    [entry setObject:controller forKey:@"controller"];
    [controller release];
    [_modules addObject:entry];
  }

  [_genericModuleListTableView reloadData];

  if ([_modules count]) {
    // After an import, land on the table that was just added rather than on
    // whatever happens to sort first, so the result of the action is visible.
    NSUInteger row = 0;
    if ([fileNameToSelect length]) {
      for (NSUInteger i = 0; i < [_modules count]; i++) {
        if ([[[_modules objectAtIndex:i]
                objectForKey:TakaoCINTableFileNameKey]
                isEqualToString:fileNameToSelect]) {
          row = i;
          break;
        }
      }
    }
    [_genericModuleListTableView
            selectRowIndexes:[NSIndexSet indexSetWithIndex:row]
        byExtendingSelection:NO];
    [_genericModuleListTableView scrollRowToVisible:row];
    [self _showSettingsForRow:row];
  } else {
    [self _showSettingsForRow:-1];
  }

  [self _updateButtonState];
}

- (void)_showSettingsForRow:(NSInteger)row {
  NSEnumerator *enumerator =
      [[[[_genericSettingView subviews] copy] autorelease] objectEnumerator];
  NSView *subview = nil;
  while (subview = [enumerator nextObject]) [subview removeFromSuperview];

  // The empty state describes having imported nothing at all; it must not
  // stand in for "no row is selected", which is a different situation.
  if (![_modules count]) {
    [_genericSettingView addSubview:_emptyStateTextField];
    return;
  }

  if (row < 0 || row >= (NSInteger)[_modules count]) return;

  id controller = [[_modules objectAtIndex:row] objectForKey:@"controller"];
  if (controller) [_genericSettingView addSubview:[controller view]];
}

- (void)_updateButtonState {
  [_removeButton setEnabled:[_genericModuleListTableView selectedRow] >= 0];
}

#pragma mark Actions

- (IBAction)importTable:(id)sender {
  NSOpenPanel *panel = [NSOpenPanel openPanel];
  [panel setAllowsMultipleSelection:NO];
  [panel setCanChooseDirectories:NO];
  [panel setCanChooseFiles:YES];
  [panel setAllowedFileTypes:[NSArray arrayWithObject:@"cin"]];
  [panel setMessage:LFLSTR(@"Choose a .cin input table file to import.")];
  [panel setPrompt:LFLSTR(@"Import")];

  [panel beginSheetModalForWindow:[self _window]
                completionHandler:^(NSInteger result) {
                  if (result != NSModalResponseOK) return;
                  NSURL *url = [[panel URLs] lastObject];
                  if (!url) return;

                  // Let the open panel finish dismissing before we put up
                  // the confirmation sheet on the same window.
                  dispatch_async(dispatch_get_main_queue(), ^{
                    [self _confirmImportOfFileAtPath:[url path]];
                  });
                }];
}

- (void)_confirmImportOfFileAtPath:(NSString *)path {
  NSError *error = nil;
  NSDictionary *table = [TakaoCINTable inspectFileAtPath:path error:&error];
  if (!table) {
    [self _showAlertWithTitle:LFLSTR(@"This table cannot be imported")
                      message:[error localizedDescription]];
    return;
  }

  NSString *fileName = [table objectForKey:TakaoCINTableFileNameKey];
  BOOL replacing = [TakaoCINTable isTableInstalledWithFileName:fileName];

  NSMutableString *details = [NSMutableString string];
  [details appendFormat:LFLSTR(@"%@ entries"),
                        [NSNumberFormatter
                            localizedStringFromNumber:
                                [table objectForKey:TakaoCINTableEntryCountKey]
                                          numberStyle:
                                              NSNumberFormatterDecimalStyle]];

  NSString *selectionKeys =
      [table objectForKey:TakaoCINTableSelectionKeysKey];
  if ([selectionKeys length])
    [details
        appendFormat:LFLSTR(@"\nSelection keys: %@"), selectionKeys];

  [details appendFormat:LFLSTR(@"\nWill be installed as: %@"), fileName];

  NSString *sourceEncoding =
      [table objectForKey:TakaoCINTableSourceEncodingKey];
  if (sourceEncoding)
    [details appendFormat:LFLSTR(@"\n\nThis file is %@ and will be converted "
                                 @"to UTF-8 on import."),
                          sourceEncoding];

  if (replacing)
    [details appendString:LFLSTR(@"\n\nA table with this name is already "
                                 @"installed and will be replaced.")];

  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:[NSString
                            stringWithFormat:LFLSTR(@"Import “%@”?"),
                                             [table objectForKey:
                                                        TakaoCINTableDisplayNameKey]]];
  [alert setInformativeText:details];
  [alert addButtonWithTitle:replacing ? LFLSTR(@"Replace") : LFLSTR(@"Import")];
  [alert addButtonWithTitle:LFLSTR(@"Cancel")];

  [alert beginSheetModalForWindow:[self _window]
                completionHandler:^(NSModalResponse response) {
                  if (response != NSAlertFirstButtonReturn) return;

                  NSError *installError = nil;
                  if (![TakaoCINTable installTable:table
                                         overwrite:YES
                                             error:&installError]) {
                    [self _showAlertWithTitle:LFLSTR(@"The table could not be "
                                                     @"imported")
                                      message:[installError localizedDescription]];
                    return;
                  }

                  BOOL reloaded = [self _requestReloadAndReturnIMEIsRunning];
                  [self reloadTablesSelectingFileName:fileName];

                  // The new row appearing selected is the confirmation: the
                  // user just answered a sheet naming this table, so saying
                  // it worked adds a click without adding information. The
                  // one case worth interrupting for is the IME not running,
                  // where the table is installed but nothing has loaded it.
                  if (reloaded) return;

                  // Let the confirmation sheet finish dismissing before the
                  // next one goes up on the same window.
                  dispatch_async(dispatch_get_main_queue(), ^{
                    [self _showAlertWithTitle:LFLSTR(@"Table imported")
                                      message:
                                          LFLSTR(@"Switch away from and back "
                                                 @"to ChiaKey to start using "
                                                 @"it.")];
                  });
                }];
}

- (IBAction)removeSelectedTable:(id)sender {
  NSInteger row = [_genericModuleListTableView selectedRow];
  if (row < 0 || row >= (NSInteger)[_modules count]) return;

  NSDictionary *table = [_modules objectAtIndex:row];
  NSString *fileName = [table objectForKey:TakaoCINTableFileNameKey];
  NSString *displayName = [table objectForKey:TakaoCINTableDisplayNameKey];

  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setAlertStyle:NSAlertStyleWarning];
  [alert setMessageText:[NSString stringWithFormat:LFLSTR(@"Remove “%@”?"),
                                                   displayName]];
  [alert setInformativeText:
             LFLSTR(@"The table file will be deleted and the input method "
                    @"will no longer appear in the input menu. You can import "
                    @"it again later.")];
  [alert addButtonWithTitle:LFLSTR(@"Remove")];
  [alert addButtonWithTitle:LFLSTR(@"Cancel")];

  [alert beginSheetModalForWindow:[self _window]
                completionHandler:^(NSModalResponse response) {
                  if (response != NSAlertFirstButtonReturn) return;

                  NSError *error = nil;
                  if (![TakaoCINTable removeTableWithFileName:fileName
                                                        error:&error]) {
                    [self _showAlertWithTitle:LFLSTR(@"The table could not be "
                                                     @"removed")
                                      message:[error localizedDescription]];
                    return;
                  }

                  [self _requestReloadAndReturnIMEIsRunning];
                  [self reloadTables];
                }];
}

#pragma mark Helpers

- (BOOL)_requestReloadAndReturnIMEIsRunning {
  if (!ChiaKeyIMEIsRunning()) return NO;
  ChiaKeyPostServiceNotification(ChiaKeyReloadRequestedNotification);
  return YES;
}

- (void)_showAlertWithTitle:(NSString *)title message:(NSString *)message {
  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:title ? title : @""];
  [alert setInformativeText:message ? message : @""];
  [alert addButtonWithTitle:LFLSTR(@"OK")];
  [alert beginSheetModalForWindow:[self _window] completionHandler:nil];
}

- (NSWindow *)_window {
  return [_genericModuleListTableView window];
}

#pragma mark NSTableView data source methods

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView {
  return [_modules count];
}

- (id)tableView:(NSTableView *)aTableView
    objectValueForTableColumn:(NSTableColumn *)aTableColumn
                          row:(NSInteger)rowIndex {
  if ([[aTableColumn identifier] isEqualToString:@"modules"])
    return [[_modules objectAtIndex:rowIndex]
        objectForKey:TakaoCINTableDisplayNameKey];
  return nil;
}

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification {
  [self _showSettingsForRow:[_genericModuleListTableView selectedRow]];
  [self _updateButtonState];
}

#pragma mark Drag and drop

// One table per drop: each import puts up its own confirmation sheet, and
// queueing several of those behind one another is worse than asking the user
// to drop them one at a time.
- (NSString *)_droppedTablePathFromInfo:(id<NSDraggingInfo>)info {
  NSDictionary *options =
      [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:YES]
                                  forKey:NSPasteboardURLReadingFileURLsOnlyKey];
  NSArray *urls = [[info draggingPasteboard]
      readObjectsForClasses:[NSArray arrayWithObject:[NSURL class]]
                    options:options];

  NSString *found = nil;
  NSEnumerator *enumerator = [urls objectEnumerator];
  NSURL *url = nil;
  while (url = [enumerator nextObject]) {
    if (![[[url pathExtension] lowercaseString] isEqualToString:@"cin"])
      continue;
    if (found) return nil;
    found = [url path];
  }
  return found;
}

- (NSDragOperation)tableView:(NSTableView *)tableView
                validateDrop:(id<NSDraggingInfo>)info
                 proposedRow:(NSInteger)row
       proposedDropOperation:(NSTableViewDropOperation)dropOperation {
  if (![self _droppedTablePathFromInfo:info]) return NSDragOperationNone;

  // The list has no user-defined order, so highlight the whole table rather
  // than pretending the drop lands between two particular rows.
  [tableView setDropRow:-1 dropOperation:NSTableViewDropOn];
  return NSDragOperationCopy;
}

- (BOOL)tableView:(NSTableView *)tableView
       acceptDrop:(id<NSDraggingInfo>)info
              row:(NSInteger)row
    dropOperation:(NSTableViewDropOperation)dropOperation {
  NSString *path = [self _droppedTablePathFromInfo:info];
  if (!path) return NO;

  // Let the drag session finish before a sheet goes up on the window.
  dispatch_async(dispatch_get_main_queue(), ^{
    [self _confirmImportOfFileAtPath:path];
  });
  return YES;
}

@end
