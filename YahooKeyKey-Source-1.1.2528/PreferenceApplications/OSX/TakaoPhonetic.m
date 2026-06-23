/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoPhonetic.h"

#import "TakaoHelper.h"

@implementation TakaoPhonetic

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

  NSString *useCharactersSupportedByEncoding =
      [_phoneticDictionary valueForKey:@"UseCharactersSupportedByEncoding"];
  if ([useCharactersSupportedByEncoding isEqualToString:@""])
    [_useCharactersSupportedByEncodingCheckBox setIntValue:1];
  else
    [_useCharactersSupportedByEncodingCheckBox setIntValue:0];
}

- (void)awakeFromNib {
  _phoneticDictionary = [NSMutableDictionary new];
  [_phoneticDictionary setValue:@"Standard" forKey:@"KeyboardLayout"];
  [_phoneticDictionary setValue:@"false"
                         forKey:@"UseCharactersSupportedByEncoding"];

  LFRetainAssign(_preferenceFilePath,
                 [TakaoHelper plistFilePath:PLIST_PHONETIC_FILENAME]);

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
  if (!_phoneticDictionary) {
    _phoneticDictionary = [[NSMutableDictionary alloc] init];
  }

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

  if ([_useCharactersSupportedByEncodingCheckBox intValue])
    [_phoneticDictionary setValue:@""
                           forKey:@"UseCharactersSupportedByEncoding"];
  else
    [_phoneticDictionary setValue:@"BIG-5"
                           forKey:@"UseCharactersSupportedByEncoding"];
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
