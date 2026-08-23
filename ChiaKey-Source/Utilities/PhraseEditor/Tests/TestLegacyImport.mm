// Checks for importing a Yahoo! KeyKey export through PEUserPhraseStore --
// the path the "Import Yahoo! KeyKey Data…" button in Preferences takes.
//
// Two things separate it from importing one of our own backups. The file's
// numbers were estimated against KeyKey's lexicon, and user_unigrams
// probabilities are read as-is, so they are renormalized on the way in. And
// its learning caches name phrases that lexicon could produce, which is not
// the same set ours can, so unreachable entries are dropped.
//
// Runs against a temporary home (CFFIXED_USER_HOME) holding a fixture
// lexicon, so it never touches the real profile -- main() refuses to run if
// the store resolves anywhere else.
#import <Foundation/Foundation.h>
#include <sqlite3.h>

#include <fstream>
#include <iostream>
#include <string>

#import "MJSRExportCipher.h"
#import "Mandarin.h"
#import "PEUserPhraseStore.h"

using namespace std;
using Formosa::Mandarin::BPMF;

static int failures = 0;
static NSString *g_home = nil;

#define CHECK(cond)                                         \
  do {                                                      \
    if (!(cond)) {                                          \
      cerr << "FAIL " << __LINE__ << ": " << #cond << endl; \
      failures++;                                           \
    }                                                       \
  } while (0)

static const size_t kPageSize = 1024;
static const size_t kReserved = 32;

// "ㄑㄩㄢˊ" -> the two-byte absolute-order key the databases use.
static string Q(const string &composed) {
  string result;
  size_t start = 0;
  while (start <= composed.size()) {
    size_t comma = composed.find(',', start);
    string one = composed.substr(
        start, comma == string::npos ? string::npos : comma - start);
    BPMF b = BPMF::FromComposedString(one);
    if (!b.isEmpty()) result += b.absoluteOrderString();
    if (comma == string::npos) break;
    start = comma + 1;
  }
  return result;
}

static NSString *HomePath(NSString *relative) {
  return [g_home stringByAppendingPathComponent:relative];
}

static NSString *UserDataPath(NSString *name) {
  return [HomePath(@"Library/Application Support/ChiaKey")
      stringByAppendingPathComponent:name];
}

@interface PEUserPhraseStore (TestingSeam)
- (NSMutableArray *)_lexiconCandidatePaths;
@end

// CFFIXED_USER_HOME redirects NSHomeDirectory(), but the store's last-resort
// candidate is a bundle-identifier lookup for the installed IME, which no
// environment variable moves. On a machine with ChiaKey installed that made
// every lexicon lookup here reach the real app's bundled database -- so the
// "no lexicon at all" case could never happen, and the others would have
// passed even with a broken fixture. Dropping the candidates outside the
// temporary home leaves the real resolution logic under test.
@interface PEConfinedStore : PEUserPhraseStore
@end

@implementation PEConfinedStore
- (NSMutableArray *)_lexiconCandidatePaths {
  NSMutableArray *confined = [NSMutableArray array];
  for (NSString *path in [super _lexiconCandidatePaths]) {
    if ([path hasPrefix:g_home]) [confined addObject:path];
  }
  return confined;
}
@end

static void Exec(sqlite3 *db, const string &sql) {
  sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL);
}

static int CountRows(const string &table) {
  sqlite3 *db = NULL;
  sqlite3_open([UserDataPath(@"SmartMandarinUserData.db") UTF8String], &db);
  sqlite3_stmt *st = NULL;
  int count = -1;
  string sql = "SELECT COUNT(*) FROM " + table;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, NULL) == SQLITE_OK) {
    if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
  }
  sqlite3_close(db);
  return count;
}

static double ProbabilityOf(const string &phrase) {
  sqlite3 *db = NULL;
  sqlite3_open([UserDataPath(@"SmartMandarinUserData.db") UTF8String], &db);
  sqlite3_stmt *st = NULL;
  double probability = 999.0;
  if (sqlite3_prepare_v2(
          db, "SELECT probability FROM user_unigrams WHERE current = ?", -1,
          &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, phrase.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW)
      probability = sqlite3_column_double(st, 0);
    sqlite3_finalize(st);
  }
  sqlite3_close(db);
  return probability;
}

// ---------------------------------------------------------------- fixtures

