// [AUTO_HEADER]

#import "CVSearchController.h"

@implementation CVSearchTextField

- (void)awakeFromNib {
  _readingBegin = 0;
  _readingLength = 0;
  _cursorIndex = 0;
}
- (void)timerFireMethod:(NSTimer *)theTimer {
  _cursorIsVisible = !_cursorIsVisible;
  [self setNeedsDisplay:YES];
}
- (void)startAnimatingCursor {
  if (_timer != nil) [_timer invalidate];

  _timer = [NSTimer scheduledTimerWithTimeInterval:0.5
                                            target:self
                                          selector:@selector(timerFireMethod:)
                                          userInfo:nil
                                           repeats:YES];
}
- (void)stopAnimatingCursor {
  if (_timer != nil) {
    [_timer invalidate];
    _timer = nil;
  }
}
- (void)drawRect:(NSRect)aRect {
  [super drawRect:aRect];

  NSMutableAttributedString *cursorString = [[[NSMutableAttributedString alloc]
      initWithAttributedString:[self attributedStringValue]] autorelease];

  if (_cursorIndex > [cursorString length])
    _cursorIndex = (int)[cursorString length];

  // draw the cursor
  [cursorString
      deleteCharactersInRange:NSMakeRange(_cursorIndex, [cursorString length] -
                                                            _cursorIndex)];
  NSRect rect;
  float h = aRect.size.height;
  rect =
      [cursorString boundingRectWithSize:NSMakeSize(1600, h)
                                 options:NSStringDrawingUsesLineFragmentOrigin];
  float w = rect.size.width;

  if (_cursorIsVisible) {
    [NSGraphicsContext saveGraphicsState];
    [[NSGraphicsContext currentContext] setShouldAntialias:NO];
    _cursorPosition = NSMakePoint(w + 5, aRect.size.height - 6);

    NSBezierPath *cursorPath = [NSBezierPath bezierPath];
    [cursorPath moveToPoint:NSMakePoint(w + 5, 6)];
    [cursorPath lineToPoint:NSMakePoint(w + 5, aRect.size.height - 6)];
    [cursorPath closePath];
    [cursorPath setLineWidth:1.0];
    [[NSColor blackColor] setStroke];
    [cursorPath stroke];
    [NSGraphicsContext restoreGraphicsState];
  }

  if (_readingLength < 1) return;
  NSMutableAttributedString *stringBeforeReading =
      [[[NSMutableAttributedString alloc]
          initWithAttributedString:[self attributedStringValue]] autorelease];
  if (_readingBegin + _readingLength > [stringBeforeReading length]) return;
  [stringBeforeReading
      deleteCharactersInRange:NSMakeRange(_readingBegin,
                                          [stringBeforeReading length] -
                                              _readingBegin)];
  NSMutableAttributedString *stringBeforeEndOfReading =
      [[[NSMutableAttributedString alloc]
          initWithAttributedString:[self attributedStringValue]] autorelease];
  [stringBeforeEndOfReading
      deleteCharactersInRange:NSMakeRange(
                                  _readingBegin + _readingLength,
                                  [stringBeforeEndOfReading length] -
                                      (_readingBegin + _readingLength))];
  rect = [stringBeforeReading
      boundingRectWithSize:NSMakeSize(1600, h)
                   options:NSStringDrawingUsesLineFragmentOrigin];
  float widthOfStringBeforeReading = rect.size.width;
  rect = [stringBeforeEndOfReading
      boundingRectWithSize:NSMakeSize(1600, h)
                   options:NSStringDrawingUsesLineFragmentOrigin];
  float widthOfStringBeforeEndOfReading = rect.size.width;

  NSBezierPath *underlinePath = [NSBezierPath bezierPath];
  [underlinePath moveToPoint:NSMakePoint(widthOfStringBeforeReading + 5,
                                         aRect.size.height - 5)];
  [underlinePath lineToPoint:NSMakePoint(widthOfStringBeforeEndOfReading + 5,
                                         aRect.size.height - 5)];
  [underlinePath closePath];
  [underlinePath setLineWidth:2.0];
  [[NSColor grayColor] setStroke];
  [underlinePath stroke];
}
- (void)setCursorIndex:(int)newIndex;
{
  _cursorIndex = newIndex;
  _cursorIsVisible = YES;
}
- (int)cursorIndex {
  return _cursorIndex;
}
- (void)setReadingFrom:(int)begin length:(int)length {
  _readingBegin = begin;
  _readingLength = length;
}
- (NSPoint)cursorPosition {
  return _cursorPosition;
}

@end

