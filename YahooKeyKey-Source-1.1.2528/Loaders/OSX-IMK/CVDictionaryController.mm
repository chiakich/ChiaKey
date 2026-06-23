// [AUTO_HEADER]

#import "CVDictionaryController.h"

#import "CVDictionaryController+Orz.h"
#import "OpenVanillaController.h"
#import "TrackerMaker.h"

#if (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5)
#warning This is a simple implementation only suited for the usage here
@interface NSString (ReplaceOccurencesReplacement)
- (NSString *)stringByReplacingOccurrencesOfString:(NSString *)target
                                        withString:(NSString *)replacement;
@end

@implementation NSString (ReplaceOccurencesReplacement)
- (NSString *)stringByReplacingOccurrencesOfString:(NSString *)target
                                        withString:(NSString *)replacement {
  UniChar c = [target characterAtIndex:0];
  size_t length = [self length];
  size_t i;

  NSMutableString *muStr = [NSMutableString string];

  for (i = 0; i < length; ++i) {
    UniChar sc = [self characterAtIndex:i];
    if (sc == c)
      [muStr appendString:replacement];
    else
      [muStr appendString:[NSString stringWithCharacters:&sc length:1]];
  }

  return muStr;
}
@end
#endif

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

@interface NSXMLNode (ZBXMLExtension)
- (NSXMLNode *)nodeForXPath:(NSString *)xpath;
@end

@implementation NSXMLNode (ZBXMLExtension)
- (NSXMLNode *)nodeForXPath:(NSString *)xpath {
  NSError *err;
  NSArray *nodes = [self nodesForXPath:xpath error:&err];
  if ([nodes count]) {
    return [nodes objectAtIndex:0];
  }
  return nil;
}
@end

#pragma mark -

@implementation CVDictionaryController

- (id)init {
  self = [super init];
  if (self != nil) {
    BOOL loaded = [NSBundle loadNibNamed:@"DictionaryWindow" owner:self];
    NSAssert((loaded == YES), @"NIB did not load");
  }
  return self;
}

#pragma mark Simple Cache mechanism

// The full path of the folder for caching the fetched XML files.
- (NSString *)_cachesDirectory {
  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory,
                                                       NSUserDomainMask, YES);
  NSString *basePath =
      ([paths count] > 0) ? [paths objectAtIndex:0] : NSTemporaryDirectory();
  return
      [basePath stringByAppendingPathComponent:@"com.chiaki.KeyKey.Dictionary"];
}
// Clean up some stored XML files in the cache folder
// if they are already too old. A XML file will be expired
// after 12 hours.
- (void)_cleanCache {
  NSString *tempPath = [self _cachesDirectory];
  NSArray *files =
      [[NSFileManager defaultManager] directoryContentsAtPath:tempPath];
  if (files && [files count]) {
    NSEnumerator *enumerator = [files objectEnumerator];
    NSString *file;
    while (file = [enumerator nextObject]) {
      NSString *fullPath = [tempPath stringByAppendingPathComponent:file];
      NSDate *createDate = [[[NSFileManager defaultManager]
          fileAttributesAtPath:fullPath
                  traverseLink:YES] objectForKey:NSFileCreationDate];
      NSDate *expireDate =
          [NSDate dateWithTimeIntervalSinceNow:(-12 * 60 * 60)];
      if ([createDate compare:expireDate] == NSOrderedAscending) {
        [[NSFileManager defaultManager] removeFileAtPath:fullPath handler:nil];
      }
    }
  }
}

- (void)refreshContent:(NSString *)html {
  [[_webview mainFrame] loadHTMLString:html baseURL:nil];
  [_loadingProgressIndicator stopAnimation:self];
}

#pragma mark Yahoo! Dictionary XML Parser

