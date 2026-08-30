//
//  PEUserPhraseStore.mm
//  PhraseEditor
//
//  Direct SQLite data layer for the phrase editor. Opens the user phrase DB
//  in WAL mode, performs all mutations by rowid, serves windowed queries for
//  the virtualized table view, and coordinates with the running input method
//  through the lock/dirty files and distributed notifications declared in
//  ChiaKeyUserPhraseCoordination.h.
//

#import "PEUserPhraseStore.h"

#import <AppKit/AppKit.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#import "ChiaKeyServiceCoordination.h"
#import "ChiaKeyUserPhraseCoordination.h"
#include "MJSRExportCipher.h"
#include "MJSRLearningCacheTables.h"
#include "Mandarin.h"

using Formosa::Mandarin::BPMF;

static NSString *const kChiaKeyLoaderName = @"ChiaKey";
// Used only to locate the lexicon bundled inside the running IME app as a
// last-resort source for reading derivation (see -_lexiconDB).
static NSString *const kChiaKeyIMEBundleIdentifier =
    @"com.chiakey.inputmethod.ChiaKey";
static const NSTimeInterval kChangeNotificationThrottle = 0.5;
// Generous on purpose: a phrase line is 45-90 bytes, so a million of them
// already comes to 60 MB. Overridable for the tests.
#ifndef PE_MAX_IMPORT_FILE_SIZE
#define PE_MAX_IMPORT_FILE_SIZE (256ULL * 1024 * 1024)
#endif
#ifndef PE_MAX_LEARNING_BLOB_SIZE
#define PE_MAX_LEARNING_BLOB_SIZE (96UL * 1024 * 1024)
#endif
static const unsigned long long kPEMaxImportFileSize = PE_MAX_IMPORT_FILE_SIZE;
static const NSUInteger kPEMaxLearningBlobSize = PE_MAX_LEARNING_BLOB_SIZE;
// Keep the editing lock visibly fresh, well within the staleness timeout.
static const NSTimeInterval kEditingLockRefreshInterval = 60.0;

#pragma mark - Codec helpers (Formosa)

// qstring (2 bytes per syllable, absolute order) -> "ㄋㄧˇ,ㄏㄠˇ"
static std::string PEComposedFromQstring(const std::string &qstring) {
  std::string result;
  if (qstring.size() % 2) return result;
  for (size_t i = 0; i < qstring.size(); i += 2) {
    if (i) result += ",";
    result +=
        BPMF::FromAbsoluteOrderString(qstring.substr(i, 2)).composedString();
  }
  return result;
}

// "ㄋㄧˇ,ㄏㄠˇ" -> qstring; empty syllables are skipped like the IME does.
static std::string PEQstringFromComposed(const std::string &composed) {
  std::string result;
  size_t start = 0;
  while (start <= composed.size()) {
    size_t comma = composed.find(',', start);
    std::string one = composed.substr(
        start, comma == std::string::npos ? std::string::npos : comma - start);
    BPMF b = BPMF::FromComposedString(one);
    if (!b.isEmpty()) result += b.absoluteOrderString();
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return result;
}

static NSString *PENSString(const std::string &s) {
  NSString *result = [NSString stringWithUTF8String:s.c_str()];
  return result ? result : @"";
}

static std::string PEStdString(NSString *s) {
  const char *utf8 = [s UTF8String];
  return utf8 ? std::string(utf8) : std::string();
}

// Escapes LIKE metacharacters, for use with ESCAPE '\'.
static std::string PEEscapeForLike(const std::string &s) {
  std::string r;
  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (c == '%' || c == '_' || c == '\\') r += '\\';
    r += c;
  }
  return r;
}

#pragma mark - PEPhraseRecord

@implementation PEPhraseRecord

@synthesize rowid = _rowid;
@synthesize phrase = _phrase;
@synthesize reading = _reading;

- (void)dealloc {
  [_phrase release];
  [_reading release];
  [super dealloc];
}

@end

#pragma mark - PEUserPhraseStore

@interface PEUserPhraseStore ()
- (void)_markDirtyAndScheduleChangeNotification;
- (sqlite3 *)_lexiconDB;
- (BOOL)_importFromFile:(NSString *)path
                 legacy:(BOOL)legacy
   learningDataRestored:(BOOL *)learningDataRestored;
- (NSSet *)_keysOfTable:(NSString *)table;
- (NSSet *)_keysOf:(NSString *)exportTable missingFrom:(NSString *)table;
- (BOOL)_table:(const char *)table existsInSchema:(const char *)schema;
- (BOOL)_restoreLearningCachesFromDatabase:(NSString *)path
                                    legacy:(BOOL)legacy;
- (NSUInteger)_dropLearningEntriesUnknownToLexicon:(NSDictionary *)scope;
@end

@implementation PEUserPhraseStore {
  sqlite3 *_userDB;
  sqlite3 *_lexDB;  // main lexicon, read-only, lazily opened, may stay NULL
  BOOL _lexTried;

  BOOL _sessionActive;
  id _editingActivity;
  NSTimer *_lockRefreshTimer;
  NSTimer *_changeNotificationTimer;

  // Cached counts for the current generation, keyed by normalized filter.
  NSMutableDictionary *_countCache;
}

+ (instancetype)sharedStore {
  static PEUserPhraseStore *store = nil;
  if (!store) {
    store = [[self alloc] init];
  }
  return store;
}

- (id)init {
  self = [super init];
  if (self) {
    _countCache = [NSMutableDictionary new];
    [self _openUserDB];
  }
  return self;
}

- (void)dealloc {
  [self endEditingSession];
  if (_userDB) sqlite3_close(_userDB);
  if (_lexDB) sqlite3_close(_lexDB);
  [_countCache release];
  [super dealloc];
}

#pragma mark Paths

- (NSString *)_userDataDirectory {
  return [NSHomeDirectory()
      stringByAppendingPathComponent:
          [@"Library/Application Support"
              stringByAppendingPathComponent:kChiaKeyLoaderName]];
}

- (NSString *)_userDBPath {
  return [[self _userDataDirectory]
      stringByAppendingPathComponent:@"SmartMandarinUserData.db"];
}

#pragma mark DB opening

