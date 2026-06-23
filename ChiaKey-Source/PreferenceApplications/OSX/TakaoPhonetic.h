/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "TakaoSettings.h"

/*
        @header TakaoPhonetic
*/

/*!
        @class TakaoPhonetic
        @abstract The controller to interact with the user interface
        and the settings of The Phonetic Input Method.
*/
@interface TakaoPhonetic : NSObject {
  IBOutlet id _keyboardLayoutPopUpButton;
  IBOutlet id _useCharactersSupportedByEncodingCheckBox;

  NSMutableDictionary *_phoneticDictionary;
  NSString *_preferenceFilePath;
}

/*!
        @method writePreference:
        @abstract Writes the new setting into the plist file wile user
        click on any button or check box.
        @param sender The sender object.
*/
- (IBAction)writePreference:(id)sender;

@end