- (NSString *)_parseXMLEntry:(NSXMLNode *)entry index:(int)index {
  NSMutableString *entryHTML = [NSMutableString string];
  NSError *err;
  BOOL isChineseItem = NO;

  NSString *enterImageLink = [[NSBundle mainBundle] pathForResource:@"enter"
                                                             ofType:@"gif"
                                                        inDirectory:@"WebUI"];
  enterImageLink =
      [NSString stringWithFormat:@"<img src=\"file://%@\" alt=\"enter\" />",
                                 enterImageLink];
  NSString *speakImageLink = [[NSBundle mainBundle] pathForResource:@"speak"
                                                             ofType:@"gif"
                                                        inDirectory:@"WebUI"];
  speakImageLink =
      [NSString stringWithFormat:@"<img src=\"file://%@\" alt=\"speak\" />",
                                 speakImageLink];

  NSXMLNode *titleNode = [entry nodeForXPath:@"./HW"];
  if (titleNode) {
    NSString *title = [titleNode stringValue];
    NSString *searchString;
    NSArray *titleArray = [title componentsSeparatedByString:@"@"];
    if ([titleArray count] > 1) {
      title = [NSString stringWithFormat:@"%@<sup>%@</sup>",
                                         [titleArray objectAtIndex:0],
                                         [titleArray objectAtIndex:1]];
      searchString = [titleArray objectAtIndex:0];
    } else {
      searchString = title;
    }
    searchString = [searchString stringByReplacingOccurrencesOfString:@"'"
                                                           withString:@"\\'"];
    NSString *link =
        [NSString stringWithFormat:
                      @"<a href=\"#\" title=\"%@\" onclick=\"return "
                      @"sendString('%@')\">%@</a>",
                      LFLSTR(@"Enter"), searchString, enterImageLink];
    title = [NSString stringWithFormat:
                          @"<h2><a href=\"#\" onclick=\"return "
                          @"doSearch('%@');\">%@</a> %@</h2>\n",
                          searchString, title, link];
    [entryHTML appendString:title];
  }
  NSXMLNode *soundNode = [entry nodeForXPath:@"./SOUND"];

  if (soundNode) {
    if ([[soundNode stringValue] length]) {
      NSString *mp3URL = [YAHOO_DICTIONARY_URL
          stringByAppendingString:[soundNode stringValue]];
      [entryHTML
          appendFormat:
              @"<p><object "
              @"classid=\"clsid:d27cdb6e-ae6d-11cf-96b8-444553540000\" "
              @"codebase=\"http://download.macromedia.com/pub/shockwave/cabs/"
              @"flash/swflash.cab#version=7,0,0,0\" width=\"152\" "
              @"height=\"21\" id=\"audio_key\" align=\"middle\"><param "
              @"name=\"wmode\" value=\"transparent\"><param "
              @"name=\"allowScriptAccess\" value=\"sameDomain\" /><param "
              @"name=\"movie\" "
              @"value=\"http://tw.yimg.com/i/tw/dictionary/audio_key.swf\" "
              @"/><param name=\"loop\" value=\"false\" /><param name=\"menu\" "
              @"value=\"false\" /><param name=\"quality\" value=\"high\" "
              @"/><param name=\"bgcolor\" value=\"#ffffff\" /><param "
              @"name=\"FlashVars\" value=\"audio=%@\"><embed "
              @"src=\"http://tw.yimg.com/i/tw/dictionary/audio_key.swf\" "
              @"wmode=\"transparent\" loop=\"false\" menu=\"false\" "
              @"quality=\"high\" bgcolor=\"#ffffff\" width=\"152\" "
              @"height=\"21\" name=\"audio_key\" align=\"middle\" "
              @"allowScriptAccess=\"sameDomain\" FlashVars=\"audio=%@\" "
              @"type=\"application/x-shockwave-flash\" "
              @"pluginspage=\"http://www.macromedia.com/go/getflashplayer\" "
              @"/></object></p>",
              mp3URL, mp3URL];
    }
  }

  NSXMLNode *kkNode = [entry nodeForXPath:@"./SYMBOL/KK"];
  if (kkNode) {
    NSString *kkString = [kkNode stringValue];
    if ([kkString length])
      [entryHTML appendFormat:@"<p>KK: [%@]</p>\n", [kkNode stringValue]];
  }

  NSXMLNode *djNode = [entry nodeForXPath:@"./SYMBOL/DJ"];
  if (djNode) {
    NSString *djString = [djNode stringValue];
    if ([djString length])
      [entryHTML appendFormat:@"<p>DJ: [%@]</p>\n", [djNode stringValue]];
  }

  NSXMLNode *pingyinNode = [entry nodeForXPath:@"./SYMBOL/PINYIN"];
  if (pingyinNode)
    [entryHTML appendFormat:@"<p>%@: [%@]</p>\n",
                            [NSString stringWithUTF8String:"拼音"],
                            [pingyinNode stringValue]];

  NSXMLNode *zhuyinNode = [entry nodeForXPath:@"./SYMBOL/ZHUYIN"];
  if (zhuyinNode)
    [entryHTML appendFormat:@"<p>%@: [%@]</p>\n",
                            [NSString stringWithUTF8String:"注音"],
                            [zhuyinNode stringValue]];

  if (pingyinNode || zhuyinNode) isChineseItem = YES;

  NSArray *posArray = [entry nodesForXPath:@"./POS" error:&err];
  NSEnumerator *posEnumerator = [posArray objectEnumerator];
  NSXMLNode *pos;
  while (pos = [posEnumerator nextObject]) {
    NSMutableString *posHTML = [NSMutableString string];
    NSXMLNode *descNode = [pos nodeForXPath:@"./DESC"];
    if (descNode)
      [posHTML appendFormat:@"<h3>%@</h3>\n", [descNode stringValue]];
    NSArray *defArray = [pos nodesForXPath:@"./DEF" error:&err];
    BOOL hasOnlyOneItem = NO;
    if ([defArray count] == 1) hasOnlyOneItem = YES;
    int i = 0;
    NSEnumerator *defEnumerator = [defArray objectEnumerator];
    NSXMLNode *def;
    while (def = [defEnumerator nextObject]) {
      NSString *expl = [[def nodeForXPath:@"./EXPL"] stringValue];
      if (expl) {
        NSString *explString = @"";
        if (isChineseItem) {
          NSArray *explArray = [expl componentsSeparatedByString:@"; "];
          NSEnumerator *explEnumerator = [explArray objectEnumerator];
          NSString *explItem;
          int i = 1;
          while (explItem = [explEnumerator nextObject]) {
            NSString *link = [NSString
                stringWithFormat:
                    @"<a href=\"#\" title=\"%@\" onclick=\"return "
                    @"sendString('%@')\">%@</a>",
                    LFLSTR(@"Enter"),
                    [explItem stringByReplacingOccurrencesOfString:@"'"
                                                        withString:@"\\'"],
                    enterImageLink];
            explString =
                [explString stringByAppendingFormat:@"%@ %@", explItem, link];
            if (i < [explArray count]) {
              explString = [explString stringByAppendingString:@"; "];
            }
            i++;
          }
        } else {
          explString = expl;
        }
        if (hasOnlyOneItem) {
          [posHTML appendFormat:@"<p>%@</p>\n", explString];
        } else {
          i++;
          [posHTML appendFormat:@"<p>%d. %@</p>\n", i, explString];
        }
      }
      NSArray *exampleArray = [def nodesForXPath:@"./EXAMPLE" error:&err];
      NSEnumerator *exampleEnumerator = [exampleArray objectEnumerator];
      NSXMLNode *example;
      while (example = [exampleEnumerator nextObject]) {
        NSString *sentense = [[example nodeForXPath:@"./SENTENCE"] stringValue];
        NSString *speak;
        speak = [sentense stringByReplacingOccurrencesOfString:@"<b>"
                                                    withString:@""];
        speak = [speak stringByReplacingOccurrencesOfString:@"</b>"
                                                 withString:@""];
        speak = [speak stringByReplacingOccurrencesOfString:@"'"
                                                 withString:@"\\'"];
        speak = [speak stringByReplacingOccurrencesOfString:@"\""
                                                 withString:@"\\'"];
        speak = [NSString stringWithFormat:
                              @"<a href=\"#\" title=\"%@\" onclick=\"return "
                              @"speak('%@')\">%@</a>",
                              LFLSTR(@"Speak"), speak, speakImageLink];
        sentense = [sentense stringByAppendingString:speak];
        NSString *trans = [[example nodeForXPath:@"./TRANS"] stringValue];
        [posHTML appendFormat:@"<blockquote><p>%@<br />%@</p></blockquote>\n",
                              sentense, trans];
      }
    }
    [entryHTML appendString:posHTML];
  }

  NSArray *rulArray = [entry nodesForXPath:@"./RUL/DESC" error:&err];
  if ([rulArray count]) {
    [entryHTML appendFormat:@"<h3>%@</h3>\n",
                            [NSString stringWithUTF8String:"詞態變化"]];
    [entryHTML appendFormat:
                   @"<a href=\"#\" class=\"toggle\" onclick=\"return "
                   @"toggle(this, 'rul_%d')\">%@</a>\n",
                   index, [NSString stringWithUTF8String:"展開"]];
    [entryHTML
        appendFormat:@"<div id=\"rul_%d\" style=\"display: none;\">\n", index];
    [entryHTML appendFormat:@"<ul>\n"];
    NSEnumerator *rulEnumerator = [rulArray objectEnumerator];
    NSXMLNode *rul;
    while (rul = [rulEnumerator nextObject]) {
      [entryHTML appendFormat:@"<li>%@</li>\n", [rul stringValue]];
    }
    [entryHTML appendString:@"</ul></div>\n"];
  }
  NSArray *synArray = [entry nodesForXPath:@"./SYN/DEF" error:&err];
  if ([synArray count]) {
    [entryHTML
        appendFormat:@"<h3>%@</h3>", [NSString stringWithUTF8String:"同義詞"]];
    [entryHTML
        appendFormat:
            @"<a href=\"#\" onclick=\"return toggle(this, 'syn_%d')\">%@</a>\n",
            index, [NSString stringWithUTF8String:"展開"]];
    [entryHTML
        appendFormat:
            @"<div id=\"syn_%d\" class=\"toggle\" style=\"display: none;\">\n",
            index];
    NSEnumerator *synEnumerator = [synArray objectEnumerator];
    NSXMLNode *syn;
    while (syn = [synEnumerator nextObject]) {
      NSString *desc = [[syn nodeForXPath:@"./DESC"] stringValue];
      if (desc) {
        [entryHTML appendFormat:@"<p>%@</p>", desc];
      }
      NSArray *synEntryArray = [syn nodesForXPath:@"./ENTRY" error:&err];
      if ([synEntryArray count]) {
        [entryHTML appendString:@"<ul>"];
        NSEnumerator *synEntryEnumerator = [synEntryArray objectEnumerator];
        NSXMLNode *synEntry;
        while (synEntry = [synEntryEnumerator nextObject]) {
          NSString *synString = [synEntry stringValue];
          NSString *searchString;
          NSRange r = [synString rangeOfString:@"<sup>"];
          if (r.location != NSNotFound) {
            searchString = [synString substringToIndex:r.location];
          } else {
            searchString = synString;
          }
          NSString *link = [NSString
              stringWithFormat:
                  @"<a href=\"#\" title=\"%@\" onclick=\"return "
                  @"sendString('%@')\">%@</a>",
                  LFLSTR(@"Enter"),
                  [searchString stringByReplacingOccurrencesOfString:@"'"
                                                          withString:@"\\'"],
                  enterImageLink];
          [entryHTML
              appendFormat:
                  @"<li><a href=\"#\" onclick=\"return "
                  @"doSearch('%@');\">%@</a> %@</li>\n",
                  [searchString stringByReplacingOccurrencesOfString:@"'"
                                                          withString:@"\\'"],
                  synString, link];
        }
        [entryHTML appendString:@"</ul>"];
      }
    }
    [entryHTML appendString:@"</div>"];
  }
  NSArray *syn2Array = [entry nodesForXPath:@"./SYN2/DEF" error:&err];
  if ([syn2Array count]) {
    [entryHTML appendFormat:@"<h3>%@</h3>",
                            [NSString stringWithUTF8String:"同義詞參見"]];
    [entryHTML appendFormat:
                   @"<a href=\"#\" class=\"toggle\" onclick=\"return "
                   @"toggle(this, 'syn2_%d')\">%@</a>\n",
                   index, [NSString stringWithUTF8String:"展開"]];
    [entryHTML
        appendFormat:@"<div id=\"syn2_%d\" style=\"display: none;\">\n", index];

    NSEnumerator *syn2Enumerator = [syn2Array objectEnumerator];
    NSXMLNode *syn;
    while (syn = [syn2Enumerator nextObject]) {
      NSString *desc = [[syn nodeForXPath:@"./DESC"] stringValue];
      if (desc) {
        [entryHTML appendFormat:@"<p>%@</p>", desc];
      }
      NSArray *synEntryArray = [syn nodesForXPath:@"./ENTRY" error:&err];
      if ([synEntryArray count]) {
        [entryHTML appendString:@"<ul>"];
        NSEnumerator *synEntryEnumerator = [synEntryArray objectEnumerator];
        NSXMLNode *synEntry;
        while (synEntry = [synEntryEnumerator nextObject]) {
          NSString *synString = [synEntry stringValue];
          NSString *searchString;
          NSRange r = [synString rangeOfString:@"<sup>"];
          if (r.location != NSNotFound) {
            searchString = [synString substringToIndex:r.location];
          } else {
            searchString = synString;
          }
          NSString *link = [NSString
              stringWithFormat:
                  @"<a href=\"#\" title=\"%@\" onclick=\"return "
                  @"sendString('%@')\">%@</a>",
                  LFLSTR(@"Enter"),
                  [searchString stringByReplacingOccurrencesOfString:@"'"
                                                          withString:@"\\'"],
                  enterImageLink];
          [entryHTML
              appendFormat:
                  @"<li><a href=\"#\" onclick=\"return "
                  @"doSearch('%@');\">%@</a> %@</li>\n",
                  [searchString stringByReplacingOccurrencesOfString:@"'"
                                                          withString:@"\\'"],
                  synString, link];
        }
        [entryHTML appendString:@"</ul>"];
      }
    }
    [entryHTML appendFormat:@"</div>"];
  }
  NSXMLNode *sourceNode = [entry nodeForXPath:@"./SOURCE"];
  if (sourceNode) {
    [entryHTML appendFormat:@"<p class=\"credit\">%@: %@</p>\n",
                            [NSString stringWithUTF8String:"資料來源"],
                            [sourceNode stringValue]];
  }
  return entryHTML;
}

