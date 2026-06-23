/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

/*!
        @header TakaoWindow
*/

/*!
        @class TakaoWindow
        @abstract The main window of the preference tool.
*/

@interface TakaoWindow : NSWindow {
  BOOL _useSlowMotion;
}

/*!
        @method useSlowMotion
        @abstract Asks the window to use slow motion animation on resizing.
*/
- (void)useSlowMotion;
/*!
        @method stopSlowMotion
        @abstract Asks the window to use stop motion animation on resizing.
*/
- (void)stopSlowMotion;
@end
