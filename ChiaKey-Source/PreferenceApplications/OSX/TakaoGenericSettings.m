/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoGenericSettings.h"

#import "TakaoHelper.h"

@implementation TakaoGenericSettings

- (void)dealloc {
  [_genericDictionary release];
  [_preferenceFilePath release];
  [_moduleIdentifier release];
  [_localizedName release];
  [super dealloc];
}

- (id)init {
  if (self = [super init]) {
    BOOL loaded = [[NSBundle mainBundle] loadNibNamed:@"TakaoGenericSettings" owner:self topLevelObjects:nil];
    NSAssert((loaded == YES), @"NIB did not load");
  }
  return self;
}

- (void)setUI {
  if (!_genericDictionary) return;

  [_titleTextField
      setStringValue:[NSString stringWithFormat:LFLSTR(@"Settings for '%@'"),
                                                _localizedName]];

  NSNumber *max = [NSNumber
      numberWithInt:[[_genericDictionary valueForKey:@"MaximumRadicalLength"]
                        intValue]];
  if ([max intValue] < 1)
    max = [NSNumber numberWithInt:1];
  else if ([max intValue] > 128)
    max = [NSNumber numberWithInt:128];

  [_maximumRadicalLengthTextField setStringValue:[max stringValue]];
  [_maximumRadicalLengthStepper setIntValue:[max intValue]];

  if ([[_genericDictionary valueForKey:@"ShouldCommitAtMaximumRadicalLength"]
          isEqualToString:@"true"])
    [_shouldCommitAtMaximumRadicalLengthCheckBox setIntValue:1];
  else
    [_shouldCommitAtMaximumRadicalLengthCheckBox setIntValue:0];

  if ([[_genericDictionary valueForKey:@"ClearReadingBufferAtCompositionError"]
          isEqualToString:@"true"])
    [_clearReadingBufferAtCompositionErrorCheckBox setIntValue:1];
  else
    [_clearReadingBufferAtCompositionErrorCheckBox setIntValue:0];

  if ([[_genericDictionary valueForKey:@"UseDynamicFrequency"]
          isEqualToString:@"true"])
    [_useDynamicFrequencyCheckBox setIntValue:1];
  else
    [_useDynamicFrequencyCheckBox setIntValue:0];

  if ([[_genericDictionary valueForKey:@"ComposeWhileTyping"]
          isEqualToString:@"true"])
    [_composeWhileTypingCheckBox setIntValue:1];
  else
    [_composeWhileTypingCheckBox setIntValue:0];

  if ([[_genericDictionary valueForKey:@"UseSpaceAsFirstCandidateSelectionKey"]
          isEqualToString:@"true"])
    [_useSpaceAsFirstCandidateSelectionKeyCheckBox setIntValue:1];
  else
    [_useSpaceAsFirstCandidateSelectionKeyCheckBox setIntValue:0];

  NSString *matchOneChar = [_genericDictionary valueForKey:@"MatchOneChar"];
  if ([matchOneChar length] > 1) {
    unichar theChar;
    theChar = [matchOneChar characterAtIndex:0];
    matchOneChar = [NSString stringWithCharacters:&theChar length:1];
  }
  [_matchOneCharTextField setStringValue:matchOneChar];

  NSString *matchZeroOrMoreChar =
      [_genericDictionary valueForKey:@"MatchZeroOrMoreChar"];
  if ([matchZeroOrMoreChar length] > 1) {
    unichar theChar;
    theChar = [matchZeroOrMoreChar characterAtIndex:0];
    matchZeroOrMoreChar = [NSString stringWithCharacters:&theChar length:1];
  }
  [_matchZeroOrMoreCharTextField setStringValue:matchZeroOrMoreChar];
}
- (void)loadSettingsWithName:(NSString *)name
               localizedName:(NSString *)localizedName {
  LFRetainAssign(_moduleIdentifier, name);
  LFRetainAssign(_localizedName, localizedName);

  _genericDictionary = [[NSMutableDictionary alloc] init];
  [_genericDictionary setValue:@"20" forKey:@"MaximumRadicalLength"];
  [_genericDictionary setValue:@"false"
                        forKey:@"ShouldCommitAtMaximumRadicalLength"];
  [_genericDictionary setValue:@"false"
                        forKey:@"ClearReadingBufferAtCompositionError"];
  [_genericDictionary setValue:@"false" forKey:@"UseDynamicFrequency"];
  [_genericDictionary setValue:@"false" forKey:@"ComposeWhileTyping"];
  [_genericDictionary setValue:@"false"
                        forKey:@"UseSpaceAsFirstCandidateSelectionKey"];
  [_genericDictionary setValue:@"?" forKey:@"MatchOneChar"];
  [_genericDictionary setValue:@"*" forKey:@"MatchZeroOrMoreChar"];

  LFRetainAssign(_preferenceFilePath,
                 [TakaoHelper plistFilePath:PLIST_GENERIC_FILENAME(name)]);

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
    if (dictionary) [_genericDictionary addEntriesFromDictionary:dictionary];
  }  // end data
  [self setUI];
  [self writePreference:self];
}
- (NSView *)view {
  return view;
}

