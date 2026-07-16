//
//  PEController.m
//  PhraseEditor
//
//  Developed by Lithoglyph Inc on 2008/10/19.
//  Copyright 2008  Yahoo! Inc. All rights reserved.
//

#import "PEController.h"

#import <AddressBook/AddressBook.h>

// Rows are fetched from SQLite in fixed-size windows so the table stays
// responsive with tens of thousands of phrases.
static const NSUInteger kPageSize = 256;

static void PEPresentSheetAlert(NSWindow *window, NSString *messageText,
                                NSString *informativeText,
                                NSAlertStyle alertStyle) {
  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:messageText];
  [alert setInformativeText:informativeText];
  [alert addButtonWithTitle:LFLSTR(@"OK")];
  [alert setAlertStyle:alertStyle];
  [alert beginSheetModalForWindow:window completionHandler:nil];
}

@implementation PEController

- (void)dealloc {
  [_pageCache release];
  [_filter release];
  [_store release];
  [_searchField release];
  [super dealloc];
}

- (void)awakeFromNib {
  [NSApp setDelegate:(id)self];
  _store = [[PEUserPhraseStore sharedStore] retain];
  if (![_store isAvailable]) {
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    [alert setMessageText:LFLSTR(@"Unable to open the phrase database.")];
    [alert setInformativeText:
               LFLSTR(@"The Phrase Editor cannot open your user phrase "
                      @"database. Check the permissions of the ChiaKey "
                      @"folder in Application Support.")];
    [alert addButtonWithTitle:LFLSTR(@"OK")];
    [alert runModal];
    [NSApp terminate:self];
  }

  _pageCache = [NSMutableDictionary new];
  _sortKey = PEPhraseSortKeyInsertion;
  _sortAscending = YES;
  _editingRowid = -1;

  NSToolbar *toolbar =
      [[[NSToolbar alloc] initWithIdentifier:@"toolbar"] autorelease];
  [toolbar setDelegate:(id)self];
  [[self window] setToolbar:toolbar];
  [[self window] setDelegate:(id)self];
  [[self window] center];
  if (@available(macOS 11, *)) {
    [[self window] setToolbarStyle:NSWindowToolbarStylePreference];
  }

  [_tableView
      registerForDraggedTypes:[NSArray arrayWithObject:NSPasteboardTypeString]];
  [_tableView setDraggingSourceOperationMask:NSDragOperationEvery forLocal:NO];
  [_tableView setAllowsMultipleSelection:YES];
  [_tableView setDataSource:self];
  [_tableView setDelegate:self];
  [_tableView setRowHeight:20.0];
  [_tableView setUsesAlternatingRowBackgroundColors:YES];
  // The ancient xib hardcodes a white table background, which fights dark
  // mode (white rows with light text); use semantic colors instead.
  [_tableView setBackgroundColor:[NSColor controlBackgroundColor]];
  [[_tableView enclosingScrollView]
      setBackgroundColor:[NSColor controlBackgroundColor]];

  [[_tableView tableColumnWithIdentifier:@"phrase"]
      setSortDescriptorPrototype:[NSSortDescriptor sortDescriptorWithKey:@"phrase"
                                                               ascending:YES]];
  [[_tableView tableColumnWithIdentifier:@"reading"]
      setSortDescriptorPrototype:[NSSortDescriptor
                                     sortDescriptorWithKey:@"reading"
                                                 ascending:YES]];

  [_phraseWindow setDefaultButtonCell:[_okButton cell]];

  [_store beginEditingSession];
  [self reloadData];
}

#pragma mark Windowed data source

