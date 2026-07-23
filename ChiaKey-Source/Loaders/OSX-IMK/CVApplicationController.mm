// [AUTO_HEADER]

#import "CVApplicationController.h"

#import "CVNotifyController.h"
#import "ChiaKeyServiceCoordination.h"
#import "ChiaKeyUserPhraseCoordination.h"
#import "OpenVanillaLoader.h"

static const NSTimeInterval kChiaKeyUserPhrasePollInterval = 5.0;

static NSString *const ChiaKeyLexiconAutoUpdateLastCheckDefaultsKey =
    @"ChiaKeyLexiconAutoUpdateLastCheck";
static NSString *const ChiaKeyLexiconAutoUpdateLastResultDefaultsKey =
    @"ChiaKeyLexiconAutoUpdateLastResult";
static NSString *const ChiaKeyGlobalPreferencesFilename =
    @"com.chiakey.ChiaKey.plist";
static NSString *const ChiaKeyLexiconAutoUpdateEnabledPreferenceKey =
    @"ShouldAutoUpdateLexicon";
static const NSTimeInterval kChiaKeyLexiconAutoUpdateCheckInterval =
    24.0 * 60.0 * 60.0;
static const NSInteger kChiaKeyLexiconAutoUpdateMinimumAgeDays = 3;

static NSString *const ChiaKeyApplicationAutoUpdateLastCheckDefaultsKey =
    @"ChiaKeyApplicationAutoUpdateLastCheck";
static NSString *const ChiaKeyApplicationAutoUpdateEnabledPreferenceKey =
    @"ShouldAutoUpdateApplication";
static const NSTimeInterval kChiaKeyApplicationAutoUpdateCheckInterval =
    24.0 * 60.0 * 60.0;

// Re-check cadence while the process stays resident, not the check
// interval itself: the 24h throttles above still gate actual work. A
// shorter re-arm keeps the wait after sleep/wake or a missed cycle bounded
// to about an hour instead of however long this process happens to run.
static const NSTimeInterval kChiaKeyAutoUpdateCheckTimerInterval =
    60.0 * 60.0;

static BOOL CVCodePointIsAllowedPhraseCharacter(unsigned int codePoint) {
  return (codePoint >= 0x2E80 && codePoint < 0xFF00) ||
         (codePoint >= 0x20000 && codePoint <= 0x323AF);
}

@implementation CVApplicationController

- (void)_initializeControllerIfNeeded {
  if (_plainTextCandidateController) return;

  _loader = nil;

  _plainTextCandidateController = [CVPlainTextCandidateController new];
  _horizontalCandidateController = [CVHorizontalCandidateController new];
  _verticalCandidateController = [CVVerticalCandidateController new];
  _searchController = [CVSearchController new];
  _symbolController = [CVSymbolController new];
  _tooltipController = [CVToolTipController new];
  _aboutController = [CVAboutController new];
  _inputMethodToggleWindowController =
      [CVInputMethodToggleWindowController new];

}

- (id)init {
  self = [super init];
  if (self) {
    [self _initializeControllerIfNeeded];
  }
  return self;
}

