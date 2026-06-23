/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoWindow.h"

@implementation TakaoWindow

- (void)useSlowMotion {
  _useSlowMotion = YES;
}

- (void)stopSlowMotion {
  _useSlowMotion = NO;
}

- (NSTimeInterval)animationResizeTime:(NSRect)newWindowFrame {
  if (_useSlowMotion)
    return (NSTimeInterval)1;
  else
    return (NSTimeInterval)0.1;
}

@end