- (void)_openUserDB {
  if (_userDB) return;

  ChiaKeyEnsureUserDataDirectoryPrivate();

  if (sqlite3_open([[self _userDBPath] UTF8String], &_userDB) != SQLITE_OK) {
    if (_userDB) {
      sqlite3_close(_userDB);
      _userDB = NULL;
    }
    return;
  }

  sqlite3_busy_timeout(_userDB, 3000);
  // WAL lets the IME keep reading while the editor writes. The mode is
  // persistent, so this is a one-time migration for the file.
  sqlite3_exec(_userDB, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);

  // First use: create the schema exactly as the IME does. Do NOT add
  // columns; the IME imports with positional INSERTs (see the rewrite
  // guide's schema landmine).
  const char *schema =
      "CREATE TABLE IF NOT EXISTS user_unigrams "
      "(qstring, current, probability, backoff);"
      "CREATE INDEX IF NOT EXISTS user_unigrams_index "
      "ON user_unigrams (qstring);"
      // Lets ORDER BY current serve phrase-sorted pages from the index
      // instead of re-sorting the whole table on every window fetch. An
      // index is safe (it adds no column, so the IME's positional imports
      // are unaffected).
      "CREATE INDEX IF NOT EXISTS user_unigrams_current_index "
      "ON user_unigrams (current);"
      "CREATE TABLE IF NOT EXISTS user_bigram_cache "
      "(qstring, previous, current, probability);"
      "CREATE INDEX IF NOT EXISTS user_bigram_cache_index "
      "ON user_bigram_cache (qstring);"
      "CREATE TABLE IF NOT EXISTS user_candidate_override_cache "
      "(qstring, current);"
      "CREATE INDEX IF NOT EXISTS user_candidate_override_cache_index "
      "ON user_candidate_override_cache (qstring);"
      // Context-keyed overrides: same shape as the table above, but the qstring
      // is "previous reading + space + this reading".
      "CREATE TABLE IF NOT EXISTS user_context_override_cache "
      "(qstring, current);"
      "CREATE UNIQUE INDEX IF NOT EXISTS "
      "user_context_override_cache_qstring_unique "
      "ON user_context_override_cache (qstring);"
      // Per-entry learning statistics live beside the cache tables rather than
      // as extra columns in them, for the same landmine: the IME's positional
      // INSERTs would break.
      "CREATE TABLE IF NOT EXISTS user_learning_stats "
      "(store, qstring, selection_count, last_used);"
      "CREATE UNIQUE INDEX IF NOT EXISTS user_learning_stats_key "
      "ON user_learning_stats (store, qstring);";
  sqlite3_exec(_userDB, schema, NULL, NULL, NULL);

  // The unique keys the IME's incremental saves depend on. Kept separate from
  // the batch above because they can only be created once the duplicates an
  // older full-table rewrite could leave behind are gone, and because a failure
  // here must not abort the rest of the schema. This mirrors
  // LanguageModel::MigrateUserLearningTables() -- either side may be the first
  // to open a given database, and importing without these keys would let
  // INSERT OR REPLACE quietly become INSERT.
  static const char *const dedupe[] = {
      "DELETE FROM user_bigram_cache WHERE rowid NOT IN "
      "(SELECT MAX(rowid) FROM user_bigram_cache GROUP BY qstring)",
      "DELETE FROM user_candidate_override_cache WHERE rowid NOT IN "
      "(SELECT MAX(rowid) FROM user_candidate_override_cache GROUP BY qstring)",
      "CREATE UNIQUE INDEX IF NOT EXISTS user_bigram_cache_qstring_unique "
      "ON user_bigram_cache (qstring)",
      "CREATE UNIQUE INDEX IF NOT EXISTS "
      "user_candidate_override_cache_qstring_unique "
      "ON user_candidate_override_cache (qstring)",
  };
  for (size_t i = 0; i < sizeof(dedupe) / sizeof(dedupe[0]); i++)
    sqlite3_exec(_userDB, dedupe[i], NULL, NULL, NULL);
}

- (BOOL)isAvailable {
  return _userDB != NULL;
}

#pragma mark Editing session

- (void)beginEditingSession {
  if (_sessionActive || !_userDB) return;
  _sessionActive = YES;

  // The lock is refreshed from the main run loop. Keep this lightweight
  // editor process out of App Nap for the session so an otherwise-open
  // editor cannot let its lock look stale while the user is away.
  _editingActivity = [[[NSProcessInfo processInfo]
      beginActivityWithOptions:NSActivityUserInitiated
                        reason:@"Editing ChiaKey user phrases"] retain];

  ChiaKeyClaimUserPhraseEditingLock([self _userDataDirectory]);
  _lockRefreshTimer =
      [[NSTimer scheduledTimerWithTimeInterval:kEditingLockRefreshInterval
                                        target:self
                                      selector:@selector(_refreshEditingLock:)
                                      userInfo:nil
                                       repeats:YES] retain];

  ChiaKeyPostUserPhraseNotification(
      ChiaKeyPhraseEditorDidBeginEditingNotification);
}

- (void)_refreshEditingLock:(NSTimer *)timer {
  ChiaKeyRefreshUserPhraseEditingLock([self _userDataDirectory]);
}

- (void)endEditingSession {
  if (!_sessionActive) return;
  _sessionActive = NO;

  [_lockRefreshTimer invalidate];
  [_lockRefreshTimer release];
  _lockRefreshTimer = nil;

  if (_editingActivity) {
    [[NSProcessInfo processInfo] endActivity:_editingActivity];
    [_editingActivity release];
    _editingActivity = nil;
  }

  // Flush any pending change notification before ending.
  if (_changeNotificationTimer) {
    [_changeNotificationTimer invalidate];
    [_changeNotificationTimer release];
    _changeNotificationTimer = nil;
    ChiaKeyPostUserPhraseNotification(ChiaKeyUserPhraseDidChangeNotification);
  }

  // Remove the lock before posting End so the IME's own check agrees. If
  // another live session owns the lock, leave it (and the End notification)
  // to that owner -- the IME must stay suspended for it.
  if (ChiaKeyReleaseUserPhraseEditingLockIfOwner([self _userDataDirectory])) {
    ChiaKeyPostUserPhraseNotification(
        ChiaKeyPhraseEditorDidEndEditingNotification);
  }
}

- (void)_markDirtyAndScheduleChangeNotification {
  [self invalidateCachedCounts];
  ChiaKeyTouchCoordinationFile(
      ChiaKeyUserPhraseDirtyFlagPath([self _userDataDirectory]));

  // A background import owns a separate store instance and has no run loop
  // for NSTimer. Its dirty file is already durable, so post immediately.
  if (![NSThread isMainThread]) {
    ChiaKeyPostUserPhraseNotification(ChiaKeyUserPhraseDidChangeNotification);
    return;
  }

  if (_changeNotificationTimer) return;  // already scheduled
  _changeNotificationTimer = [[NSTimer
      scheduledTimerWithTimeInterval:kChangeNotificationThrottle
                              target:self
                            selector:@selector(_postChangeNotification:)
                            userInfo:nil
                             repeats:NO] retain];
}

