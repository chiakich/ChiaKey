/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
//
//  DownloadController.m
//  DownloadUpdate
//

#import "DownloadController.h"

#import "LFUtilities.h"

static DownloadManager *_manager;

@implementation DownloadManager

+ (DownloadManager *)sharedManager {
  if (!_manager) {
    _manager = [[DownloadManager alloc] init];
  }
  return _manager;
}
- (void)dealloc {
  [m_actionURLString release];
  [m_actionFilePath release];
  [m_actionURLString release];
  [m_signatureFilePath release];
  [m_changelogURLString release];
  [super dealloc];
}

- (void)setDownloadURLString:(NSString *)actionURLString
          signatureURLString:(NSString *)signatureURLString
              actionFilePath:(NSString *)actionFilePath
           signatureFilePath:(NSString *)signatureFilePath
          changelogURLString:(NSString *)changelogURLString;
{
  LFRetainAssign(m_actionURLString, actionURLString);
  LFRetainAssign(m_actionFilePath, actionFilePath);
  LFRetainAssign(m_signatureURLString, signatureURLString);
  LFRetainAssign(m_signatureFilePath, signatureFilePath);
  LFRetainAssign(m_changelogURLString, changelogURLString);
}

- (NSString *)actionURLString {
  return m_actionURLString;
}
- (NSString *)signatureURLString {
  return m_signatureURLString;
}
- (NSString *)actionFilePath {
  return m_actionFilePath;
}
- (NSString *)signatureFilePath {
  return m_signatureFilePath;
}
- (NSString *)changelogURLString {
  return m_changelogURLString;
}

@end

@implementation DownloadController

- (void)awakeFromNib {
  m_actionURLString =
      [[[DownloadManager sharedManager] actionURLString] retain];
  m_signatureURLString =
      [[[DownloadManager sharedManager] signatureURLString] retain];
  m_actionFilePath = [[[DownloadManager sharedManager] actionFilePath] retain];
  m_signatureFilePath =
      [[[DownloadManager sharedManager] signatureFilePath] retain];
  m_changelogURLString =
      [[[DownloadManager sharedManager] changelogURLString] retain];

  [u_web setFrameLoadDelegate:(id)self];
  [u_web setResourceLoadDelegate:(id)self];
  [u_web setUIDelegate:(id)self];
  [u_web setPolicyDelegate:(id)self];
  [u_web setDownloadDelegate:(id)self];

  [u_retry setHidden:YES];
  [u_install setHidden:YES];

  [[self window] orderOut:self];
  [[self window] center];
  [[self window] setLevel:NSFloatingWindowLevel];
  [[self window] setDelegate:(id)self];
  [[self window] setDefaultButtonCell:[u_cancel cell]];

  [u_askWindow setDefaultButtonCell:[u_askYes cell]];
  [u_askWindow setLevel:NSFloatingWindowLevel];
  [u_askWindow center];
  [u_askWindow orderFront:self];

  if (![m_changelogURLString length] ||
      [m_changelogURLString isEqualToString:@"(null)"]) {
    NSString *webText = LFLSTR(@"Please download the newest version.");
    NSString *html =
        [NSString stringWithFormat:
                      @"<html><head><meta http-equiv=\"Content-type\" "
                      @"content=\"text/html; "
                      @"charset=utf-8\"></head><body><p>%@</p></body></html>",
                      webText];
    [[u_web mainFrame] loadHTMLString:html baseURL:nil];
  } else {
    NSURL *url = [NSURL URLWithString:m_changelogURLString];
    NSURLRequest *request = [NSURLRequest requestWithURL:url];
    [[u_web mainFrame] loadRequest:request];
  }
}

- (IBAction)clickNo:(id)sender {
  [NSApp terminate:self];
  return;
}
- (IBAction)clickYes:(id)sender {
  NSString *downloadFolder =
      [m_actionFilePath stringByDeletingLastPathComponent];
  if (![[NSFileManager defaultManager] fileExistsAtPath:downloadFolder
                                            isDirectory:NULL]) {
    [[NSFileManager defaultManager] createDirectoryAtPath:downloadFolder
                                               attributes:nil];
  }
  [u_askWindow orderOut:self];
  [[self window] orderFront:self];
  [self startDownload];
}

- (void)dealloc {
  [m_actionURLString release];
  [m_signatureURLString release];
  [m_actionFilePath release];
  [m_signatureFilePath release];
  [m_changelogURLString release];
  [super dealloc];
}

- (void)downloadFailed {
  [u_text setStringValue:LFLSTR(@"Error: unable to download updates.")];
  [u_retry setHidden:NO];
  [[self window] setDefaultButtonCell:[u_retry cell]];
}

- (void)downloadIsInvalid {
  [u_text setStringValue:LFLSTR(@"Error: the downloaded file is corrupt.")];
  [u_retry setHidden:NO];
  [[self window] setDefaultButtonCell:[u_retry cell]];
}

