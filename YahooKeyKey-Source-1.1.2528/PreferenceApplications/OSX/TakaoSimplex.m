/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoSimplex.h"

#import "TakaoHelper.h"

@implementation TakaoSimplex

- (void)dealloc {
  [_preferenceFilePath release];
  [_simplexDictionary release];
  [super dealloc];
}
- (void)setUI {
  if ([[_simplexDictionary valueForKey:@"ClearReadingBufferAtCompositionError"]
          isEqualToString:@"true"])
    [_clearReadingBufferAtCompositionErrorCheckBox setIntValue:1];
  else
    [_clearReadingBufferAtCompositionErrorCheckBox setIntValue:0];

  if ([[_simplexDictionary valueForKey:@"ComposeWhileTyping"]
          isEqualToString:@"true"])
    [_composeWhenTypingCheckBox setIntValue:1];
  else
    [_composeWhenTypingCheckBox setIntValue:0];

  if ([[_simplexDictionary valueForKey:@"UseCharactersSupportedByEncoding"]
          isEqualToString:@""])
    [_useCharactersSupportedByEncodingCheckBox setIntValue:1];
  else
    [_useCharactersSupportedByEncodingCheckBox setIntValue:0];
}
- (void)awakeFromNib {
  _simplexDictionary = [NSMutableDictionary new];
  [_simplexDictionary setValue:@"false"
                        forKey:@"ClearReadingBufferAtCompositionError"];
  [_simplexDictionary setValue:@"false" forKey:@"ComposeWhileTyping"];
  [_simplexDictionary setValue:@"false" forKey:@"ShouldUseAllUnicodePlanes"];
  LFRetainAssign(_preferenceFilePath,
                 [TakaoHelper plistFilePath:PLIST_SIMPLEX_FILENAME]);

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
      [_simplexDictionary addEntriesFromDictionary:dictionary];
    }
  }
  [self setUI];
  [self writePreference:self];
}

- (void)updateDictionary {
  if ([_clearReadingBufferAtCompositionErrorCheckBox intValue])
    [_simplexDictionary setValue:@"true"
                          forKey:@"ClearReadingBufferAtCompositionError"];
  else
    [_simplexDictionary setValue:@"false"
                          forKey:@"ClearReadingBufferAtCompositionError"];

  if ([_composeWhenTypingCheckBox intValue])
    [_simplexDictionary setValue:@"true" forKey:@"ComposeWhileTyping"];
  else
    [_simplexDictionary setValue:@"false" forKey:@"ComposeWhileTyping"];

  if ([_useCharactersSupportedByEncodingCheckBox intValue])
    [_simplexDictionary setValue:@""
                          forKey:@"UseCharactersSupportedByEncoding"];
  else
    [_simplexDictionary setValue:@"BIG-5"
                          forKey:@"UseCharactersSupportedByEncoding"];
}

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
      dataWithPropertyList:_simplexDictionary
                    format:NSPropertyListXMLFormat_v1_0
                   options:0
                     error:nil];

  if (data) {
    [data writeToFile:_preferenceFilePath atomically:YES];
  }
}

@end