- (void)dealloc {
  [_verticalCandidateController release];
  [_horizontalCandidateController release];
  [_plainTextCandidateController release];
  [_searchController release];
  [_symbolController release];
  [_tooltipController release];
  [_aboutController release];
  [_inputMethodToggleWindowController release];
  [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
  [_userPhrasePollTimer invalidate];
  [_userPhrasePollTimer release];
  [_autoUpdateCheckTimer invalidate];
  [_autoUpdateCheckTimer release];
  [_lastSeenUserPhraseDirtyDate release];
  [super dealloc];
}
- (void)setLoader:(OpenVanillaLoader *)aLoader {
  OpenVanillaLoader *tmp = _loader;

  //	NSLog(@"if this instance's _loader is clean? %p", tmp);
  //
  //	NSLog(@"loader instance: %p", aLoader);
  //	NSLog(@"loader desc: %@", aLoader);

  _loader = [aLoader retain];
  if (tmp) {
    [tmp release];
  }

  //	NSLog(@"finished retaining loader %@", _loader);
}
- (OpenVanillaLoader *)loader {
  return _loader;
}

#pragma mark User Interface Controllers

- (CVVerticalCandidateController *)verticalCandidateController {
  return _verticalCandidateController;
}
- (CVHorizontalCandidateController *)horizontalCandidateController {
  return _horizontalCandidateController;
}
- (CVPlainTextCandidateController *)plainTextCandidateController {
  return _plainTextCandidateController;
}
- (CVSymbolController *)symbolController {
  return _symbolController;
}
- (CVToolTipController *)tooltipController {
  return _tooltipController;
}
- (CVSearchController *)searchController {
  return _searchController;
}
- (CVAboutController *)aboutController {
  return _aboutController;
}
- (CVInputMethodToggleWindowController *)inputMethodToggleWindowController {
  return _inputMethodToggleWindowController;
}

#pragma mark To initialize the Application Controller

- (void)awakeFromNib {
  [self _initializeControllerIfNeeded];
}

- (IBAction)showAboutWindow:(id)sender {
  [[self aboutController] showWindow:sender];
}

- (NSDictionary *)_dictionaryWithIdentifier:(string)identifier
                              localizedName:(NSString *)localizedName {
  NSString *identifierString =
      [NSString stringWithUTF8String:identifier.c_str()];
  NSDictionary *d = [NSDictionary
      dictionaryWithObjectsAndKeys:identifierString, @"identifier",
                                   localizedName, @"localizedName", nil];
  return d;
}
- (NSArray *)inputMethodsArray {
  NSMutableArray *a = [NSMutableArray array];

  NSMutableSet *excludeSet = [NSMutableSet set];

  PVPlistValue *configDict =
      [OpenVanillaLoader sharedLoader]->configRootDictionary();
  PVPlistValue *suppressSetting =
      configDict->valueForKey("ModulesSuppressedFromUI");
  if (suppressSetting) {
    if (suppressSetting->type() == PVPlistValue::Array) {
      size_t c = suppressSetting->arraySize();
      for (size_t i = 0; i < c; i++) {
        [excludeSet
            addObject:[NSString
                          stringWithUTF8String:suppressSetting
                                                   ->arrayElementAtIndex(i)
                                                   ->stringValue()
                                                   .c_str()]];
      }
    }
  }

  if (![excludeSet containsObject:@"SmartMandarin"])
    [a addObject:[self _dictionaryWithIdentifier:("SmartMandarin")
                                   localizedName:LFLSTR(@"Smart Phonetic")]];
  if (![excludeSet containsObject:@"TraditionalMandarin"])
    [a addObject:[self _dictionaryWithIdentifier:("TraditionalMandarin")
                                   localizedName:LFLSTR(
                                                     @"Traditional Phonetic")]];
  if (![excludeSet containsObject:@"Generic-cj-cin"])
    [a addObject:[self _dictionaryWithIdentifier:("Generic-cj-cin")
                                   localizedName:LFLSTR(@"Cangjie")]];
  if (![excludeSet containsObject:@"Generic-simplex-cin"])
    [a addObject:[self _dictionaryWithIdentifier:("Generic-simplex-cin")
                                   localizedName:LFLSTR(@"Simplex")]];

  [excludeSet addObject:@"SmartMandarin"];
  [excludeSet addObject:@"TraditionalMandarin"];
  [excludeSet addObject:@"Generic-cj-cin"];
  [excludeSet addObject:@"Generic-simplex-cin"];

  vector<pair<string, string> >::iterator iter;
  vector<pair<string, string> > idNamePairs =
      [OpenVanillaLoader sharedLoader]->allInputMethodIdentifiersAndNames();

  for (iter = idNamePairs.begin(); iter != idNamePairs.end(); ++iter) {
    pair<string, string> idNamePair = *iter;
    string identifier = idNamePair.first;
    string localizedName = idNamePair.second;

    if (![excludeSet
            containsObject:[NSString
                               stringWithUTF8String:identifier.c_str()]]) {
      [a addObject:[self
                       _dictionaryWithIdentifier:identifier
                                   localizedName:[NSString stringWithUTF8String:
                                                               localizedName
                                                                   .c_str()]]];
    }
  }

  if (![a count]) {
    [a addObject:[self _dictionaryWithIdentifier:("SmartMandarin")
                                   localizedName:LFLSTR(@"Smart Phonetic")]];
  }

  return a;
}

// These are local UI/notification helpers, not exported IPC entry points.
- (void)reloadOpenVanilla {
  NSLog(@"Reloading OpenVanilla");
  [[OpenVanillaLoader sharedInstance] reload];
  NSLog(@"Finished reloading OpenVanilla");
}

- (NSString *)primaryInputMethod {
  return [NSString
      stringWithUTF8String:[OpenVanillaLoader sharedLoader]
                               ->primaryInputMethod()
                               .c_str()];
}

- (NSArray *)dynamicallyLoadedModulePackageInfo {
  return [_loader dynamicallyLoadedModulePackageInfo];
}

@end

#pragma mark -

@implementation CVApplicationController (AppDelegate)

- (NSString *)_validatedString:(NSString *)originalString {
  NSString *string = [originalString
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];

  if (![string length]) return nil;

  std::vector<std::string> codepoints =
      OVUTF8Helper::SplitStringByCodePoint([string UTF8String]);
  std::string validatedString;
  for (std::vector<std::string>::const_iterator iter = codepoints.begin();
       iter != codepoints.end(); ++iter) {
    unsigned int codePoint = OVUTF8Helper::CodePointFromSingleUTF8String(*iter);
    if (CVCodePointIsAllowedPhraseCharacter(codePoint)) {
      validatedString += *iter;
    }
  }

  return [NSString stringWithUTF8String:validatedString.c_str()];
}

- (NSUInteger)_codePointCountOfString:(NSString *)string {
  if (![string length]) return 0;

  return OVUTF8Helper::SplitStringByCodePoint([string UTF8String]).size();
}

- (BOOL)_confirmAddPhrase:(NSString *)phrase {
  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:LFLSTR(@"Confirm Add Phrase")];
  [alert setInformativeText:[NSString
                                stringWithFormat:
                                    LFLSTR(@"Allow ChiaKey to add \"%@\" to "
                                           @"your user dictionary?"),
                                    phrase]];
  [alert addButtonWithTitle:LFLSTR(@"Add Phrase")];
  [alert addButtonWithTitle:LFLSTR(@"Cancel")];
  return [alert runModal] == NSAlertFirstButtonReturn;
}

