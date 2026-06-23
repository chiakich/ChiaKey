//
//  PEController.m
//  PhraseEditor
//
//  Developed by Lithoglyph Inc on 2008/10/19.
//  Copyright 2008  Yahoo! Inc. All rights reserved.
//

#import "PEController.h"

#import <AddressBook/AddressBook.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
typedef unsigned int NSUInteger;
#endif

@implementation PEController

- (void)dealloc {
  [[NSNotificationCenter defaultCenter]
      removeObserver:@"NSPopUpButtonCellWillPopUpNotification"];
  [super dealloc];
}
- (void)awakeFromNib {
  [NSApp setDelegate:(id)self];
  _loader = [[NSConnection
      rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
                                          host:nil] retain];
  if (_loader) {
    [_loader setProtocolForProxy:@protocol(TakaoPhraseEditorService)];
  } else {
    NSAlert *alert =
        [NSAlert alertWithMessageText:LFLSTR(@"ChiaKey is not running.")
                        defaultButton:LFLSTR(@"OK")
                      alternateButton:nil
                          otherButton:nil
            informativeTextWithFormat:
                LFLSTR(@"Since ChiaKey is not running, you cannot use "
                       @"the Phrase Editor to alter your phrases.")];
    [alert runModal];
    [NSApp terminate:self];
  }

  NSToolbar *toolbar =
      [[[NSToolbar alloc] initWithIdentifier:@"toolbar"] autorelease];
  [toolbar setDelegate:(id)self];
  [[self window] setToolbar:toolbar];
  [[self window] setDelegate:(id)self];
  [[self window] center];
  if (@available(macOS 11, *)) {
    [[self window] setToolbarStyle:NSWindowToolbarStylePreference];
  }

  [NSApp setDelegate:(id)self];

#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
  NSTokenFieldCell *cell = [[[NSTokenFieldCell alloc] init] autorelease];
  [[_tableView tableColumnWithIdentifier:@"reading"] setDataCell:cell];
#endif
  [_tableView
      registerForDraggedTypes:[NSArray arrayWithObject:NSStringPboardType]];
  [_tableView setDraggingSourceOperationMask:NSDragOperationEvery forLocal:NO];
  [_tableView setAllowsMultipleSelection:YES];
  [_tableView setDataSource:self];
  [_tableView setDelegate:(id)self];
  [_tableView setRowHeight:20.0];

  [_phraseWindow setDefaultButtonCell:[_okButton cell]];
  [self updateStatus];
}
- (void)updateStatus {
  int count = [_loader userPhraseDBNumberOfRow];
  if (count) {
    NSString *message =
        [NSString stringWithFormat:LFLSTR(@"%d user phrase(s)"), count];
    [_statusTextField setStringValue:message];
  } else {
    [_statusTextField setStringValue:LFLSTR(@"No user phrases")];
  }
}

- (NSString *)validatedString:(NSString *)originalString {
  NSString *string = [originalString
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];

  if (!string && ![string length]) return nil;
  int i;
  NSMutableString *validatedString = [NSMutableString string];
  for (i = 0; i < [string length]; i++) {
    unichar aChar = [originalString characterAtIndex:i];
    if (aChar >= 0x2E80 && aChar < 0xFF00) {
      [validatedString appendFormat:@"%C", aChar];
    }
  }
  return validatedString;
}

#pragma mark Interface Builder actions

- (void)reloadData {
  [_tableView reloadData];
  [self updateStatus];
}

