/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "TakaoGenericSettings.h"

/*!
        @header TakaoGeneric
*/

/*!
        @class TakaoGeneric
        @abstract The controller for the table input method pane: the list of
        imported CIN tables, their per-table settings, and importing and
        removing them.
        @discussion The so-called Generic Input Method modules are based on
        UTF-8 encoded plain-text files, which contain the data of
        input key sequences and output characters. The list shown here is
        read from the user's table folder rather than from the running IME,
        so an imported table shows up immediately whether or not the IME is
        running to pick it up. Tables bundled with the app are deliberately
        absent: they live inside the app bundle, cannot be removed, and are
        hidden from the input menu through the General pane instead.
*/

@interface TakaoGeneric : NSObject <NSTableViewDataSource, NSTableViewDelegate> {
  IBOutlet NSTableView *_genericModuleListTableView;
  IBOutlet NSView *_genericSettingView;

  NSMutableArray *_modules;
  NSButton *_importButton;
  NSButton *_removeButton;
  NSTextField *_emptyStateTextField;
}

/*!
        @method reloadTables
        @abstract Re-reads the user's table folder and rebuilds the list.
*/
- (void)reloadTables;

/*!
        @method reloadTablesSelectingFileName:
        @abstract Re-reads the user's table folder and selects a given table.
        @param fileNameToSelect File name to select afterwards, or nil for the
        first row.
*/
- (void)reloadTablesSelectingFileName:(NSString *)fileNameToSelect;

/*!
        @method importTable:
        @abstract Asks for a CIN file, validates it, and installs it into the
        user's table folder.
        @param sender The sender object.
*/
- (IBAction)importTable:(id)sender;

/*!
        @method removeSelectedTable:
        @abstract Removes the selected imported table after confirmation.
        @param sender The sender object.
*/
- (IBAction)removeSelectedTable:(id)sender;

@end
