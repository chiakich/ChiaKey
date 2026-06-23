/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "TakaoSettings.h"

/*!
        @header TakaoPhrases
*/

enum transaction_type { tDownload = -1, tUpload = 1 };

/*!
        @class TakaoPhrases
        @abstract The class to interact with the interface of exporting and
   importing customzied user phrase database.
*/

@interface TakaoPhrases : NSObject {
  IBOutlet id window;
  IBOutlet id _exportDotMacButton;
  IBOutlet id _importDotMacButton;
  IBOutlet id _progressWindow;
  IBOutlet id _progressIndicator;
  NSTimer *_transactionTimer;
  int _transactionType;
}

/*!
        @method exportDatabase:
        @abstract Launch the save dialog and export the customized user phrase
   database into a plain-text file.
        @param sender The sender object.
*/
- (IBAction)exportDatabase:(id)sender;
/*!
        @method importDatabase:
        @abstract Launch the open dialog and import the customized user phrase
   database from a plain-text file.
        @param sender The sender object.
*/
- (IBAction)importDatabase:(id)sender;
/*!
        @method exportDatabaseToDotMac:
        @abstract Exports the user phrase database to a text file and then
   uploads it to the iDisk service of MobileMe
        @param sender The sender object.
*/
- (IBAction)exportDatabaseToDotMac:(id)sender;
/*!
        @method importDatabaseFromDotMac:
        @abstract Downloads user phrase database as a text file from the iDisk
   service of MobileMe and then performs import.
        @param sender The sender object.
*/
- (IBAction)importDatabaseFromDotMac:(id)sender;
- (IBAction)launchEditor:(id)sender;
@end
