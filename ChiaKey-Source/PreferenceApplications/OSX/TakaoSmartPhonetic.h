/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "TakaoSettings.h"

/*!
        @header TakaoSmartPhonetic
*/

/*!
        @class TakaoSmartPhonetic
        @abstract The controller to interact with the user interface and the
        settings of The Smart Phonetic Input Method.
*/

@interface TakaoSmartPhonetic : NSObject {
  IBOutlet id _keyboardLayoutPopUpButton;
  IBOutlet id _useCharactersSupportedByEncodingCheckBox;
  IBOutlet id _clearComposingTextWithEscCheckBox;
  IBOutlet id _showCandidateListWithSpaceCheckBox;
  IBOutlet id _candidateCursorAtEndOfTargetBlockMatrix;
  IBOutlet id _selectionKeyComboBox;
  IBOutlet id _composingTextBufferSizeSlider;

  NSMutableDictionary *_phoneticDictionary;
  NSString *_preferenceFilePath;
}
/*!
        @method setSelectionKey:
        @abstract Responding on the candidate selection keys are changed.
        @param sender The sender object.
*/
- (IBAction)setSelectionKey:(id)sender;
/*!
        @method writePreference:
        @abstract Writes the new setting into the plist file while user
        click on any button or check box.
        @param sender The sender object.
*/
- (IBAction)writePreference:(id)sender;
@end