- (void)_parseXML:(NSData *)xml keyword:(NSString *)keyword {
  NSMutableString *html = [NSMutableString stringWithString:BASEHTML];

  if (![xml length]) {
    if ([[self window] isVisible]) NSBeep();
    [html appendString:LFLSTR(@"<h2>Error</h2><p>Unable to fetch data from the "
                              @"online dictionary.</p></body></html>")];
    [self performSelectorOnMainThread:@selector(refreshContent:)
                           withObject:html
                        waitUntilDone:NO];
    return;
  }

  NSError *err;
  NSXMLDocument *xmlDoc =
      [[[NSXMLDocument alloc] initWithData:xml options:0
                                     error:&err] autorelease];
  NSArray *entries = [xmlDoc nodesForXPath:@"/DICTIONARY/ENTRIES/ENTRY"
                                     error:&err];
  int i = 0;
  if (![entries count]) {
    if ([[self window] isVisible]) NSBeep();
    [html
        appendFormat:
            LFLSTR(@"<h2>Not Found</h2><p>Unable to find results matching %@."),
            keyword];
  } else {
    NSEnumerator *entryEnumerator = [entries objectEnumerator];
    NSXMLNode *entry;
    while (entry = [entryEnumerator nextObject]) {
      i++;
      NSString *entryHTML = [self _parseXMLEntry:entry index:i];
      [html appendString:entryHTML];
    }
  }

  [html appendString:COPYRIGHT];
  [html appendString:_beaconHTML];
  [html appendString:@"</body></html>"];

  [self performSelectorOnMainThread:@selector(refreshContent:)
                         withObject:html
                      waitUntilDone:NO];
}

