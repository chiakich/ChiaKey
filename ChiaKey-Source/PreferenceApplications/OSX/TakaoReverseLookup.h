/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "TakaoSettings.h"

@interface TakaoReverseLookup : NSObject {
  IBOutlet NSView *_view;
  IBOutlet NSPopUpButton *_popUpButton;

  NSArray *_reverseLookupArray;
  NSString *_preferenceFilePath;
}

- (NSView *)view;
- (IBAction)changeReverseLookupSetting:(id)sender;

@end
