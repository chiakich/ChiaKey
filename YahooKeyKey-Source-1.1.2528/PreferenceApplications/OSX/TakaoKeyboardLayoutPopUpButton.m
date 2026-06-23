/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoKeyboardLayoutPopUpButton.h"

#import "TakaoSettings.h"

@implementation TakaoKeyboardLayoutPopUpButton

- (void)dealloc {
  [_standardMenu release];
  [_realMenu release];
  [super dealloc];
}
- (void)_init {
  _standardMenu = [[NSMenu alloc] initWithTitle:@"Menu"];
  _realMenu = [[NSMenu alloc] initWithTitle:@"Menu"];

  NSMenuItem *sStandardMenuItem =
      [[[NSMenuItem alloc] initWithTitle:LFLSTR(@"Standard")
                                  action:NULL
                           keyEquivalent:@""] autorelease];
  NSMenuItem *sEtenMenuItem =
      [[[NSMenuItem alloc] initWithTitle:LFLSTR(@"ETen")
                                  action:NULL
                           keyEquivalent:@""] autorelease];
  NSMenuItem *sHanyuPinyinMenuItem =
      [[[NSMenuItem alloc] initWithTitle:LFLSTR(@"Hanyu Pinyin")
                                  action:NULL
                           keyEquivalent:@""] autorelease];

  [_standardMenu addItem:sStandardMenuItem];
  [_standardMenu addItem:sEtenMenuItem];
  [_standardMenu addItem:sHanyuPinyinMenuItem];

  NSMenuItem *rStandardMenuItem =
      [[[NSMenuItem alloc] initWithTitle:LFLSTR(@"Standard")
                                  action:NULL
                           keyEquivalent:@""] autorelease];
  NSMenuItem *rEtenMenuItem =
      [[[NSMenuItem alloc] initWithTitle:LFLSTR(@"ETen")
                                  action:NULL
                           keyEquivalent:@""] autorelease];
  NSMenuItem *rHanyuPinyinMenuItem =
      [[[NSMenuItem alloc] initWithTitle:LFLSTR(@"Hanyu Pinyin")
                                  action:NULL
                           keyEquivalent:@""] autorelease];
  NSMenuItem *rEten26MenuItem =
      [[[NSMenuItem alloc] initWithTitle:LFLSTR(@"ETen 26")
                                  action:NULL
                           keyEquivalent:@""] autorelease];
  NSMenuItem *rHsuMenuItem =
      [[[NSMenuItem alloc] initWithTitle:LFLSTR(@"Hsu")
                                  action:NULL
                           keyEquivalent:@""] autorelease];

  [_realMenu addItem:rStandardMenuItem];
  [_realMenu addItem:rEtenMenuItem];
  [_realMenu addItem:rHanyuPinyinMenuItem];
  [_realMenu addItem:rEten26MenuItem];
  [_realMenu addItem:rHsuMenuItem];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(changeMenu:)
             name:@"NSPopUpButtonWillPopUpNotification"
           object:nil];

  [self setMenu:_standardMenu];
}
- (id)initWithCoder:(NSCoder *)decoder {
  self = [super initWithCoder:decoder];
  if (self) {
    [self _init];
  }
  return self;
}
- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self _init];
  }
  return self;
}
- (void)changeMenu:(NSNotification *)notification {
  NSEvent *e = [NSApp currentEvent];

  //	if ([e modifierFlags] & NSShiftKeyMask) {
  if (([e modifierFlags] &
       (NSEventModifierFlagCommand | NSEventModifierFlagShift)) ==
      (NSEventModifierFlagCommand | NSEventModifierFlagShift)) {
    [self setMenu:_realMenu];
  } else {
    [self setMenu:_standardMenu];
  }
}

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
- (void)selectItemAtIndex:(NSInteger)index
#else
- (void)selectItemAtIndex:(int)index
#endif

{
  if (index > 2 && [[[self menu] itemArray] count] < 4) {
    NSMenu *menu = [_standardMenu copy];
    [menu addItemWithTitle:LFLSTR(@"Others") action:NULL keyEquivalent:@""];
    [self setMenu:menu];
    [menu autorelease];
  }
  [super selectItemAtIndex:index];
}

@end