- (void)_postChangeNotification:(NSTimer *)timer {
  [_changeNotificationTimer release];
  _changeNotificationTimer = nil;
  ChiaKeyPostUserPhraseNotification(ChiaKeyUserPhraseDidChangeNotification);
}

#pragma mark Query building

- (void)invalidateCachedCounts {
  [_countCache removeAllObjects];
}

// WHERE clause for a filter; appends bound parameter values to `params`.
static std::string PEFilterClause(NSString *filter,
                                  std::vector<std::string> &params) {
  if (![filter length]) return "";

  std::string text = PEStdString(filter);
  std::string clause = "WHERE (current LIKE ? ESCAPE '\\')";
  params.push_back("%" + PEEscapeForLike(text) + "%");

  // If the filter parses as composed Bopomofo, also match readings by
  // whole-syllable prefix.
  std::string qstring = PEQstringFromComposed(text);
  if (qstring.size()) {
    clause += " OR (qstring LIKE ? ESCAPE '\\')";
    params.push_back(PEEscapeForLike(qstring) + "%");
  }
  return clause;
}

static std::string PEOrderClause(PEPhraseSortKey sortKey, BOOL ascending) {
  const char *dir = ascending ? "ASC" : "DESC";
  char buf[128];
  switch (sortKey) {
    case PEPhraseSortKeyPhrase:
      snprintf(buf, sizeof(buf), "ORDER BY current %s, rowid %s", dir, dir);
      break;
    case PEPhraseSortKeyReading:
      snprintf(buf, sizeof(buf), "ORDER BY qstring %s, rowid %s", dir, dir);
      break;
    case PEPhraseSortKeyInsertion:
    default:
      snprintf(buf, sizeof(buf), "ORDER BY rowid %s", dir);
      break;
  }
  return buf;
}

- (NSUInteger)numberOfPhrasesMatchingFilter:(NSString *)filter {
  if (!_userDB) return 0;

  NSString *normalizedFilter = filter ? filter : @"";
  NSNumber *cachedCount = [_countCache objectForKey:normalizedFilter];
  if (cachedCount) return [cachedCount unsignedIntegerValue];

  std::vector<std::string> params;
  std::string sql =
      "SELECT COUNT(*) FROM user_unigrams " + PEFilterClause(filter, params);

  NSUInteger count = 0;
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(_userDB, sql.c_str(), -1, &st, NULL) == SQLITE_OK) {
    for (size_t i = 0; i < params.size(); i++) {
      sqlite3_bind_text(st, (int)i + 1, params[i].c_str(), -1,
                        SQLITE_TRANSIENT);
    }
    if (sqlite3_step(st) == SQLITE_ROW) {
      count = (NSUInteger)sqlite3_column_int64(st, 0);
    }
    sqlite3_finalize(st);
  }

  [_countCache setObject:[NSNumber numberWithUnsignedInteger:count]
                  forKey:normalizedFilter];
  return count;
}

- (NSArray *)phrasesInRange:(NSRange)range
                     filter:(NSString *)filter
                    sortKey:(PEPhraseSortKey)sortKey
                  ascending:(BOOL)ascending {
  NSMutableArray *result = [NSMutableArray arrayWithCapacity:range.length];
  if (!_userDB || !range.length) return result;

  std::vector<std::string> params;
  std::string sql = "SELECT rowid, qstring, current FROM user_unigrams " +
                    PEFilterClause(filter, params) + " " +
                    PEOrderClause(sortKey, ascending) + " LIMIT ? OFFSET ?";

  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(_userDB, sql.c_str(), -1, &st, NULL) != SQLITE_OK) {
    return result;
  }
  int bindIndex = 1;
  for (size_t i = 0; i < params.size(); i++) {
    sqlite3_bind_text(st, bindIndex++, params[i].c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int64(st, bindIndex++, (sqlite3_int64)range.length);
  sqlite3_bind_int64(st, bindIndex++, (sqlite3_int64)range.location);

  while (sqlite3_step(st) == SQLITE_ROW) {
    PEPhraseRecord *record = [[[PEPhraseRecord alloc] init] autorelease];
    record.rowid = sqlite3_column_int64(st, 0);
    const char *q = (const char *)sqlite3_column_text(st, 1);
    const char *cur = (const char *)sqlite3_column_text(st, 2);
    record.phrase = cur ? PENSString(cur) : @"";
    record.reading = PENSString(PEComposedFromQstring(q ? q : ""));
    [result addObject:record];
  }
  sqlite3_finalize(st);
  return result;
}

- (PEPhraseRecord *)phraseForRowid:(long long)rowid {
  if (!_userDB) return nil;

  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(
          _userDB, "SELECT qstring, current FROM user_unigrams WHERE rowid = ?",
          -1, &st, NULL) != SQLITE_OK) {
    return nil;
  }
  sqlite3_bind_int64(st, 1, rowid);

  PEPhraseRecord *record = nil;
  if (sqlite3_step(st) == SQLITE_ROW) {
    record = [[[PEPhraseRecord alloc] init] autorelease];
    record.rowid = rowid;
    const char *q = (const char *)sqlite3_column_text(st, 0);
    const char *cur = (const char *)sqlite3_column_text(st, 1);
    record.phrase = cur ? PENSString(cur) : @"";
    record.reading = PENSString(PEComposedFromQstring(q ? q : ""));
  }
  sqlite3_finalize(st);
  return record;
}

#pragma mark Mutations

- (BOOL)_insertPhrase:(NSString *)phrase reading:(NSString *)reading {
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(_userDB,
                         "INSERT INTO user_unigrams (qstring, current, "
                         "probability, backoff) VALUES (?, ?, -1.0, 0.0)",
                         -1, &st, NULL) != SQLITE_OK) {
    return NO;
  }
  sqlite3_bind_text(st, 1, PEQstringFromComposed(PEStdString(reading)).c_str(),
                    -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 2, [phrase UTF8String], -1, SQLITE_TRANSIENT);
  BOOL ok = (sqlite3_step(st) == SQLITE_DONE);
  sqlite3_finalize(st);
  return ok;
}

- (PEPhraseRecord *)addPhrase:(NSString *)phrase {
  if (!_userDB || ![phrase length]) return nil;

  NSString *reading = [self defaultReadingForPhrase:phrase];
  if (![self _insertPhrase:phrase reading:reading]) return nil;

  PEPhraseRecord *record = [[[PEPhraseRecord alloc] init] autorelease];
  record.rowid = sqlite3_last_insert_rowid(_userDB);
  record.phrase = phrase;
  record.reading = reading;

  [self _markDirtyAndScheduleChangeNotification];
  return record;
}

