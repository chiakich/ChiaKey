/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "TakaoSettings.h"

/*!
        @header TakaoCangjie
*/

/*!
        @class TakaoCangjie
        @abstract The Controller to interact with the user
        interface and the settings of The Cangjie Input Method.
*/

@interface TakaoCangjie : NSObject {
  IBOutlet id _useDynamicFrequencyCheckBox;
  IBOutlet id _shouldCommitAtMaximumRadicalLengthCheckBox;
  IBOutlet id _clearReadingBufferAtCompositionErrorCheckBox;
  IBOutlet id _composeWhenTypingCheckBox;
  IBOutlet id _useCharactersSupportedByEncodingCheckBox;
  IBOutlet id _useOverrideTablePopUpButton;

  NSMutableDictionary *_cangjieDictionary;
  NSString *_preferenceFilePath;
}
/*!
        @method writePreference:
        @abstract Write the new setting into the plist file while user
        click on any button or check box.
        @param sender The sender object.
*/
- (IBAction)writePreference:(id)sender;
/*!
        @method setClearOnError:
        @abstract Responding on checking the "SetClearOnError" option.
        @discussion ClearOnError and AutoCompose should not be set in
        the same time, so, if one option is checked, another one must
        be unchecked.
        @param sender The sender object.
*/
- (IBAction)setClearOnError:(id)sender;
/*!
        @method setAutoCompose:
        @abstract Responding on checking the "setAutoCompose" option.
        @discussion ClearOnError and AutoCompose should not be set in
        the same time, so, if one option is checked, another one must
        be unchecked.
        @param sender The sender object.
*/
- (IBAction)setAutoCompose:(id)sender;
@end
