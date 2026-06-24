#define OVServiceLoadedModulePackageIdentifierKey \
  @"OVServiceLoadedModulePackageIdentifierKey"
#define OVServiceLoadedModulePackageLocalizedNameKey \
  @"OVServiceLoadedModulePackageLocalizedNameKey"
#define OVServiceLoadedModulePackageBundlePathKey \
  @"OVServiceLoadedModulePackageBundlePathKey"
#define OVServiceLoadedModulePackageEnabledKey \
  @"OVServiceLoadedModulePackageEnabledKey"

@protocol OpenVanillaService
- (oneway void)reloadOpenVanilla;
- (oneway void)sendString:(NSString *)text;
- (oneway void)sendKey:(NSString *)key;

// <lithoglyph>
- (NSString *)primaryInputMethod;
- (NSArray *)identifiersAndLocalizedNamesWithPattern:(NSString *)pattern;
- (bool)exportUserPhraseDBToFile:(NSString *)path;
- (bool)importUserPhraseDBFromFile:(NSString *)path;

- (NSString *)version;
- (NSString *)databaseVersion;

#pragma mark Loaded Module Package related

- (NSArray *)dynamicallyLoadedModulePackageInfo;
- (void)setBlackListOfPackageIdentifers:(NSArray *)inIdentifiers;

- (NSString *)userInformationForCareService;

#pragma mark User phrase and phrase editor related

- (BOOL)userPhraseDBCanProvideService;
- (int)userPhraseDBNumberOfRow;
- (NSDictionary *)userPhraseDBDictionaryAtRow:(int)row;
- (NSArray *)userPhraseDBReadingsForPhrase:(NSString *)phrase;
- (void)userPhraseDBSave;
- (void)userPhraseDBSetNewReading:(NSString *)reading forPhraseAtRow:(int)row;
- (void)userPhraseDBDeleteRow:(int)row;
- (void)userPhraseDBAddNewRow:(NSString *)phrase;
- (void)userPhraseDBAddNewRows:(NSArray *)array;
- (void)userPhraseDBSetPhrase:(NSString *)phrase atRow:(int)row;

// </lithoglyph>
@end
