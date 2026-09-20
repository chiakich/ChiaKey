/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoKeyboardLayoutPopUpButton.h"

#import "TakaoSettings.h"

// Identifiers as written to the plist, paired with their localized titles.
static NSString *const kLayoutIdentifiers[] = {@"Standard", @"ETen",
                                               @"Hanyu Pinyin", @"ETen26",
                                               @"Hsu"};
static NSString *const kLayoutTitleKeys[] = {@"Standard", @"ETen",
                                             @"Hanyu Pinyin", @"ETen 26",
                                             @"Hsu"};
static const NSUInteger kLayoutCount =
    sizeof(kLayoutIdentifiers) / sizeof(kLayoutIdentifiers[0]);

@implementation TakaoKeyboardLayoutPopUpButton

- (void)_init {
  NSMenu *menu = [[[NSMenu alloc] initWithTitle:@"Menu"] autorelease];
  for (NSUInteger i = 0; i < kLayoutCount; i++) {
    NSMenuItem *item =
        [[[NSMenuItem alloc] initWithTitle:LFLSTR(kLayoutTitleKeys[i])
                                    action:NULL
                             keyEquivalent:@""] autorelease];
    [item setRepresentedObject:kLayoutIdentifiers[i]];
    [menu addItem:item];
  }
  [self setMenu:menu];
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
- (void)selectLayoutIdentifier:(NSString *)identifier {
  // Legacy key-string spellings some older plists carry for these two layouts.
  if ([identifier isEqualToString:@"bpmfdtnlvkhgvcgycjqwsexuaorwiqzpmntlhfjkd"])
    identifier = @"ETen26";
  else if ([identifier
               isEqualToString:@"bpmfdtnlgkhjvcjvcrzasexuyhgeiawomnklldfjs"])
    identifier = @"Hsu";

  NSInteger index = 0;
  for (NSUInteger i = 0; i < kLayoutCount; i++) {
    if ([kLayoutIdentifiers[i] isEqualToString:identifier]) {
      index = (NSInteger)i;
      break;
    }
  }
  [self selectItemAtIndex:index];
}
- (NSString *)selectedLayoutIdentifier {
  NSString *identifier = [[self selectedItem] representedObject];
  return identifier ? identifier : kLayoutIdentifiers[0];
}

@end
