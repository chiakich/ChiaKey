// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface HelpController : NSWindowController {
  IBOutlet WebView *_webview;
}
- (void)openInternationPref;
- (void)logout;
- (IBAction)logout:(id)sender;

@end
