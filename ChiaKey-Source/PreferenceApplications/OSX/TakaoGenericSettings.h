/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "TakaoSettings.h"

/*!
        @header TakaoGenericSettings
*/

/*!
        @class TakaoGenericSettings
        @abstract The controller to interact the user interface and the settings
   of each Generic Input Methods.
*/

@interface TakaoGenericSettings : NSObject {
  NSMutableDictionary *_genericDictionary;
  NSString *_preferenceFilePath;
  NSString *_moduleIdentifier;
  NSString *_localizedName;
  IBOutlet id view;
  IBOutlet id _titleTextField;
  IBOutlet id _maximumRadicalLengthTextField;
  IBOutlet id _maximumRadicalLengthStepper;
  IBOutlet id _shouldCommitAtMaximumRadicalLengthCheckBox;
  IBOutlet id _clearReadingBufferAtCompositionErrorCheckBox;
  IBOutlet id _useDynamicFrequencyCheckBox;
  IBOutlet id _matchOneCharTextField;
  IBOutlet id _matchZeroOrMoreCharTextField;
  IBOutlet id _composeWhileTypingCheckBox;
  IBOutlet id _useSpaceAsFirstCandidateSelectionKeyCheckBox;
}
/*!
        @method view
        @abstract Return the NSView object of the settings of current
        Generic Input Method module.
*/
- (NSView *)view;
/*!
        @method loadSettingsWithName:localizedName:
        @abstract Sets the name and localized name of the TakaoGenericSettings
        object and start to load settings from the specified plist file.
        @param name The identifier name of a Generic Input Method module.
        @param localizedName The localized name.
*/
- (void)loadSettingsWithName:(NSString *)name
               localizedName:(NSString *)localizedName;

#pragma mark Interface Builder actions

/*!
        @method changeMaximumRadicalLength:
        @abstract Responding on the setting of the Maximum Radical Length
        is changed.
        @param sender The sender object.
*/
- (IBAction)changeMaximumRadicalLength:(id)sender;
/*!
        @method changeMatchOneChar:
        @abstract Responding on the settting of Match One Char is changed.
        @param sender The sender object.
*/
- (IBAction)changeMatchOneChar:(id)sender;
/*!
        @method changeMatchZeroOrMoreChar:
        @abstract Responding on the settting of Match Zero or More Char
        is changed.
        @param sender The sender object.
*/
- (IBAction)changeMatchZeroOrMoreChar:(id)sender;
/*!
        @method writePreference:
        @abstract Writes the settings into the plist file.
        @param sender The sender object.
*/
- (IBAction)writePreference:(id)sender;
@end