- (NSArray *)addPhrases:(NSArray *)phrases {
  NSMutableArray *records = [NSMutableArray array];
  if (!_userDB || ![phrases count]) return records;

  sqlite3_exec(_userDB, "BEGIN", NULL, NULL, NULL);
  for (NSString *phrase in phrases) {
    if (![phrase length]) continue;
    NSString *reading = [self defaultReadingForPhrase:phrase];
    if (![self _insertPhrase:phrase reading:reading]) continue;
    PEPhraseRecord *record = [[[PEPhraseRecord alloc] init] autorelease];
    record.rowid = sqlite3_last_insert_rowid(_userDB);
    record.phrase = phrase;
    record.reading = reading;
    [records addObject:record];
  }
  sqlite3_exec(_userDB, "COMMIT", NULL, NULL, NULL);
  [self _markDirtyAndScheduleChangeNotification];
  return records;
}

- (BOOL)containsPhrase:(NSString *)phrase {
  if (!_userDB) return NO;
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(_userDB,
                         "SELECT 1 FROM user_unigrams WHERE current = ? "
                         "LIMIT 1",
                         -1, &st, NULL) != SQLITE_OK) {
    return NO;
  }
  sqlite3_bind_text(st, 1, [phrase UTF8String], -1, SQLITE_TRANSIENT);
  BOOL exists = (sqlite3_step(st) == SQLITE_ROW);
  sqlite3_finalize(st);
  return exists;
}

- (void)setPhrase:(NSString *)phrase forRowid:(long long)rowid {
  if (!_userDB || ![phrase length]) return;

  NSString *reading = [self defaultReadingForPhrase:phrase];
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(
          _userDB,
          "UPDATE user_unigrams SET qstring = ?, current = ? WHERE rowid = ?",
          -1, &st, NULL) != SQLITE_OK) {
    return;
  }
  sqlite3_bind_text(st, 1, PEQstringFromComposed(PEStdString(reading)).c_str(),
                    -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 2, [phrase UTF8String], -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(st, 3, rowid);
  sqlite3_step(st);
  sqlite3_finalize(st);
  [self _markDirtyAndScheduleChangeNotification];
}

- (void)setReading:(NSString *)reading forRowid:(long long)rowid {
  if (!_userDB) return;

  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(_userDB,
                         "UPDATE user_unigrams SET qstring = ? WHERE rowid = ?",
                         -1, &st, NULL) != SQLITE_OK) {
    return;
  }
  sqlite3_bind_text(st, 1, PEQstringFromComposed(PEStdString(reading)).c_str(),
                    -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(st, 2, rowid);
  sqlite3_step(st);
  sqlite3_finalize(st);
  [self _markDirtyAndScheduleChangeNotification];
}

- (void)deletePhrasesWithRowids:(NSArray *)rowids {
  if (!_userDB || ![rowids count]) return;

  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(_userDB, "DELETE FROM user_unigrams WHERE rowid = ?",
                         -1, &st, NULL) != SQLITE_OK) {
    return;
  }
  sqlite3_exec(_userDB, "BEGIN", NULL, NULL, NULL);
  for (NSNumber *rowid in rowids) {
    sqlite3_reset(st);
    sqlite3_bind_int64(st, 1, [rowid longLongValue]);
    sqlite3_step(st);
  }
  sqlite3_exec(_userDB, "COMMIT", NULL, NULL, NULL);
  sqlite3_finalize(st);
  [self _markDirtyAndScheduleChangeNotification];
}

#pragma mark Reading derivation

// Split out so a test can confine the search: the bundle lookup below reaches
// the installed IME regardless of CFFIXED_USER_HOME.
- (NSMutableArray *)_lexiconCandidatePaths {
  NSMutableArray *candidates = [NSMutableArray array];
  // The lexicon auto-updater installs here; prefer it.
  NSString *activeDir = [[self _userDataDirectory]
      stringByAppendingPathComponent:@"Lexicons/active"];
  [candidates
      addObject:[activeDir stringByAppendingPathComponent:@"ChiaKeySource.db"]];
  [candidates
      addObject:[activeDir stringByAppendingPathComponent:@"KeyKeySource.db"]];

  // Fall back to the lexicon bundled inside the IME app, which the IME itself
  // uses before one is installed. Without this, a fresh or offline install
  // would derive every reading as the "ㄅ" placeholder.
  //
  // We ship inside that bundle (…/ChiaKey.app/Contents/SharedSupport/), so
  // walk up from our own path first. A bundle-identifier lookup is the
  // fallback's fallback: with a dev install sitting next to a release one it
  // can hand back the other app's lexicon.
  NSURL *imeURL = nil;
  NSString *enclosing = [[[[[NSBundle mainBundle] bundlePath]
      stringByDeletingLastPathComponent]  // SharedSupport
      stringByDeletingLastPathComponent]  // Contents
      stringByDeletingLastPathComponent];
  if ([[enclosing pathExtension] isEqualToString:@"app"]) {
    imeURL = [NSURL fileURLWithPath:enclosing];
  }
  if (!imeURL) {
    imeURL = [[NSWorkspace sharedWorkspace]
        URLForApplicationWithBundleIdentifier:kChiaKeyIMEBundleIdentifier];
  }
  if (imeURL) {
    NSString *bundledDir = [[imeURL path]
        stringByAppendingPathComponent:@"Contents/Resources/Databases"];
    [candidates
        addObject:[bundledDir
                      stringByAppendingPathComponent:@"ChiaKeySource.db"]];
    [candidates
        addObject:[bundledDir
                      stringByAppendingPathComponent:@"KeyKeySource.db"]];
  }

  return candidates;
}

- (sqlite3 *)_lexiconDB {
  if (_lexTried) return _lexDB;
  _lexTried = YES;

  NSFileManager *fm = [NSFileManager defaultManager];
  for (NSString *path in [self _lexiconCandidatePaths]) {
    if (![fm fileExistsAtPath:path]) continue;
    sqlite3 *db = NULL;
    if (sqlite3_open_v2([path UTF8String], &db, SQLITE_OPEN_READONLY, NULL) ==
        SQLITE_OK) {
      sqlite3_stmt *probe = NULL;
      if (sqlite3_prepare_v2(db, "SELECT qstring FROM unigrams LIMIT 1", -1,
                             &probe, NULL) == SQLITE_OK) {
        sqlite3_finalize(probe);
        _lexDB = db;
        break;
      }
    }
    if (db) sqlite3_close(db);
  }
  return _lexDB;
}

