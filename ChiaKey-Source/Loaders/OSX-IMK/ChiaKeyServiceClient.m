// [AUTO_HEADER]

#import "OpenVanillaService.h"

#import <dispatch/dispatch.h>

@interface ChiaKeyServiceClient ()
@property(nonatomic, retain) NSXPCConnection *connection;
@end

@implementation ChiaKeyServiceClient

+ (instancetype)sharedClient {
  static ChiaKeyServiceClient *client = nil;
  if (!client) {
    client = [[self alloc] init];
  }
  return client;
}

- (id)init {
  self = [super init];
  if (self) {
    _connection = [[NSXPCConnection alloc]
        initWithMachServiceName:OPENVANILLA_DO_CONNECTION_NAME
                        options:0];
    [_connection setRemoteObjectInterface:
                     [NSXPCInterface
                         interfaceWithProtocol:@protocol(OpenVanillaXPCService)]];
    [_connection resume];
  }
  return self;
}

- (void)dealloc {
  [_connection invalidate];
  [_connection release];
  [super dealloc];
}

- (id<OpenVanillaXPCService>)_proxyWithSemaphore:(dispatch_semaphore_t)semaphore {
  return [_connection remoteObjectProxyWithErrorHandler:^(NSError *error) {
    if (semaphore) dispatch_semaphore_signal(semaphore);
  }];
}

- (BOOL)_waitForSemaphore:(dispatch_semaphore_t)semaphore {
  return dispatch_semaphore_wait(
             semaphore,
             dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5 * NSEC_PER_SEC))) == 0;
}

- (BOOL)isAvailable {
  __block BOOL returned = NO;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[self _proxyWithSemaphore:semaphore]
      versionWithReply:^(NSString *value) {
        returned = YES;
        dispatch_semaphore_signal(semaphore);
      }];
  [self _waitForSemaphore:semaphore];
  dispatch_release(semaphore);
  return returned;
}

- (oneway void)reloadOpenVanilla {
  [[self _proxyWithSemaphore:NULL] reloadOpenVanillaWithReply:^{}];
}

- (oneway void)sendString:(NSString *)text {
  [[self _proxyWithSemaphore:NULL] sendString:text withReply:^{}];
}

- (oneway void)sendKey:(NSString *)key {
  [[self _proxyWithSemaphore:NULL] sendKey:key withReply:^{}];
}

- (NSString *)primaryInputMethod {
  __block NSString *result = nil;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[self _proxyWithSemaphore:semaphore]
      primaryInputMethodWithReply:^(NSString *value) {
        result = [value retain];
        dispatch_semaphore_signal(semaphore);
      }];
  [self _waitForSemaphore:semaphore];
  dispatch_release(semaphore);
  return [result autorelease];
}

- (NSArray *)identifiersAndLocalizedNamesWithPattern:(NSString *)pattern {
  __block NSArray *result = nil;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[self _proxyWithSemaphore:semaphore]
      identifiersAndLocalizedNamesWithPattern:pattern
                                        reply:^(NSArray *value) {
                                          result = [value retain];
                                          dispatch_semaphore_signal(semaphore);
                                        }];
  [self _waitForSemaphore:semaphore];
  dispatch_release(semaphore);
  return [result autorelease];
}

- (bool)exportUserPhraseDBToFile:(NSString *)path {
  __block BOOL result = NO;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[self _proxyWithSemaphore:semaphore]
      exportUserPhraseDBToFile:path
                         reply:^(BOOL value) {
                           result = value;
                           dispatch_semaphore_signal(semaphore);
                         }];
  [self _waitForSemaphore:semaphore];
  dispatch_release(semaphore);
  return result;
}

- (bool)importUserPhraseDBFromFile:(NSString *)path {
  __block BOOL result = NO;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[self _proxyWithSemaphore:semaphore]
      importUserPhraseDBFromFile:path
                           reply:^(BOOL value) {
                             result = value;
                             dispatch_semaphore_signal(semaphore);
                           }];
  [self _waitForSemaphore:semaphore];
  dispatch_release(semaphore);
  return result;
}

- (NSString *)version {
  __block NSString *result = nil;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[self _proxyWithSemaphore:semaphore]
      versionWithReply:^(NSString *value) {
        result = [value retain];
        dispatch_semaphore_signal(semaphore);
      }];
  [self _waitForSemaphore:semaphore];
  dispatch_release(semaphore);
  return [result autorelease];
}

