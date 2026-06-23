// [AUTO_HEADER]

#import "CVPlainTextCandidateController.h"

@implementation CVPlainTextCandidateController
- (id)init {
  self = [super init];
  if (self != nil) {
    BOOL loaded = [[NSBundle mainBundle] loadNibNamed:@"PlainTextCandidateWindow" owner:self topLevelObjects:nil];
    NSAssert((loaded == YES), @"NIB did not load");
  }
  return self;
}
- (void)moveTextWindowToPoint:(NSPoint)point {
  [[self window]
      setFrameOrigin:NSMakePoint(point.x,
                                 point.y - [[self window] frame].size.height)];
}
- (void)hideTextWindow {
  [[self window] orderOut:self];
}
- (void)showTextWindow {
  [[self window] orderFront:self];
}
- (void)updateStringContent:(NSString*)content {
  [(CVBubbleTextWindow*)[self window] setText:content];
  [(CVBubbleTextWindow*)[self window] updateWindowSize];
}
- (void)updateContent:(PVPlainTextCandidatePanel*)panel
              atPoint:(NSPoint)position {
  if (!panel->isVisible()) [self hideTextWindow];

  string content = panel->accessTextStorage()->content();
  [self moveTextWindowToPoint:position];
  [self updateStringContent:[NSString stringWithUTF8String:content.c_str()]];

  if (panel->isVisible()) [self showTextWindow];
}
@end