- (PEPhraseRecord *)recordForRow:(NSInteger)row {
  if (row < 0 || (NSUInteger)row >= _rowCount) return nil;

  NSNumber *pageKey = [NSNumber numberWithUnsignedInteger:row / kPageSize];
  NSArray *page = [_pageCache objectForKey:pageKey];
  if (!page) {
    page = [_store
        phrasesInRange:NSMakeRange([pageKey unsignedIntegerValue] * kPageSize,
                                   kPageSize)
                filter:_filter
               sortKey:_sortKey
             ascending:_sortAscending];
    [_pageCache setObject:page forKey:pageKey];
  }
  NSUInteger offset = (NSUInteger)row % kPageSize;
  if (offset >= [page count]) return nil;
  // The page array is the record's only owner and reloadData empties the
  // cache; keep returned records alive for the current cycle.
  return [[[page objectAtIndex:offset] retain] autorelease];
}

- (void)reloadData {
  [_pageCache removeAllObjects];
  _rowCount = [_store numberOfPhrasesMatchingFilter:_filter];
  [_tableView reloadData];
  [self updateStatus];
}

- (void)updateStatus {
  NSUInteger total = [_filter length]
                         ? [_store numberOfPhrasesMatchingFilter:nil]
                         : _rowCount;
  if ([_filter length]) {
    [_statusTextField setStringValue:
                          [NSString stringWithFormat:
                                        LFLSTR(@"%lu of %lu user phrase(s)"),
                                        (unsigned long)_rowCount,
                                        (unsigned long)total]];
  } else if (total) {
    [_statusTextField
        setStringValue:[NSString stringWithFormat:LFLSTR(@"%lu user phrase(s)"),
                                                  (unsigned long)total]];
  } else {
    [_statusTextField setStringValue:LFLSTR(@"No user phrases")];
  }
}

- (NSString *)validatedString:(NSString *)originalString {
  NSString *string = [originalString
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  if (![string length]) return nil;

  NSMutableString *validatedString = [NSMutableString string];
  NSUInteger i;
  for (i = 0; i < [string length]; i++) {
    unichar aChar = [string characterAtIndex:i];
    if (aChar >= 0x2E80 && aChar < 0xFF00) {
      [validatedString appendFormat:@"%C", aChar];
    }
  }
  return validatedString;
}

#pragma mark Search

- (void)searchFieldChanged:(id)sender {
  NSString *value = [[sender stringValue]
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  NSString *newFilter = [value length] ? value : nil;
  if (newFilter == _filter || [newFilter isEqualToString:_filter]) return;

  [_filter release];
  _filter = [newFilter copy];
  [self reloadData];
}

- (void)clearSearch {
  if (![_filter length] && ![[_searchField stringValue] length]) return;
  [_searchField setStringValue:@""];
  [_filter release];
  _filter = nil;
}

#pragma mark Interface Builder actions

- (IBAction)add:(id)sender {
  // New rows always show up at the bottom of the unfiltered list.
  [self clearSearch];
  _sortKey = PEPhraseSortKeyInsertion;
  _sortAscending = YES;
  [_tableView setSortDescriptors:[NSArray array]];

  PEPhraseRecord *record =
      [_store addPhrase:[NSString stringWithUTF8String:"新詞"]];
  [self reloadData];
  if (!record || !_rowCount) return;

  [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:_rowCount - 1]
          byExtendingSelection:NO];
  [_tableView scrollRowToVisible:_rowCount - 1];
  [self editPhrase:self];
}

- (IBAction)delete:(id)sender {
  NSIndexSet *rowIndexes = [_tableView selectedRowIndexes];
  if (![rowIndexes count]) return;

  NSMutableArray *rowids = [NSMutableArray array];
  NSUInteger currentIndex = [rowIndexes firstIndex];
  while (currentIndex != NSNotFound) {
    PEPhraseRecord *record = [self recordForRow:(NSInteger)currentIndex];
    if (record) {
      [rowids addObject:[NSNumber numberWithLongLong:record.rowid]];
    }
    currentIndex = [rowIndexes indexGreaterThanIndex:currentIndex];
  }
  [_store deletePhrasesWithRowids:rowids];
  [self reloadData];
}

- (NSString *)stringForCopying {
  NSIndexSet *rowIndexes = [_tableView selectedRowIndexes];
  NSMutableString *s = [NSMutableString string];

  BOOL prependType =
      ([[NSApp currentEvent] modifierFlags] & NSEventModifierFlagOption) != 0;
  NSUInteger currentIndex = [rowIndexes firstIndex];
  while (currentIndex != NSNotFound) {
    PEPhraseRecord *record = [self recordForRow:(NSInteger)currentIndex];
    currentIndex = [rowIndexes indexGreaterThanIndex:currentIndex];
    if (!record) continue;

    if ([s length]) [s appendString:@"\n"];
    if (prependType) [s appendString:@"+bpmf "];
    [s appendFormat:@"%@ %@", record.phrase, record.reading];
  }
  return s;
}

- (IBAction)copy:(id)sender {
  if (![[_tableView selectedRowIndexes] count]) return;

  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
  [pasteboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString]
                     owner:self];
  [pasteboard setString:[self stringForCopying] forType:NSPasteboardTypeString];
}

