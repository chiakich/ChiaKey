// [AUTO_HEADER]

#import "CVDictionaryController+Orz.h"

@implementation CVDictionaryController (Orz)

- (void)showOrzSong {
  NSMutableString *html = [NSMutableString string];
  [html appendString:BASEHTML];
  [html
      appendString:
          [NSString
              stringWithUTF8String:
                  "	<h2>orz 之歌</h2><p>作者：<a "
                  "href=\"http://zonble.twbbs.org/\">zonble</a></p><p><a "
                  "href=\"http://zonble.twbbs.org/MIDI/orz.mp3\">MP3</a></"
                  "p><p>小姐啊小姐，妳真是美麗。<br "
                  "/>\n烏溜溜的秀髮，水汪汪的眼睛。<br "
                  "/>\n只要一想到妳，我滿心都XD。</"
                  "p>\n<p>今天啊今天，我鼓起勇氣。<br "
                  "/>\n到底心歸何處，我想要問問妳。<br />\n但妳的答案，真讓我 "
                  "&gt;_&lt;。</p>\n<p>說我是好人，妳說真是對不起。<br "
                  "/>\n說我是好人，就宣判我的命運。<br "
                  "/>\n說我是好人，我挫折俯跪不起。<br "
                  "/>\n說我是好人，我只能orz…。</p>"]];
  [html appendString:@"</body></html>"];
  [self refreshContent:html];
  [self showWindow:self];
  _isVisible = YES;
  [_loadingProgressIndicator setHidden:YES];
}

@end