- (BOOL)isLexiconAvailable {
  return [self _lexiconDB] != NULL;
}

- (NSArray *)readingsForCharacter:(NSString *)character {
  NSMutableArray *result = [NSMutableArray array];
  NSMutableSet *seen = [NSMutableSet set];
  sqlite3 *lex = [self _lexiconDB];

  if (lex) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(lex,
                           "SELECT qstring FROM unigrams WHERE current = ? "
                           "ORDER BY probability DESC",
                           -1, &st, NULL) == SQLITE_OK) {
      sqlite3_bind_text(st, 1, [character UTF8String], -1, SQLITE_TRANSIENT);
      while (sqlite3_step(st) == SQLITE_ROW) {
        const char *q = (const char *)sqlite3_column_text(st, 0);
        if (!q) continue;
        std::string qstring(q);
        // Skip special entries (e.g. "xxx#" markers), same as the IME.
        if (qstring.size() && qstring[qstring.size() - 1] == '#') continue;
        NSString *composed = PENSString(PEComposedFromQstring(qstring));
        if (![composed length] || [seen containsObject:composed]) continue;
        [seen addObject:composed];
        [result addObject:composed];
      }
      sqlite3_finalize(st);
    }

    // Rare characters missing from unigrams: fall back to the bpmf table,
    // whose keys are single-syllable absolute-order strings.
    if (![result count]) {
      sqlite3_stmt *cin = NULL;
      if (sqlite3_prepare_v2(
              lex, "SELECT key FROM 'Mandarin-bpmf-cin' WHERE value = ?", -1,
              &cin, NULL) == SQLITE_OK) {
        sqlite3_bind_text(cin, 1, [character UTF8String], -1, SQLITE_TRANSIENT);
        while (sqlite3_step(cin) == SQLITE_ROW) {
          const char *key = (const char *)sqlite3_column_text(cin, 0);
          if (!key) continue;
          NSString *composed = PENSString(PEComposedFromQstring(key));
          if (![composed length] || [seen containsObject:composed]) continue;
          [seen addObject:composed];
          [result addObject:composed];
        }
        sqlite3_finalize(cin);
      }
    }
  }

  if (![result count]) [result addObject:@"ㄅ"];  // last-resort placeholder
  return result;
}

- (NSString *)defaultReadingForPhrase:(NSString *)phrase {
  NSMutableArray *parts = [NSMutableArray array];
  NSUInteger idx = 0;
  NSUInteger len = [phrase length];
  while (idx < len) {
    NSRange r = [phrase rangeOfComposedCharacterSequenceAtIndex:idx];
    NSString *ch = [phrase substringWithRange:r];
    idx = r.location + r.length;
    [parts addObject:[[self readingsForCharacter:ch] objectAtIndex:0]];
  }
  return [parts componentsJoinedByString:@","];
}

#pragma mark Import / export (MJSR 1.0.0, matching BPMFUserPhraseHelper)

static NSString *PEHexEncodeFile(NSString *path) {
  NSData *data = [NSData dataWithContentsOfFile:path];
  if (![data length]) return nil;

  const unsigned char *bytes = (const unsigned char *)[data bytes];
  NSUInteger length = [data length];
  NSMutableString *out =
      [NSMutableString stringWithCapacity:length * 2 + length / 15];
  for (NSUInteger i = 0; i < length; i++) {
    // Same layout as the IME's export: 30 bytes per line.
    if (!(i % 30)) [out appendString:@"\n"];
    [out appendFormat:@"%02x", bytes[i]];
  }
  return out;
}

static NSData *PEHexDecode(NSString *hex) {
  NSMutableData *data = [NSMutableData dataWithCapacity:[hex length] / 2];
  unsigned int byte = 0;
  int nibbles = 0;
  for (NSUInteger i = 0; i < [hex length]; i++) {
    unichar c = [hex characterAtIndex:i];
    unsigned int v;
    if (c >= '0' && c <= '9') {
      v = c - '0';
    } else if (c >= 'a' && c <= 'f') {
      v = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      v = c - 'A' + 10;
    } else {
      continue;  // whitespace / line breaks
    }
    byte = (byte << 4) | v;
    if (++nibbles == 2) {
      unsigned char b = (unsigned char)byte;
      [data appendBytes:&b length:1];
      byte = 0;
      nibbles = 0;
    }
  }
  return data;
}

- (NSString *)_tempDatabasePath {
  NSString *name = [NSString
      stringWithFormat:@"ChiaKeyPhraseEditorExport-%@.db",
                       [[NSProcessInfo processInfo] globallyUniqueString]];
  return [NSTemporaryDirectory() stringByAppendingPathComponent:name];
}

- (BOOL)exportUserPhraseDBToFile:(NSString *)path {
  if (!_userDB) return NO;

  NSMutableString *out =
      [NSMutableString stringWithString:@"MJSR version 1.0.0\n"];

  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(_userDB,
                         "SELECT qstring, current, probability, backoff "
                         "FROM user_unigrams",
                         -1, &st, NULL) != SQLITE_OK) {
    return NO;
  }
  while (sqlite3_step(st) == SQLITE_ROW) {
    const char *q = (const char *)sqlite3_column_text(st, 0);
    const char *cur = (const char *)sqlite3_column_text(st, 1);
    const char *prob = (const char *)sqlite3_column_text(st, 2);
    const char *backoff = (const char *)sqlite3_column_text(st, 3);
    if (!cur) continue;

    std::string qstring(q ? q : "");
    // Same exclusions as BPMFUserPhraseHelper::Export.
    if (qstring.find("punctuation") != std::string::npos ||
        qstring.find("passthru") != std::string::npos)
      continue;

    // %s mangles non-ASCII in NSString formatting; go through NSString.
    [out appendFormat:@"%@\t%@\t%s\t%s\n", PENSString(cur),
                      PENSString(PEComposedFromQstring(qstring)),
                      prob ? prob : "-1.0", backoff ? backoff : "0.0"];
  }
  sqlite3_finalize(st);

  // Learning data (bigram + candidate-override caches) as a hex-encoded side
  // database. The two original tables stay byte-compatible with older ChiaKey
  // exports; user_learning_stats rides along beside them, and older builds
  // simply never look at it because their import names its columns explicitly.
  NSString *tempPath = [self _tempDatabasePath];
  char *sql = sqlite3_mprintf(
      // No KEY: see BPMFUserPhraseHelper::Export.
      "ATTACH DATABASE %Q AS export;"
      "CREATE TABLE export.user_bigram_cache "
      "(qstring, previous, current, probability);"
      "CREATE TABLE export.user_candidate_override_cache (qstring, current);"
      "CREATE TABLE export.user_context_override_cache (qstring, current);"
      "CREATE TABLE export.user_learning_stats "
      "(store, qstring, selection_count, last_used);"
      "INSERT INTO export.user_bigram_cache "
      "SELECT qstring, previous, current, probability FROM user_bigram_cache;"
      "INSERT INTO export.user_candidate_override_cache "
      "SELECT qstring, current FROM user_candidate_override_cache;"
      "INSERT INTO export.user_context_override_cache "
      "SELECT qstring, current FROM user_context_override_cache;"
      "INSERT INTO export.user_learning_stats "
      "SELECT store, qstring, selection_count, last_used FROM "
      "user_learning_stats;"
      "DETACH DATABASE export;",
      [tempPath UTF8String]);
  int attachResult = sqlite3_exec(_userDB, sql, NULL, NULL, NULL);
  sqlite3_free(sql);

  if (attachResult == SQLITE_OK) {
    NSString *hex = PEHexEncodeFile(tempPath);
    if (hex) {
      [out appendString:
               @"\n# What follows is the \"Automatic Learning\" "
               @"database, do not remove this\n"];
      [out appendString:@"<database>"];
      [out appendString:hex];
      [out appendString:@"\n</database>\n"];
    }
  }
  [[NSFileManager defaultManager] removeItemAtPath:tempPath error:NULL];

  return [out writeToFile:path
               atomically:YES
                 encoding:NSUTF8StringEncoding
                    error:NULL];
}

