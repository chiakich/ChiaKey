/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoLoadedModule.h"

#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>

#import "TakaoPreference.h"
#import "TakaoReverseLookup.h"
#import "TakaoWordCount.h"

static AuthorizationRef authorizationRef = NULL;

@implementation TakaoLoadedModule

- (void)dealloc {
  [_modules release];
  [super dealloc];
}

- (void)awakeFromNib {
  [_pluginView setFrame:NSMakeRect(0, 0, 480, 230)];
  [_tableView setDelegate:(id)self];
  [_tableView setDataSource:self];
  [self hideProgressIndicator];
}

- (void)setModules:(NSArray *)modules {
  if (!_modules) {
    _modules = [[NSMutableArray alloc] init];
  }
  [_modules removeAllObjects];
  NSEnumerator *e = [modules objectEnumerator];
  NSDictionary *d = nil;
  while (d = [e nextObject]) {
    NSMutableDictionary *dictionary =
        [NSMutableDictionary dictionaryWithDictionary:d];
    if ([[d valueForKey:OVServiceLoadedModulePackageIdentifierKey]
            isEqualToString:@"YKAFWordCount"]) {
      [dictionary setValue:[[[TakaoWordCount alloc] init] autorelease]
                    forKey:@"controller"];
    } else if ([[d valueForKey:OVServiceLoadedModulePackageIdentifierKey]
                   isEqualToString:@"YKReverseLookup"]) {
      [dictionary setValue:[[[TakaoReverseLookup alloc] init] autorelease]
                    forKey:@"controller"];
    }

    [_modules addObject:dictionary];
  }
  [_tableView reloadData];

  if (![_modules count]) {
    [_pluginView setFrame:NSMakeRect(0, 0, 480, 230)];
    [[NSApp delegate] setActiveView:_pluginView animate:YES];
    [_pluginView setNeedsDisplay:YES];
  }
}
- (void)reloadServer {
  id ovService;
  @try {
    ovService = [NSConnection
        rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
                                            host:nil];
    if (ovService) {
      [ovService reloadOpenVanilla];
    }
  } @catch (NSException *e) {
    // NSLog(@"Exceptions raise on retreiving version info");
    // return;
  }
}
- (void)reloadData {
  id ovService;
  @try {
    ovService = [NSConnection
        rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
                                            host:nil];
    if (ovService) {
      NSArray *info = [ovService dynamicallyLoadedModulePackageInfo];
      [self setModules:info];
    }
  } @catch (NSException *e) {
    // NSLog(@"Exceptions raise on retreiving version info");
    // return;
  }
  [self setUIEnabled:YES];
}
- (void)setContentView:(NSView *)view {
  NSArray *viewArray = [NSArray arrayWithArray:[_contentView subviews]];
  NSEnumerator *e = [viewArray objectEnumerator];
  NSView *subView = nil;
  while (subView = [e nextObject]) {
    [subView removeFromSuperview];
  }
  if (view) {
    [view setFrame:[_contentView bounds]];
    [_contentView addSubview:view];
  }
}

- (void)alertDidEnd:(NSAlert *)alert
         returnCode:(int)returnCode
        contextInfo:(void *)contextInfo {
  if (returnCode != NSOKButton) {
    [self setUIEnabled:YES];
    return;
  }

  NSDictionary *selectedPlugin = (NSDictionary *)contextInfo;
  NSString *path =
      [selectedPlugin valueForKey:OVServiceLoadedModulePackageBundlePathKey];

  if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
    NSAlert *alert =
        [NSAlert alertWithMessageText:
                     LFLSTR(@"The specified module is already removed!")
                        defaultButton:LFLSTR(@"OK")
                      alternateButton:nil
                          otherButton:nil
            informativeTextWithFormat:@""];
    [alert beginSheetModalForWindow:_window
                      modalDelegate:nil
                     didEndSelector:NULL
                        contextInfo:NULL];
    [self setUIEnabled:YES];
    return;
  }

  OSStatus status;

  if (authorizationRef == NULL) {
    status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment,
                                 kAuthorizationFlagDefaults, &authorizationRef);
  } else {
    status = noErr;
  }

  if (status != noErr) {
    NSLog(@"Could not get authorization, failing.");
    return;
  }

  char *args[3];
  args[0] = "-rf";
  args[1] = (char *)[path UTF8String];
  args[2] = (char *)NULL;

  status = AuthorizationExecuteWithPrivileges(
      authorizationRef, "/bin/rm", kAuthorizationFlagDefaults, args, NULL);

  if (status != noErr) {
    NSAlert *alert = [NSAlert
             alertWithMessageText:LFLSTR(
                                      @"Failed to remove the specific plugin!")
                    defaultButton:LFLSTR(@"OK")
                  alternateButton:nil
                      otherButton:nil
        informativeTextWithFormat:
            LFLSTR(@"PLease check your system privilege and try again.")];
    [alert beginSheetModalForWindow:_window
                      modalDelegate:nil
                     didEndSelector:NULL
                        contextInfo:NULL];
    return;
  }

  [self setUIEnabled:NO];
  [self reloadServer];
  [self performSelector:@selector(reloadData) withObject:nil afterDelay:5.0];
}

