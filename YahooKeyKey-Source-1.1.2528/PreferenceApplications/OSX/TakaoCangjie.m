/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoCangjie.h"

#import "TakaoHelper.h"

@implementation TakaoCangjie

- (void)dealloc {
  [_preferenceFilePath release];
  [_cangjieDictionary release];
  [super dealloc];
}

// Initialize the status of the user interface.
- (void)setUI {
  if ([[_cangjieDictionary valueForKey:@"ShouldCommitAtMaximumRadicalLength"]
          isEqualToString:@"true"])
    [_shouldCommitAtMaximumRadicalLengthCheckBox setIntValue:1];
  else
    [_shouldCommitAtMaximumRadicalLengthCheckBox setIntValue:0];

  if ([[_cangjieDictionary valueForKey:@"UseDynamicFrequency"]
          isEqualToString:@"true"])
    [_useDynamicFrequencyCheckBox setIntValue:1];
  else
    [_useDynamicFrequencyCheckBox setIntValue:0];

  if ([[_cangjieDictionary valueForKey:@"ClearReadingBufferAtCompositionError"]
          isEqualToString:@"true"])
    [_clearReadingBufferAtCompositionErrorCheckBox setIntValue:1];
  else
    [_clearReadingBufferAtCompositionErrorCheckBox setIntValue:0];

  if ([[_cangjieDictionary valueForKey:@"ComposeWhileTyping"]
          isEqualToString:@"true"])
    [_composeWhenTypingCheckBox setIntValue:1];
  else
    [_composeWhenTypingCheckBox setIntValue:0];

  if ([[_cangjieDictionary valueForKey:@"UseCharactersSupportedByEncoding"]
          isEqualToString:@""])
    [_useCharactersSupportedByEncodingCheckBox setIntValue:1];
  else
    [_useCharactersSupportedByEncodingCheckBox setIntValue:0];

  if ([[_cangjieDictionary valueForKey:@"UseOverrideTable"]
          isEqualToString:@""])
    [_useOverrideTablePopUpButton selectItemAtIndex:0];
  else if ([[_cangjieDictionary valueForKey:@"UseOverrideTable"]
               isEqualToString:@"Punctuations-cj-mixedwidth-cin"])
    [_useOverrideTablePopUpButton selectItemAtIndex:1];
  else if ([[_cangjieDictionary valueForKey:@"UseOverrideTable"]
               isEqualToString:@"Punctuations-cj-halfwidth-cin"])
    [_useOverrideTablePopUpButton selectItemAtIndex:2];
}

- (void)awakeFromNib {
  _cangjieDictionary = [NSMutableDictionary new];
  [_cangjieDictionary setValue:@"true" forKey:@"UseDynamicFrequency"];
  [_cangjieDictionary setValue:@"true"
                        forKey:@"ClearReadingBufferAtCompositionError"];
  [_cangjieDictionary setValue:@"false" forKey:@"ComposeWhileTyping"];
  [_cangjieDictionary setValue:@"false"
                        forKey:@"ShouldCommitAtMaximumRadicalLength"];
  [_cangjieDictionary setValue:@"" forKey:@"UseCharactersSupportedByEncoding"];
  [_cangjieDictionary setValue:@"Punctuations-punctuations-cin"
                        forKey:@"UseOverrideTable"];

  LFRetainAssign(_preferenceFilePath,
                 [TakaoHelper plistFilePath:PLIST_CANGJIE_FILENAME]);

  NSData *data = [NSData dataWithContentsOfFile:_preferenceFilePath
                                        options:0
                                          error:nil];
  if (data) {
    NSPropertyListFormat format;

    NSMutableDictionary *dictionary = [NSPropertyListSerialization
        propertyListWithData:data
                      options:0
                       format:&format
                        error:nil];
    if (dictionary) {
      [_cangjieDictionary addEntriesFromDictionary:dictionary];
    }
  }  // end data
  [self setUI];
  [self writePreference:self];
}

- (void)updateDictionary {
  if ([_shouldCommitAtMaximumRadicalLengthCheckBox intValue])
    [_cangjieDictionary setValue:@"true"
                          forKey:@"ShouldCommitAtMaximumRadicalLength"];
  else
    [_cangjieDictionary setValue:@"false"
                          forKey:@"ShouldCommitAtMaximumRadicalLength"];

  if ([_useDynamicFrequencyCheckBox intValue])
    [_cangjieDictionary setValue:@"true" forKey:@"UseDynamicFrequency"];
  else
    [_cangjieDictionary setValue:@"false" forKey:@"UseDynamicFrequency"];

  if ([_clearReadingBufferAtCompositionErrorCheckBox intValue])
    [_cangjieDictionary setValue:@"true"
                          forKey:@"ClearReadingBufferAtCompositionError"];
  else
    [_cangjieDictionary setValue:@"false"
                          forKey:@"ClearReadingBufferAtCompositionError"];

  if ([_composeWhenTypingCheckBox intValue])
    [_cangjieDictionary setValue:@"true" forKey:@"ComposeWhileTyping"];
  else
    [_cangjieDictionary setValue:@"false" forKey:@"ComposeWhileTyping"];

  if ([_useCharactersSupportedByEncodingCheckBox intValue])
    [_cangjieDictionary setValue:@""
                          forKey:@"UseCharactersSupportedByEncoding"];
  else
    [_cangjieDictionary setValue:@"BIG-5"
                          forKey:@"UseCharactersSupportedByEncoding"];

  switch ([_useOverrideTablePopUpButton indexOfSelectedItem]) {
    case 1:
      [_cangjieDictionary setValue:@"Punctuations-cj-mixedwidth-cin"
                            forKey:@"UseOverrideTable"];
      break;
    case 2:
      [_cangjieDictionary setValue:@"Punctuations-cj-halfwidth-cin"
                            forKey:@"UseOverrideTable"];
      break;
    default:
      [_cangjieDictionary setValue:@"" forKey:@"UseOverrideTable"];
      break;
  }
}

#pragma mark Interface Builder actions

- (IBAction)setClearOnError:(id)sender {
  if ([_clearReadingBufferAtCompositionErrorCheckBox intValue]) {
    [_composeWhenTypingCheckBox setIntValue:0];
  }
  [self writePreference:sender];
}
- (IBAction)setAutoCompose:(id)sender {
  if ([_composeWhenTypingCheckBox intValue]) {
    [_clearReadingBufferAtCompositionErrorCheckBox setIntValue:0];
  }
  [self writePreference:sender];
}
- (IBAction)writePreference:(id)sender {
  [self updateDictionary];
  NSData *data = [NSPropertyListSerialization
      dataWithPropertyList:_cangjieDictionary
                    format:NSPropertyListXMLFormat_v1_0
                   options:0
                     error:nil];

  if (data) {
    [data writeToFile:_preferenceFilePath atomically:YES];
  }
}

@end
