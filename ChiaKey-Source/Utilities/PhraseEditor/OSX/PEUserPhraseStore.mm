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

#import "ChiaKeyServiceCoordination.h"
#import "ChiaKeyUserPhraseCoordination.h"

#include <sqlite3.h>

#include <string>
#include <vector>

#include "Mandarin.h"

using Formosa::Mandarin::BPMF;

static NSString *const kChiaKeyLoaderName = @"ChiaKey";
// Used only to locate the lexicon bundled inside the running IME app as a
// last-resort source for reading derivation (see -_lexiconDB).
static NSString *const kChiaKeyIMEBundleIdentifier =
    @"com.chiakey.inputmethod.ChiaKey";
static const NSTimeInterval kChangeNotificationThrottle = 0.5;
// Keep the editing lock visibly fresh, well within the staleness timeout.
static const NSTimeInterval kEditingLockRefreshInterval = 60.0;

#pragma mark - Codec helpers (Formosa)

// qstring (2 bytes per syllable, absolute order) -> "ㄋㄧˇ,ㄏㄠˇ"
static std::string PEComposedFromQstring(const std::string &qstring) {
  std::string result;
  if (qstring.size() % 2) return result;
  for (size_t i = 0; i < qstring.size(); i += 2) {
    if (i) result += ",";
    result += BPMF::FromAbsoluteOrderString(qstring.substr(i, 2))
                  .composedString();
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
      // Per-entry learning statistics live beside the two cache tables rather
      // than as extra columns in them, for the same landmine: the IME's
      // positional INSERTs would break.
      "CREATE TABLE IF NOT EXISTS user_learning_stats "
      "(store, qstring, selection_count, last_used);"
      "CREATE UNIQUE INDEX IF NOT EXISTS user_learning_stats_key "
      "ON user_learning_stats (store, qstring);";
  sqlite3_exec(_userDB, schema, NULL, NULL, NULL);
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
  _changeNotificationTimer =
      [[NSTimer scheduledTimerWithTimeInterval:kChangeNotificationThrottle
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
  std::string sql = "SELECT COUNT(*) FROM user_unigrams " +
                    PEFilterClause(filter, params);

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
    sqlite3_bind_text(st, bindIndex++, params[i].c_str(), -1,
                      SQLITE_TRANSIENT);
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

- (sqlite3 *)_lexiconDB {
  if (_lexTried) return _lexDB;
  _lexTried = YES;

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
  NSURL *imeURL = [[NSWorkspace sharedWorkspace]
      URLForApplicationWithBundleIdentifier:kChiaKeyIMEBundleIdentifier];
  if (imeURL) {
    NSString *bundledDir = [[imeURL path]
        stringByAppendingPathComponent:@"Contents/Resources/Databases"];
    [candidates addObject:[bundledDir
                              stringByAppendingPathComponent:@"ChiaKeySource.db"]];
    [candidates addObject:[bundledDir
                              stringByAppendingPathComponent:@"KeyKeySource.db"]];
  }

  NSFileManager *fm = [NSFileManager defaultManager];
  for (NSString *path in candidates) {
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
        sqlite3_bind_text(cin, 1, [character UTF8String], -1,
                          SQLITE_TRANSIENT);
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
      "ATTACH DATABASE %Q AS export KEY 'mjsrexport';"
      "CREATE TABLE export.user_bigram_cache "
      "(qstring, previous, current, probability);"
      "CREATE TABLE export.user_candidate_override_cache (qstring, current);"
      "CREATE TABLE export.user_learning_stats "
      "(store, qstring, selection_count, last_used);"
      "INSERT INTO export.user_bigram_cache "
      "SELECT qstring, previous, current, probability FROM user_bigram_cache;"
      "INSERT INTO export.user_candidate_override_cache "
      "SELECT qstring, current FROM user_candidate_override_cache;"
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
      [out appendString:@"\n# What follows is the \"Automatic Learning\" "
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

- (BOOL)importUserPhraseDBFromFile:(NSString *)path {
  if (!_userDB) return NO;
  NSString *content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:NULL];
  if (!content) return NO;

  NSArray *lines = [content componentsSeparatedByCharactersInSet:
                                [NSCharacterSet newlineCharacterSet]];
  if (![lines count] ||
      [(NSString *)[lines objectAtIndex:0] rangeOfString:@"MJSR version 1.0.0"]
              .location == NSNotFound) {
    return NO;
  }

  sqlite3_stmt *insert = NULL;
  if (sqlite3_prepare_v2(_userDB,
                         "INSERT INTO user_unigrams VALUES (?, ?, ?, ?)", -1,
                         &insert, NULL) != SQLITE_OK) {
    return NO;
  }

  sqlite3_exec(_userDB, "BEGIN", NULL, NULL, NULL);
  NSUInteger lineIndex = 1;
  BOOL sawDatabaseSection = NO;
  for (; lineIndex < [lines count]; lineIndex++) {
    NSString *line = [lines objectAtIndex:lineIndex];
    if ([line hasPrefix:@"#"]) continue;
    if ([line rangeOfString:@"<database>"].location != NSNotFound) {
      sawDatabaseSection = YES;
      lineIndex++;
      break;
    }

    NSMutableArray *fields = [NSMutableArray array];
    for (NSString *tok in
         [line componentsSeparatedByCharactersInSet:
                   [NSCharacterSet whitespaceCharacterSet]]) {
      if ([tok length]) [fields addObject:tok];
    }
    if ([fields count] < 2) continue;

    NSString *phrase = [fields objectAtIndex:0];
    NSString *reading = [fields objectAtIndex:1];
    std::string qstring = PEQstringFromComposed(PEStdString(reading));
    // Reading must cover the phrase, character for character.
    NSUInteger codePoints = 0;
    for (NSUInteger i = 0; i < [phrase length]; codePoints++) {
      i = NSMaxRange([phrase rangeOfComposedCharacterSequenceAtIndex:i]);
    }
    if (!qstring.size() || qstring.size() / 2 != codePoints) continue;
    if ([self _phraseExistsWithQstring:qstring phrase:phrase]) continue;

    const char *prob =
        [fields count] > 2 ? [[fields objectAtIndex:2] UTF8String] : "-1.0";
    const char *backoff =
        [fields count] > 3 ? [[fields objectAtIndex:3] UTF8String] : "0.0";

    sqlite3_reset(insert);
    sqlite3_bind_text(insert, 1, qstring.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert, 2, [phrase UTF8String], -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert, 3, prob, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert, 4, backoff, -1, SQLITE_TRANSIENT);
    sqlite3_step(insert);
  }
  sqlite3_exec(_userDB, "COMMIT", NULL, NULL, NULL);
  sqlite3_finalize(insert);

  // Restore the learning-data blob, if present.
  if (sawDatabaseSection) {
    NSMutableString *hex = [NSMutableString string];
    for (; lineIndex < [lines count]; lineIndex++) {
      NSString *line = [lines objectAtIndex:lineIndex];
      if ([line rangeOfString:@"</database>"].location != NSNotFound) break;
      [hex appendString:line];
    }

    NSData *blob = PEHexDecode(hex);
    if ([blob length]) {
      NSString *tempPath = [self _tempDatabasePath];
      if ([blob writeToFile:tempPath atomically:YES]) {
        // OR REPLACE because the cache tables now carry a unique key on
        // qstring, and a hand-made or foreign blob may hold duplicates.
        char *sql = sqlite3_mprintf(
            "ATTACH DATABASE %Q AS export KEY 'mjsrexport';"
            "DELETE FROM user_bigram_cache;"
            "INSERT OR REPLACE INTO user_bigram_cache (qstring, previous, "
            "current, probability) SELECT qstring, previous, current, "
            "probability FROM export.user_bigram_cache;"
            "DELETE FROM user_candidate_override_cache;"
            "INSERT OR REPLACE INTO user_candidate_override_cache "
            "(qstring, current) "
            "SELECT qstring, current FROM export.user_candidate_override_cache;"
            "DELETE FROM user_learning_stats;",
            [tempPath UTF8String]);
        sqlite3_exec(_userDB, sql, NULL, NULL, NULL);
        sqlite3_free(sql);

        // Separate step: a file written by an older ChiaKey has no stats table,
        // and that failure must not abort the DETACH. Entries without stats
        // fall back to a single selection when the IME loads them.
        sqlite3_exec(_userDB,
                     "INSERT OR REPLACE INTO user_learning_stats "
                     "(store, qstring, selection_count, last_used) "
                     "SELECT store, qstring, selection_count, last_used "
                     "FROM export.user_learning_stats",
                     NULL, NULL, NULL);
        sqlite3_exec(_userDB, "DETACH DATABASE export", NULL, NULL, NULL);
        [[NSFileManager defaultManager] removeItemAtPath:tempPath error:NULL];
      }
    }
  }

  [self _markDirtyAndScheduleChangeNotification];
  return YES;
}

@end