#pragma mark Fetch Yahoo! Dictionary XML file in anothe thread

- (void)fetchXML:(NSArray *)parameters {
  id pool = [NSAutoreleasePool new];
  NSURL *url = [parameters objectAtIndex:0];
  NSString *fullPath = [parameters objectAtIndex:1];
  NSString *keyword = [parameters objectAtIndex:2];
  NSData *xml = [NSData dataWithContentsOfURL:url];
  [self _parseXML:xml keyword:keyword];
  [xml writeToFile:fullPath atomically:YES];
  [pool drain];
  [pool release];
}

- (void)doFetchInThread:(NSURL *)url
               filePath:(NSString *)path
                keyword:(NSString *)keyword {
  [NSThread detachNewThreadSelector:@selector(fetchXML:)
                           toTarget:self
                         withObject:[NSArray arrayWithObjects:url, path,
                                                              keyword, nil]];
}

- (void)updateContent:(NSString *)urlstring
             filename:(NSString *)filename
              keyword:(NSString *)keyword {
  NSString *html = [BASEHTML
      stringByAppendingString:
          LFLSTR(@"<h2>Loading...</h2><p>Please wait</p></body></html>")];
  [self refreshContent:html];
  [_loadingProgressIndicator setHidden:NO];
  [_loadingProgressIndicator startAnimation:self];

  NSString *tempPath = [self _cachesDirectory];
  NSString *fullPath = [tempPath stringByAppendingPathComponent:filename];
  NSURL *url = [NSURL URLWithString:urlstring];
  NSData *xml;

  if (![[NSFileManager defaultManager] fileExistsAtPath:tempPath
                                            isDirectory:NULL]) {
    [[NSFileManager defaultManager] createDirectoryAtPath:tempPath
                                               attributes:nil];
  }
  if (![[NSFileManager defaultManager] fileExistsAtPath:fullPath
                                            isDirectory:NULL]) {
    [self doFetchInThread:url filePath:fullPath keyword:keyword];
  } else {
    NSDate *createDate = [[[NSFileManager defaultManager]
        fileAttributesAtPath:fullPath
                traverseLink:YES] objectForKey:NSFileCreationDate];
    NSDate *expireDate = [NSDate dateWithTimeIntervalSinceNow:(-2 * 60 * 60)];
    if ([createDate compare:expireDate] == NSOrderedAscending) {
      [self doFetchInThread:url filePath:fullPath keyword:keyword];
    } else {
      xml = [NSData dataWithContentsOfFile:fullPath];
      [self _parseXML:xml keyword:keyword];
    }
  }
}

