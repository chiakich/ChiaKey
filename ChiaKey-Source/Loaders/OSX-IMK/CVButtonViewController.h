// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

@interface CVButtonViewController : NSObject {
  NSView *_view;
  NSMutableArray *_buttonArray;
}
- (id)initWithDictionary:(NSDictionary *)d;
- (NSView *)view;
- (IBAction)sendString:(id)sender;
@end