- (IBAction)add:(id)sender {
  [_loader userPhraseDBAddNewRow:[NSString stringWithUTF8String:"新詞"]];
  [_tableView reloadData];
  int count = [self numberOfRowsInTableView:_tableView];
  [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:count - 1]
          byExtendingSelection:NO];
  [self editPhrase:self];
  [self updateStatus];
}
- (IBAction)delete:(id)sender {
  NSIndexSet *rowIndexes = [_tableView selectedRowIndexes];
  if (![rowIndexes count]) {
    return;
  }

  NSUInteger currentIndex = [rowIndexes lastIndex];

  while (currentIndex != NSNotFound) {
    [_loader userPhraseDBDeleteRow:currentIndex];
    currentIndex = [rowIndexes indexLessThanIndex:currentIndex];
  }
  [_tableView reloadData];
  [self updateStatus];
}
- (NSString *)stringForCopying {
  NSIndexSet *rowIndexes = [_tableView selectedRowIndexes];
  NSMutableString *s = [NSMutableString string];

  NSUInteger currentIndex = [rowIndexes firstIndex];

  while (currentIndex != NSNotFound) {
    NSDictionary *dataDict = [_loader userPhraseDBDictionaryAtRow:currentIndex];

    NSString *phrase = [dataDict objectForKey:@"Text"];
    NSString *reading = [dataDict objectForKey:@"BPMF"];

    NSString *string = [NSString stringWithFormat:@"%@ %@", phrase, reading];
    NSEvent *e = [NSApp currentEvent];
    if ([e modifierFlags] & NSAlternateKeyMask) {
      string = [NSString stringWithFormat:@"+bpmf %@", string];
    }
    currentIndex = [rowIndexes indexGreaterThanIndex:currentIndex];
    if (currentIndex != NSNotFound) {
      [s appendFormat:@"%@\n", string];
    } else {
      [s appendString:string];
    }
  }
  return s;
}