- (IBAction)editPhrase:(id)sender {
  NSInteger selectedRow = [_tableView selectedRow];
  if (selectedRow < 0) return;

  [_tableView editColumn:[_tableView columnWithIdentifier:@"phrase"]
                     row:selectedRow
               withEvent:nil
                  select:YES];
}

- (IBAction)editReading:(id)sender {
  NSInteger selectedRow = [_tableView selectedRow];
  if (selectedRow < 0) return;
  [self editReadingForRecord:[self recordForRow:selectedRow]];
}

- (void)editReadingForRecord:(PEPhraseRecord *)record {
  if (![record.phrase length]) return;

  _editingRowid = record.rowid;

  NSArray *bpmfArray = [record.reading componentsSeparatedByString:@","];
  NSMutableArray *a = [NSMutableArray array];
  NSUInteger idx = 0;
  NSUInteger charIndex = 0;
  NSString *phrase = record.phrase;
  while (idx < [phrase length]) {
    NSRange r = [phrase rangeOfComposedCharacterSequenceAtIndex:idx];
    NSString *ch = [phrase substringWithRange:r];
    idx = r.location + r.length;

    NSArray *readings = [_store readingsForCharacter:ch];
    NSString *current = charIndex < [bpmfArray count]
                            ? [bpmfArray objectAtIndex:charIndex]
                            : [readings objectAtIndex:0];

    NSMutableDictionary *d = [NSMutableDictionary dictionary];
    [d setValue:ch forKey:@"Text"];
    [d setValue:current forKey:@"CurrentBPMF"];
    [d setValue:readings forKey:@"BPMF"];
    [a addObject:d];
    charIndex++;
  }
  [_phraseTableView setArray:a];

  [[self window] beginSheet:_phraseWindow completionHandler:nil];
}

- (void)importAddressBook {
  [_progressIndicator setUsesThreadedAnimation:YES];
  [_progressIndicator startAnimation:self];
  [_progressTextField setStringValue:LFLSTR(@"Progressing...")];

  [[self window] beginSheet:_progressWindow completionHandler:nil];

  BOOL skipExisting = ![_importAlreadyExistCheckBox intValue];

  NSArray *people = [[ABAddressBook sharedAddressBook] people];
  NSMutableArray *array = [NSMutableArray array];
  NSMutableSet *batchSeen = [NSMutableSet set];
  NSEnumerator *enumerator = [people objectEnumerator];
  ABPerson *person;
  while (person = [enumerator nextObject]) {
    NSString *lastName =
        [self validatedString:[person valueForProperty:kABLastNameProperty]];
    if (!lastName) lastName = @"";

    NSString *firstName =
        [self validatedString:[person valueForProperty:kABFirstNameProperty]];
    if (!firstName) firstName = @"";

    NSString *name = [NSString stringWithFormat:@"%@%@", lastName, firstName];

    if ([_importLastAndFirstNameCheckBox intValue] && [name length] < 5 &&
        [name length] > 1 && ![batchSeen containsObject:name] &&
        !(skipExisting && [_store containsPhrase:name])) {
      [batchSeen addObject:name];
      [array addObject:name];
    }
    if ([_importFirstNameCheckBox intValue] && [firstName length] < 5 &&
        [firstName length] > 1 && ![batchSeen containsObject:firstName] &&
        !(skipExisting && [_store containsPhrase:firstName])) {
      [batchSeen addObject:firstName];
      [array addObject:firstName];
    }
  }

  if ([array count]) {
    NSArray *records = [_store addPhrases:array];
    // 沈 as a surname reads ㄕㄣˇ, not the default ㄔㄣˊ.
    for (PEPhraseRecord *record in records) {
      if (![record.phrase hasPrefix:[NSString stringWithUTF8String:"沈"]])
        continue;
      NSMutableArray *parts = [[[record.reading
          componentsSeparatedByString:@","] mutableCopy] autorelease];
      if (![parts count]) continue;
      [parts replaceObjectAtIndex:0
                       withObject:[NSString stringWithUTF8String:"ㄕㄣˇ"]];
      [_store setReading:[parts componentsJoinedByString:@","]
                forRowid:record.rowid];
    }
  }

  [[self window] endSheet:_progressWindow];
  [_progressIndicator stopAnimation:self];
  [_progressWindow orderOut:self];

  [self reloadData];
}