- (void)awakeFromNib {
  Takao::TrackerMaker tm;
  _beaconURL = [NSString
      stringWithUTF8String:tm.onekeyTrackerURLString("dictionary").c_str()];
  [_beaconURL retain];

  _beaconHTML =
      [NSString stringWithFormat:@"<img src=\"%@\" style=\"display: none;\" />",
                                 _beaconURL];
  [_beaconHTML retain];

  [self _cleanCache];
  [_webview setUIDelegate:(id)self];
  [_webview setFrameLoadDelegate:(id)self];
  [_webview setPolicyDelegate:(id)self];
  [_webview setApplicationNameForUserAgent:@"Chiaki KeyKey"];

  [_webview setCustomUserAgent:@"ChiakiKeyKey/2026.06 (macOS)"];

  id toolbar = [[[NSToolbar alloc] initWithIdentifier:@"dictionary toolbar"]
      autorelease];
  [toolbar setAllowsUserCustomization:NO];
  [toolbar setAutosavesConfiguration:NO];
  [toolbar setSizeMode:NSToolbarSizeModeDefault];
  [toolbar setDisplayMode:NSToolbarDisplayModeIconOnly];
  [toolbar setDelegate:(id)self];
  [[self window] setToolbar:toolbar];

  NSRect screenRect = [[NSScreen mainScreen] visibleFrame];
  NSRect windowRect = [[self window] frame];
  int x =
      screenRect.origin.x + screenRect.size.width - windowRect.size.width - 10;
  int y = screenRect.origin.y + screenRect.size.height -
          windowRect.size.height - 10;
  [[self window] setDelegate:(id)self];
  [[self window] setTitle:LFLSTR(@"Dictionary")];
  [[self window]
      setFrame:NSMakeRect(x, y, windowRect.size.width, windowRect.size.height)
       display:YES];
}
- (void)temporaryHide {
  [[self window] orderOut:self];
}
- (void)restoreWindowStatus {
  if (_isVisible) [self showWindow:self];
}
- (BOOL)isVisible {
  return _isVisible;
}

