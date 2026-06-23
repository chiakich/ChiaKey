//
//  PESettingTableView.h
//  PhraseEditor
//
//  Developed by Lithoglyph Inc on 2008/11/2.
//  Copyright 2008 Yahoo! Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface PESettingTableView : NSTableView {
  NSMutableArray *_array;
}

- (void)setArray:(NSArray *)array;
- (NSString *)currentReading;

@end
