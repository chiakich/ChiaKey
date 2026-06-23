// [AUTO_HEADER]

#import "HelpController.h"

@implementation HelpController

- (void)awakeFromNib {
  [[self window] setDelegate:(id)self];
  [[self window] setLevel:NSFloatingWindowLevel];
  [[self window] center];

  [_webview setUIDelegate:(id)self];
  [_webview setFrameLoadDelegate:(id)self];

  NSString *htmlPath = [[NSBundle mainBundle] pathForResource:@"install"
                                                       ofType:@"html"];
  // Load the html file of the help document.
  if ([htmlPath length]) {
    NSURL *url = [NSURL fileURLWithPath:htmlPath];
    NSURLRequest *request = [NSURLRequest requestWithURL:url];
    [[_webview mainFrame] loadRequest:request];
  }
}

- (void)openInternationPref {
  NSString *scriptSource =
      @"tell application \"System Preferences\"\n activate\n set the current "
      @"pane to pane id \"com.apple.Localization\"\nend tell";
  NSAppleScript *script =
      [[[NSAppleScript alloc] initWithSource:scriptSource] autorelease];
  [script executeAndReturnError:nil];
}
- (void)logout {
  NSString *scriptSource = @"tell application \"System Events\" to log out";
  NSAppleScript *script =
      [[[NSAppleScript alloc] initWithSource:scriptSource] autorelease];
  [script executeAndReturnError:nil];
}
- (IBAction)logout:(id)sender {
  [self logout];
  [[NSApplication sharedApplication] terminate:self];
}

#pragma mark NSWindow delegate

- (void)windowWillClose:(NSNotification *)notification {
  [[NSApplication sharedApplication] terminate:self];
}

#pragma mark WebView delegate

// Ignores all drag and drop into the web view.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
- (NSUInteger)webView:(WebView *)sender
    dragDestinationActionMaskForDraggingInfo:(id<NSDraggingInfo>)draggingInfo
#else
- (int)webView:(WebView *)sender
    dragDestinationActionMaskForDraggingInfo:(id<NSDraggingInfo>)draggingInfo
#endif
{
  return WebDragDestinationActionNone;
}
// Disables dragging links, allows only dragging images and selected text.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
- (NSUInteger)webView:(WebView *)sender
    dragSourceActionMaskForPoint:(NSPoint)point
#else
- (int)webView:(WebView *)sender
    dragSourceActionMaskForPoint:(NSPoint)point
#endif
{
  return WebDragSourceActionNone;
}

- (void)webView:(WebView *)sender
    windowScriptObjectAvailable:(WebScriptObject *)windowScriptObject {
  id scriptObject = windowScriptObject;
  [scriptObject setValue:self forKey:@"HelpController"];
}

- (NSArray *)webView:(WebView *)sender
    contextMenuItemsForElement:(NSDictionary *)element
              defaultMenuItems:(NSArray *)defaultMenuItems {
  NSMutableArray *menuItems = [NSMutableArray array];
  NSEnumerator *enumerator = [defaultMenuItems objectEnumerator];
  NSMenuItem *aMenuItem;

  while (aMenuItem = [enumerator nextObject]) {
    int tag = [aMenuItem tag];
    if (tag != WebMenuItemTagReload &&
        tag != WebMenuItemTagOpenLinkInNewWindow &&
        tag != WebMenuItemTagDownloadLinkToDisk &&
        tag != WebMenuItemTagCopyLinkToClipboard &&
        tag != WebMenuItemTagOpenFrameInNewWindow &&
        tag != WebMenuItemTagGoBack && tag != WebMenuItemTagGoForward &&
        tag != WebMenuItemTagStop) {
      [menuItems addObject:aMenuItem];
    }
  }
  return menuItems;
}

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)selector {
  if (selector == @selector(logout)) {
    return NO;
  }
  if (selector == @selector(openInternationPref)) {
    return NO;
  }
  return YES;
}

@end
