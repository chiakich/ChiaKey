// [AUTO_HEADER]

#import "HelpController.h"

@implementation HelpController

- (void)awakeFromNib {
  [[self window] setDelegate:(id)self];
  [[self window] setLevel:NSFloatingWindowLevel];
  [[self window] center];

  WKUserContentController *userContentController =
      [[[WKUserContentController alloc] init] autorelease];
  [userContentController addScriptMessageHandler:self name:@"HelpController"];

  WKWebViewConfiguration *configuration =
      [[[WKWebViewConfiguration alloc] init] autorelease];
  [configuration setUserContentController:userContentController];

  _webView = [[WKWebView alloc] initWithFrame:[_webViewContainer bounds]
                                configuration:configuration];
  [_webView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [_webView unregisterDraggedTypes];
  [_webViewContainer addSubview:_webView];

  NSString *htmlPath = [[NSBundle mainBundle] pathForResource:@"install"
                                                       ofType:@"html"];
  // Load the html file of the help document.
  if ([htmlPath length]) {
    NSURL *url = [NSURL fileURLWithPath:htmlPath];
    NSURL *directoryURL = [url URLByDeletingLastPathComponent];
    [_webView loadFileURL:url allowingReadAccessToURL:directoryURL];
  }
}

- (void)dealloc {
  [[[_webView configuration] userContentController]
      removeScriptMessageHandlerForName:@"HelpController"];
  [_webView release];
  [super dealloc];
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

#pragma mark WKScriptMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController
      didReceiveScriptMessage:(WKScriptMessage *)message {
  if (![[message name] isEqualToString:@"HelpController"]) {
    return;
  }

  if ([[message body] isEqual:@"openInternationPref"]) {
    [self openInternationPref];
    return;
  }

  if ([[message body] isEqual:@"logout"]) {
    [self logout];
  }
}

@end
