/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

// Lists every Bopomofo layout the engines accept. Items are in a fixed order
// and identified by the KeyboardLayout string the module plists store.
@interface TakaoKeyboardLayoutPopUpButton : NSPopUpButton

- (void)selectLayoutIdentifier:(NSString *)identifier;
- (NSString *)selectedLayoutIdentifier;

@end
