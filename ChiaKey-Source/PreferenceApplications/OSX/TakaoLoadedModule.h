/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "TakaoSettings.h"

@interface TakaoLoadedModule : NSObject {
  IBOutlet NSView *_pluginView;

  IBOutlet NSTableView *_tableView;
  IBOutlet NSView *_contentView;
  IBOutlet NSWindow *_window;
  IBOutlet NSButton *_removeButton;

  IBOutlet NSProgressIndicator *_progressIndicator;
  IBOutlet NSTextField *_progressLabel;

  NSMutableArray *_modules;
}

- (void)setModules:(NSArray *)modules;
- (void)reloadServer;
- (void)reloadData;
- (void)setContentView:(NSView *)view;
- (void)setUIEnabled:(BOOL)enabled;
- (void)showProgressIndicator;
- (void)hideProgressIndicator;

- (IBAction)removePlugin:(id)sender;

@end