// A lexicon that knows 權 and 重 but has never heard of 甲乙.
static void WriteFixtureLexicon() {
  NSString *dir = UserDataPath(@"Lexicons/active");
  [[NSFileManager defaultManager] createDirectoryAtPath:dir
                            withIntermediateDirectories:YES
                                             attributes:nil
                                                  error:NULL];
  NSString *path = [dir stringByAppendingPathComponent:@"ChiaKeySource.db"];
  [[NSFileManager defaultManager] removeItemAtPath:path error:NULL];

  sqlite3 *db = NULL;
  sqlite3_open([path UTF8String], &db);
  Exec(db, "CREATE TABLE unigrams (qstring, current, probability, backoff)");
  Exec(db, "INSERT INTO unigrams VALUES('" + Q("ㄑㄩㄢˊ") +
               "', '\xe6\xac\x8a', -2.0, 0.0)");
  Exec(db, "INSERT INTO unigrams VALUES('" + Q("ㄓㄨㄥˋ") +
               "', '\xe9\x87\x8d', -2.0, 0.0)");
  sqlite3_close(db);
}

// The cache database a KeyKey export carries, with one entry this lexicon can
// produce (權 -> 重) and one it cannot (乙).
static string BuildCacheDatabase() {
  NSString *path = HomePath(@"cache.db");
  [[NSFileManager defaultManager] removeItemAtPath:path error:NULL];

  sqlite3 *db = NULL;
  sqlite3_open([path UTF8String], &db);
  int reserved = (int)kReserved;
  Exec(db, "PRAGMA page_size=1024");
  sqlite3_file_control(db, "main", SQLITE_FCNTL_RESERVE_BYTES, &reserved);
  Exec(db,
       "CREATE TABLE user_bigram_cache (qstring, previous, current, "
       "probability);"
       "CREATE TABLE user_candidate_override_cache (qstring, current)");
  Exec(db, "INSERT INTO user_bigram_cache VALUES('" + Q("ㄑㄩㄢˊ") + " " +
               Q("ㄓㄨㄥˋ") +
               "', '\xe6\xac\x8a', '\xe9\x87\x8d', '-1.174761')");
  Exec(db, "INSERT INTO user_bigram_cache VALUES('" + Q("ㄐㄧㄚˇ") + " " +
               Q("ㄧˇ") + "', '\xe7\x94\xb2', '\xe4\xb9\x99', '-1.174761')");
  Exec(db, "INSERT INTO user_candidate_override_cache VALUES('" + Q("ㄑㄩㄢˊ") +
               "', '\xe6\xac\x8a')");
  Exec(db, "INSERT INTO user_candidate_override_cache VALUES('" + Q("ㄧˇ") +
               "', '\xe4\xb9\x99')");
  sqlite3_close(db);

  ifstream ifs([path UTF8String], ios::binary);
  string data((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
  ifs.close();
  [[NSFileManager defaultManager] removeItemAtPath:path error:NULL];
  return data;
}

// Same AES-128-CTR walk DecryptExportDatabase undoes; CTR is symmetric.
static string EncryptCacheDatabase(const string &plain) {
  unsigned char key[16];
  const char *phrase = MANJUSRI_EXPORT_KEY;
  size_t phraseSize = strlen(phrase);
  for (size_t i = 0; i < sizeof(key); ++i)
    key[i] = (unsigned char)phrase[i % phraseSize];
  Manjusri::AES128Encryptor cipher(key);

  string result(plain);
  for (size_t offset = 0; offset < result.size(); offset += kPageSize) {
    unsigned char *page = (unsigned char *)&result[offset];
    unsigned char counter[16];
    for (size_t i = 0; i < sizeof(counter); ++i) {
      counter[i] = (unsigned char)(0x20 + i + offset / kPageSize);
    }
    counter[4] = 0xf5;  // forces the counter to carry mid-page
    memcpy(page + kPageSize - 16, counter, sizeof(counter));

    unsigned int base =
        (unsigned int)counter[4] | ((unsigned int)counter[5] << 8) |
        ((unsigned int)counter[6] << 16) | ((unsigned int)counter[7] << 24);
    unsigned char keystream[16];
    for (size_t i = 0; i < kPageSize - kReserved; ++i) {
      if (!(i % 16)) {
        unsigned int value = (unsigned int)(base + i / 16);
        counter[4] = (unsigned char)(value);
        counter[5] = (unsigned char)(value >> 8);
        counter[6] = (unsigned char)(value >> 16);
        counter[7] = (unsigned char)(value >> 24);
        cipher.encryptBlock(counter, keystream);
      }
      page[i] ^= keystream[i % 16];
    }
    if (!offset) memcpy(page + 16, plain.data() + 16, 8);
  }
  return result;
}

// A KeyKey-shaped export: two phrases with KeyKey's own probabilities, then
// the encrypted cache block.
static NSString *WriteExportFile(NSString *name) {
  string block = EncryptCacheDatabase(BuildCacheDatabase());

  NSString *path = HomePath(name);
  ofstream ofs([path UTF8String], ios::binary);
  ofs << "MJSR version 1.0.0\n";
  ofs << "\xe6\xac\x8a\xe9\x87\x8d\t"
      << "\xe3\x84\xa9\xe3\x84\xa2\xcb\x87,\xe3\x84\x93\xe3\x84\xa8\xe3\x84\xa5"
         "\xcb\x8b"
      << "\t-3.5\t0.25\n";
  ofs << "\xe7\x94\xb2\xe4\xb9\x99\t"
      << "\xe3\x84\x90\xe3\x84\xa7\xe3\x84\x9a\xcb\x87,\xe3\x84\xa7\xcb\x87"
      << "\t-4.5\t0.25\n";
  ofs << "\n<database>";
  static const char *kHexDigits = "0123456789abcdef";
  for (size_t i = 0; i < block.size(); ++i) {
    if (!(i % 30)) ofs << "\n";
    unsigned char c = (unsigned char)block[i];
    ofs << kHexDigits[c >> 4] << kHexDigits[c & 0x0f];
  }
  ofs << "\n</database>\n";
  ofs.close();
  return path;
}

static void ResetUserDatabase() {
  for (NSString *suffix in @[ @"", @"-wal", @"-shm" ]) {
    [[NSFileManager defaultManager]
        removeItemAtPath:UserDataPath([@"SmartMandarinUserData.db"
                             stringByAppendingString:suffix])
                   error:NULL];
  }
}

// ------------------------------------------------------------------- tests

// The whole point of the legacy path: KeyKey's numbers are replaced, and
// entries this lexicon cannot produce are gone -- but the ones it can are
// kept, including the phrases the same file brought over.
static void TestLegacyImportNormalizesAndFilters() {
  ResetUserDatabase();
  PEUserPhraseStore *store = [[PEConfinedStore alloc] init];
  CHECK([store isAvailable]);
  CHECK([store isLexiconAvailable]);

  CHECK([store
      importLegacyUserPhraseDBFromFile:WriteExportFile(@"legacy-export.txt")]);

  CHECK(CountRows("user_unigrams") == 2);
  CHECK(ProbabilityOf("\xe6\xac\x8a\xe9\x87\x8d") == -1.0);  // was -3.5
  CHECK(ProbabilityOf("\xe7\x94\xb2\xe4\xb9\x99") == -1.0);  // was -4.5

  // 權 -> 重 survives (the lexicon has both); 甲 -> 乙 does not.
  CHECK(CountRows("user_bigram_cache") == 1);
  // 權 survives; 乙 alone is unknown to the lexicon and was not imported as a
  // phrase either.
  CHECK(CountRows("user_candidate_override_cache") == 1);
  [store release];
}

// The ordinary path must not touch anything: our own export was written
// against this same lexicon, and its probabilities mean what they say.
static void TestOrdinaryImportKeepsFileValues() {
  ResetUserDatabase();
  PEUserPhraseStore *store = [[PEConfinedStore alloc] init];
  CHECK([store isAvailable]);

  CHECK([store importUserPhraseDBFromFile:WriteExportFile(@"own-export.txt")]);

  CHECK(ProbabilityOf("\xe6\xac\x8a\xe9\x87\x8d") == -3.5);
  CHECK(CountRows("user_bigram_cache") == 2);
  CHECK(CountRows("user_candidate_override_cache") == 2);
  [store release];
}

// Seeds what ChiaKey itself has learned, so an import has something to
// collide with.
static void SeedExistingLearning() {
  sqlite3 *db = NULL;
  sqlite3_open([UserDataPath(@"SmartMandarinUserData.db") UTF8String], &db);
  Exec(db, "INSERT INTO user_bigram_cache VALUES('" + Q("ㄉㄚˋ") + " " +
               Q("ㄌㄠˇ") + "', '\xe5\xa4\xa7', '\xe8\x80\x81', '-1.0')");
  Exec(db, "INSERT INTO user_candidate_override_cache VALUES('" + Q("ㄑㄩㄢˊ") +
               "', '\xe5\x85\xa8')");  // 全, not the 權 the file carries
  Exec(db, "INSERT INTO user_learning_stats VALUES('override', '" +
               Q("ㄑㄩㄢˊ") + "', 9, 1700000000)");
  sqlite3_close(db);
}

static string TextOfOverride(const string &qstring) {
  sqlite3 *db = NULL;
  sqlite3_open([UserDataPath(@"SmartMandarinUserData.db") UTF8String], &db);
  sqlite3_stmt *st = NULL;
  string result;
  if (sqlite3_prepare_v2(
          db,
          "SELECT current FROM user_candidate_override_cache WHERE "
          "qstring = ?",
          -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, qstring.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) {
      const char *text = (const char *)sqlite3_column_text(st, 0);
      if (text) result = text;
    }
    sqlite3_finalize(st);
  }
  sqlite3_close(db);
  return result;
}

// A legacy file arrives while ChiaKey is already in use, so it merges into
// what is there. Wiping the caches would throw away everything the user has
// taught this input method since installing it.
static void TestLegacyImportMergesInsteadOfReplacing() {
  ResetUserDatabase();
  PEUserPhraseStore *store = [[PEConfinedStore alloc] init];
  SeedExistingLearning();

  CHECK([store
      importLegacyUserPhraseDBFromFile:WriteExportFile(@"merge-export.txt")]);

  // The seeded bigram survives alongside the one entry the file contributes.
  CHECK(CountRows("user_bigram_cache") == 2);
  // On a collision the entry trained here wins: 全 is not replaced by 權.
  CHECK(TextOfOverride(Q("\u3111\u3129\u3122\u02ca")) == "\xe5\x85\xa8");
  // Stats the user accumulated are still there.
  CHECK(CountRows("user_learning_stats") == 1);
  [store release];
}

// Restoring one of our own backups is a restore: the file replaces the tables.
static void TestOrdinaryImportReplacesCaches() {
  ResetUserDatabase();
  PEUserPhraseStore *store = [[PEConfinedStore alloc] init];
  SeedExistingLearning();

  CHECK([store importUserPhraseDBFromFile:WriteExportFile(@"restore.txt")]);

  CHECK(CountRows("user_bigram_cache") == 2);  // only what the file holds
  CHECK(TextOfOverride(Q("\u3111\u3129\u3122\u02ca")) == "\xe6\xac\x8a");
  CHECK(CountRows("user_learning_stats") == 0);
  [store release];
}

// Without a lexicon there is no way to tell a stale entry from a good one, so
// nothing may be dropped.
static void TestMissingLexiconDropsNothing() {
  ResetUserDatabase();
  NSString *lexicon = UserDataPath(@"Lexicons/active/ChiaKeySource.db");
  NSString *stashed = HomePath(@"stashed-lexicon.db");
  [[NSFileManager defaultManager] moveItemAtPath:lexicon
                                          toPath:stashed
                                           error:NULL];

  PEUserPhraseStore *store = [[PEConfinedStore alloc] init];
  CHECK(![store isLexiconAvailable]);
  CHECK([store
      importLegacyUserPhraseDBFromFile:WriteExportFile(@"no-lexicon.txt")]);
  CHECK(CountRows("user_bigram_cache") == 2);
  CHECK(CountRows("user_candidate_override_cache") == 2);
  [store release];

  [[NSFileManager defaultManager] moveItemAtPath:stashed
                                          toPath:lexicon
                                           error:NULL];
}

int main(int argc, const char **argv) {
  @autoreleasepool {
    if (argc < 2) {
      cerr << "usage: TestLegacyImport <temp home>" << endl;
      return 2;
    }
    g_home =
        [[NSString stringWithUTF8String:argv[1]] stringByStandardizingPath];

    // The store resolves its own paths from NSHomeDirectory(); if the harness
    // failed to redirect it, stop before writing to the real profile.
    NSString *resolved = [NSHomeDirectory() stringByStandardizingPath];
    if (![resolved isEqualToString:g_home]) {
      cerr << "refusing to run: home is " << [NSHomeDirectory() UTF8String]
           << ", expected " << [g_home UTF8String] << endl;
      return 2;
    }

    WriteFixtureLexicon();
    TestLegacyImportNormalizesAndFilters();
    TestOrdinaryImportKeepsFileValues();
    TestLegacyImportMergesInsteadOfReplacing();
    TestOrdinaryImportReplacesCaches();
    TestMissingLexiconDropsNothing();

    if (failures) {
      cerr << failures << " check(s) failed" << endl;
      return 1;
    }
    cout << "TestLegacyImport: OK" << endl;
  }
  return 0;
}
