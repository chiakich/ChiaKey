/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoWordCount.h"

#import "TakaoHelper.h"

@implementation TakaoWordCount

- (void)dealloc {
  [_preferenceFilePath release];
  [super dealloc];
}

- (id)init {
  if (self = [super init]) {
    BOOL loaded = [[NSBundle mainBundle] loadNibNamed:@"TakaoWordCount" owner:self topLevelObjects:nil];
    NSAssert((loaded == YES), @"NIB did not load");
    LFRetainAssign(_preferenceFilePath,
                   [TakaoHelper plistFilePath:PLIST_WORDCOUNT_FILENAME]);
  }
  return self;
}

- (NSView *)view {
  return _view;
}

- (void)update {
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

    if ([dictionary valueForKey:@"TodayCount"])
      [_todayCount setStringValue:[dictionary valueForKey:@"TodayCount"]];
    else
      [_todayCount setStringValue:@"0"];

    if ([dictionary valueForKey:@"WeeklyCount"])
      [_weeklyCount setStringValue:[dictionary valueForKey:@"WeeklyCount"]];
    else
      [_weeklyCount setStringValue:@"0"];

    if ([dictionary valueForKey:@"TotalCount"])
      [_totalCount setStringValue:[dictionary valueForKey:@"TotalCount"]];
    else
      [_totalCount setStringValue:@"0"];

  } else {
    [_todayCount setStringValue:@"0"];
    [_weeklyCount setStringValue:@"0"];
    [_totalCount setStringValue:@"0"];
  }
}

- (IBAction)clear:(id)sender {
  if ([[NSFileManager defaultManager] fileExistsAtPath:_preferenceFilePath]) {
    [[NSFileManager defaultManager] removeItemAtPath:_preferenceFilePath
                                               error:nil];
  }

  [_todayCount setStringValue:@"0"];
  [_weeklyCount setStringValue:@"0"];
  [_totalCount setStringValue:@"0"];
}

@end
