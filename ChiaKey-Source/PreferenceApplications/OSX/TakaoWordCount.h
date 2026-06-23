/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "TakaoSettings.h"

@interface TakaoWordCount : NSObject {
  NSString *_preferenceFilePath;

  IBOutlet NSView *_view;
  IBOutlet NSTextField *_todayCount;
  IBOutlet NSTextField *_weeklyCount;
  IBOutlet NSTextField *_totalCount;
}

- (NSView *)view;
- (void)update;
- (IBAction)clear:(id)sender;

@end
