//
//  ZBTargetView.m
//  SignatureMaker
//
/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#import "ZBTargetView.h"

@implementation ZBTargetView

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Initialization code here.
  }
  return self;
}

- (void)drawRect:(NSRect)rect {
  NSBezierPath *b = [NSBezierPath bezierPathWithRect:rect];
  [[NSColor whiteColor] setStroke];
  [b stroke];
}

@end
