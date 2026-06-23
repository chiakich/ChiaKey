// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "LFUtilities.h"

@interface NSString (FallbackableLocalizedStringValue)
- (NSString *)fallbackableLocalizedStringValueForLocale:(NSString *)locale;
@end

@interface NSDictionary (FallbackableLocalizedStringValue)
- (NSString *)fallbackableLocalizedStringValueForLocale:(NSString *)locale;
@end

@interface CVSmileyViewController : NSObject {
  IBOutlet NSView *_view;
  IBOutlet NSTableView *_tableView;
  IBOutlet NSButton *_sendButton;
  NSArray *_array;
}

- (id)initWithDictionary:(NSDictionary *)d;
- (NSView *)view;

- (IBAction)sendStringFromTableView:(id)sender;
- (IBAction)sendString:(id)sender;

@end
