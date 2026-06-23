// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "CVFloatingBackground.h"
#import "CVFloatingWindow.h"
#import "CVNotifyWindow.h"

@interface CVNotifyController : NSWindowController {
  IBOutlet NSTextField *_messageTextField;

  BOOL _shouldStay;
  NSColor *_backgroundColor;
  NSColor *_foregroundColor;
  NSTimer *_waitTimer;
  NSTimer *_fadeTimer;
}

#pragma mark Class Methods
+ (void)notify:(NSString *)message;
+ (void)notifyAndStay:(NSString *)message;
+ (void)addInstanceCount;
+ (void)removeInstanceCount;
+ (void)resetCount;
+ (int)countNotifyWindows;
+ (void)setLastLocation:(NSPoint)location;
+ (NSPoint)lastLocation;

#pragma mark Instance Methods
//- (CVFloatingWindow *)window;
- (NSString *)message;
- (void)setMessage:(NSString *)message;
- (NSColor *)textColor;
- (void)setTextColor:(NSColor *)aColor;
- (BOOL)shouldStay;
- (void)setShouldStay:(BOOL)flag;
- (void)showNotifyWindow;
- (void)fadeNotifyWindow;
- (void)close;
- (IBAction)fade;
@end
