// [AUTO_HEADER]

#import "CVApplicationController.h"

#import "CVNotifyController.h"
#import "OpenVanillaLoader.h"

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

static BOOL CVCodePointIsAllowedPhraseCharacter(unsigned int codePoint) {
  return (codePoint >= 0x2E80 && codePoint < 0xFF00) ||
         (codePoint >= 0x20000 && codePoint <= 0x323AF);
}

@implementation CVApplicationController

- (void)_initializeControllerIfNeeded {
  if (_serverConnection) return;

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

  _serverPort = [[NSPort port] retain];
  _serverConnection =
      [[NSConnection connectionWithReceivePort:_serverPort
                                      sendPort:_serverPort] retain];

  // NSConnection *connection = [NSConnection defaultConnection];
  [_serverConnection setRootObject:self];

  if ([_serverConnection registerName:OPENVANILLA_DO_CONNECTION_NAME]) {
    //	    NSLog(@"OpenVanilla DO service registered: %@",
    // OPENVANILLA_DO_CONNECTION_NAME);
  } else {
    NSLog(@"Failed to register DO service");
  }
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

#pragma mark -
#pragma mark Distributed Object Methods

- (oneway void)reloadOpenVanilla {
  NSLog(@"Reloading OpenVanilla");
  [[OpenVanillaLoader sharedInstance] reload];
  NSLog(@"Finished reloading OpenVanilla");
}
- (NSString *)primaryInputMethod {
  NSString *primaryInputMethod =
      [NSString stringWithUTF8String:[OpenVanillaLoader sharedLoader]
                                         ->primaryInputMethod()
                                         .c_str()];
  return primaryInputMethod;
}
- (NSArray *)identifiersAndLocalizedNamesWithPattern:(NSString *)pattern {
  // NSLog(@"calling remote stuff");
  return [_loader identifiersAndLocalizedNamesWithPattern:pattern];
}
- (bool)exportUserPhraseDBToFile:(NSString *)path {
  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:LFLSTR(@"Confirm User Phrase Export")];
  [alert setInformativeText:[NSString
                                stringWithFormat:
                                    LFLSTR(@"Allow ChiaKey to export your user "
                                           @"phrase dictionary to this file?\n%@"),
                                    path]];
  [alert addButtonWithTitle:LFLSTR(@"Export")];
  [alert addButtonWithTitle:LFLSTR(@"Cancel")];
  if ([alert runModal] != NSAlertFirstButtonReturn) return false;

  return [_loader exportUserPhraseDBToFile:path];
}
- (bool)importUserPhraseDBFromFile:(NSString *)path {
  NSAlert *alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:LFLSTR(@"Confirm User Phrase Import")];
  [alert setInformativeText:[NSString
                                stringWithFormat:
                                    LFLSTR(@"Allow ChiaKey to import user "
                                           @"phrases from this file?\n%@"),
                                    path]];
  [alert addButtonWithTitle:LFLSTR(@"Import")];
  [alert addButtonWithTitle:LFLSTR(@"Cancel")];
  if ([alert runModal] != NSAlertFirstButtonReturn) return false;

  return [_loader importUserPhraseDBFromFile:path];
}

- (NSString *)version {
  return [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];
}

- (NSString *)userInformationForCareService {
  stringstream sst;
  PVPlistValue *allPlists =
      [_loader loader]->loaderAndModulePropertyListsCombined();
  sst << *allPlists << endl;
  delete allPlists;
  return [NSString stringWithUTF8String:sst.str().c_str()];
}

- (oneway void)sendString:(NSString *)text {
}
- (oneway void)sendKey:(NSString *)key {
}

- (BOOL)userPhraseDBCanProvideService {
  return [_loader userPhraseDBCanProvideService];
}
- (int)userPhraseDBNumberOfRow {
  return [_loader userPhraseDBNumberOfRow];
}
- (NSDictionary *)userPhraseDBDictionaryAtRow:(int)row {
  return [_loader userPhraseDBDictionaryAtRow:row];
}
- (NSArray *)userPhraseDBReadingsForPhrase:(NSString *)phrase {
  return [_loader userPhraseDBReadingsForPhrase:phrase];
}
- (void)userPhraseDBSave {
  [_loader userPhraseDBSave];
}
- (void)userPhraseDBSetNewReading:(NSString *)reading forPhraseAtRow:(int)row {
  [_loader userPhraseDBSetNewReading:reading forPhraseAtRow:row];
}
- (void)userPhraseDBDeleteRow:(int)row {
  [_loader userPhraseDBDeleteRow:row];
}
- (void)userPhraseDBAddNewRow:(NSString *)phrase {
  [_loader userPhraseDBAddNewRow:phrase];
}
- (void)userPhraseDBAddNewRows:(NSArray *)array {
  [_loader userPhraseDBAddNewRows:array];
}

- (void)userPhraseDBSetPhrase:(NSString *)phrase atRow:(int)row {
  [_loader userPhraseDBSetPhrase:phrase atRow:row];
}

- (NSString *)databaseVersion {
  return [_loader databaseVersion];
}

- (NSArray *)dynamicallyLoadedModulePackageInfo {
  return [_loader dynamicallyLoadedModulePackageInfo];
}

- (void)setBlackListOfPackageIdentifers:(NSArray *)inIdentifiers {
  [_loader setBlackListOfPackageIdentifers:inIdentifiers];
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
      [self userPhraseDBAddNewRow:phrase];
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
      [self userPhraseDBAddNewRow:phrase];
      int lastRow = [self userPhraseDBNumberOfRow] - 1;
      [self userPhraseDBSetPhrase:phrase atRow:lastRow];
      [self userPhraseDBSetNewReading:reading forPhraseAtRow:lastRow];

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

- (BOOL)_isSilentLexiconUpdateEnabled {
  NSString *path = [self _globalPreferencesPath];
  NSDictionary *preferences =
      [NSDictionary dictionaryWithContentsOfFile:path];
  NSString *value =
      [preferences objectForKey:ChiaKeyLexiconAutoUpdateEnabledPreferenceKey];
  if (![value length]) return YES;

  return [value isEqualToString:@"true"];
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

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
  [[NSAppleEventManager sharedAppleEventManager]
      setEventHandler:self
          andSelector:@selector(handleIncomingURL:withReplyEvent:)
        forEventClass:kInternetEventClass
           andEventID:kAEGetURL];

  [self _runSilentLexiconUpdateIfNeeded];
}

@end