- (IBAction)copy:(id)sender {
  NSIndexSet *rowIndexes = [_tableView selectedRowIndexes];
  if (![rowIndexes count]) {
    return;
  }

  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
  NSArray *types = [NSArray arrayWithObjects:NSStringPboardType, nil];
  [pasteboard declareTypes:types owner:self];
  [pasteboard setString:[self stringForCopying] forType:NSStringPboardType];
}
- (IBAction)editPhrase:(id)sender {
  int selectedRow = [_tableView selectedRow];
  if (selectedRow < 0) return;

  [_tableView editColumn:[_tableView columnWithIdentifier:@"phrase"]
                     row:selectedRow
               withEvent:nil
                  select:YES];
}
- (IBAction)editReading:(id)sender {
  int selectedRow = [_tableView selectedRow];
  if (selectedRow < 0) return;

  _editingRow = selectedRow;
  NSDictionary *dataDict = [_loader userPhraseDBDictionaryAtRow:selectedRow];
  NSString *phrase = [dataDict objectForKey:@"Text"];

  if (![phrase length]) return;

  NSArray *bpmfArray =
      [[dataDict objectForKey:@"BPMF"] componentsSeparatedByString:@","];
  int i = 0;
  NSMutableArray *a = [NSMutableArray array];
  for (i = 0; i < [phrase length]; i++) {
    NSMutableDictionary *d = [NSMutableDictionary dictionary];
    unichar aChar = [phrase characterAtIndex:i];
    NSString *s = [NSString stringWithCharacters:&aChar length:1];
    [d setValue:s forKey:@"Text"];
    NSArray *readings = [_loader userPhraseDBReadingsForPhrase:s];
    [d setValue:[bpmfArray objectAtIndex:i] forKey:@"CurrentBPMF"];
    [d setValue:readings forKey:@"BPMF"];
    [a addObject:d];
  }
  [_phraseTableView setArray:a];

  [NSApp beginSheet:_phraseWindow
      modalForWindow:[self window]
       modalDelegate:self
      didEndSelector:nil
         contextInfo:nil];
}
- (void)fixReadingForAddressBookImportFromIndex:(int)index {
  int i = index;
  for (i = index; i < [_loader userPhraseDBNumberOfRow]; i++) {
    NSDictionary *dataDict = [_loader userPhraseDBDictionaryAtRow:i];
    NSString *phrase = [dataDict valueForKey:@"Text"];
    NSString *reading = [dataDict valueForKey:@"BPMF"];
    if ([phrase hasPrefix:[NSString stringWithUTF8String:"沈"]]) {
      NSArray *a = [reading componentsSeparatedByString:@","];
      NSMutableString *string = [NSMutableString stringWithUTF8String:"ㄕㄣˇ,"];
      int j = 1;
      for (j = 1; j < [a count]; j++) {
        [string appendString:[a objectAtIndex:j]];
        if (j < [a count] - 1) [string appendString:@","];
      }
      [_loader userPhraseDBSetNewReading:string forPhraseAtRow:i];
    }
  }
}
- (void)importAddressBook {
  [_progressIndicator setUsesThreadedAnimation:YES];
  [_progressIndicator startAnimation:self];
  [_progressTextField setStringValue:LFLSTR(@"Progressing...")];

  int currentRows = [_loader userPhraseDBNumberOfRow];

  [NSApp beginSheet:_progressWindow
      modalForWindow:[self window]
       modalDelegate:self
      didEndSelector:nil
         contextInfo:nil];

  NSMutableArray *dictArray = [NSMutableArray array];
  if (![_importAlreadyExistCheckBox intValue]) {
    int count = [_loader userPhraseDBNumberOfRow];
    int i = 0;
    for (i = 0; i < count; i++) {
      NSDictionary *dataDict = [_loader userPhraseDBDictionaryAtRow:i];
      NSString *phrase = [dataDict objectForKey:@"Text"];
      if (![dictArray containsObject:phrase]) [dictArray addObject:phrase];
    }
  }

  NSArray *people = [[ABAddressBook sharedAddressBook] people];
  int count = [people count];

  NSMutableArray *array = [NSMutableArray array];
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

    if ([_importLastAndFirstNameCheckBox intValue] &&
        [name length]<5 && [name length]> 1) {
      if (![_importAlreadyExistCheckBox intValue]) {
        if (![dictArray containsObject:name]) {
          [array addObject:name];
        }
      } else {
        [array addObject:name];
      }
    }
    if ([_importFirstNameCheckBox intValue] &&
        [firstName length]<5 && [firstName length]> 1) {
      if (![_importAlreadyExistCheckBox intValue]) {
        if (![dictArray containsObject:firstName]) {
          [array addObject:firstName];
        }
      } else {
        [array addObject:firstName];
      }
    }
  }
  if ([array count]) {
    [_loader userPhraseDBAddNewRows:array];
    [self fixReadingForAddressBookImportFromIndex:currentRows];
  }

  [NSApp endSheet:_progressWindow];
  [_progressIndicator stopAnimation:self];
  [_progressWindow orderOut:self];

  [_tableView reloadData];
  [self updateStatus];
}
- (IBAction)continueImportAction:(id)sender {
  if (![_importLastAndFirstNameCheckBox intValue] &&
      ![_importFirstNameCheckBox intValue]) {
    NSAlert *alert =
        [NSAlert alertWithMessageText:LFLSTR(@"Error")
                        defaultButton:LFLSTR(@"OK")
                      alternateButton:nil
                          otherButton:nil
            informativeTextWithFormat:
                LFLSTR(@"You must select at least one option from \"Import "
                       @"Last + First Name\" and \"Import Fist Name\".")];
    [alert beginSheetModalForWindow:_importOptionWindow
                      modalDelegate:self
                     didEndSelector:nil
                        contextInfo:nil];
    return;
  }

  [NSApp endSheet:[[self window] attachedSheet]];
  [[[self window] attachedSheet] orderOut:self];
  [self importAddressBook];
}
- (IBAction)cancelImportAction:(id)sender {
  [NSApp endSheet:[[self window] attachedSheet]];
  [[[self window] attachedSheet] orderOut:self];
}
- (IBAction)importAddressBook:(id)sender {
  if ([[self window] attachedSheet]) {
    [NSApp endSheet:[[self window] attachedSheet]];
    [[[self window] attachedSheet] orderOut:self];
  }
  [_importOptionWindow setDefaultButtonCell:[_continueImportButton cell]];
  [NSApp beginSheet:_importOptionWindow
      modalForWindow:[self window]
       modalDelegate:self
      didEndSelector:nil
         contextInfo:nil];
}
- (void)openPanelDidEnd:(NSOpenPanel *)panel
             returnCode:(int)returnCode
            contextInfo:(void *)contextInfo {
  if (returnCode == NSFileHandlingPanelOKButton) {
    NSString *path = [[panel filenames] objectAtIndex:0];
    [panel orderOut:self];
    if (_loader) {
      bool rtn = [_loader importUserPhraseDBFromFile:path];
      if (rtn) {
        NSAlert *alert =
            [NSAlert alertWithMessageText:LFLSTR(@"Done")
                            defaultButton:LFLSTR(@"OK")
                          alternateButton:nil
                              otherButton:nil
                informativeTextWithFormat:
                    LFLSTR(@"Your phrases are successfully imported.")];
        [alert setAlertStyle:NSInformationalAlertStyle];
        [alert beginSheetModalForWindow:[self window]
                          modalDelegate:self
                         didEndSelector:nil
                            contextInfo:nil];
      } else {
        NSAlert *alert = [NSAlert
                 alertWithMessageText:LFLSTR(@"Error")
                        defaultButton:LFLSTR(@"OK")
                      alternateButton:nil
                          otherButton:nil
            informativeTextWithFormat:LFLSTR(@"Unable to import database.")];
        [alert setAlertStyle:NSWarningAlertStyle];
        [alert beginSheetModalForWindow:[self window]
                          modalDelegate:self
                         didEndSelector:nil
                            contextInfo:nil];
      }
    }
  } else {
    // NSLog(@"Cancel");
  }
}
- (IBAction)importAction:(id)sender;
{
  if (!_loader) {
    NSAlert *alert =
        [NSAlert alertWithMessageText:LFLSTR(@"Unable to import database.")
                        defaultButton:LFLSTR(@"OK")
                      alternateButton:nil
                          otherButton:nil
            informativeTextWithFormat:
                LFLSTR(@"If you are not runnung ChiaKey, you are not "
                       @"able to import your database.")];
    [alert beginSheetModalForWindow:[self window]
                      modalDelegate:self
                     didEndSelector:nil
                        contextInfo:nil];
    return;
  }
  NSOpenPanel *panel = [NSOpenPanel openPanel];
  [panel setAllowedFileTypes:[NSArray arrayWithObjects:@"txt", nil]];
  [panel setExtensionHidden:NO];
  [panel setCanCreateDirectories:NO];
  [panel setTitle:LFLSTR(@"Import Database")];
  [panel setMessage:LFLSTR(@"Import customized phrases to your own database.")];
  [panel setPrompt:LFLSTR(@"Choose")];

  [panel beginSheetForDirectory:nil
                           file:nil
                 modalForWindow:[self window]
                  modalDelegate:self
                 didEndSelector:@selector(openPanelDidEnd:
                                               returnCode:contextInfo:)
                    contextInfo:nil];
}

