// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface HelpController : NSWindowController <WKScriptMessageHandler> {
  IBOutlet NSView *_webViewContainer;
  WKWebView *_webView;
}
- (void)openInternationPref;
- (void)logout;
- (IBAction)logout:(id)sender;

@end