- (void)handleIncomingURL:(NSAppleEventDescriptor *)event
           withReplyEvent:(NSAppleEventDescriptor *)replyEvent {
  NSString *url =
      [[[event paramDescriptorForKeyword:keyDirectObject] stringValue]
          stringByRemovingPercentEncoding];
  if ([url hasPrefix:@"chiakey://"]) {
    NSString *string =
        [url substringWithRange:NSMakeRange(10, [url length] - 10)];
    NSArray *a = [string componentsSeparatedByString:@"_"];
    if ([a count] < 2) {
      NSString *phrase = [self _validatedString:string];
      if (![phrase length]) {
        [CVNotifyController
            notify:LFLSTR(@"The phrase you want to add is invalid.")];
        return;
      }
      if (![self _confirmAddPhrase:phrase]) {
        return;
      }
      [_loader userPhraseDBAddNewRow:phrase];
      NSString *msg = [NSString
          stringWithFormat:@"%@%@", LFLSTR(@"Add new phrase: "), phrase];
      [CVNotifyController notify:msg];
    } else if ([a count] == 2) {
      NSString *phrase = [self _validatedString:[a objectAtIndex:0]];
      NSString *reading = [a objectAtIndex:1];
      if ([self _codePointCountOfString:phrase] !=
          [[reading componentsSeparatedByString:@","] count]) {
        [CVNotifyController
            notify:LFLSTR(@"The phrase you want to add is invalid.")];
        return;
      }
      if (![self _confirmAddPhrase:phrase]) {
        return;
      }
      // Insert phrase + reading in one shot by rowid; the old add-then-set-by
      // -position path landed the reading on the wrong row once rowid holes
      // existed, and dropped it entirely during an editor session.
      [_loader userPhraseDBAddNewRow:phrase reading:reading];

      NSString *msg = [NSString
          stringWithFormat:@"%@%@", LFLSTR(@"Add new phrase: "), phrase];
      [CVNotifyController notify:msg];
    }
  }
}