- (IBAction)removePlugin:(id)sender {
  int selectedRow = [_tableView selectedRow];

  if (selectedRow < 0) {
    NSAlert *alert =
        [NSAlert alertWithMessageText:LFLSTR(@"Please select a plugin.")
                        defaultButton:LFLSTR(@"OK")
                      alternateButton:nil
                          otherButton:nil
            informativeTextWithFormat:@""];
    [alert beginSheetModalForWindow:_window
                      modalDelegate:self
                     didEndSelector:NULL
                        contextInfo:NULL];
    return;
  }

  NSDictionary *selectedPlugin = [_modules objectAtIndex:selectedRow];
  NSString *message = [NSString
      stringWithFormat:
          LFLSTR(@"Do you really want to remove the plugin '%@'?"),
          [selectedPlugin
              valueForKey:OVServiceLoadedModulePackageLocalizedNameKey]];
  NSAlert *alert = [NSAlert alertWithMessageText:message
                                   defaultButton:LFLSTR(@"Remove")
                                 alternateButton:LFLSTR(@"Cancel")
                                     otherButton:nil
                       informativeTextWithFormat:@""];
  [alert beginSheetModalForWindow:_window
                    modalDelegate:self
                   didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                      contextInfo:selectedPlugin];
}
- (void)setUIEnabled:(BOOL)enabled {
  [_tableView setEnabled:enabled];
  [_removeButton setEnabled:enabled];
  if (enabled) {
    [self hideProgressIndicator];
  } else {
    [self showProgressIndicator];
  }
}
- (void)showProgressIndicator {
  [_progressIndicator setHidden:NO];
  [_progressIndicator startAnimation:self];
  [_progressLabel setHidden:NO];
}
- (void)hideProgressIndicator {
  [_progressIndicator setHidden:YES];
  [_progressIndicator stopAnimation:self];
  [_progressLabel setHidden:YES];
}

#pragma mark -
#pragma mark NSTableView datasource and delegate

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
#else
- (int)numberOfRowsInTableView:(NSTableView *)aTableView
#endif
{
  return [_modules count];
}

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
- (void)tableView:(NSTableView *)aTableView
    setObjectValue:(id)anObject
    forTableColumn:(NSTableColumn *)aTableColumn
               row:(NSInteger)rowIndex
#else
- (void)tableView:(NSTableView *)aTableView
    setObjectValue:(id)anObject
    forTableColumn:(NSTableColumn *)aTableColumn
               row:(int)rowIndex
#endif
{
  if ([[aTableColumn identifier] isEqualToString:@"enabled"]) {
    [[_modules objectAtIndex:rowIndex]
        setValue:anObject
          forKey:OVServiceLoadedModulePackageEnabledKey];
    NSEnumerator *e = [_modules objectEnumerator];
    NSDictionary *d = nil;
    NSMutableArray *a = [NSMutableArray array];
    while (d = [e nextObject]) {
      if (![[d valueForKey:OVServiceLoadedModulePackageEnabledKey] boolValue]) {
        [a addObject:[d valueForKey:OVServiceLoadedModulePackageIdentifierKey]];
      }
    }
    id ovService;
    @try {
      ovService = [NSConnection rootProxyForConnectionWithRegisteredName:
                                    OPENVANILLA_DO_CONNECTION_NAME
                                                                    host:nil];
      if (ovService) {
        [ovService setBlackListOfPackageIdentifers:a];
      }
    } @catch (NSException *e) {
      // NSLog(@"Exceptions raise on retreiving version info");
      // return;
    }
  }
  [self reloadData];
  [self reloadServer];
}

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
- (id)tableView:(NSTableView *)aTableView
    objectValueForTableColumn:(NSTableColumn *)aTableColumn
                          row:(NSInteger)rowIndex
#else
- (id)tableView:(NSTableView *)aTableView
    objectValueForTableColumn:(NSTableColumn *)aTableColumn
                          row:(int)rowIndex
#endif
{
  if ([[aTableColumn identifier] isEqualToString:@"name"]) {
    return [[_modules objectAtIndex:rowIndex]
        valueForKey:OVServiceLoadedModulePackageLocalizedNameKey];
  } else if ([[aTableColumn identifier] isEqualToString:@"enabled"]) {
    return [[_modules objectAtIndex:rowIndex]
        valueForKey:OVServiceLoadedModulePackageEnabledKey];
  }
  return nil;
}

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification {
  NSTableView *tableView = [aNotification object];
  int selectedRow = [tableView selectedRow];
  if (selectedRow < 0) {
    [self setContentView:nil];
    [_pluginView setFrame:NSMakeRect(0, [_pluginView bounds].size.height - 230,
                                     480, 230)];
  } else {
    id controller =
        [[_modules objectAtIndex:selectedRow] valueForKey:@"controller"];
    if (controller && [controller respondsToSelector:@selector(view)] &&
        [controller view]) {
      if ([controller respondsToSelector:@selector(update)]) {
        [controller update];
      }
      [self setContentView:[controller view]];
      [_pluginView
          setFrame:NSMakeRect(0, [_pluginView bounds].size.height - 348, 480,
                              348)];
    } else {
      [self setContentView:nil];
      [_pluginView
          setFrame:NSMakeRect(0, [_pluginView bounds].size.height - 230, 480,
                              230)];
    }
  }
  [[NSApp delegate] setActiveView:_pluginView animate:YES];
  [_pluginView setNeedsDisplay:YES];
}

@end
