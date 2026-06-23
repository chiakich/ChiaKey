//
//  PEController.h
//  PhraseEditor
//
//  Developed by Lithoglyph Inc on 2008/10/19.
//  Copyright 2008 Yahoo! Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import "LFUtilities.h"
#import "PESettingTableView.h"

#define HELP_URL @"http://tw.widget.yahoo.com/keykey/help/phraseeditor.html"

#ifndef OPENVANILLA_DO_CONNECTION_NAME
#error Must define OPENVANILLA_DO_CONNECTION_NAME
#endif

static NSString *addToolbarItemIdentifier = @"Add";
static NSString *deleteToolbarItemIdentifier = @"Delete";
static NSString *editPhraseToolbarItemIdentifier = @"Edit Phrase";
static NSString *editReaingToolbarItemIdentifier = @"Edit Reading";
static NSString *importAddressBookToolbarItemIdentifier =
    @"Import Address Book";
static NSString *reloadToolbarItemIdentifier = @"Reload";

@protocol TakaoPhraseEditorService
- (BOOL)userPhraseDBCanProvideService;
- (int)userPhraseDBNumberOfRow;
- (NSDictionary *)userPhraseDBDictionaryAtRow:(int)row;
- (NSArray *)userPhraseDBReadingsForPhrase:(NSString *)phrase;
- (void)userPhraseDBSave;
- (void)userPhraseDBSetNewReading:(NSString *)reading forPhraseAtRow:(int)row;
- (void)userPhraseDBDeleteRow:(int)row;
- (void)userPhraseDBAddNewRow:(NSString *)phrase;
- (void)userPhraseDBAddNewRows:(NSArray *)array;
- (void)userPhraseDBSetPhrase:(NSString *)phrase atRow:(int)row;

- (bool)exportUserPhraseDBToFile:(NSString *)path;
- (bool)importUserPhraseDBFromFile:(NSString *)path;
@end

#pragma mark -

@interface PEController : NSWindowController {
  id _loader;
  IBOutlet NSTableView *_tableView;
  IBOutlet NSTextField *_statusTextField;

  IBOutlet NSWindow *_phraseWindow;
  IBOutlet PESettingTableView *_phraseTableView;
  IBOutlet NSButton *_okButton;

  IBOutlet NSWindow *_importOptionWindow;
  IBOutlet NSButton *_continueImportButton;
  IBOutlet NSButton *_importLastAndFirstNameCheckBox;
  IBOutlet NSButton *_importFirstNameCheckBox;
  IBOutlet NSButton *_importAlreadyExistCheckBox;

  IBOutlet NSWindow *_progressWindow;
  IBOutlet NSProgressIndicator *_progressIndicator;
  IBOutlet NSTextField *_progressTextField;

  int _editingRow;
}

- (void)updateStatus;
- (NSString *)validatedString:(NSString *)originalString;

#pragma mark Interface Builder actions

- (IBAction)add:(id)sender;
- (IBAction)delete:(id)sender;
- (IBAction)copy:(id)sender;
- (IBAction)editPhrase:(id)sender;
- (IBAction)editReading:(id)sender;
- (IBAction)importAddressBook:(id)sender;
- (IBAction)continueImportAction:(id)sender;
- (IBAction)cancelImportAction:(id)sender;
- (IBAction)importAction:(id)sender;
- (IBAction)exportAction:(id)sender;

- (IBAction)okAction:(id)sender;
- (IBAction)cancelAction:(id)sender;

- (IBAction)launchOnlineHelp:(id)sender;

@end
