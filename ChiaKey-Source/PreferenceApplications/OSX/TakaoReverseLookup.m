/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoReverseLookup.h"

#import "TakaoHelper.h"

@implementation TakaoReverseLookup

- (void)dealloc {
  [_reverseLookupArray release];
  [_preferenceFilePath release];
  [super dealloc];
}

- (id)init {
  if (self = [super init]) {
    NSLog(@"init");
    BOOL loaded = [[NSBundle mainBundle] loadNibNamed:@"TakaoReverseLookup" owner:self topLevelObjects:nil];
    NSAssert((loaded == YES), @"NIB did not load");
    NSLog(@"init2");
  }
  return self;
}

- (void)awakeFromNib {
  LFRetainAssign(_preferenceFilePath,
                 [TakaoHelper plistFilePath:PLIST_GLOBAL_FILENAME]);
  _reverseLookupArray = [[NSArray alloc]
      initWithObjects:@"ReverseLookup-Generic-cj-cin",
                      @"ReverseLookup-Mandarin-bpmf-cin",
                      @"ReverseLookup-Mandarin-bpmf-cin-HanyuPinyin", nil];

  NSData *data = [NSData dataWithContentsOfFile:_preferenceFilePath
                                        options:0
                                          error:nil];
  if (data) {
    NSPropertyListFormat format;

    NSMutableDictionary *d = [NSPropertyListSerialization
        propertyListWithData:data
                      options:0
                       format:&format
                        error:nil];
    if (d) {
      NSArray *activatedAroundFilters =
          [d valueForKey:@"ActivatedAroundFilters"];
      NSEnumerator *enumerator = [activatedAroundFilters objectEnumerator];
      NSString *aroundFilter = nil;
      while (aroundFilter = [enumerator nextObject]) {
        if ([_reverseLookupArray containsObject:aroundFilter]) {
          if ([aroundFilter isEqualToString:@"ReverseLookup-Generic-cj-cin"]) {
            [_popUpButton selectItemAtIndex:1];
          } else if ([aroundFilter
                         isEqualToString:@"ReverseLookup-Mandarin-bpmf-cin"]) {
            [_popUpButton selectItemAtIndex:2];
          } else if ([aroundFilter
                         isEqualToString:
                             @"ReverseLookup-Mandarin-bpmf-cin-HanyuPinyin"]) {
            [_popUpButton selectItemAtIndex:3];
          }
          return;
        }
      }
    }
  }
  [_popUpButton selectItemAtIndex:0];
}

- (NSView *)view {
  return _view;
}
- (IBAction)changeReverseLookupSetting:(id)sender {
  NSInteger selected = [sender indexOfSelectedItem];
  NSString *reverseLookupModuleName = @"";
  if (selected > 0 && selected < 4) {
    reverseLookupModuleName = [_reverseLookupArray objectAtIndex:selected - 1];
  }
  NSMutableDictionary *newDictionary = [NSMutableDictionary dictionary];
  NSData *data = [NSData dataWithContentsOfFile:_preferenceFilePath
                                        options:0
                                          error:nil];
  if (data) {
    NSPropertyListFormat format;

    NSMutableDictionary *d = [NSPropertyListSerialization
        propertyListWithData:data
                      options:0
                       format:&format
                        error:nil];
    if (d) {
      NSMutableArray *newArray = [NSMutableArray array];
      NSArray *activatedAroundFilters =
          [d valueForKey:@"ActivatedAroundFilters"];
      NSEnumerator *enumerator = [activatedAroundFilters objectEnumerator];
      NSString *aroundFilter = nil;
      while (aroundFilter = [enumerator nextObject]) {
        if (![aroundFilter hasPrefix:@"ReverseLookup-"]) {
          [newArray addObject:aroundFilter];
        }
      }
      if ([reverseLookupModuleName length]) {
        [newArray addObject:reverseLookupModuleName];
      }
      [d setValue:newArray forKey:@"ActivatedAroundFilters"];
    }
    [newDictionary addEntriesFromDictionary:d];
  } else {
    if ([reverseLookupModuleName length]) {
      [newDictionary setValue:[NSArray arrayWithObject:reverseLookupModuleName]
                       forKey:@"ActivatedAroundFilters"];
    }
  }
  data = [NSPropertyListSerialization
      dataWithPropertyList:newDictionary
                    format:NSPropertyListXMLFormat_v1_0
                   options:0
                     error:nil];

  if (data) {
    [data writeToFile:_preferenceFilePath atomically:YES];
  }
}

@end
