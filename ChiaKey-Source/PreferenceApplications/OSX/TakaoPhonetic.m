/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoPhonetic.h"

#import "TakaoHelper.h"
#import "TakaoKeyboardLayoutPopUpButton.h"

@implementation TakaoPhonetic

- (void)dealloc {
  [_preferenceFilePath release];
  [_phoneticDictionary release];
  [super dealloc];
}
- (void)setUI {
  if (!_phoneticDictionary) return;

  [_keyboardLayoutPopUpButton
      selectLayoutIdentifier:[_phoneticDictionary
                                 valueForKey:@"KeyboardLayout"]];

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

  [_phoneticDictionary
      setValue:[_keyboardLayoutPopUpButton selectedLayoutIdentifier]
        forKey:@"KeyboardLayout"];

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