- (void)savePanelDidEnd:(NSSavePanel *)panel
             returnCode:(int)returnCode
            contextInfo:(void *)contextInfo {
  if (returnCode == NSFileHandlingPanelOKButton) {
    NSString *path = [panel filename];
    [panel orderOut:self];
    if (_loader) {
      bool rtn = [_loader exportUserPhraseDBToFile:path];
      if (rtn) {
        NSAlert *alert =
            [NSAlert alertWithMessageText:LFLSTR(@"Done")
                            defaultButton:LFLSTR(@"OK")
                          alternateButton:nil
                              otherButton:nil
                informativeTextWithFormat:
                    LFLSTR(@"Your phrases are successfully exported.")];
        [alert setAlertStyle:NSInformationalAlertStyle];
        [alert beginSheetModalForWindow:[self window]
                          modalDelegate:self
                         didEndSelector:nil
                            contextInfo:nil];
      } else {
        NSAlert *alert = [NSAlert
                 alertWithMessageText:LFLSTR(@"Error")
                        defaultButton:LFLSTR(@"OK")
                      alternateButton:nil
                          otherButton:nil
            informativeTextWithFormat:LFLSTR(@"Unable to export database.")];
        [alert setAlertStyle:NSWarningAlertStyle];
        [alert beginSheetModalForWindow:[self window]
                          modalDelegate:self
                         didEndSelector:nil
                            contextInfo:nil];
      }
    }
  } else {
    // NSLog(@"Cancel");
  }
}
- (IBAction)exportAction:(id)sender {
  if (!_loader) {
    NSAlert *alert =
        [NSAlert alertWithMessageText:LFLSTR(@"Unable to export database.")
                        defaultButton:LFLSTR(@"OK")
                      alternateButton:nil
                          otherButton:nil
            informativeTextWithFormat:
                LFLSTR(@"If you are not runnung ChiaKey, you are not "
                       @"able to export your database.")];

    [alert beginSheetModalForWindow:[self window]
                      modalDelegate:self
                     didEndSelector:nil
                        contextInfo:nil];
    return;
  }
  NSSavePanel *panel = [NSSavePanel savePanel];
  [panel setAllowedFileTypes:[NSArray arrayWithObjects:@"txt", nil]];
  [panel setExtensionHidden:NO];
  [panel setCanCreateDirectories:NO];
  [panel setNameFieldLabel:LFLSTR(@"Export As:")];
  [panel setRequiredFileType:@"txt"];
  [panel setTitle:LFLSTR(@"Export Database")];
  [panel setMessage:LFLSTR(@"Exporting your own customized phrases database.")];
  [panel setPrompt:LFLSTR(@"Export")];
  [panel beginSheetForDirectory:nil
                           file:nil
                 modalForWindow:[self window]
                  modalDelegate:self
                 didEndSelector:@selector(savePanelDidEnd:
                                               returnCode:contextInfo:)
                    contextInfo:nil];
}
- (IBAction)okAction:(id)sender {
  NSString *s = [_phraseTableView currentReading];
  [_loader userPhraseDBSetNewReading:s forPhraseAtRow:_editingRow];
  [_tableView reloadData];
  [self updateStatus];

  [NSApp endSheet:_phraseWindow];
  [_phraseWindow orderOut:self];
  [NSApp abortModal];
}
- (IBAction)cancelAction:(id)sender {
  [NSApp endSheet:_phraseWindow];
  [_phraseWindow orderOut:self];
  [NSApp abortModal];
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
  if (_loader != nil) [_loader userPhraseDBSave];
}

