/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoSmartPhonetic.h"

#import "TakaoHelper.h"

@implementation TakaoSmartPhonetic

- (void)dealloc {
  [_preferenceFilePath release];
  [_phoneticDictionary release];
  [super dealloc];
}
- (void)setUI {
  if (!_phoneticDictionary) return;

  NSString *keyboardLayout =
      [_phoneticDictionary valueForKey:@"KeyboardLayout"];

  if ([keyboardLayout isEqualToString:@"ETen"]) {
    [_keyboardLayoutPopUpButton selectItemAtIndex:1];
  } else if ([keyboardLayout isEqualToString:@"Hanyu Pinyin"]) {
    [_keyboardLayoutPopUpButton selectItemAtIndex:2];
  } else if (
      [keyboardLayout isEqualToString:@"ETen26"] ||
      [keyboardLayout
          isEqualToString:@"bpmfdtnlvkhgvcgycjqwsexuaorwiqzpmntlhfjkd"] ||
      [keyboardLayout isEqualToString:@"Hsu"] ||
      [keyboardLayout
          isEqualToString:@"bpmfdtnlgkhjvcjvcrzasexuyhgeiawomnklldfjs"]) {
    [_keyboardLayoutPopUpButton selectItemAtIndex:3];
  } else {
    [_keyboardLayoutPopUpButton selectItemAtIndex:0];
  }

  NSString *clearComposingTextWithEsc =
      [_phoneticDictionary valueForKey:@"ClearComposingTextWithEsc"];
  if ([clearComposingTextWithEsc isEqualToString:@"true"])
    [_clearComposingTextWithEscCheckBox setIntValue:1];
  else
    [_clearComposingTextWithEscCheckBox setIntValue:0];

  NSString *showCandidateListWithSpace =
      [_phoneticDictionary valueForKey:@"ShowCandidateListWithSpace"];
  if ([showCandidateListWithSpace isEqualToString:@"true"])
    [_showCandidateListWithSpaceCheckBox setIntValue:1];
  else
    [_showCandidateListWithSpaceCheckBox setIntValue:0];

  NSString *candidateCursorAtEndOfTargetBlock =
      [_phoneticDictionary valueForKey:@"CandidateCursorAtEndOfTargetBlock"];
  if ([candidateCursorAtEndOfTargetBlock isEqualToString:@"true"])
    [_candidateCursorAtEndOfTargetBlockMatrix selectCellAtRow:1 column:0];
  else
    [_candidateCursorAtEndOfTargetBlockMatrix selectCellAtRow:0 column:0];

  NSString *useCharactersSupportedByEncoding =
      [_phoneticDictionary valueForKey:@"UseCharactersSupportedByEncoding"];
  if ([useCharactersSupportedByEncoding isEqualToString:@""])
    [_useCharactersSupportedByEncodingCheckBox setIntValue:1];
  else
    [_useCharactersSupportedByEncodingCheckBox setIntValue:0];

  NSString *selectionKeys =
      [_phoneticDictionary valueForKey:@"CandidateSelectionKeys"];
  [_selectionKeyComboBox setStringValue:selectionKeys];

  NSString *composingTextBufferSize =
      [_phoneticDictionary valueForKey:@"ComposingTextBufferSize"];
  int bufferSize = [composingTextBufferSize intValue];
  [_composingTextBufferSizeSlider setIntValue:bufferSize];
}
- (void)awakeFromNib {
  _phoneticDictionary = [NSMutableDictionary new];
  [_phoneticDictionary setValue:@"Standard" forKey:@"KeyboardLayout"];
  [_phoneticDictionary setValue:@"" forKey:@"UseCharactersSupportedByEncoding"];
  [_phoneticDictionary setValue:@"false" forKey:@"ClearComposingTextWithEsc"];
  [_phoneticDictionary setValue:@"true" forKey:@"ShowCandidateListWithSpace"];
  [_phoneticDictionary setValue:@"false"
                         forKey:@"CandidateCursorAtEndOfTargetBlock"];
  [_phoneticDictionary setValue:@"12345678" forKey:@"CandidateSelectionKeys"];
  [_phoneticDictionary setValue:@"10" forKey:@"ComposingTextBufferSize"];
  LFRetainAssign(_preferenceFilePath,
                 [TakaoHelper plistFilePath:PLIST_SMARTPHONETIC_FILENAME]);

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
    if (dictionary) [_phoneticDictionary addEntriesFromDictionary:dictionary];
  }  // end data
  [self setUI];
  [self writePreference:self];
}
- (void)updateDictionary {
  if (!_phoneticDictionary) _phoneticDictionary = [NSMutableDictionary new];

  switch ([_keyboardLayoutPopUpButton indexOfSelectedItem]) {
    case 1:
      [_phoneticDictionary setValue:@"ETen" forKey:@"KeyboardLayout"];
      break;
    case 2:
      [_phoneticDictionary setValue:@"Hanyu Pinyin" forKey:@"KeyboardLayout"];
      break;
    case 3:
      if ([[_keyboardLayoutPopUpButton itemArray] count] > 4)
        [_phoneticDictionary setValue:@"ETen26" forKey:@"KeyboardLayout"];
      break;
    case 4:
      if ([[_keyboardLayoutPopUpButton itemArray] count] > 4)
        [_phoneticDictionary setValue:@"Hsu" forKey:@"KeyboardLayout"];
      break;
    default:
      [_phoneticDictionary setValue:@"Standard" forKey:@"KeyboardLayout"];
      break;
  }

  if ([_clearComposingTextWithEscCheckBox intValue])
    [_phoneticDictionary setValue:@"true" forKey:@"ClearComposingTextWithEsc"];
  else
    [_phoneticDictionary setValue:@"false" forKey:@"ClearComposingTextWithEsc"];

  if ([_showCandidateListWithSpaceCheckBox intValue])
    [_phoneticDictionary setValue:@"true" forKey:@"ShowCandidateListWithSpace"];
  else
    [_phoneticDictionary setValue:@"false"
                           forKey:@"ShowCandidateListWithSpace"];

  if ([[_candidateCursorAtEndOfTargetBlockMatrix selectedCell] tag])
    [_phoneticDictionary setValue:@"true"
                           forKey:@"CandidateCursorAtEndOfTargetBlock"];
  else
    [_phoneticDictionary setValue:@"false"
                           forKey:@"CandidateCursorAtEndOfTargetBlock"];

  if ([_useCharactersSupportedByEncodingCheckBox intValue])
    [_phoneticDictionary setValue:@""
                           forKey:@"UseCharactersSupportedByEncoding"];
  else
    [_phoneticDictionary setValue:@"BIG-5"
                           forKey:@"UseCharactersSupportedByEncoding"];

  NSString *selecitonKeys = [_selectionKeyComboBox stringValue];
  [_phoneticDictionary setValue:selecitonKeys forKey:@"CandidateSelectionKeys"];

  int bufferSize = [_composingTextBufferSizeSlider intValue];
  NSString *bufferSizeString = [NSString stringWithFormat:@"%d", bufferSize];
  [_phoneticDictionary setValue:bufferSizeString
                         forKey:@"ComposingTextBufferSize"];
}
- (BOOL)validateSelectionKeys:(NSString *)selectionKeys {
  int i = 0;
  for (i = 0; i < [selectionKeys length] - 1; i++) {
    char currKey = [selectionKeys characterAtIndex:i];
    int j = i + 1;
    for (j = i + 1; i < [selectionKeys length]; i++) {
      char checkKey = [selectionKeys characterAtIndex:j];
      if (currKey == checkKey) return NO;
    }
  }
  return YES;
}

#pragma mark Interface Builder actions

- (IBAction)setSelectionKey:(id)sender {
  NSString *selectionKeys = [_selectionKeyComboBox stringValue];
  if (![self validateSelectionKeys:selectionKeys]) selectionKeys = @"12345678";
  if ([selectionKeys length] > 8)
    selectionKeys = [selectionKeys substringToIndex:8];
  if ([selectionKeys length] < 8) selectionKeys = @"12345678";

  [_selectionKeyComboBox setStringValue:selectionKeys];
  [self writePreference:sender];
}
- (IBAction)writePreference:(id)sender {
  [self updateDictionary];
  NSData *data = [NSPropertyListSerialization
      dataWithPropertyList:_phoneticDictionary
                    format:NSPropertyListXMLFormat_v1_0
                   options:0
                     error:nil];

  if (data) {
    [data writeToFile:_preferenceFilePath atomically:YES];
  }
}
@end