- (BOOL)_phraseExistsWithQstring:(const std::string &)qstring
                          phrase:(NSString *)phrase {
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(_userDB,
                         "SELECT 1 FROM user_unigrams WHERE qstring = ? AND "
                         "current = ? LIMIT 1",
                         -1, &st, NULL) != SQLITE_OK) {
    return NO;
  }
  sqlite3_bind_text(st, 1, qstring.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 2, [phrase UTF8String], -1, SQLITE_TRANSIENT);
  BOOL exists = (sqlite3_step(st) == SQLITE_ROW);
  sqlite3_finalize(st);
  return exists;
}

// Does the current lexicon -- or the user's own phrases -- know this text
// under this reading? Learning-cache entries name a text outright rather than
// weighting one, so an entry naming something unreachable is not inert: it
// puts that text in front of the walker anyway.
- (BOOL)_reading:(const std::string &)qstring canProduce:(const char *)text {
  if (!text || !qstring.size()) return NO;

  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(_userDB,
                         "SELECT 1 FROM user_unigrams WHERE qstring = ? AND "
                         "current = ? LIMIT 1",
                         -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, qstring.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, text, -1, SQLITE_TRANSIENT);
    BOOL found = (sqlite3_step(st) == SQLITE_ROW);
    sqlite3_finalize(st);
    if (found) return YES;
  }

  sqlite3 *lex = [self _lexiconDB];
  if (!lex) return YES;  // cannot judge; keep the entry rather than lose it

  if (sqlite3_prepare_v2(
          lex,
          "SELECT 1 FROM unigrams WHERE qstring = ? AND current = ? LIMIT 1",
          -1, &st, NULL) != SQLITE_OK) {
    return YES;
  }
  sqlite3_bind_text(st, 1, qstring.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 2, text, -1, SQLITE_TRANSIENT);
  BOOL found = (sqlite3_step(st) == SQLITE_ROW);
  sqlite3_finalize(st);
  return found;
}

// Bigram and context-override keys are two readings joined by a space; the
// entry names a text for the second one.
static std::string PECurrentReadingOfKey(const std::string &key) {
  size_t space = key.rfind(' ');
  return space == std::string::npos ? key : key.substr(space + 1);
}

// The qstrings present in a table, used to scope the filter below to the rows
// an import actually brought in.
- (NSSet *)_keysOfTable:(NSString *)table {
  NSMutableSet *keys = [NSMutableSet set];
  char *sql = sqlite3_mprintf("SELECT qstring FROM %s", [table UTF8String]);
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(_userDB, sql, -1, &st, NULL) == SQLITE_OK) {
    while (sqlite3_step(st) == SQLITE_ROW) {
      const char *key = (const char *)sqlite3_column_text(st, 0);
      if (key) [keys addObject:PENSString(key)];
    }
    sqlite3_finalize(st);
  }
  sqlite3_free(sql);
  return keys;
}

// Drops learning-cache rows the current lexicon cannot produce. Only worth
// doing for entries from another input method: our own were learned against
// this same lexicon, so `scope` limits this to the keys just imported.
// The keys `exportTable` would add to `table`: what it holds, minus what is
// already there. Must be called before the merge.
- (NSSet *)_keysOf:(NSString *)exportTable missingFrom:(NSString *)table {
  NSMutableSet *added =
      [[[self _keysOfTable:exportTable] mutableCopy] autorelease];
  [added minusSet:[self _keysOfTable:table]];
  return added;
}

- (NSUInteger)_dropLearningEntriesUnknownToLexicon:(NSDictionary *)scope {
  static const struct {
    const char *table;
    BOOL keyIsCombined;
    const char *store;  // matching user_learning_stats rows, if any
  } tables[] = {
      {"user_bigram_cache", YES, "bigram"},
      {"user_candidate_override_cache", NO, "override"},
      {"user_context_override_cache", YES, NULL},
  };

  NSUInteger dropped = 0;
  for (size_t i = 0; i < sizeof(tables) / sizeof(tables[0]); i++) {
    NSSet *only =
        [scope objectForKey:[NSString stringWithUTF8String:tables[i].table]];
    if (scope && !only) continue;

    NSMutableArray *doomed = [NSMutableArray array];
    char *sql =
        sqlite3_mprintf("SELECT qstring, current FROM %s", tables[i].table);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(_userDB, sql, -1, &st, NULL) == SQLITE_OK) {
      while (sqlite3_step(st) == SQLITE_ROW) {
        const char *key = (const char *)sqlite3_column_text(st, 0);
        const char *text = (const char *)sqlite3_column_text(st, 1);
        if (!key) continue;
        if (only && ![only containsObject:PENSString(key)]) continue;
        std::string reading =
            tables[i].keyIsCombined ? PECurrentReadingOfKey(key) : key;
        if (![self _reading:reading canProduce:text]) {
          [doomed addObject:PENSString(key)];
        }
      }
      sqlite3_finalize(st);
    }
    sqlite3_free(sql);

    for (NSString *key in doomed) {
      char *del = sqlite3_mprintf("DELETE FROM %s WHERE qstring = %Q",
                                  tables[i].table, [key UTF8String]);
      if (sqlite3_exec(_userDB, del, NULL, NULL, NULL) == SQLITE_OK) dropped++;
      sqlite3_free(del);

      if (!tables[i].store) continue;
      char *stats = sqlite3_mprintf(
          "DELETE FROM user_learning_stats WHERE store = %Q AND qstring = %Q",
          tables[i].store, [key UTF8String]);
      sqlite3_exec(_userDB, stats, NULL, NULL, NULL);
      sqlite3_free(stats);
    }
  }
  return dropped;
}

