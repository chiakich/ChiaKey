#import <Foundation/Foundation.h>

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

@protocol OpenVanillaXPCService
- (void)reloadOpenVanillaWithReply:(void (^)(void))reply;
- (void)sendString:(NSString *)text withReply:(void (^)(void))reply;
- (void)sendKey:(NSString *)key withReply:(void (^)(void))reply;

- (void)primaryInputMethodWithReply:(void (^)(NSString *value))reply;
- (void)identifiersAndLocalizedNamesWithPattern:(NSString *)pattern
                                          reply:(void (^)(NSArray *value))reply;
- (void)exportUserPhraseDBToFile:(NSString *)path
                            reply:(void (^)(BOOL value))reply;
- (void)importUserPhraseDBFromFile:(NSString *)path
                              reply:(void (^)(BOOL value))reply;

- (void)versionWithReply:(void (^)(NSString *value))reply;
- (void)databaseVersionWithReply:(void (^)(NSString *value))reply;

- (void)dynamicallyLoadedModulePackageInfoWithReply:
    (void (^)(NSArray *value))reply;
- (void)setBlackListOfPackageIdentifers:(NSArray *)inIdentifiers
                                  reply:(void (^)(void))reply;
- (void)userInformationForCareServiceWithReply:
    (void (^)(NSString *value))reply;

- (void)userPhraseDBCanProvideServiceWithReply:(void (^)(BOOL value))reply;
- (void)userPhraseDBNumberOfRowWithReply:(void (^)(int value))reply;
- (void)userPhraseDBDictionaryAtRow:(int)row
                              reply:(void (^)(NSDictionary *value))reply;
- (void)userPhraseDBReadingsForPhrase:(NSString *)phrase
                                reply:(void (^)(NSArray *value))reply;
- (void)userPhraseDBSaveWithReply:(void (^)(void))reply;
- (void)userPhraseDBSetNewReading:(NSString *)reading
                    forPhraseAtRow:(int)row
                             reply:(void (^)(void))reply;
- (void)userPhraseDBDeleteRow:(int)row reply:(void (^)(void))reply;
- (void)userPhraseDBAddNewRow:(NSString *)phrase reply:(void (^)(void))reply;
- (void)userPhraseDBAddNewRows:(NSArray *)array reply:(void (^)(void))reply;
- (void)userPhraseDBSetPhrase:(NSString *)phrase
                        atRow:(int)row
                        reply:(void (^)(void))reply;
@end

@interface ChiaKeyServiceClient : NSObject <OpenVanillaService>
+ (instancetype)sharedClient;
- (BOOL)isAvailable;
@end
