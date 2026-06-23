// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#import "LFUtilities.h"

#define BASEHTML                                                               \
  [NSString                                                                    \
      stringWithUTF8String:                                                    \
          "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" "  \
          "\"http://www.w3.org/TR/xhtml1/DTD/"                                 \
          "xhtml1-transitional.dtd\"><html><head><meta "                       \
          "http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" "  \
          "/><style type='text/css'>\nimg {border:0;}\nbody { font-size: "     \
          "10pt; font-family: Helvetica, sans-serif; line-height: 1.2em; "     \
          "padding: 0; margin: 10px 20px;}\n a, a:visited {text-decoration: "  \
          "none; color: blue;}\n a:hover {text-decoration: underline;} b "     \
          "{font-weight: bold; color: darkred;}\n p {margin: 0.5em 0;}\n "     \
          "blockquote {border: 1px solid #CCC; margin: 0; padding: 5px; "      \
          "margin-bottom: 10px; margin-left: 20px;}\n blockquote p {margin: "  \
          "0.2em 0;}\n h2 {font-family: Times, Times New Roman, serif; "       \
          "font-weight: normal; font-size: 20pt; border-bottom: 1px solid "    \
          "#CCC; color: 000; line-height: 1.5em;}\n h2 a, h2 a:visited, h2 "   \
          "a:hover {color: #333; text-decoration: none;}\n h3 {font-size: "    \
          "12pt; }\n .credit {margin-top: 20px; color: #999; font-size: 8pt; " \
          "line-height: 1.2em;} @media screen { div{display: none;}\n "        \
          "}@media print { div{display: black;}\nobject {display: "            \
          "none;}\nembed {display:none;}\nbody {font-family: Times, Times "    \
          "New Roman, serif;}}</style>\n<script "                              \
          "type='text/javascript'>\nfunction doSearch(keyword) {\n var d = "   \
          "window.DictionaryController; \n if (d) { d.search(keyword); }\n "   \
          "return false;}\nfunction speak(text) {\n var d = "                  \
          "window.DictionaryController; \n if (d) { d.speak(text); }\n "       \
          "return false;}\nfunction sendString(string) {\n var d = "           \
          "window.DictionaryController; \n if (d) { d.sendString(string); "    \
          "}\n return false;}\nfunction toggle(link, divName) { d = "          \
          "document.getElementById(divName); if (d.style.display == "          \
          "\"none\") { d.style.display = \"block\"; link.innerHTML = "         \
          "\"折疊\"; } else { d.style.display = \"none\"; link.innerHTML = " \
          "\"展開\";}  return false;}</script>\n<script "                    \
          "type=\"text/javascript\" "                                          \
          "src=\"http://del.icio.us/js/playtagger\"></script></"               \
          "head>\n<body>\n"]
#define COPYRIGHT                                                            \
  LFLSTR(                                                                    \
      [NSString stringWithUTF8String:"<p class=\"credit\">2008-2010 Yahoo! " \
                                     "Taiwan All Rights Reserved.</p>"])

@interface CVDictionaryController : NSWindowController {
  IBOutlet id _webview;
  IBOutlet id _loadingProgressIndicator;
  IBOutlet id _changeSizeSegmentedControl;
  BOOL _isVisible;

  NSString *_beaconURL;
  NSString *_beaconHTML;
}
- (void)temporaryHide;
- (void)restoreWindowStatus;
- (BOOL)isVisible;
- (void)refreshContent:(NSString *)html;
- (void)search:(NSString *)keyword;

#pragma mark Internface Builder actions

- (IBAction)hide:(id)sender;
- (IBAction)changeTextSize:(id)sender;
@end
