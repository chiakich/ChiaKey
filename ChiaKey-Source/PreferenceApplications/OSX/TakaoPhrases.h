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

/*!
        @class TakaoPhrases
        @abstract The class to interact with the interface of exporting and
   importing customzied user phrase database.
*/

@interface TakaoPhrases : NSObject {
  IBOutlet id window;
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
- (IBAction)launchEditor:(id)sender;
@end
