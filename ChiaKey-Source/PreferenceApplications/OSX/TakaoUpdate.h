/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "TakaoSettings.h"

/*!
        @header TakaoUpdate
*/

/*
        @class TakaoUpdate
        @abstract The controller to handle checking for updates
        and download the newer version.
*/

@interface TakaoUpdate : NSObject {
  IBOutlet id _applicationCheckButton;
  IBOutlet id _applicationCurrentVersionTextField;
  IBOutlet id _applicationIncludeBetaCheckBox;
  IBOutlet id _applicationInstallButton;
  IBOutlet id _applicationLatestCheckTextField;
  IBOutlet id _applicationLatestVersionTextField;
  IBOutlet id _applicationProgressIndicator;
  IBOutlet id _lexiconCheckButton;
  IBOutlet id _lexiconCurrentVersionTextField;
  IBOutlet id _lexiconInstallButton;
  IBOutlet id _lexiconLatestVersionTextField;
  IBOutlet id _lexiconLatestCheckTextField;
  IBOutlet id _lexiconProgressIndicator;
  IBOutlet id _window;
  NSString *_availableApplicationPackageName;
  NSString *_availableApplicationPackageURL;
  NSString *_availableApplicationTag;
  NSString *_availableLexiconTag;
  BOOL _didAutoCheckOnShow;
  NSTask *_task;
}

/*!
        @method checkApplicationUpdateNow:
        @abstract To check for input method updates.
        @param sender The sender object.
*/
- (IBAction)checkApplicationUpdateNow:(id)sender;
- (IBAction)checkLexiconUpdateNow:(id)sender;
- (IBAction)installApplicationUpdate:(id)sender;
- (IBAction)installLexiconUpdate:(id)sender;
- (IBAction)toggleIncludeBetaReleases:(id)sender;
- (void)updatePaneDidBecomeActive;
@end