- (IBAction)continueImportAction:(id)sender {
  if (![_importLastAndFirstNameCheckBox intValue] &&
      ![_importFirstNameCheckBox intValue]) {
    PEPresentSheetAlert(_importOptionWindow, LFLSTR(@"Error"),
                        LFLSTR(@"You must select at least one option from "
                               @"\"Import Last + First Name\" and \"Import "
                               @"Fist Name\"."),
                        NSAlertStyleWarning);
    return;
  }

  [[self window] endSheet:[[self window] attachedSheet]];
  [[[self window] attachedSheet] orderOut:self];
  [self importAddressBook];
}

- (IBAction)cancelImportAction:(id)sender {
  [[self window] endSheet:[[self window] attachedSheet]];
  [[[self window] attachedSheet] orderOut:self];
}

- (IBAction)importAddressBook:(id)sender {
  if ([[self window] attachedSheet]) {
    [[self window] endSheet:[[self window] attachedSheet]];
    [[[self window] attachedSheet] orderOut:self];
  }
  [_importOptionWindow setDefaultButtonCell:[_continueImportButton cell]];
  [[self window] beginSheet:_importOptionWindow completionHandler:nil];
}

- (void)importUserPhraseDatabaseFromURL:(NSURL *)url {
  if (!url || !_store) return;

  BOOL rtn = [_store importUserPhraseDBFromFile:[url path]];
  [self reloadData];
  if (rtn) {
    PEPresentSheetAlert([self window], LFLSTR(@"Done"),
                        LFLSTR(@"Your phrases are successfully imported."),
                        NSAlertStyleInformational);
  } else {
    PEPresentSheetAlert([self window], LFLSTR(@"Error"),
                        LFLSTR(@"Unable to import database."),
                        NSAlertStyleWarning);
  }
}

- (IBAction)importAction:(id)sender {
  NSOpenPanel *panel = [NSOpenPanel openPanel];
  [panel setAllowedFileTypes:[NSArray arrayWithObjects:@"txt", nil]];
  [panel setExtensionHidden:NO];
  [panel setCanCreateDirectories:NO];
  [panel setTitle:LFLSTR(@"Import Database")];
  [panel setMessage:LFLSTR(@"Import customized phrases to your own database.")];
  [panel setPrompt:LFLSTR(@"Choose")];

  [panel beginSheetModalForWindow:[self window]
                completionHandler:^(NSModalResponse result) {
                  if (result != NSModalResponseOK) return;
                  [self importUserPhraseDatabaseFromURL:
                            [[panel URLs] objectAtIndex:0]];
                }];
}