- (NSString *)_bundledLexiconInstallerPath {
  NSString *resourcesPath = [[NSBundle mainBundle] resourcePath];
  NSString *scriptPath = [resourcesPath
      stringByAppendingPathComponent:@"Scripts/install-lexicon-release.sh"];
  if ([[NSFileManager defaultManager] fileExistsAtPath:scriptPath])
    return scriptPath;

  scriptPath =
      [resourcesPath stringByAppendingPathComponent:@"install-lexicon-release.sh"];
  if ([[NSFileManager defaultManager] fileExistsAtPath:scriptPath])
    return scriptPath;

  return nil;
}

- (NSString *)_globalPreferencesPath {
  NSArray *paths = NSSearchPathForDirectoriesInDomains(
      NSLibraryDirectory, NSUserDomainMask, YES);
  if (![paths count]) return nil;

  return [[[paths objectAtIndex:0] stringByAppendingPathComponent:@"Preferences"]
      stringByAppendingPathComponent:ChiaKeyGlobalPreferencesFilename];
}

// Opt-out, not opt-in: an absent key means the feature is on.
- (BOOL)_isBooleanPreferenceEnabled:(NSString *)key {
  NSDictionary *preferences =
      [NSDictionary dictionaryWithContentsOfFile:[self _globalPreferencesPath]];
  NSString *value = [preferences objectForKey:key];
  if (![value length]) return YES;

  return [value isEqualToString:@"true"];
}

- (BOOL)_isSilentLexiconUpdateEnabled {
  return [self
      _isBooleanPreferenceEnabled:ChiaKeyLexiconAutoUpdateEnabledPreferenceKey];
}

- (BOOL)_shouldRunSilentLexiconUpdate {
  if (![self _isSilentLexiconUpdateEnabled]) {
    return NO;
  }

  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  NSDate *lastCheck =
      [defaults objectForKey:ChiaKeyLexiconAutoUpdateLastCheckDefaultsKey];
  if ([lastCheck isKindOfClass:[NSDate class]] &&
      [[NSDate date] timeIntervalSinceDate:lastCheck] <
          kChiaKeyLexiconAutoUpdateCheckInterval) {
    return NO;
  }

  [defaults setObject:[NSDate date]
               forKey:ChiaKeyLexiconAutoUpdateLastCheckDefaultsKey];
  [defaults synchronize];
  return YES;
}

- (void)_runSilentLexiconUpdateIfNeeded {
  if (![self _shouldRunSilentLexiconUpdate]) return;

  NSString *scriptPath = [self _bundledLexiconInstallerPath];
  if (![scriptPath length]) {
    NSLog(@"ChiaKey lexicon auto-update skipped: installer not found.");
    return;
  }

  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    NSTask *task = [[NSTask alloc] init];
    NSPipe *pipe = [NSPipe pipe];
    NSString *minimumAge =
        [NSString stringWithFormat:@"%ld",
                                   (long)kChiaKeyLexiconAutoUpdateMinimumAgeDays];

    [task setLaunchPath:@"/bin/bash"];
    [task setArguments:[NSArray arrayWithObjects:scriptPath,
                                                 @"--skip-current",
                                                 @"--min-release-age-days",
                                                 minimumAge,
                                                 nil]];
    [task setStandardOutput:pipe];
    [task setStandardError:pipe];

    BOOL launched = NO;
    @try {
      [task launch];
      launched = YES;
    } @catch (NSException *exception) {
      NSLog(@"ChiaKey lexicon auto-update launch failed: %@", exception);
    }

    if (launched) {
      [task waitUntilExit];
      NSData *data = [[pipe fileHandleForReading] readDataToEndOfFile];
      NSString *output =
          [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]
              autorelease];
      int status = [task terminationStatus];
      NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
      NSString *result = [NSString stringWithFormat:@"status=%d\n%@",
                                                    status,
                                                    output ? output : @""];
      [defaults setObject:result
                   forKey:ChiaKeyLexiconAutoUpdateLastResultDefaultsKey];
      [defaults synchronize];

      if (status == 0) {
        NSLog(@"ChiaKey lexicon auto-update finished: %@", output);
        [self performSelectorOnMainThread:@selector(reloadOpenVanilla)
                               withObject:nil
                            waitUntilDone:NO];
      } else {
        NSLog(@"ChiaKey lexicon auto-update failed: %@", output);
      }
    }

    [task release];
    [pool drain];
  });
}

