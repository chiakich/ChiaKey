// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "CVFloatingBackground.h"

@interface CVSearchTextField : NSTextField {
  int _readingBegin;
  int _readingLength;
  int _cursorIndex;
  NSPoint _cursorPosition;

  BOOL _cursorIsVisible;
  NSTimer *_timer;
}
- (void)setReadingFrom:(int)from length:(int)length;
- (void)setCursorIndex:(int)newIndex;
- (int)cursorIndex;
- (NSPoint)cursorPosition;
- (void)startAnimatingCursor;
- (void)stopAnimatingCursor;
@end

@interface CVSearchController : NSWindowController {
  IBOutlet NSTextField *_promptTextField;
  IBOutlet NSTextField *_promptDescriptionTextField;
  IBOutlet CVSearchTextField *_bufferTextField;

  float _bufferHeight;
  float _promptDecriptionLineHeight;
}
- (void)setCursorIndex:(int)newIndex;
- (int)cursorIndex;
- (NSPoint)cursorPosition;
- (void)setBufferHeight:(float)newHeight;
- (float)bufferHeight;
- (void)setPrompt:(NSString *)prompt;
- (NSString *)prompt;
- (void)setBuffer:(NSString *)buffer;
- (NSString *)buffer;
- (void)setPromptDescription:(NSString *)promptDescription;
- (NSString *)promptDesciption;
- (void)setReadingFrom:(int)from length:(int)length;
- (void)showWithPrompt:(NSString *)prompt
     promptDescription:(NSString *)promptDescription
                buffer:(NSString *)buffer
                 point:(NSPoint)point
           readingFrom:(int)from
         readingLength:(int)length
           cursorIndex:(int)cursor;
- (void)showWithPrompt:(NSString *)prompt
     promptDescription:(NSString *)promptDescription
                buffer:(NSString *)buffer
           readingFrom:(int)from
         readingLength:(int)length
           cursorIndex:(int)cursor;
- (void)show;
- (void)hide;

@end