- (void)exportUserPhraseDatabaseToURL:(NSURL *)url {
  if (!url || !_store) return;

  BOOL rtn = [_store exportUserPhraseDBToFile:[url path]];
  if (rtn) {
    PEPresentSheetAlert([self window], LFLSTR(@"Done"),
                        LFLSTR(@"Your phrases are successfully exported."),
                        NSAlertStyleInformational);
  } else {
    PEPresentSheetAlert([self window], LFLSTR(@"Error"),
                        LFLSTR(@"Unable to export database."),
                        NSAlertStyleWarning);
  }
}

- (IBAction)exportAction:(id)sender {
  NSSavePanel *panel = [NSSavePanel savePanel];
  [panel setAllowedFileTypes:[NSArray arrayWithObjects:@"txt", nil]];
  [panel setExtensionHidden:NO];
  [panel setCanCreateDirectories:NO];
  [panel setNameFieldLabel:LFLSTR(@"Export As:")];
  [panel setTitle:LFLSTR(@"Export Database")];
  [panel setMessage:LFLSTR(@"Exporting your own customized phrases database.")];
  [panel setPrompt:LFLSTR(@"Export")];
  [panel beginSheetModalForWindow:[self window]
                completionHandler:^(NSModalResponse result) {
                  if (result != NSModalResponseOK) return;
                  [self exportUserPhraseDatabaseToURL:[panel URL]];
                }];
}

- (IBAction)okAction:(id)sender {
  if (_editingRowid >= 0) {
    [_store setReading:[_phraseTableView currentReading]
              forRowid:_editingRowid];
    _editingRowid = -1;
    [self reloadData];
  }

  [[self window] endSheet:_phraseWindow];
  [_phraseWindow orderOut:self];
}

- (IBAction)cancelAction:(id)sender {
  _editingRowid = -1;
  [[self window] endSheet:_phraseWindow];
  [_phraseWindow orderOut:self];
}

- (IBAction)launchOnlineHelp:(id)sender {
  NSURL *url = [NSURL URLWithString:HELP_URL];
  [[NSWorkspace sharedWorkspace] openURL:url];
}

#pragma mark Validate menu item

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
  if ([[self window] attachedSheet]) return NO;
  return YES;
}

- (BOOL)validateToolbarItem:(NSToolbarItem *)theItem {
  SEL selector = [theItem action];
  if (selector == @selector(delete:) || selector == @selector(editPhrase:) ||
      selector == @selector(editReading:)) {
    if ([_tableView selectedRow] < 0) return NO;
  }
  return YES;
}

#pragma mark -
#pragma mark NSApplication delegate methods.

- (void)applicationWillTerminate:(NSNotification *)aNotification {
  [_store endEditingSession];
}

#pragma mark NSTableView data source / delegate (view-based, windowed)

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView {
  return (NSInteger)_rowCount;
}

- (NSView *)tableView:(NSTableView *)tableView
    viewForTableColumn:(NSTableColumn *)tableColumn
                   row:(NSInteger)row {
  NSString *identifier = [tableColumn identifier];
  BOOL isPhrase = [identifier isEqualToString:@"phrase"];

  NSTableCellView *cellView = [tableView makeViewWithIdentifier:identifier
                                                          owner:self];
  if (!cellView) {
    cellView = [[[NSTableCellView alloc] initWithFrame:NSZeroRect] autorelease];
    [cellView setIdentifier:identifier];

    NSTextField *textField =
        [[[NSTextField alloc] initWithFrame:NSZeroRect] autorelease];
    [textField setBordered:NO];
    [textField setDrawsBackground:NO];
    [textField setLineBreakMode:NSLineBreakByTruncatingTail];
    NSFont *font = [NSFont fontWithName:@"LiHeiPro" size:isPhrase ? 16.0 : 12.0];
    if (!font)
      font = [NSFont systemFontOfSize:isPhrase ? 16.0 : 12.0];
    [textField setFont:font];
    if (isPhrase) {
      [textField setEditable:YES];
      [textField setTarget:self];
      [textField setAction:@selector(phraseFieldEdited:)];
      // Commit on blur too, not only on Return.
      [[textField cell] setSendsActionOnEndEditing:YES];
    } else {
      [textField setEditable:NO];
      [textField setSelectable:NO];
      [textField setTextColor:[NSColor secondaryLabelColor]];
    }
    [textField setTranslatesAutoresizingMaskIntoConstraints:NO];
    [cellView addSubview:textField];
    [cellView setTextField:textField];

    [NSLayoutConstraint activateConstraints:[NSArray arrayWithObjects:
        [[textField leadingAnchor]
            constraintEqualToAnchor:[cellView leadingAnchor] constant:2.0],
        [[textField trailingAnchor]
            constraintEqualToAnchor:[cellView trailingAnchor] constant:-2.0],
        [[textField centerYAnchor]
            constraintEqualToAnchor:[cellView centerYAnchor]],
        nil]];
  }

  PEPhraseRecord *record = [self recordForRow:row];
  [[cellView textField]
      setStringValue:(isPhrase ? record.phrase : record.reading) ?: @""];
  return cellView;
}

