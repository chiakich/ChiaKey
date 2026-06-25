/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoPreference.h"

#import "TakaoGeneric.h"
#import "TakaoGlobal.h"
#import "TakaoWindow.h"

@implementation TakaoPreference

- (void)awakeFromNib {
  [NSApp setDelegate:(id)self];

  _defaultApplicationImage = [[NSApp applicationIconImage] copy];

  NSArray *fetchedModules = nil;
  NSMutableArray *genericModules = [NSMutableArray array];
  NSArray *loadedModules = nil;

  id ovService;
  @try {
    ovService = [NSConnection
        rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
                                            host:nil];
    if (ovService) {
      [ovService setProtocolForProxy:@protocol(OpenVanillaService)];
      fetchedModules =
          [ovService identifiersAndLocalizedNamesWithPattern:@"Generic-*"];

      NSEnumerator *moduleEnumerator = [fetchedModules objectEnumerator];
      NSArray *itemArray = nil;
      while (itemArray = [moduleEnumerator nextObject]) {
        NSString *name = [itemArray objectAtIndex:0];
        if (![name isEqualToString:@"Generic-cj-cin"] &&
            ![name isEqualToString:@"Generic-simplex-cin"])
          [genericModules addObject:itemArray];
      }

      NSArray *info = [ovService dynamicallyLoadedModulePackageInfo];
      loadedModules = [NSArray arrayWithArray:info];
    }

    if ([genericModules count]) {
      _hasGenericInputMethods = YES;
      [_takaoGenericController setModules:genericModules];
      [_takaoGlobalController setInputMethods:genericModules];
    } else {
      [_takaoGlobalController setInputMethods:nil];
    }

    if ([loadedModules count]) {
      _hasLoadedModules = YES;
      [_takaoLoadedModuleController setModules:loadedModules];
    }
  } @catch (NSException *e) {
    // NSLog(@"Exceptions raise on retreiving version info");
    // return;
  }

  id toolbar = [[[NSToolbar alloc] initWithIdentifier:@"preferences toolbar"]
      autorelease];
  [toolbar setAllowsUserCustomization:NO];
  [toolbar setAutosavesConfiguration:NO];
  [toolbar setSizeMode:NSToolbarSizeModeDefault];
  [toolbar setDisplayMode:NSToolbarDisplayModeIconAndLabel];
  [toolbar setDelegate:(id)self];
  [toolbar setSelectedItemIdentifier:GeneralToolbarItemIdentifier];
#if (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5)
  // cancel unified look and feel?
  int newHeight = [_generalView frame].size.height -
                  [_keyboardLayoutContentView frame].size.height + 10;
  [_generalView setFrame:NSMakeRect(0, 0, 480, newHeight)];
  [_keyboardLayoutContentView removeFromSuperview];
#else
  [_keyboardLayoutContentView addSubview:_keyboardLayoutView];
#endif

  if (@available(macOS 11, *)) {
    [window setToolbarStyle:NSWindowToolbarStylePreference];
  }

  [window setLevel:NSFloatingWindowLevel];
  [window setToolbar:toolbar];
  [window center];
  [window setDelegate:(id)self];

  [self setActiveView:_generalView animate:NO];
  [self setAppIcon:[NSImage imageNamed:@"general"]];

  [window setTitle:LFLSTR(GeneralToolbarItemIdentifier)];
}

- (void)setActiveView:(NSView *)view animate:(BOOL)flag {
  NSEvent *e = [NSApp currentEvent];
  if ([e modifierFlags] & NSEventModifierFlagShift) {
    [window useSlowMotion];
  } else {
    [window stopSlowMotion];
  }

  NSRect windowFrame = [window frame];
  windowFrame.size.height = [view frame].size.height + WINDOW_TITLE_HEIGHT;
  windowFrame.size.width = [view frame].size.width;
  windowFrame.origin.y =
      NSMaxY([window frame]) - ([view frame].size.height + WINDOW_TITLE_HEIGHT);

  if ([[[window contentView] subviews] count] != 0) {
    NSView *currentView = [[[window contentView] subviews] objectAtIndex:0];
    if (currentView != view) {
      [currentView removeFromSuperview];
    }
  }
  [window setFrame:windowFrame display:YES animate:flag];
  NSRect viewFrame = [view frame];
  [[window contentView]
      setFrame:NSMakeRect(0, 0, viewFrame.size.width, viewFrame.size.height)];
  [view setFrame:NSMakeRect(0, 0, viewFrame.size.width, viewFrame.size.height)];
  [[window contentView] addSubview:view];
}

- (void)setAppIcon:(NSImage *)image {
  NSImage *i =
      [[[NSImage alloc] initWithSize:NSMakeSize(512.0, 512.0)] autorelease];
  int height = [i size].height;
  int width = [i size].width;
  NSRect fullRect = NSMakeRect(0, 0, width, height);
  NSRect newRect = NSMakeRect(width / 2, 0, width / 2, height / 2);
  [i lockFocus];
  [_defaultApplicationImage drawInRect:fullRect
                             fromRect:NSZeroRect
                             operation:NSCompositingOperationSourceOver
                              fraction:1.0];
  [image drawInRect:newRect
           fromRect:NSZeroRect
          operation:NSCompositingOperationSourceOver
           fraction:1.0];
  [i unlockFocus];
  [window setMiniwindowImage:i];
}

- (void)toggleActivePreferenceView:(id)sender {
  NSView *view = nil;

  if ([[sender itemIdentifier] isEqualToString:GeneralToolbarItemIdentifier])
    view = _generalView;
  else if ([[sender itemIdentifier]
               isEqualToString:PhoneticToolbarItemIdentifier])
    view = _phoneticView;
  else if ([[sender itemIdentifier]
               isEqualToString:CangjieToolbarItemIdentifier])
    view = _cangjieView;
  else if ([[sender itemIdentifier]
               isEqualToString:SimplexToolbarItemIdentifier])
    view = _simpexView;
  else if ([[sender itemIdentifier]
               isEqualToString:PhraseToolbarItemIdentifier])
    view = _phraseView;
  else if ([[sender itemIdentifier]
               isEqualToString:UpdateToolbarItemIdentifier])
    view = _updateView;
  else if ([[sender itemIdentifier]
               isEqualToString:GenericToolbarItemIdentifier])
    view = _genericSettingView;
  else if ([[sender itemIdentifier]
               isEqualToString:PluginToolbarItemIdentifier])
    view = _pluginView;

  if (view != _generalView) {
    [[NSColorPanel sharedColorPanel] orderOut:self];
  }

  [self setActiveView:view animate:YES];
  [self setAppIcon:[sender image]];
  [window setTitle:LFLSTR([sender itemIdentifier])];

  if ([[sender itemIdentifier] isEqualToString:UpdateToolbarItemIdentifier] &&
      [_takaoUpdateController respondsToSelector:@selector(updatePaneDidBecomeActive)]) {
    [_takaoUpdateController updatePaneDidBecomeActive];
  }
}

#pragma mark NSWindow delegate methods

- (void)windowWillClose:(NSNotification *)notification {
  [[NSApplication sharedApplication] terminate:self];
}

@end
