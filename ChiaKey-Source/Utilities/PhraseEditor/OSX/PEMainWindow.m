//
//  PEMainWindow.m
//  Takao
//
//  Developed by Lithoglyph Inc on 2008/12/9.
//  Copyright 2008 Yahoo! Inc. All rights reserved.
//

#import "PEMainWindow.h"

#import "PEPhraseTableView.h"

@implementation PEMainWindow

- (void)sendEvent:(NSEvent *)event {
  if ([event type] == NSEventTypeKeyDown) {
    if ([event keyCode] == 53) {
      NSText *text = [self fieldEditor:YES forObject:self];
      if ([text delegate] != nil &&
          [[text delegate] respondsToSelector:@selector(cancelEditing:)]) {
        [self makeFirstResponder:[text delegate]];
        [[text delegate] cancelEditing:text];
        [text setDelegate:nil];
        return;
      }
    } else if ([[event charactersIgnoringModifiers] characterAtIndex:0] == 13) {
      NSText *text = [self fieldEditor:YES forObject:self];
      if ([text delegate] != nil &&
          [[text delegate] respondsToSelector:@selector(cancelEditing:)]) {
        [self makeFirstResponder:[text delegate]];
        [self endEditingFor:text];
        [text setDelegate:nil];
        return;
      }
    }
  }
  [super sendEvent:event];
}

@end
