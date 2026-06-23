//
//  PEPhraseTableView.m
//  PhraseEditor
//
//  Developed by Lithoglyph Inc on 2008/11/2.
//  Copyright 2008 Yahoo! Inc. All rights reserved.
//

#import "PEPhraseTableView.h"

#import "PEController.h"

@implementation PEPhraseTableView

- (NSMenu *)menuForEvent:(NSEvent *)event {
  NSPoint location = [event locationInWindow];
  int clickedRow = (int)[self rowAtPoint:[self convertPoint:location fromView:nil]];
  if (clickedRow < 0) {
    return nil;
  }
  [self selectRowIndexes:[NSIndexSet indexSetWithIndex:clickedRow] byExtendingSelection:NO];
  return [self menu];
}

- (void)cancelEditing:(NSText *)text {
  [text setDelegate:nil];
  [self abortEditing];
  [[[NSApp delegate] window] makeFirstResponder:self];
}

- (IBAction)delete:(id)sender {
  [[self delegate] delete:self];
}

- (IBAction)copy:(id)sender {
  [[self delegate] copy:self];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
  if ([menuItem action] == @selector(copy:) || [menuItem action] == @selector
                                                   (delete:)) {
    if ([self selectedRow] < 0) {
      return NO;
    }
  }
  return YES;
}

#pragma mark -
#pragma mark Keyboard events

- (void)keyDown:(NSEvent *)theEvent {
  if ([self selectedRow] < 0) {
    [super keyDown:theEvent];
    return;
  }

  unichar key = [[theEvent charactersIgnoringModifiers] characterAtIndex:0];

  if (key == NSDeleteFunctionKey || key == NSDeleteCharFunctionKey ||
      key == NSDeleteCharacter) {
    [[self delegate] delete:self];
    return;
  } else if (key == ' ') {
  } else if (key == 13) {
  }

  [super keyDown:theEvent];
}

#pragma mark Mouse events

- (void)mouseDown:(NSEvent *)event {
  [super mouseDown:event];
  [self mouseUp:event];
}

- (void)mouseUp:(NSEvent *)event {
  if (![self isEnabled]) return;

  NSPoint location = [event locationInWindow];
  int clickedRow = [self rowAtPoint:[self convertPoint:location fromView:nil]];
  int clickedRColumnIndex = [self columnAtPoint:[self convertPoint:location
                                                          fromView:nil]];
  NSTableColumn *clickedColumn =
      [[self tableColumns] objectAtIndex:clickedRColumnIndex];
  NSString *identifier = [clickedColumn identifier];

  if (clickedRow < 0) return;

  if ([event clickCount] > 1) {
    if ([identifier isEqualToString:@"phrase"]) {
      if ([[self delegate] respondsToSelector:@selector(editPhrase:)])
        [[self delegate] editPhrase:self];
    } else if ([identifier isEqualToString:@"reading"]) {
      if ([[self delegate] respondsToSelector:@selector(editReading:)])
        [[self delegate] editReading:self];
    }
  }
}

@end