#pragma mark Application update check

// The IME's whole role in updating itself is deciding "it is time to look" and
// spawning the Updater app. Fetching, prompting and installing all happen in
// that separate process: an IMK bundle can be torn down at any input-source
// switch, and nothing on the input path may block on the network or a modal.

- (NSURL *)_bundledUpdaterURL {
  NSString *sharedSupportPath = [[NSBundle mainBundle] sharedSupportPath];
  if (![sharedSupportPath length]) return nil;

  NSString *updaterPath =
      [sharedSupportPath stringByAppendingPathComponent:@"Updater.app"];
  if (![[NSFileManager defaultManager] fileExistsAtPath:updaterPath])
    return nil;

  return [NSURL fileURLWithPath:updaterPath];
}

- (BOOL)_shouldRunApplicationUpdateCheck {
  if (![self _isBooleanPreferenceEnabled:
                 ChiaKeyApplicationAutoUpdateEnabledPreferenceKey]) {
    return NO;
  }

  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  NSDate *lastCheck =
      [defaults objectForKey:ChiaKeyApplicationAutoUpdateLastCheckDefaultsKey];
  if ([lastCheck isKindOfClass:[NSDate class]] &&
      [[NSDate date] timeIntervalSinceDate:lastCheck] <
          kChiaKeyApplicationAutoUpdateCheckInterval) {
    return NO;
  }

  // Stamp before launching, not after: a check that crashes or is declined
  // must still cost a full day, otherwise every restart re-prompts.
  [defaults setObject:[NSDate date]
               forKey:ChiaKeyApplicationAutoUpdateLastCheckDefaultsKey];
  [defaults synchronize];
  return YES;
}

- (void)_runApplicationUpdateCheckIfNeeded {
  if (![self _shouldRunApplicationUpdateCheck]) return;

  NSURL *updaterURL = [self _bundledUpdaterURL];
  if (!updaterURL) {
    NSLog(@"ChiaKey application update check skipped: Updater.app not found.");
    return;
  }

  if (@available(macOS 10.15, *)) {
    NSWorkspaceOpenConfiguration *configuration =
        [NSWorkspaceOpenConfiguration configuration];
    [configuration setActivates:NO];
    [configuration setAddsToRecentItems:NO];

    [[NSWorkspace sharedWorkspace]
        openApplicationAtURL:updaterURL
               configuration:configuration
           completionHandler:^(NSRunningApplication *application,
                               NSError *error) {
             if (error) {
               NSLog(@"ChiaKey application update check failed to launch: %@",
                     error);
             }
           }];
  } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    NSError *error = nil;
    if (![[NSWorkspace sharedWorkspace]
            launchApplicationAtURL:updaterURL
                           options:NSWorkspaceLaunchWithoutActivation
                     configuration:@{}
                             error:&error]) {
      NSLog(@"ChiaKey application update check failed to launch: %@", error);
    }
#pragma clang diagnostic pop
  }
}

- (void)_runAutoUpdateChecks:(NSTimer *)timer {
  [self _runSilentLexiconUpdateIfNeeded];
  [self _runApplicationUpdateCheckIfNeeded];
}

- (void)_startAutoUpdateCheckTimer {
  _autoUpdateCheckTimer = [[NSTimer
      scheduledTimerWithTimeInterval:kChiaKeyAutoUpdateCheckTimerInterval
                              target:self
                            selector:@selector(_runAutoUpdateChecks:)
                            userInfo:nil
                             repeats:YES] retain];
  // Coalesce with other system wakeups instead of demanding an exact fire
  // time; the 24h throttles make a few extra minutes of slack irrelevant.
  [_autoUpdateCheckTimer
      setTolerance:kChiaKeyAutoUpdateCheckTimerInterval * 0.1];
}

#pragma mark Phrase Editor coordination

// The distributed notifications give immediacy; the poll timer below is the
// authoritative fallback (missed notifications, editor crash, IME started
// mid-session), driven by the lock/dirty files next to the user DB.