#pragma mark NSTableView datasource methods

- (void)tableView:(NSTableView *)aTableView
    willDisplayCell:(id)aCell
     forTableColumn:(NSTableColumn *)aTableColumn
                row:(int)rowIndex {
  NSString *identifier = [aTableColumn identifier];
  if ([identifier isEqualToString:@"phrase"]) {
    NSFont *font = [NSFont fontWithName:@"LiHeiPro" size:16.0];
    [aCell setFont:font];
  } else {
    NSFont *font = [NSFont fontWithName:@"LiHeiPro" size:12.0];
    [aCell setFont:font];
  }
}
- (int)numberOfRowsInTableView:(NSTableView *)aTableView {
  return [_loader userPhraseDBNumberOfRow];
}

- (id)tableView:(NSTableView *)aTableView
    objectValueForTableColumn:(NSTableColumn *)aTableColumn
                          row:(int)rowIndex {
  NSString *identifier = [aTableColumn identifier];
  NSDictionary *dataDict = [_loader userPhraseDBDictionaryAtRow:rowIndex];

  if ([identifier isEqualToString:@"phrase"]) {
    return [dataDict objectForKey:@"Text"];
  } else if ([identifier isEqualToString:@"reading"]) {
    NSString *reading = [dataDict objectForKey:@"BPMF"];
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
    return reading;
#else
    NSArray *array = [reading componentsSeparatedByString:@","];
    NSMutableString *string = [NSMutableString string];
    NSEnumerator *enumerator = [array objectEnumerator];
    NSString *part;
    while (part = [enumerator nextObject]) [string appendFormat:@"%@ ", part];
    return string;
#endif
  }
  return nil;
}

- (void)tableView:(NSTableView *)aTableView
    setObjectValue:(id)anObject
    forTableColumn:(NSTableColumn *)aTableColumn
               row:(int)rowIndex {
  NSString *identifier = [aTableColumn identifier];

  if ([identifier isEqualToString:@"phrase"]) {
    NSString *string = [self validatedString:(NSString *)anObject];
    if (![string length]) return;

    if ([string length] > 7) string = [string substringToIndex:7];

    NSDictionary *dataDict = [_loader userPhraseDBDictionaryAtRow:rowIndex];
    if (![[dataDict objectForKey:@"Text"] isEqualToString:string]) {
      [_loader userPhraseDBSetPhrase:string atRow:rowIndex];
      [aTableView reloadData];
      [self editReading:self];
      [self updateStatus];
    }
  }
}

- (BOOL)tableView:(NSTableView *)tv
    writeRowsWithIndexes:(NSIndexSet *)rowIndexes
            toPasteboard:(NSPasteboard *)pboard {
  [pboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:self];
  [pboard setString:[self stringForCopying] forType:NSStringPboardType];
  return YES;
}
- (NSDragOperation)tableView:(NSTableView *)tv
                validateDrop:(id<NSDraggingInfo>)info
                 proposedRow:(int)row
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
                                   NSToolbarSeparatorItemIdentifier,
                                   importAddressBookToolbarItemIdentifier, nil];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
  return [NSArray arrayWithObjects:addToolbarItemIdentifier,
                                   deleteToolbarItemIdentifier,
                                   editPhraseToolbarItemIdentifier,
                                   editReaingToolbarItemIdentifier,
                                   NSToolbarSeparatorItemIdentifier,
                                   importAddressBookToolbarItemIdentifier,
                                   reloadToolbarItemIdentifier, nil];
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
  } else {
    item = nil;
  }
  return item;
}

@end