- (NSString *)databaseVersion {
  __block NSString *result = nil;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[self _proxyWithSemaphore:semaphore]
      databaseVersionWithReply:^(NSString *value) {
        result = [value retain];
        dispatch_semaphore_signal(semaphore);
      }];
  [self _waitForSemaphore:semaphore];
  dispatch_release(semaphore);
  return [result autorelease];
}

- (NSArray *)dynamicallyLoadedModulePackageInfo {
  __block NSArray *result = nil;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[self _proxyWithSemaphore:semaphore]
      dynamicallyLoadedModulePackageInfoWithReply:^(NSArray *value) {
        result = [value retain];
        dispatch_semaphore_signal(semaphore);
      }];
  [self _waitForSemaphore:semaphore];
  dispatch_release(semaphore);
  return [result autorelease];
}

- (void)setBlackListOfPackageIdentifers:(NSArray *)inIdentifiers {
  [[self _proxyWithSemaphore:NULL] setBlackListOfPackageIdentifers:inIdentifiers
                                                             reply:^{}];
}

- (NSString *)userInformationForCareService {
  __block NSString *result = nil;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[self _proxyWithSemaphore:semaphore]
      userInformationForCareServiceWithReply:^(NSString *value) {
        result = [value retain];
        dispatch_semaphore_signal(semaphore);
      }];
  [self _waitForSemaphore:semaphore];
  dispatch_release(semaphore);
  return [result autorelease];
}

- (BOOL)userPhraseDBCanProvideService {
  __block BOOL result = NO;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[self _proxyWithSemaphore:semaphore]
      userPhraseDBCanProvideServiceWithReply:^(BOOL value) {
        result = value;
        dispatch_semaphore_signal(semaphore);
      }];
  [self _waitForSemaphore:semaphore];
  dispatch_release(semaphore);
  return result;
}

- (int)userPhraseDBNumberOfRow {
  __block int result = 0;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[self _proxyWithSemaphore:semaphore]
      userPhraseDBNumberOfRowWithReply:^(int value) {
        result = value;
        dispatch_semaphore_signal(semaphore);
      }];
  [self _waitForSemaphore:semaphore];
  dispatch_release(semaphore);
  return result;
}

- (NSDictionary *)userPhraseDBDictionaryAtRow:(int)row {
  __block NSDictionary *result = nil;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[self _proxyWithSemaphore:semaphore]
      userPhraseDBDictionaryAtRow:row
                            reply:^(NSDictionary *value) {
                              result = [value retain];
                              dispatch_semaphore_signal(semaphore);
                            }];
  [self _waitForSemaphore:semaphore];
  dispatch_release(semaphore);
  return [result autorelease];
}

- (NSArray *)userPhraseDBReadingsForPhrase:(NSString *)phrase {
  __block NSArray *result = nil;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[self _proxyWithSemaphore:semaphore]
      userPhraseDBReadingsForPhrase:phrase
                              reply:^(NSArray *value) {
                                result = [value retain];
                                dispatch_semaphore_signal(semaphore);
                              }];
  [self _waitForSemaphore:semaphore];
  dispatch_release(semaphore);
  return [result autorelease];
}

- (void)userPhraseDBSave {
  [[self _proxyWithSemaphore:NULL] userPhraseDBSaveWithReply:^{}];
}

- (void)userPhraseDBSetNewReading:(NSString *)reading forPhraseAtRow:(int)row {
  [[self _proxyWithSemaphore:NULL] userPhraseDBSetNewReading:reading
                                               forPhraseAtRow:row
                                                        reply:^{}];
}

- (void)userPhraseDBDeleteRow:(int)row {
  [[self _proxyWithSemaphore:NULL] userPhraseDBDeleteRow:row reply:^{}];
}

- (void)userPhraseDBAddNewRow:(NSString *)phrase {
  [[self _proxyWithSemaphore:NULL] userPhraseDBAddNewRow:phrase reply:^{}];
}

- (void)userPhraseDBAddNewRows:(NSArray *)array {
  [[self _proxyWithSemaphore:NULL] userPhraseDBAddNewRows:array reply:^{}];
}

- (void)userPhraseDBSetPhrase:(NSString *)phrase atRow:(int)row {
  [[self _proxyWithSemaphore:NULL] userPhraseDBSetPhrase:phrase
                                                   atRow:row
                                                   reply:^{}];
}

@end