- (void)phraseFieldEdited:(id)sender {
  NSInteger row = [_tableView rowForView:sender];
  if (row < 0) return;

  PEPhraseRecord *record = [self recordForRow:row];
  if (!record) return;

  NSString *string = [self validatedString:[sender stringValue]];
  if (![string length]) {
    [sender setStringValue:record.phrase ?: @""];
    return;
  }
  if ([string length] > 7) string = [string substringToIndex:7];

  if ([record.phrase isEqualToString:string]) {
    [sender setStringValue:string];
    return;
  }

  // reloadData empties the page cache that owns `record`; don't touch the
  // record after it.
  long long rowid = record.rowid;
  [_store setPhrase:string forRowid:rowid];
  [self reloadData];
  [self editReadingForRecord:[_store phraseForRowid:rowid]];
}

- (void)tableView:(NSTableView *)tableView
    sortDescriptorsDidChange:(NSArray *)oldDescriptors {
  NSArray *descriptors = [tableView sortDescriptors];
  if ([descriptors count]) {
    NSSortDescriptor *descriptor = [descriptors objectAtIndex:0];
    if ([[descriptor key] isEqualToString:@"phrase"]) {
      _sortKey = PEPhraseSortKeyPhrase;
    } else if ([[descriptor key] isEqualToString:@"reading"]) {
      _sortKey = PEPhraseSortKeyReading;
    } else {
      _sortKey = PEPhraseSortKeyInsertion;
    }
    _sortAscending = [descriptor ascending];
  } else {
    _sortKey = PEPhraseSortKeyInsertion;
    _sortAscending = YES;
  }
  [self reloadData];
}

- (BOOL)tableView:(NSTableView *)tv
    writeRowsWithIndexes:(NSIndexSet *)rowIndexes
            toPasteboard:(NSPasteboard *)pboard {
  [pboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString]
                 owner:self];
  [pboard setString:[self stringForCopying] forType:NSPasteboardTypeString];
  return YES;
}

- (NSDragOperation)tableView:(NSTableView *)tv
                validateDrop:(id<NSDraggingInfo>)info
                 proposedRow:(NSInteger)row
       proposedDropOperation:(NSTableViewDropOperation)op {
  return NSDragOperationNone;
}

#pragma mark NSWindow delegate methods

- (void)windowWillClose:(NSNotification *)notification {
  [NSApp terminate:self];
}

#pragma mark -
#pragma mark NSToolbar delegate methods

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
  return [NSArray arrayWithObjects:addToolbarItemIdentifier,
                                   deleteToolbarItemIdentifier,
                                   editPhraseToolbarItemIdentifier,
                                   editReaingToolbarItemIdentifier,
                                   reloadToolbarItemIdentifier,
                                   importAddressBookToolbarItemIdentifier,
                                   NSToolbarFlexibleSpaceItemIdentifier,
                                   searchToolbarItemIdentifier, nil];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
  return [NSArray arrayWithObjects:addToolbarItemIdentifier,
                                   deleteToolbarItemIdentifier,
                                   editPhraseToolbarItemIdentifier,
                                   editReaingToolbarItemIdentifier,
                                   importAddressBookToolbarItemIdentifier,
                                   reloadToolbarItemIdentifier,
                                   NSToolbarFlexibleSpaceItemIdentifier,
                                   searchToolbarItemIdentifier, nil];
}

