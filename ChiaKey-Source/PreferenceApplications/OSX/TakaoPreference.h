/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

/*!
        @header TakaoPreference
*/

#import <Cocoa/Cocoa.h>

#import "TakaoSettings.h"

@class TakaoWindow;
#define WINDOW_TITLE_HEIGHT 78

static NSString *GeneralToolbarItemIdentifier = @"General";
static NSString *PhoneticToolbarItemIdentifier = @"Phonetic";
static NSString *CangjieToolbarItemIdentifier = @"Cangjie";
static NSString *SimplexToolbarItemIdentifier = @"Simplex";
static NSString *PhraseToolbarItemIdentifier = @"Phrase";
static NSString *MiscToolbarItemIdentifier = @"Misc";
static NSString *UpdateToolbarItemIdentifier = @"Update";
static NSString *GenericToolbarItemIdentifier = @"Generic";
static NSString *PluginToolbarItemIdentifier = @"Plugins";

/*!
        @class TakaoPreference
        @abstract The main class of the TakaoPreference application, it handles
        the events on clicking on the toolbar, and then show the pane of each
        item of the preference tool.
*/

@interface TakaoPreference : NSObject {
  IBOutlet NSView *_generalView;
  IBOutlet NSView *_phoneticView;
  IBOutlet NSView *_cangjieView;
  IBOutlet NSView *_simpexView;
  IBOutlet NSView *_phraseView;
  IBOutlet NSView *_updateView;
  IBOutlet NSView *_genericSettingView;
  IBOutlet NSView *_pluginView;

  IBOutlet NSView *_keyboardLayoutContentView;
  IBOutlet NSView *_keyboardLayoutView;

  IBOutlet TakaoWindow *window;
  IBOutlet id _takaoGenericController;
  IBOutlet id _takaoGlobalController;
  IBOutlet id _takaoLoadedModuleController;
  NSImage *_defaultApplicationImage;

  BOOL _hasGenericInputMethods;
  BOOL _hasLoadedModules;
}

- (void)setAppIcon:(NSImage *)image;
/*!
        @method setActiveView:animate:
        @abstract Sets the active view of the preference tool.
        @param view The active view.
        @param flag Should use animation effect or not.
*/
- (void)setActiveView:(NSView *)view animate:(BOOL)flag;
/*!
        @method toggleActivePreferenceView:
        @abstract Responding on clicking on an item on the toolbar.
        @param sender The sender object.
*/
- (void)toggleActivePreferenceView:(id)sender;

@end
