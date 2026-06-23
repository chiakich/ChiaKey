// [AUTO_HEADER]

#import "CVNotifyController.h"

@implementation CVNotifyController

int c_count;
NSPoint c_lastLocation;

#pragma mark Class methods

+ (void)notify:(NSString *)message {
  CVNotifyController *c = [[CVNotifyController alloc] init];
  [c setMessage:message];
  [c setShouldStay:NO];
  [c showNotifyWindow];
}
+ (void)notifyAndStay:(NSString *)message {
  CVNotifyController *c = [[CVNotifyController alloc] init];
  [c setMessage:message];
  [c setShouldStay:YES];
  [c showNotifyWindow];
}
+ (void)addInstanceCount {
  c_count++;
}
+ (void)removeInstanceCount {
  c_count--;
  if (c_count < 0) c_count = 0;
}
+ (void)resetCount {
  c_count = 0;
}
+ (int)countNotifyWindows {
  return c_count;
}
+ (void)setLastLocation:(NSPoint)location {
  c_lastLocation = location;
}
+ (NSPoint)lastLocation {
  return c_lastLocation;
}

#pragma mark Instance methods

- (void)dealloc {
  [_foregroundColor release];
  [_backgroundColor release];
  [super dealloc];
}
- (id)init {
  if (self = [super init]) {
    BOOL loaded = [[NSBundle mainBundle] loadNibNamed:@"NotifyWindow" owner:self topLevelObjects:nil];
    NSAssert((loaded == YES), @"NIB did not load");
  }
  return self;
}
- (void)awakeFromNib {
  _backgroundColor = [[NSColor blackColor] retain];
  _foregroundColor = [[NSColor whiteColor] retain];
  [[[self window] contentView] setBackgroundFillColor:_backgroundColor];
  [[[self window] contentView] setBorderColor:_foregroundColor];
  [_messageTextField setTextColor:_foregroundColor];

  NSRect screenRect = [[NSScreen mainScreen] visibleFrame];
  NSRect windowRect = [[self window] frame];
  float x =
      screenRect.origin.x + screenRect.size.width - windowRect.size.width - 10;
  float y = screenRect.origin.y + screenRect.size.height -
            windowRect.size.height - 10;
  float w = windowRect.size.width;
  float h = windowRect.size.height;

  [[self window] setFrame:NSMakeRect(x, y, w, h) display:YES];
}
- (NSString *)message {
  return [_messageTextField stringValue];
}
- (void)setMessage:(NSString *)message {
  NSMutableParagraphStyle *paragraphStyle =
      [[NSMutableParagraphStyle new] autorelease];
  [paragraphStyle setAlignment:NSTextAlignmentCenter];
  NSDictionary *attributes = [NSDictionary
      dictionaryWithObjectsAndKeys:[NSFont
                                       systemFontOfSize:[NSFont
                                                            systemFontSize]],
                                   NSFontAttributeName, paragraphStyle,
                                   NSParagraphStyleAttributeName,
                                   [NSColor whiteColor],
                                   NSForegroundColorAttributeName, nil];
  NSAttributedString *a =
      [[NSAttributedString alloc] initWithString:message attributes:attributes];
  [_messageTextField setAttributedStringValue:a];
  float width = [_messageTextField bounds].size.width;
  NSRect rect = [a boundingRectWithSize:NSMakeSize(width, 1600)
                                options:NSStringDrawingUsesLineFragmentOrigin];
  float height = rect.size.height;
  float x = [_messageTextField frame].origin.x;
  float y = ([[self window] frame].size.height - height) / 2;
  NSRect newFrame = NSMakeRect(x, y, width, height);
  [_messageTextField setFrame:newFrame];
}
- (NSColor *)textColor {
  return [_messageTextField textColor];
}
- (void)setTextColor:(NSColor *)aColor {
  [_messageTextField setTextColor:aColor];
}
- (BOOL)shouldStay {
  return _shouldStay;
}
- (void)setShouldStay:(BOOL)shouldStay {
  _shouldStay = shouldStay;
}
- (void)setStartLocation {
  if ([CVNotifyController countNotifyWindows] > 0) {
    NSPoint p = [CVNotifyController lastLocation];
    NSRect screenRect = [[NSScreen mainScreen] visibleFrame];
    NSRect windowRect = [[self window] frame];

    float x = p.x;
    float y = p.y - windowRect.size.height - 10;

    if (y < screenRect.origin.y) return;

    float w = windowRect.size.width;
    float h = windowRect.size.height;
    [[self window] setFrame:NSMakeRect(x, y, w, h) display:YES];
  }
}
- (void)moveIn {
  NSRect afterRect = [[self window] frame];
  NSPoint p = afterRect.origin;
  [CVNotifyController setLastLocation:p];

  NSRect beforeRect = afterRect;
  beforeRect.origin.y += 10;
  [[self window] setFrame:beforeRect display:YES];
  [[self window] orderFront:self];
  [[self window] setFrame:afterRect display:YES animate:YES];
}
- (void)showNotifyWindow {
  [[[self window] contentView] setNeedsDisplay:YES];
  [self setStartLocation];
  [self moveIn];
  [CVNotifyController addInstanceCount];
  if (_shouldStay)
    _waitTimer =
        [NSTimer scheduledTimerWithTimeInterval:5
                                         target:self
                                       selector:@selector(fadeNotifyWindow)
                                       userInfo:nil
                                        repeats:NO];
  else
    _waitTimer =
        [NSTimer scheduledTimerWithTimeInterval:1
                                         target:self
                                       selector:@selector(fadeNotifyWindow)
                                       userInfo:nil
                                        repeats:NO];
}
- (void)doFade {
  float opacity = [[self window] alphaValue];
  opacity = opacity - 0.2;
  if (opacity <= 0)
    [self close];
  else
    [[self window] setAlphaValue:opacity];
}
- (void)fadeNotifyWindow {
  [_waitTimer invalidate];
  [CVNotifyController removeInstanceCount];
  _fadeTimer = [NSTimer scheduledTimerWithTimeInterval:0.1
                                                target:self
                                              selector:@selector(doFade)
                                              userInfo:nil
                                               repeats:YES];
}
- (void)close {
  [_fadeTimer invalidate];
  [[self window] orderOut:self];
  [self autorelease];
}
- (IBAction)fade {
  [CVNotifyController removeInstanceCount];
  _fadeTimer = [NSTimer scheduledTimerWithTimeInterval:0.1
                                                target:self
                                              selector:@selector(doFade)
                                              userInfo:nil
                                               repeats:YES];
}
@end