#pragma mark Interface Builder actions

- (IBAction)changeMaximumRadicalLength:(id)sender {
  int length = [sender intValue];
  [_maximumRadicalLengthStepper setIntValue:length];
  [_maximumRadicalLengthTextField setIntValue:length];
}
- (IBAction)changeMatchOneChar:(id)sender {
  if ([[_matchOneCharTextField stringValue] length] > 1) {
    unichar theChar;
    theChar = [[_matchOneCharTextField stringValue] characterAtIndex:0];
    [_matchOneCharTextField
        setStringValue:[NSString stringWithCharacters:&theChar length:1]];
  }

  if ([[_matchOneCharTextField stringValue]
          isEqualToString:[_matchZeroOrMoreCharTextField stringValue]]) {
    NSString *MatchOneChar = [_genericDictionary valueForKey:@"MatchOneChar"];
    [_matchOneCharTextField setStringValue:MatchOneChar];
    return;
  } else {
    [self writePreference:sender];
  }
}
- (IBAction)changeMatchZeroOrMoreChar:(id)sender {
  if ([[_matchZeroOrMoreCharTextField stringValue] length] > 1) {
    unichar theChar;
    theChar = [[_matchZeroOrMoreCharTextField stringValue] characterAtIndex:0];
    [_matchZeroOrMoreCharTextField
        setStringValue:[NSString stringWithCharacters:&theChar length:1]];
  }
  if ([[_matchZeroOrMoreCharTextField stringValue]
          isEqualToString:[_matchOneCharTextField stringValue]]) {
    NSString *MatchZeroOrMoreChar =
        [_genericDictionary valueForKey:@"MatchZeroOrMoreChar"];
    [_matchZeroOrMoreCharTextField setStringValue:MatchZeroOrMoreChar];
    return;
  } else {
    [self writePreference:sender];
  }
}
- (void)updateDictionary {
  if ([_shouldCommitAtMaximumRadicalLengthCheckBox intValue])
    [_genericDictionary setValue:@"true"
                          forKey:@"ShouldCommitAtMaximumRadicalLength"];
  else
    [_genericDictionary setValue:@"false"
                          forKey:@"ShouldCommitAtMaximumRadicalLength"];

  if ([_clearReadingBufferAtCompositionErrorCheckBox intValue])
    [_genericDictionary setValue:@"true"
                          forKey:@"ClearReadingBufferAtCompositionError"];
  else
    [_genericDictionary setValue:@"false"
                          forKey:@"ClearReadingBufferAtCompositionError"];

  if ([_useDynamicFrequencyCheckBox intValue])
    [_genericDictionary setValue:@"true" forKey:@"UseDynamicFrequency"];
  else
    [_genericDictionary setValue:@"false" forKey:@"UseDynamicFrequency"];

  if ([_composeWhileTypingCheckBox intValue])
    [_genericDictionary setValue:@"true" forKey:@"ComposeWhileTyping"];
  else
    [_genericDictionary setValue:@"false" forKey:@"ComposeWhileTyping"];

  if ([_useSpaceAsFirstCandidateSelectionKeyCheckBox intValue])
    [_genericDictionary setValue:@"true"
                          forKey:@"UseSpaceAsFirstCandidateSelectionKey"];
  else
    [_genericDictionary setValue:@"false"
                          forKey:@"UseSpaceAsFirstCandidateSelectionKey"];

  [_genericDictionary setValue:[_matchOneCharTextField stringValue]
                        forKey:@"MatchOneChar"];
  [_genericDictionary setValue:[_matchZeroOrMoreCharTextField stringValue]
                        forKey:@"MatchZeroOrMoreChar"];
  [_genericDictionary setValue:[_maximumRadicalLengthTextField stringValue]
                        forKey:@"MaximumRadicalLength"];
}
- (IBAction)writePreference:(id)sender {
  [self updateDictionary];
  NSData *data = [NSPropertyListSerialization
      dataWithPropertyList:_genericDictionary
                    format:NSPropertyListXMLFormat_v1_0
                   options:0
                     error:nil];

  if (data) [data writeToFile:_preferenceFilePath atomically:YES];
}
@end
