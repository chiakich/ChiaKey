// [AUTO_HEADER]

#import "TrackerSender.h"

#if (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5)
typedef unsigned int NSUInteger;
typedef int NSInteger;
#endif

#import "LFHTTPRequest.h"

TrackerSender *SharedTrackerSender = nil;

@implementation TrackerSender
+ (TrackerSender *)sharedTrackerSender {
  return SharedTrackerSender
             ? SharedTrackerSender
             : (SharedTrackerSender = [[TrackerSender alloc] init]);
}
- (void)sendTrackerWithURLString:(NSString *)urlString {
  LFHTTPRequest *request = [[LFHTTPRequest alloc] init];
  [request setDelegate:(id)self];

  [request setUserAgent:@"ChiakiKeyKey/2026.06 (macOS)"];
  BOOL result = [request performMethod:@"GET"
                                 onURL:[NSURL URLWithString:urlString]
                              withData:nil];
}
- (void)httpRequest:(LFHTTPRequest *)request
    didReceiveStatusCode:(NSUInteger)statusCode
                     URL:(NSURL *)url
          responseHeader:(CFHTTPMessageRef)header {
}
- (void)httpRequestDidComplete:(LFHTTPRequest *)request {
  [request autorelease];
}
- (void)httpRequest:(LFHTTPRequest *)request
    didFailWithError:(NSString *)error {
  [request autorelease];
}
@end