- (void)startDownload {
  m_downloadStopped = NO;
  [u_process setIndeterminate:YES];
  [u_process startAnimation:self];
  [u_process setDoubleValue:0];
  [u_retry setHidden:YES];

  NSURL *signatureURL = [NSURL URLWithString:m_signatureURLString];
  NSData *signatureData = [NSData dataWithContentsOfURL:signatureURL];
  if (![signatureData length]) [self downloadFailed];

  [signatureData writeToFile:m_signatureFilePath atomically:YES];

  NSURLRequest *theRequest =
      [NSURLRequest requestWithURL:[NSURL URLWithString:m_actionURLString]
                       cachePolicy:NSURLRequestUseProtocolCachePolicy
                   timeoutInterval:60.0];
  NSURLDownload *theDownload = [[NSURLDownload alloc] initWithRequest:theRequest
                                                             Delegate:(id)self];
  if (theDownload) {
    [theDownload setDestination:m_actionFilePath allowOverwrite:YES];
  } else {
    [self downloadFailed];
  }
}
- (IBAction)cancelDownload:(id)sender {
  m_downloadStopped = YES;
  [NSApp terminate:self];
}
- (IBAction)retry:(id)sender {
  [self startDownload];
}
- (BOOL)installUpdate {
  return [[NSWorkspace sharedWorkspace] openFile:m_actionFilePath];
}
- (IBAction)install:(id)sender {
  if (![self installUpdate]) {
    [u_text setStringValue:LFLSTR(@"Error: unable to open the downloaded file. "
                                  @"Please try again.")];
    [u_install setHidden:YES];
    [u_retry setHidden:NO];
  } else {
    [NSApp terminate:self];
  }
}
- (void)setDownloadResponse:(NSURLResponse *)aDownloadResponse {
  [aDownloadResponse retain];
  [m_downloadResponse release];
  m_downloadResponse = aDownloadResponse;
}

#pragma mark NSURLDownload

- (void)download:(NSURLDownload *)download didFailWithError:(NSError *)error {
  [download release];

  NSString *errorString = [NSString
      stringWithFormat:LFLSTR(@"Error: %@ %@"), [error localizedDescription],
                       [[error userInfo]
                           objectForKey:NSErrorFailingURLStringKey]];
  [u_text setStringValue:errorString];
  [u_retry setHidden:NO];
  [[self window] setDefaultButtonCell:[u_retry cell]];
  [u_process stopAnimation:self];
}
- (void)downloadDidFinish:(NSURLDownload *)download {
  [download release];

  id x = OPENVANILLA_DO_CONNECTION_NAME;

  id ovService = [NSConnection
      rootProxyForConnectionWithRegisteredName:OPENVANILLA_DO_CONNECTION_NAME
                                          host:nil];
  BOOL valid = false;
  if (ovService) {
    [ovService setProtocolForProxy:@protocol(OpenVanillaService)];
    valid = [ovService validateFile:m_actionFilePath
                   againstSignature:m_signatureFilePath];
  } else {
    valid = false;
  }
  if (!valid) {
    [self downloadIsInvalid];
    return;
  }

  [u_install setHidden:NO];
  [[self window] setDefaultButtonCell:[u_install cell]];
  [u_text setStringValue:
              LFLSTR(@"Download completes. Do you want to install it now?")];
  [u_process stopAnimation:self];
}
- (void)download:(NSURLDownload *)download
    didReceiveResponse:(NSURLResponse *)response {
  if (m_downloadStopped) {
    [download cancel];
    [download release];
    return;
  }
  m_bytesReceived = 0;
  [u_process setIndeterminate:NO];
  [self setDownloadResponse:response];
}

- (void)download:(NSURLDownload *)download
    didReceiveDataOfLength:(unsigned)length {
  if (m_downloadStopped) {
    [download cancel];
    [download release];
    return;
  }

  long long expectedLength = [m_downloadResponse expectedContentLength];
  m_bytesReceived += length;

  if (expectedLength != NSURLResponseUnknownLength) {
    float percentComplete = (m_bytesReceived / (float)expectedLength) * 100.0;
    [u_text setStringValue:[NSString
                               stringWithFormat:LFLSTR(@"Downloading...%d %%"),
                                                (int)percentComplete]];
    [u_process setDoubleValue:percentComplete];
  } else {
    // guess: 18 MB
    expectedLength = 18 * 1024 * 1024;
    float percentComplete = (m_bytesReceived / (float)expectedLength) * 100.0;
    [u_text
        setStringValue:[NSString
                           stringWithFormat:LFLSTR(@"Downloading... %ju KB"),
                                            (uintmax_t)(m_bytesReceived /
                                                        1024)]];
    [u_process setDoubleValue:percentComplete];
  }
}

// Window delegate
- (void)windowWillClose:(NSNotification *)notification {
  [NSApp terminate:self];
}

#pragma mark -

- (NSArray *)webView:(WebView *)sender
    contextMenuItemsForElement:(NSDictionary *)element
              defaultMenuItems:(NSArray *)defaultMenuItems {
  return [NSArray array];
}

// Ignores all drag and drop into the web view.
- (unsigned int)webView:(WebView *)sender
    dragDestinationActionMaskForDraggingInfo:(id<NSDraggingInfo>)draggingInfo {
  return WebDragDestinationActionNone;
}

// Disables dragging links, allows only dragging images and selected text.
- (unsigned int)webView:(WebView *)sender
    dragSourceActionMaskForPoint:(NSPoint)point {
  return WebDragSourceActionSelection | WebDragSourceActionImage;
}

- (void)webView:(WebView *)sender
    decidePolicyForNewWindowAction:(NSDictionary *)actionInformation
                           request:(NSURLRequest *)request
                      newFrameName:(NSString *)frameName
                  decisionListener:(id<WebPolicyDecisionListener>)listener {
  NSURL *URL = [request URL];
  [[NSWorkspace sharedWorkspace] openURL:URL];
}

@end
