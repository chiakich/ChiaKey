// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "TakaoWordCount.h"

@interface CVAboutController : NSWindowController {
  IBOutlet id _aboutTextField;
  BOOL init;
  TakaoWordCount *_wordCountController;
  NSSize defaultWindowSize;
}

#pragma mark Interface Builder actions

- (IBAction)showWindow:(id)sender;
- (IBAction)launchCustomerCare:(id)sender;

@end
