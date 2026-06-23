// [AUTO_HEADER]

#import <Foundation/Foundation.h>

@interface TrackerSender : NSObject
+ (TrackerSender *)sharedTrackerSender;
- (void)sendTrackerWithURLString:(NSString *)urlString;
@end