@implementation CVSearchController
- (id)init {
  self = [super init];
  if (self != nil) {
    BOOL loaded = [[NSBundle mainBundle] loadNibNamed:@"SearchWindow" owner:self topLevelObjects:nil];
    NSAssert((loaded == YES), @"NIB did not load");
  }
  return self;
}
- (void)awakeFromNib {
  _bufferHeight = [_bufferTextField frame].size.height;
  _promptDecriptionLineHeight = [_promptDescriptionTextField frame].size.height;
  [[self window] setAlphaValue:0.9];
  [_promptTextField setTextColor:[NSColor whiteColor]];
  [_promptDescriptionTextField setTextColor:[NSColor whiteColor]];
  [[[self window] contentView] setBackgroundFillColor:[NSColor blackColor]];
  [[[self window] contentView] setBorderColor:[NSColor whiteColor]];
}
- (void)setCursorIndex:(int)newIndex {
  [_bufferTextField setCursorIndex:newIndex];
}
- (int)cursorIndex {
  return [_bufferTextField cursorIndex];
}
- (void)setReadingFrom:(int)from length:(int)length {
  [_bufferTextField setReadingFrom:from length:length];
}
- (NSPoint)cursorPosition {
  NSPoint bufferPoint = [_bufferTextField cursorPosition];
  float x = [[self window] frame].origin.x + [_bufferTextField frame].origin.x +
            bufferPoint.x;
  float y = [[self window] frame].origin.y + [_bufferTextField frame].origin.y;
  NSPoint cursorPosition = NSMakePoint(x, y);
  return cursorPosition;
}
- (void)autoSize {
  NSAttributedString *c;
  NSRect rect;
  float h;
  c = [_promptDescriptionTextField attributedStringValue];
  NSUInteger lines = [[[c string] componentsSeparatedByString:@"\n"] count];
  h = _promptDecriptionLineHeight * lines;
  rect = [c boundingRectWithSize:NSMakeSize(1600, h)
                         options:NSStringDrawingUsesLineFragmentOrigin];

  NSRect newPromptDescriptionRect = [_promptDescriptionTextField frame];
  newPromptDescriptionRect.size.width = rect.size.width + 10;
  newPromptDescriptionRect.size.height = h;

  c = [_bufferTextField attributedStringValue];
  rect = [c boundingRectWithSize:NSMakeSize(1600, _bufferHeight)
                         options:NSStringDrawingUsesLineFragmentOrigin];
  NSRect newBufferRect = [_bufferTextField frame];
  newBufferRect.size.width = rect.size.width + 10;
  newBufferRect.size.height = _bufferHeight + 14;
  newBufferRect.origin.y = newPromptDescriptionRect.origin.y +
                           newPromptDescriptionRect.size.height + 5;

  c = [_promptTextField attributedStringValue];
  //	h = [_promptTextField frame].size.height;
  rect = [c boundingRectWithSize:NSMakeSize(1600, _bufferHeight)
                         options:NSStringDrawingUsesLineFragmentOrigin];
  NSRect newPromptRect = [_promptTextField frame];
  newPromptRect.size.width = rect.size.width + 10;
  newPromptRect.origin.y =
      newBufferRect.origin.y + newBufferRect.size.height + 5;

  int w = newBufferRect.size.width;
  if (newPromptRect.size.width > w) w = newPromptRect.size.width;
  if (newPromptDescriptionRect.size.width > w)
    w = newPromptDescriptionRect.size.width;

  newBufferRect.size.width = w;
  newPromptRect.size.width = w;
  newPromptDescriptionRect.size.width = w;

  NSRect newWindowRect = [[self window] frame];
  newWindowRect.size.width = w + 16;

  h = newPromptRect.origin.y + newPromptRect.size.height + 10;
  newWindowRect.size.height = h;

  [[self window] setFrame:newWindowRect display:YES];
  [_promptDescriptionTextField setFrame:newPromptDescriptionRect];
  [_promptDescriptionTextField setNeedsDisplay:YES];
  [_bufferTextField setFrame:newBufferRect];
  [_bufferTextField setNeedsDisplay:YES];
  [_promptTextField setFrame:newPromptRect];
  [_promptTextField setNeedsDisplay:YES];
  [[[self window] contentView] setNeedsDisplay:YES];
}
- (void)autoLocation:(NSPoint)point {
  if (point.y == 0 && point.x == 0) return;
  point.y -= [_bufferTextField frame].origin.y + 10;
  point.x -= [_bufferTextField frame].origin.x;
  [[self window] setFrameOrigin:point];
}
- (void)setBufferHeight:(float)newHeight {
  if (newHeight < 12.0) newHeight = 12.0;
  NSFont *font = [_bufferTextField font];
  font = [NSFont fontWithName:[font fontName] size:newHeight];
  [_bufferTextField setFont:font];
  _bufferHeight = newHeight;
}
- (float)bufferHeight {
  return [_bufferTextField frame].size.height;
}
- (void)setPrompt:(NSString *)prompt {
  [_promptTextField setStringValue:prompt];
}
- (NSString *)prompt {
  return [_promptTextField stringValue];
}
- (void)setBuffer:(NSString *)buffer {
  [_bufferTextField setStringValue:buffer];
}
- (NSString *)buffer {
  return [_bufferTextField stringValue];
}
- (void)setPromptDescription:(NSString *)promptDescription {
  [_promptDescriptionTextField setStringValue:promptDescription];
}
- (NSString *)promptDesciption {
  return [_promptDescriptionTextField stringValue];
}
- (void)showWithPrompt:(NSString *)prompt
     promptDescription:(NSString *)promptDescription
                buffer:(NSString *)buffer
                 point:(NSPoint)point
           readingFrom:(int)from
         readingLength:(int)length
           cursorIndex:(int)cursor {
  [self setPrompt:prompt];
  [self setPromptDescription:promptDescription];
  [self setBuffer:buffer];
  [self setReadingFrom:from length:length];
  [self setCursorIndex:cursor];
  [self autoSize];
  [self autoLocation:point];
  [self show];
}
- (void)showWithPrompt:(NSString *)prompt
     promptDescription:(NSString *)promptDescription
                buffer:(NSString *)buffer
           readingFrom:(int)from
         readingLength:(int)length
           cursorIndex:(int)cursor {
  [self setPrompt:prompt];
  [self setPromptDescription:promptDescription];
  [self setBuffer:buffer];
  [self setReadingFrom:from length:length];
  [self setCursorIndex:cursor];
  [self autoSize];
  [[self window] center];
  [self show];
}
- (void)show {
  [[self window] orderFront:self];
  [_bufferTextField setNeedsDisplay:YES];
  [_bufferTextField startAnimatingCursor];
}
- (void)hide {
  [[self window] orderOut:self];
  [_bufferTextField stopAnimatingCursor];
}

@end
