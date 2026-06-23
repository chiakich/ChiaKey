//
//  PEDialogWindow.m
//  Takao
//
//  Developed by Lithoglyph Inc zonble on 2008/11/16.
//  Copyright 2008 Yahoo! Inc. All rights reserved.
//

#import "PEDialogWindow.h"

@implementation PEDialogWindow

- (void)sendEvent:(NSEvent *)event {
  if ([event type] == NSEventTypeKeyDown) {
    if ([event keyCode] == 53) {
      if (_cancelButton) {
        [_cancelButton performClick:self];
      } else {
        [NSApp endSheet:self];
        [self orderOut:self];
      }
    }
  }
  [super sendEvent:event];
}

@end
