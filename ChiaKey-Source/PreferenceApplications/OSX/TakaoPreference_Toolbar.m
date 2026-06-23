/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
//
//  TakaoPreference_Toolbar.m
//  TakaoPreference
//
#import "TakaoPreference.h"

@implementation TakaoPreference (Toolbar)

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
  NSMutableArray *array = [NSMutableArray array];
  [array addObject:GeneralToolbarItemIdentifier];
  [array addObject:PhoneticToolbarItemIdentifier];
  [array addObject:CangjieToolbarItemIdentifier];
  [array addObject:SimplexToolbarItemIdentifier];

  if (_hasGenericInputMethods) {
    [array addObject:GenericToolbarItemIdentifier];
  }
  if (_hasLoadedModules) {
    [array addObject:PluginToolbarItemIdentifier];
  }

  [array addObject:PhraseToolbarItemIdentifier];
  [array addObject:UpdateToolbarItemIdentifier];

  return array;
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
  return [NSArray arrayWithObjects:GeneralToolbarItemIdentifier,
                                   PhoneticToolbarItemIdentifier,
                                   CangjieToolbarItemIdentifier,
                                   SimplexToolbarItemIdentifier,
                                   PhraseToolbarItemIdentifier,
                                   UpdateToolbarItemIdentifier,
                                   GenericToolbarItemIdentifier,
                                   PluginToolbarItemIdentifier, nil];
}

- (NSArray *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar {
  return [NSArray arrayWithObjects:GeneralToolbarItemIdentifier,
                                   PhoneticToolbarItemIdentifier,
                                   CangjieToolbarItemIdentifier,
                                   SimplexToolbarItemIdentifier,
                                   PhraseToolbarItemIdentifier,
                                   UpdateToolbarItemIdentifier,
                                   GenericToolbarItemIdentifier,
                                   PluginToolbarItemIdentifier, nil];
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
        itemForItemIdentifier:(NSString *)identifier
    willBeInsertedIntoToolbar:(BOOL)willBeInserted {
  NSToolbarItem *item =
      [[[NSToolbarItem alloc] initWithItemIdentifier:identifier] autorelease];
  if ([identifier isEqualToString:GeneralToolbarItemIdentifier]) {
    [item setLabel:LFLSTR(GeneralToolbarItemIdentifier)];
    [item setImage:[NSImage imageNamed:@"general"]];
    [item setTarget:self];
    [item setAction:@selector(toggleActivePreferenceView:)];
  } else if ([identifier isEqualToString:PhoneticToolbarItemIdentifier]) {
    [item setLabel:LFLSTR(PhoneticToolbarItemIdentifier)];
    [item setImage:[NSImage imageNamed:@"phonetic"]];
    [item setTarget:self];
    [item setAction:@selector(toggleActivePreferenceView:)];
  } else if ([identifier isEqualToString:CangjieToolbarItemIdentifier]) {
    [item setLabel:LFLSTR(CangjieToolbarItemIdentifier)];
    [item setImage:[NSImage imageNamed:@"cangjie"]];
    [item setTarget:self];
    [item setAction:@selector(toggleActivePreferenceView:)];
  } else if ([identifier isEqualToString:SimplexToolbarItemIdentifier]) {
    [item setLabel:LFLSTR(SimplexToolbarItemIdentifier)];
    [item setImage:[NSImage imageNamed:@"simplex"]];
    [item setTarget:self];
    [item setAction:@selector(toggleActivePreferenceView:)];
  } else if ([identifier isEqualToString:GenericToolbarItemIdentifier]) {
    [item setLabel:LFLSTR(GenericToolbarItemIdentifier)];
    [item setImage:[NSImage imageNamed:@"generic"]];
    [item setTarget:self];
    [item setAction:@selector(toggleActivePreferenceView:)];
  } else if ([identifier isEqualToString:PluginToolbarItemIdentifier]) {
    [item setLabel:LFLSTR(PluginToolbarItemIdentifier)];
    [item setImage:[NSImage imageNamed:@"plugin"]];
    [item setTarget:self];
    [item setAction:@selector(toggleActivePreferenceView:)];
  } else if ([identifier isEqualToString:PhraseToolbarItemIdentifier]) {
    [item setLabel:LFLSTR(PhraseToolbarItemIdentifier)];
    [item setImage:[NSImage imageNamed:@"phrase"]];
    [item setTarget:self];
    [item setAction:@selector(toggleActivePreferenceView:)];
  } else if ([identifier isEqualToString:UpdateToolbarItemIdentifier]) {
    [item setLabel:LFLSTR(UpdateToolbarItemIdentifier)];
    [item setImage:[NSImage imageNamed:@"update"]];
    [item setTarget:self];
    [item setAction:@selector(toggleActivePreferenceView:)];
  } else
    item = nil;
  return item;
}

@end
