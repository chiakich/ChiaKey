/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoPreference.h"

#import "TakaoGeneric.h"
#import "TakaoGlobal.h"
#import "TakaoLoadedModule.h"
#import "TakaoUpdate.h"
#import "TakaoWindow.h"

@interface TakaoPreference (Private)
- (void)_refreshInputMethodListFromStatus:(NSDictionary *)status;
- (void)_serviceStatusDidUpdate:(NSNotification *)notification;
@end

@implementation TakaoPreference

- (void)awakeFromNib {
  [NSApp setDelegate:(id)self];

  _defaultApplicationImage = [[NSApp applicationIconImage] copy];

  // Module info comes from the status file the IME publishes on every
  // start/reload (see ChiaKeyServiceCoordination.h), not from XPC.
  NSDictionary *status = ChiaKeyReadServiceStatus();

  // The General pane lists whatever the engine actually loaded, so that
  // imported tables can be hidden from the input menu. The table pane
  // manages the files themselves and loads its own list.
  [self _refreshInputMethodListFromStatus:status];

  // Republished whenever the engine reloads, which is what happens right
  // after a table is imported or removed.
  [[NSDistributedNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(_serviceStatusDidUpdate:)
             name:ChiaKeyServiceStatusDidUpdateNotification
           object:nil];

  NSArray *loadedModules = [status objectForKey:ChiaKeyStatusPackagesKey];

  if ([loadedModules count]) {
    _hasLoadedModules = YES;
    [_takaoLoadedModuleController setModules:loadedModules];
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

- (void)_refreshInputMethodListFromStatus:(NSDictionary *)status {
  NSMutableArray *genericModules = [NSMutableArray array];

  NSEnumerator *moduleEnumerator =
      [[status objectForKey:ChiaKeyStatusModulesKey] objectEnumerator];
  NSArray *itemArray = nil;
  while (itemArray = [moduleEnumerator nextObject]) {
    if (![itemArray isKindOfClass:[NSArray class]] || ![itemArray count])
      continue;
    NSString *name = [itemArray objectAtIndex:0];
    if ([name hasPrefix:@"Generic-"] &&
        ![name isEqualToString:@"Generic-cj-cin"] &&
        ![name isEqualToString:@"Generic-simplex-cin"])
      [genericModules addObject:itemArray];
  }

  [_takaoGlobalController
      setInputMethods:[genericModules count] ? genericModules : nil];
}

- (void)_serviceStatusDidUpdate:(NSNotification *)notification {
  [self _refreshInputMethodListFromStatus:ChiaKeyReadServiceStatus()];
}

- (void)setActiveView:(NSView *)view animate:(BOOL)flag {
  NSEvent *e = [NSApp currentEvent];
  if ([e modifierFlags] & NSEventModifierFlagShift) {
    [window useSlowMotion];
  } else {
    [window stopSlowMotion];
  }

  // Measured, not assumed: under the macOS 11 preference toolbar style the
  // title bar and toolbar come to 88pt, so the hardcoded 78 left every pane
  // 10pt taller than the space it was given and clipped its top row.
  CGFloat chromeHeight =
      NSHeight([window frame]) - NSHeight([window contentLayoutRect]);
  if (chromeHeight <= 0) {
    chromeHeight = WINDOW_TITLE_HEIGHT;
  }

  NSRect windowFrame = [window frame];
  windowFrame.size.height = [view frame].size.height + chromeHeight;
  windowFrame.size.width = [view frame].size.width;
  windowFrame.origin.y =
      NSMaxY([window frame]) - ([view frame].size.height + chromeHeight);

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
    [(TakaoUpdate *)_takaoUpdateController updatePaneDidBecomeActive];
  }
}

#pragma mark NSWindow delegate methods

- (void)windowWillClose:(NSNotification *)notification {
  [[NSApplication sharedApplication] terminate:self];
}

@end