- (void)_phraseEditorSessionDidBegin:(NSNotification *)notification {
  if (_observedEditorSessionActive) return;
  _observedEditorSessionActive = YES;
  [_loader userPhraseEditingSessionDidBegin];
}

- (void)_phraseEditorSessionDidEnd:(NSNotification *)notification {
  if (!_observedEditorSessionActive) return;
  _observedEditorSessionActive = NO;
  [_loader userPhraseEditingSessionDidEnd];
}

- (void)_userPhraseDidChange:(NSNotification *)notification {
  [_loader userPhraseDBDidChangeExternally];
}

- (void)_pollUserPhraseCoordinationFiles:(NSTimer *)timer {
  if (!_loader) return;

  NSString *dir = [_loader userDataDirectory];

  BOOL lockActive = ChiaKeyUserPhraseEditingLockIsActive(dir);
  if (lockActive != _observedEditorSessionActive) {
    _observedEditorSessionActive = lockActive;
    if (lockActive) {
      [_loader userPhraseEditingSessionDidBegin];
    } else {
      [_loader userPhraseEditingSessionDidEnd];
    }
  }

  NSDate *dirtyDate =
      ChiaKeyCoordinationFileDate(ChiaKeyUserPhraseDirtyFlagPath(dir));
  if (dirtyDate &&
      (!_lastSeenUserPhraseDirtyDate ||
       [dirtyDate compare:_lastSeenUserPhraseDirtyDate] ==
           NSOrderedDescending)) {
    [_lastSeenUserPhraseDirtyDate release];
    _lastSeenUserPhraseDirtyDate = [dirtyDate retain];
    // Also fires on the first sighting: an editor commit between DB load and
    // the first poll must not be folded into the baseline (the notification
    // for it may have been dropped). Costs one spurious cache flush at
    // startup, which is cheap.
    [_loader userPhraseDBDidChangeExternally];
  }
}

- (void)_startObservingPhraseEditor {
  NSDistributedNotificationCenter *center =
      [NSDistributedNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(_phraseEditorSessionDidBegin:)
                 name:ChiaKeyPhraseEditorDidBeginEditingNotification
               object:nil];
  [center addObserver:self
             selector:@selector(_phraseEditorSessionDidEnd:)
                 name:ChiaKeyPhraseEditorDidEndEditingNotification
               object:nil];
  [center addObserver:self
             selector:@selector(_userPhraseDidChange:)
                 name:ChiaKeyUserPhraseDidChangeNotification
               object:nil];

  _userPhrasePollTimer = [[NSTimer
      scheduledTimerWithTimeInterval:kChiaKeyUserPhrasePollInterval
                              target:self
                            selector:@selector(
                                         _pollUserPhraseCoordinationFiles:)
                            userInfo:nil
                             repeats:YES] retain];
}

#pragma mark Preferences app requests (see ChiaKeyServiceCoordination.h)

- (void)_reloadRequested:(NSNotification *)notification {
  [self reloadOpenVanilla];
}

- (void)_moduleBlacklistDidChange:(NSNotification *)notification {
  // Apply first, then reload, in one handler: two separate notifications
  // would have no delivery-order guarantee.
  [_loader applyPendingModuleBlacklistAndPublish];
  [self reloadOpenVanilla];
}

- (void)_startObservingPreferencesRequests {
  NSDistributedNotificationCenter *center =
      [NSDistributedNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(_reloadRequested:)
                 name:ChiaKeyReloadRequestedNotification
               object:nil];
  [center addObserver:self
             selector:@selector(_moduleBlacklistDidChange:)
                 name:ChiaKeyModuleBlacklistDidChangeNotification
               object:nil];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
  [[NSAppleEventManager sharedAppleEventManager]
      setEventHandler:self
          andSelector:@selector(handleIncomingURL:withReplyEvent:)
        forEventClass:kInternetEventClass
           andEventID:kAEGetURL];

  [self _startObservingPhraseEditor];
  [self _startObservingPreferencesRequests];
  [self _runSilentLexiconUpdateIfNeeded];
  [self _runApplicationUpdateCheckIfNeeded];
  [self _startAutoUpdateCheckTimer];
}

@end