#pragma mark Web Scripting methods.

- (void)openURL:(NSString *)urlString {
  NSURL *url = [NSURL URLWithString:urlString];
  [[NSWorkspace sharedWorkspace] openURL:url];
}

// Speak text by calling AppleScript
- (void)speak:(NSString *)text {
  text = [text
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  NSString *speak = [NSString stringWithFormat:@"say \"%@\"", text];
  NSAppleScript *script = [[NSAppleScript alloc] initWithSource:speak];
  [script executeAndReturnError:nil];
  [script release];
}
- (void)sendString:(NSString *)keyword {
  NSString *toSend = [keyword stringByAppendingString:@" "];
  [OpenVanillaController sendComposedStringToCurrentlyActiveContext:toSend];
}
- (void)search:(NSString *)keyword {
  keyword = [keyword
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  if ([keyword isEqualToString:@"orz"]) {
    [self showOrzSong];
    return;
  }
  NSString *urlString = [YAHOO_DICTIONARY_API_URL
      stringByAppendingString:
          [keyword
              stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];
  [[self window]
      setTitle:[NSString stringWithFormat:LFLSTR(@"Dictionary: %@"), keyword]];
  NSString *filename =
      [NSString stringWithFormat:@"dictionary-%@.xml", keyword];
  [self updateContent:urlString filename:filename keyword:keyword];
  [self showWindow:self];
  _isVisible = YES;
}

#pragma mark Interface Builder actions

- (IBAction)showWindow:(id)sender {
  NSPoint point = [[self window] frame].origin;

  NSScreen *screen = nil;
  NSEnumerator *enumerator = [[NSScreen screens] objectEnumerator];
  BOOL found = NO;
  NSRect frame;
  while (screen = [enumerator nextObject]) {
    frame = [screen frame];
    if (point.x >= NSMinX(frame) && point.x <= NSMaxX(frame) &&
        point.y >= NSMinY(frame) && point.y <= NSMaxX(frame)) {
      found = YES;
      break;
    }
  }
  if (!found) frame = [[NSScreen mainScreen] frame];

  NSRect windowRect = [[self window] frame];
  if (NSMaxX(windowRect) > NSMaxX(frame))
    windowRect.origin.x = NSMaxX(frame) - windowRect.size.width - 20;
  if (NSMinX(windowRect) < NSMinX(frame))
    windowRect.origin.x = NSMinX(frame) + 20;
  if (NSMaxY(windowRect) > NSMaxY(frame))
    windowRect.origin.y = NSMaxY(frame) - windowRect.size.height - 20;
  if (NSMinY(windowRect) < NSMinY(frame))
    windowRect.origin.y = NSMinX(frame) + 20;
  [[self window] setFrameOrigin:windowRect.origin];
  [super showWindow:sender];
}

- (IBAction)hide:(id)sender {
  [[self window] orderOut:self];
  _isVisible = NO;
}
- (IBAction)changeTextSize:(id)sender {
  int s = [sender selectedSegment];
  switch (s) {
    case 0:
      [_webview makeTextSmaller:sender];
      break;
    case 1:
      [_webview makeTextLarger:sender];
      break;
    default:
      break;
  }
}

#pragma mark -
#pragma mark NSWindow delegate methods

- (BOOL)windowShouldClose:(id)window {
  [window orderOut:self];
  _isVisible = NO;
  return NO;
}

#pragma mark WebView delegate methods

- (void)webView:(WebView *)sender
    windowScriptObjectAvailable:(WebScriptObject *)windowScriptObject {
  id scriptObject = windowScriptObject;
  [scriptObject setValue:self forKey:@"DictionaryController"];
}

- (NSArray *)webView:(WebView *)sender
    contextMenuItemsForElement:(NSDictionary *)element
              defaultMenuItems:(NSArray *)defaultMenuItems {
  NSMutableArray *menuItems = [NSMutableArray array];
  NSEnumerator *enumerator = [defaultMenuItems objectEnumerator];
  NSMenuItem *aMenuItem;

  while (aMenuItem = [enumerator nextObject]) {
    int tag = [aMenuItem tag];
    if (tag != WebMenuItemTagReload &&
        tag != WebMenuItemTagOpenLinkInNewWindow &&
        tag != WebMenuItemTagDownloadLinkToDisk &&
        tag != WebMenuItemTagCopyLinkToClipboard &&
        tag != WebMenuItemTagOpenFrameInNewWindow &&
        tag != WebMenuItemTagGoBack && tag != WebMenuItemTagGoForward &&
        tag != WebMenuItemTagStop) {
      [menuItems addObject:aMenuItem];
    }
  }
  return menuItems;
}

- (unsigned)webView:(WebView *)sender
    dragDestinationActionMaskForDraggingInfo:(id<NSDraggingInfo>)draggingInfo {
  return WebDragDestinationActionNone;
}
- (int)webView:(WebView *)sender dragSourceActionMaskForPoint:(NSPoint)point {
  return WebDragSourceActionNone;
}
- (void)webView:(WebView *)sender
    decidePolicyForNavigationAction:(NSDictionary *)actionInformation
                            request:(NSURLRequest *)request
                              frame:(WebFrame *)frame
                   decisionListener:(id<WebPolicyDecisionListener>)listener {
  int type =
      [[actionInformation valueForKey:WebActionNavigationTypeKey] intValue];
  NSURL *url = [request URL];
  if (type == WebNavigationTypeLinkClicked) {
    // If users are clicking on a link in webview in the first login
    // window, we open the link in the external web browser.
    [[NSWorkspace sharedWorkspace] openURL:url];
    [listener ignore];
  } else {
    [listener use];
  }
}

#pragma mark WebView scriprting object methods.

+ (NSString *)webScriptNameForSelector:(SEL)selector {
  if (selector == @selector(search:))
    return @"search";
  else if (selector == @selector(sendString:))
    return @"sendString";
  else if (selector == @selector(speak:))
    return @"speak";
  else if (selector == @selector(openURL:))
    return @"openURL";
  return nil;
}

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)selector {
  if (selector == @selector(search:)) return NO;
  if (selector == @selector(sendString:)) return NO;
  if (selector == @selector(speak:)) return NO;
  if (selector == @selector(openURL:)) return NO;
  return YES;
}

@end