- (NSArray *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar {
  return nil;
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
        itemForItemIdentifier:(NSString *)identifier
    willBeInsertedIntoToolbar:(BOOL)willBeInserted {
  NSToolbarItem *item =
      [[[NSToolbarItem alloc] initWithItemIdentifier:identifier] autorelease];
  if ([identifier isEqualToString:addToolbarItemIdentifier]) {
    [item setImage:[NSImage imageNamed:@"add"]];
    [item setLabel:LFLSTR(addToolbarItemIdentifier)];
    [item setTarget:self];
    [item setAction:@selector(add:)];
  } else if ([identifier isEqualToString:deleteToolbarItemIdentifier]) {
    [item setImage:[NSImage imageNamed:@"delete"]];
    [item setLabel:LFLSTR(deleteToolbarItemIdentifier)];
    [item setTarget:self];
    [item setAction:@selector(delete:)];
  } else if ([identifier isEqualToString:editPhraseToolbarItemIdentifier]) {
    [item setImage:[NSImage imageNamed:@"editPhrase"]];
    [item setLabel:LFLSTR(editPhraseToolbarItemIdentifier)];
    [item setTarget:self];
    [item setAction:@selector(editPhrase:)];
  } else if ([identifier isEqualToString:editReaingToolbarItemIdentifier]) {
    [item setImage:[NSImage imageNamed:@"editReading"]];
    [item setLabel:LFLSTR(editReaingToolbarItemIdentifier)];
    [item setTarget:self];
    [item setAction:@selector(editReading:)];
  } else if ([identifier
                 isEqualToString:importAddressBookToolbarItemIdentifier]) {
    [item setImage:[NSImage imageNamed:@"addressBook"]];
    [item setLabel:LFLSTR(importAddressBookToolbarItemIdentifier)];
    [item setTarget:self];
    [item setAction:@selector(importAddressBook:)];
  } else if ([identifier isEqualToString:reloadToolbarItemIdentifier]) {
    [item setImage:[NSImage imageNamed:@"reload"]];
    [item setLabel:LFLSTR(reloadToolbarItemIdentifier)];
    [item setTarget:self];
    [item setAction:@selector(reloadData)];
  } else if ([identifier isEqualToString:searchToolbarItemIdentifier]) {
    if (@available(macOS 11, *)) {
      // A plain custom-view item loses the field's bezel under the modern
      // toolbar styles; the dedicated item draws correctly.
      NSSearchToolbarItem *searchItem = [[[NSSearchToolbarItem alloc]
          initWithItemIdentifier:identifier] autorelease];
      [searchItem setLabel:LFLSTR(searchToolbarItemIdentifier)];
      [_searchField release];
      _searchField = [[searchItem searchField] retain];
      [_searchField setTarget:self];
      [_searchField setAction:@selector(searchFieldChanged:)];
      [[_searchField cell] setSendsSearchStringImmediately:NO];
      return searchItem;
    }
    if (!_searchField) {
      _searchField =
          [[NSSearchField alloc] initWithFrame:NSMakeRect(0, 0, 180, 22)];
      [_searchField setTarget:self];
      [_searchField setAction:@selector(searchFieldChanged:)];
      [[_searchField cell] setSendsSearchStringImmediately:NO];
    }
    [item setView:_searchField];
    [item setLabel:LFLSTR(searchToolbarItemIdentifier)];
    [item setMinSize:NSMakeSize(120, 22)];
    [item setMaxSize:NSMakeSize(260, 22)];
  } else {
    item = nil;
  }
  return item;
}

@end