- (BOOL)importUserPhraseDBFromFile:(NSString *)path {
  return [self _importFromFile:path legacy:NO learningDataRestored:NULL];
}

- (BOOL)importUserPhraseDBFromFile:(NSString *)path
              learningDataRestored:(BOOL *)learningDataRestored {
  return [self _importFromFile:path
                        legacy:NO
          learningDataRestored:learningDataRestored];
}

- (BOOL)importLegacyUserPhraseDBFromFile:(NSString *)path {
  return [self _importFromFile:path legacy:YES learningDataRestored:NULL];
}

- (BOOL)importLegacyUserPhraseDBFromFile:(NSString *)path
                    learningDataRestored:(BOOL *)learningDataRestored {
  return [self _importFromFile:path
                        legacy:YES
          learningDataRestored:learningDataRestored];
}

- (BOOL)_importFromFile:(NSString *)path
                 legacy:(BOOL)legacy
   learningDataRestored:(BOOL *)learningDataRestored {
  if (learningDataRestored) *learningDataRestored = NO;
  if (!_userDB) return NO;

  // fstat() the descriptor that is actually read, so the file cannot be
  // swapped between the checks and the read; a symlink's own size never
  // enters into it either.
  int fd = open([path fileSystemRepresentation], O_RDONLY);
  if (fd < 0) return NO;
  struct stat info;
  if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
    NSLog(@"Refusing to import %@: not a regular file", path);
    close(fd);
    return NO;
  }
  unsigned long long fileSize = (unsigned long long)info.st_size;
  if (fileSize > kPEMaxImportFileSize) {
    NSLog(@"Refusing to import %@: %llu bytes exceeds the %llu byte limit",
          path, fileSize, kPEMaxImportFileSize);
    close(fd);
    return NO;
  }

  NSFileHandle *handle =
      [[[NSFileHandle alloc] initWithFileDescriptor:fd
                                     closeOnDealloc:YES] autorelease];
  NSData *raw = [handle readDataOfLength:(NSUInteger)fileSize];
  NSString *content =
      [[[NSString alloc] initWithData:raw
                             encoding:NSUTF8StringEncoding] autorelease];
  if (!content) return NO;

  sqlite3_stmt *insert = NULL;
  if (sqlite3_prepare_v2(_userDB,
                         "INSERT INTO user_unigrams VALUES (?, ?, ?, ?)", -1,
                         &insert, NULL) != SQLITE_OK) {
    return NO;
  }

  // Walked line by line: an array of lines costs one NSString per line, which
  // outweighs the file itself once a backup runs to hundreds of thousands.
  NSMutableString *hex = [NSMutableString string];
  __block BOOL headerChecked = NO;
  __block BOOL headerValid = NO;
  __block BOOL sawDatabaseSection = NO;
  __block BOOL blobTooLarge = NO;

  sqlite3_exec(_userDB, "BEGIN", NULL, NULL, NULL);

  [content enumerateLinesUsingBlock:^(NSString *line, BOOL *stop) {
    if (!headerChecked) {
      headerChecked = YES;
      headerValid =
          [line rangeOfString:@"MJSR version 1.0.0"].location != NSNotFound;
      if (!headerValid) *stop = YES;
      return;
    }

    if (sawDatabaseSection) {
      if ([line rangeOfString:@"</database>"].location != NSNotFound) {
        *stop = YES;
        return;
      }
      if (blobTooLarge) return;

      // Before appending, so one huge line is never held whole either.
      if (([hex length] + [line length]) / 2 > kPEMaxLearningBlobSize) {
        NSLog(@"Ignoring the learning database in %@: over the %lu byte limit",
              path, (unsigned long)kPEMaxLearningBlobSize);
        blobTooLarge = YES;
        [hex setString:@""];
        return;
      }

      [hex appendString:line];
      return;
    }

    if ([line hasPrefix:@"#"]) return;
    if ([line rangeOfString:@"<database>"].location != NSNotFound) {
      sawDatabaseSection = YES;
      return;
    }

    NSMutableArray *fields = [NSMutableArray array];
    for (NSString *tok in [line componentsSeparatedByCharactersInSet:
                                    [NSCharacterSet whitespaceCharacterSet]]) {
      if ([tok length]) [fields addObject:tok];
    }
    if ([fields count] < 2) return;

    NSString *phrase = [fields objectAtIndex:0];
    NSString *reading = [fields objectAtIndex:1];
    std::string qstring = PEQstringFromComposed(PEStdString(reading));
    // Reading must cover the phrase, character for character.
    NSUInteger codePoints = 0;
    for (NSUInteger i = 0; i < [phrase length]; codePoints++) {
      i = NSMaxRange([phrase rangeOfComposedCharacterSequenceAtIndex:i]);
    }
    if (!qstring.size() || qstring.size() / 2 != codePoints) return;
    if ([self _phraseExistsWithQstring:qstring phrase:phrase]) return;

    // A legacy file carries probabilities estimated against Yahoo! KeyKey's
    // lexicon, and they are read as-is: user_unigrams is UNIONed straight into
    // the unigram lookup, where they compete with our own numbers. Left alone,
    // a phrase the user deliberately added years ago can land below this
    // lexicon's median and never win. Give it what a hand-added phrase gets.
    const char *prob =
        legacy ? "-1.0"
               : ([fields count] > 2 ? [[fields objectAtIndex:2] UTF8String]
                                     : "-1.0");
    const char *backoff =
        legacy ? "0.0"
               : ([fields count] > 3 ? [[fields objectAtIndex:3] UTF8String]
                                     : "0.0");

    sqlite3_reset(insert);
    sqlite3_bind_text(insert, 1, qstring.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert, 2, [phrase UTF8String], -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert, 3, prob, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert, 4, backoff, -1, SQLITE_TRANSIENT);
    sqlite3_step(insert);
  }];

  // Nothing was inserted if the header never matched; unwind instead.
  sqlite3_exec(_userDB, headerValid ? "COMMIT" : "ROLLBACK", NULL, NULL, NULL);
  sqlite3_finalize(insert);
  if (!headerValid) return NO;

  // Restore the learning-data blob, if present. Refusing the block for size
  // is reported; a block we cannot read is not (matches the C++ importer).
  BOOL cacheRestored = !blobTooLarge;
  if (sawDatabaseSection) {
    NSData *blob = PEHexDecode(hex);
    // Files written by Yahoo! KeyKey carry this block encrypted (SQLite SEE);
    // ours are in the clear. DecryptExportDatabase tells the two apart and
    // leaves the blob empty when it is neither, so an unreadable block costs
    // the caller its learning caches but not the phrases imported above.
    std::string cacheData((const char *)[blob bytes], [blob length]);
    if (!Manjusri::DecryptExportDatabase(cacheData)) cacheData.clear();

    if (cacheData.size()) {
      cacheRestored = NO;
      NSString *tempPath = [self _tempDatabasePath];
      // Wraps the bytes rather than copying the whole block again.
      NSData *decoded = [NSData dataWithBytesNoCopy:(void *)cacheData.data()
                                             length:cacheData.size()
                                       freeWhenDone:NO];
      if ([decoded writeToFile:tempPath atomically:YES]) {
        cacheRestored = [self _restoreLearningCachesFromDatabase:tempPath
                                                          legacy:legacy];
        [[NSFileManager defaultManager] removeItemAtPath:tempPath error:NULL];
      }
    }
  }

  [self _markDirtyAndScheduleChangeNotification];
  if (learningDataRestored) *learningDataRestored = cacheRestored;
  return YES;
}

