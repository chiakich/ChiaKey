//
//  TakaoCINTable.h
//
//  Import and removal of user-supplied CIN tables.
//
//  The IME scans two directories for `*.cin` at startup and on every reload
//  (see OpenVanillaLoader.mm): the bundled `Contents/Resources/DataTables`,
//  and `~/Library/Application Support/ChiaKey/DataTables`. Both are scanned
//  recursively, and a table's module identifier is its path relative to the
//  scan root with `/` and `.` turned into `-`. Only identifiers matching
//  `Generic-*` become selectable input methods, so an imported table has to
//  land in a subdirectory literally named `Generic`:
//
//      ~/Library/Application Support/ChiaKey/DataTables/Generic/jyutping.cin
//          -> module identifier "Generic-jyutping-cin"
//
//  We only ever write to the user directory. Injecting files into the app
//  bundle would invalidate its code signature and be wiped on the next
//  update, so everything listed here is by definition user-installed and
//  therefore removable.
//

#import <Cocoa/Cocoa.h>

// Keys of the dictionary describing a table, as returned by
// +inspectFileAtPath:error: and +installedTables.
extern NSString *const TakaoCINTableDisplayNameKey;   // NSString
extern NSString *const TakaoCINTableIdentifierKey;    // NSString, "Generic-x-cin"
extern NSString *const TakaoCINTableFileNameKey;      // NSString, "x.cin"
extern NSString *const TakaoCINTableEntryCountKey;    // NSNumber
extern NSString *const TakaoCINTableSelectionKeysKey; // NSString, may be nil
extern NSString *const TakaoCINTableSourceEncodingKey;// NSString, nil if UTF-8
extern NSString *const TakaoCINTableContentKey;       // NSData, UTF-8, import only
extern NSString *const TakaoCINTablePathKey;          // NSString, installed only

extern NSString *const TakaoCINTableErrorDomain;

typedef NS_ENUM(NSInteger, TakaoCINTableErrorCode) {
  TakaoCINTableErrorUnreadable = 1,
  TakaoCINTableErrorTooLarge,
  TakaoCINTableErrorUnknownEncoding,
  TakaoCINTableErrorNoCharDef,
  TakaoCINTableErrorNoEntries,
  TakaoCINTableErrorNoName,
  TakaoCINTableErrorBadFileName,
  TakaoCINTableErrorReservedName,
  TakaoCINTableErrorWriteFailed,
};

@interface TakaoCINTable : NSObject

// ~/Library/Application Support/ChiaKey/DataTables/Generic, created on demand.
// Returns nil if it does not exist and cannot be created.
+ (NSString *)userTableDirectory;

// Tables the user has imported, sorted by display name. Never includes the
// tables bundled with the app.
+ (NSArray *)installedTables;

// Reads and validates a candidate file without writing anything. On success
// returns a description of the table including its content transcoded to
// UTF-8, ready to hand to +installTable:overwrite:error:.
+ (NSDictionary *)inspectFileAtPath:(NSString *)path error:(NSError **)error;

// YES if a table with this file name is already installed.
+ (BOOL)isTableInstalledWithFileName:(NSString *)fileName;

+ (BOOL)installTable:(NSDictionary *)table
           overwrite:(BOOL)overwrite
               error:(NSError **)error;

+ (BOOL)removeTableWithFileName:(NSString *)fileName error:(NSError **)error;

@end
