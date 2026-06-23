/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
//
//  DownloadController.h
//  DownloadUpdate
//

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#import "LFUtilities.h"

#ifndef OPENVANILLA_DO_CONNECTION_NAME
#error Needs to define OPENVANILLA_DO_CONNECTION_NAME
#endif

#import "OpenVanillaService.h"

@interface DownloadManager : NSObject {
  NSString *m_actionURLString;
  NSString *m_signatureURLString;
  NSString *m_actionFilePath;
  NSString *m_signatureFilePath;
  NSString *m_changelogURLString;
}

+ (DownloadManager *)sharedManager;
- (void)setDownloadURLString:(NSString *)actionURLString
          signatureURLString:(NSString *)signatureURLString
              actionFilePath:(NSString *)actionFilePath
           signatureFilePath:(NSString *)signatureFilePath
          changelogURLString:(NSString *)changelogURLString;
- (NSString *)actionURLString;
- (NSString *)signatureURLString;
- (NSString *)actionFilePath;
- (NSString *)signatureFilePath;
- (NSString *)changelogURLString;
@end

@interface DownloadController : NSWindowController {
  IBOutlet id u_askWindow;
  IBOutlet id u_askYes;
  IBOutlet id u_askNo;

  IBOutlet id u_process;
  IBOutlet id u_cancel;
  IBOutlet id u_retry;
  IBOutlet id u_install;
  IBOutlet id u_text;
  IBOutlet id u_web;
  NSString *m_actionURLString;
  NSString *m_signatureURLString;
  NSString *m_actionFilePath;
  NSString *m_signatureFilePath;
  NSString *m_changelogURLString;
  BOOL m_downloadStopped;
  NSURLResponse *m_downloadResponse;
  long long m_bytesReceived;
}
- (IBAction)clickNo:(id)sender;
- (IBAction)clickYes:(id)sender;
- (void)startDownload;
- (IBAction)retry:(id)sender;
- (IBAction)cancelDownload:(id)sender;
- (IBAction)install:(id)sender;
@end