- (BOOL)_table:(const char *)table existsInSchema:(const char *)schema {
  char *sql = sqlite3_mprintf(
      "SELECT COUNT(*) FROM %s.sqlite_master WHERE type = 'table' AND "
      "name = %Q",
      schema, table);
  sqlite3_stmt *st = NULL;
  BOOL exists = NO;
  if (sqlite3_prepare_v2(_userDB, sql, -1, &st, NULL) == SQLITE_OK) {
    while (sqlite3_step(st) == SQLITE_ROW)
      exists = sqlite3_column_int(st, 0) > 0;
    sqlite3_finalize(st);
  }
  sqlite3_free(sql);
  return exists;
}

// Mirrors BPMFUserPhraseHelper's RestoreLearningCaches: all or nothing, so a
// failure part way through cannot leave the caches emptied while the import
// still reports success.
- (BOOL)_restoreLearningCachesFromDatabase:(NSString *)path
                                    legacy:(BOOL)legacy {
  const Manjusri::LearningCacheTable *tables = Manjusri::kLearningCacheTables;

  char *attach =
      sqlite3_mprintf("ATTACH DATABASE %Q AS export", [path UTF8String]);
  int attached = sqlite3_exec(_userDB, attach, NULL, NULL, NULL);
  sqlite3_free(attach);
  if (attached != SQLITE_OK) return NO;

  NSSet *newBigrams = nil;
  NSSet *newOverrides = nil;
  if (legacy) {
    // Only entries this file actually adds are subject to the reachability
    // filter below -- "imported minus already here", not just "imported". A
    // key the file and the user share keeps the user's value (OR IGNORE
    // below), and judging that value against the file's intent would delete
    // something this import never touched, stats and all.
    newBigrams = [self _keysOf:@"export.user_bigram_cache"
                   missingFrom:@"user_bigram_cache"];
    newOverrides = [self _keysOf:@"export.user_candidate_override_cache"
                     missingFrom:@"user_candidate_override_cache"];
  }

  BOOL ok = sqlite3_exec(_userDB, "BEGIN", NULL, NULL, NULL) == SQLITE_OK;

  for (size_t i = 0; ok && i < Manjusri::kLearningCacheTableCount; i++) {
    // An older or foreign file has nothing for the newer stores; keep what
    // is here.
    if (![self _table:tables[i].name existsInSchema:"export"]) continue;

    if (![self _table:tables[i].name existsInSchema:"main"]) {
      char *create = sqlite3_mprintf("CREATE TABLE %s (%s)", tables[i].name,
                                     tables[i].columns);
      ok = sqlite3_exec(_userDB, create, NULL, NULL, NULL) == SQLITE_OK;
      sqlite3_free(create);
      if (!ok) break;
      // Just created, so no duplicates for the unique key to trip over.
      sqlite3_exec(_userDB, tables[i].uniqueIndex, NULL, NULL, NULL);
    }

    // Restoring one of our own backups means restoring it: the file is a
    // snapshot of these tables and replaces them wholesale. A legacy file
    // is a different thing -- it comes from another input method while
    // ChiaKey is already in use, so it merges, and on a collision the
    // entry the user has been training here wins. Same rule as the
    // plain-text files: importing never overwrites current work.
    if (!legacy) {
      char *del = sqlite3_mprintf("DELETE FROM %s", tables[i].name);
      ok = sqlite3_exec(_userDB, del, NULL, NULL, NULL) == SQLITE_OK;
      sqlite3_free(del);
      if (!ok) break;
    }

    // OR REPLACE/IGNORE because the cache tables carry a unique key on
    // qstring, and a hand-made or foreign blob may hold duplicates -- a
    // legacy KeyKey blob in particular keeps one row per (qstring,
    // previous) pair, where ours keeps one row per qstring.
    char *ins = sqlite3_mprintf("INSERT %s INTO %s (%s) SELECT %s FROM "
                                "export.%s",
                                legacy ? "OR IGNORE" : "OR REPLACE",
                                tables[i].name, tables[i].columns,
                                tables[i].columns, tables[i].name);
    ok = sqlite3_exec(_userDB, ins, NULL, NULL, NULL) == SQLITE_OK;
    sqlite3_free(ins);
  }

  if (ok) ok = sqlite3_exec(_userDB, "COMMIT", NULL, NULL, NULL) == SQLITE_OK;
  if (!ok) sqlite3_exec(_userDB, "ROLLBACK", NULL, NULL, NULL);

  sqlite3_exec(_userDB, "DETACH DATABASE export", NULL, NULL, NULL);

  // After the phrases are in, so an entry naming a phrase this file also
  // brought over counts as reachable.
  if (ok && legacy) {
    [self _dropLearningEntriesUnknownToLexicon:@{
      @"user_bigram_cache" : newBigrams ? newBigrams : [NSSet set],
      @"user_candidate_override_cache" : newOverrides ? newOverrides
                                                      : [NSSet set],
    }];
  }

  return ok;
}

@end
