//
//  TestPhraseEditorImport.mm
//
//  Covers PEUserPhraseStore's MJSR import: the line walk (header, comments,
//  CRLF, the <database> section), the export/import round trip including the
//  learning tables, and the two size limits.
//
//  Runs against a temporary home (CFFIXED_USER_HOME), the same way
//  TestLegacyImport does, so it never touches the real profile -- main()
//  refuses to run if the store resolves anywhere else.
//

#import <Foundation/Foundation.h>

#include <sqlite3.h>

#import "PEUserPhraseStore.h"

static int failures = 0;
static NSString *gHome = nil;
static NSString *gDir = nil;      // the store's data directory, wiped per test
static NSString *gFileDir = nil;  // import/export files, kept across resets

#define CHECK(cond)                                                   \
  do {                                                                \
    if (!(cond)) {                                                    \
      fprintf(stderr, "FAIL %d: %s\n", __LINE__, #cond);              \
      failures++;                                                     \
    }                                                                 \
  } while (0)

// ---------------------------------------------------------------- fixtures

// Deliberately not inside gDir: ResetStoreDirectory() wipes that, and an
// export written there would be deleted before it could be imported back.
static NSString *TempPath(NSString *name) {
  return [gFileDir stringByAppendingPathComponent:name];
}

static NSString *DBPath(void) {
  return [gDir stringByAppendingPathComponent:@"SmartMandarinUserData.db"];
}

// Where the store puts its database inside the temporary home.
static NSString *StoreDirectory(void) {
  return [gHome stringByAppendingPathComponent:
                    @"Library/Application Support/ChiaKey"];
}

static void ResetStoreDirectory(void) {
  [[NSFileManager defaultManager] removeItemAtPath:gDir error:NULL];
  [[NSFileManager defaultManager] createDirectoryAtPath:gDir
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:NULL];
}

// A second connection, so a test can look at what the store wrote.
static sqlite3 *OpenDBForReading(void) {
  sqlite3 *db = NULL;
  if (sqlite3_open([DBPath() UTF8String], &db) != SQLITE_OK) return NULL;
  sqlite3_busy_timeout(db, 3000);
  return db;
}

static int CountRows(const char *table) {
  sqlite3 *db = OpenDBForReading();
  if (!db) return -1;
  char *sql = sqlite3_mprintf("SELECT COUNT(*) FROM %s", table);
  sqlite3_stmt *st = NULL;
  int count = -1;
  if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
    if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
  }
  sqlite3_free(sql);
  sqlite3_close(db);
  return count;
}

static NSString *FirstTextValue(const char *sql) {
  sqlite3 *db = OpenDBForReading();
  if (!db) return nil;
  sqlite3_stmt *st = NULL;
  NSString *result = nil;
  if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
    if (sqlite3_step(st) == SQLITE_ROW) {
      const char *text = (const char *)sqlite3_column_text(st, 0);
      if (text) result = [NSString stringWithUTF8String:text];
    }
    sqlite3_finalize(st);
  }
  sqlite3_close(db);
  return result;
}

static void ExecOnUserDB(const char *sql) {
  sqlite3 *db = OpenDBForReading();
  if (!db) return;
  sqlite3_exec(db, sql, NULL, NULL, NULL);
  sqlite3_close(db);
}

static NSString *WriteFile(NSString *name, NSString *contents) {
  NSString *path = TempPath(name);
  [contents writeToFile:path
             atomically:YES
               encoding:NSUTF8StringEncoding
                  error:NULL];
  return path;
}

// "測試" and "權重", with readings that cover them character for character.
static NSString *const kPhraseLines =
    @"測試\tㄘㄜˋ,ㄕˋ\t-3.0\t0.0\n"
    @"權重\tㄩㄢˇ,ㄓㄨㄥˋ\t-4.0\t0.0\n";

// ------------------------------------------------------------------- tests

// The whole point of the format: everything a backup carries comes back,
// learning tables included.
static void TestExportImportRoundTrip(void) {
  ResetStoreDirectory();
  PEUserPhraseStore *store = [[PEUserPhraseStore alloc] init];
  CHECK([store isAvailable]);

  ExecOnUserDB(
      "INSERT INTO user_unigrams VALUES "
      "('\x35\x5f\x6e\x3f', '\xe6\xb8\xac\xe8\xa9\xa6', '-3.0', '0.0');"
      "INSERT INTO user_bigram_cache VALUES ('aJ wl', 'a', 'b', '-1.0');"
      "INSERT INTO user_candidate_override_cache VALUES ('aJ', 'c');"
      "INSERT INTO user_context_override_cache VALUES ('aJ wl', 'd');"
      "INSERT INTO user_learning_stats VALUES ('context', 'aJ wl', 3, 99);");

  NSString *exportPath = TempPath(@"round-trip.txt");
  CHECK([store exportUserPhraseDBToFile:exportPath]);
  [store release];

  // A clean database, so anything present afterwards came from the file.
  ResetStoreDirectory();
  PEUserPhraseStore *restored = [[PEUserPhraseStore alloc] init];
  CHECK([restored isAvailable]);
  CHECK([restored importUserPhraseDBFromFile:exportPath]);

  CHECK(CountRows("user_unigrams") == 1);
  CHECK(CountRows("user_bigram_cache") == 1);
  CHECK(CountRows("user_candidate_override_cache") == 1);
  CHECK(CountRows("user_context_override_cache") == 1);
  CHECK(CountRows("user_learning_stats") == 1);
  CHECK([FirstTextValue("SELECT current FROM user_context_override_cache")
      isEqualToString:@"d"]);
  CHECK([FirstTextValue("SELECT selection_count FROM user_learning_stats")
      isEqualToString:@"3"]);
  [restored release];
}

static void TestRejectsFileWithoutHeader(void) {
  ResetStoreDirectory();
  PEUserPhraseStore *store = [[PEUserPhraseStore alloc] init];

  NSString *path = WriteFile(@"no-header.txt",
                             [@"not an MJSR file\n"
                                 stringByAppendingString:kPhraseLines]);
  CHECK(![store importUserPhraseDBFromFile:path]);
  CHECK(CountRows("user_unigrams") == 0);
  [store release];
}

static void TestRejectsMissingFile(void) {
  ResetStoreDirectory();
  PEUserPhraseStore *store = [[PEUserPhraseStore alloc] init];
  CHECK(![store importUserPhraseDBFromFile:TempPath(@"nope.txt")]);
  [store release];
}

static void TestSkipsCommentsAndBlankLines(void) {
  ResetStoreDirectory();
  PEUserPhraseStore *store = [[PEUserPhraseStore alloc] init];

  NSString *path = WriteFile(
      @"comments.txt",
      [NSString stringWithFormat:@"MJSR version 1.0.0\n"
                                 @"# a comment\n"
                                 @"\n"
                                 @"%@"
                                 @"# trailing comment\n",
                                 kPhraseLines]);
  CHECK([store importUserPhraseDBFromFile:path]);
  CHECK(CountRows("user_unigrams") == 2);
  [store release];
}

// A CRLF file used to arrive as real lines interleaved with empty ones, which
// the field count happened to skip; the line walk sees one break per line.
// Either way the phrases have to land -- this pins that.
static void TestHandlesCRLF(void) {
  ResetStoreDirectory();
  PEUserPhraseStore *store = [[PEUserPhraseStore alloc] init];

  NSString *crlf = [[NSString stringWithFormat:@"MJSR version 1.0.0\n%@",
                                              kPhraseLines]
      stringByReplacingOccurrencesOfString:@"\n"
                                withString:@"\r\n"];
  NSString *path = WriteFile(@"crlf.txt", crlf);
  CHECK([store importUserPhraseDBFromFile:path]);
  CHECK(CountRows("user_unigrams") == 2);
  [store release];
}

// A legacy file's probabilities were estimated against another lexicon, so an
// imported phrase gets what a hand-added one gets instead.
static void TestLegacyImportRenormalizesProbability(void) {
  ResetStoreDirectory();
  PEUserPhraseStore *store = [[PEUserPhraseStore alloc] init];

  NSString *path = WriteFile(
      @"legacy.txt", [NSString stringWithFormat:@"MJSR version 1.0.0\n%@",
                                                kPhraseLines]);
  CHECK([store importLegacyUserPhraseDBFromFile:path]);
  CHECK(CountRows("user_unigrams") == 2);
  CHECK([FirstTextValue("SELECT probability FROM user_unigrams LIMIT 1")
      isEqualToString:@"-1.0"]);
  [store release];
}

#ifdef PE_TEST_SMALL_LIMITS

// Built with the limits lowered, so the guards can be exercised without
// writing hundreds of megabytes.
static void TestRejectsOversizedFile(void) {
  ResetStoreDirectory();
  PEUserPhraseStore *store = [[PEUserPhraseStore alloc] init];

  NSMutableString *big = [NSMutableString
      stringWithFormat:@"MJSR version 1.0.0\n%@", kPhraseLines];
  while ([big lengthOfBytesUsingEncoding:NSUTF8StringEncoding] <=
         PE_MAX_IMPORT_FILE_SIZE) {
    [big appendString:@"# padding padding padding padding padding\n"];
  }
  NSString *path = WriteFile(@"oversized.txt", big);

  CHECK(![store importUserPhraseDBFromFile:path]);
  CHECK(CountRows("user_unigrams") == 0);
  [store release];
}

// An oversized learning block costs the caller that block only: the phrases
// still land, and whatever was already learned stays put.
//
// The block is a real export, not filler -- a blob of arbitrary hex would be
// rejected as unreadable anyway, and the test would pass without the size
// guard ever running. This one would restore if it were allowed through.
static void TestOversizedLearningBlobIsIgnored(void) {
  ResetStoreDirectory();
  PEUserPhraseStore *source = [[PEUserPhraseStore alloc] init];
  ExecOnUserDB(
      "INSERT INTO user_bigram_cache VALUES ('aJ wl', 'a', 'b', '-1.0');"
      "INSERT INTO user_context_override_cache VALUES ('aJ wl', 'd');");
  NSString *path = TempPath(@"huge-blob.txt");
  CHECK([source exportUserPhraseDBToFile:path]);
  [source release];

  unsigned long long blobSize =
      [[[NSFileManager defaultManager] attributesOfItemAtPath:path
                                                       error:NULL] fileSize] /
      2;
  CHECK(blobSize > PE_MAX_LEARNING_BLOB_SIZE);

  ResetStoreDirectory();
  PEUserPhraseStore *store = [[PEUserPhraseStore alloc] init];
  ExecOnUserDB("INSERT INTO user_bigram_cache VALUES ('x', 'y', 'z', '-1.0')");

  CHECK([store importUserPhraseDBFromFile:path]);
  // The marker survives, so the block really was skipped rather than restored.
  CHECK(CountRows("user_bigram_cache") == 1);
  CHECK([FirstTextValue("SELECT current FROM user_bigram_cache")
      isEqualToString:@"z"]);
  CHECK(CountRows("user_context_override_cache") == 0);
  [store release];
}

#endif

int main(int argc, char **argv) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  if (argc < 2) {
    fprintf(stderr, "usage: TestPhraseEditorImport <temp dir>\n");
    return 2;
  }
  gHome = [[NSString stringWithUTF8String:argv[1]] stringByStandardizingPath];

  // The store resolves its own paths from NSHomeDirectory(); if the harness
  // failed to redirect it, stop before writing to the real profile.
  NSString *resolved = [NSHomeDirectory() stringByStandardizingPath];
  if (![resolved isEqualToString:gHome]) {
    fprintf(stderr, "refusing to run: home is %s, expected %s\n",
            [NSHomeDirectory() UTF8String], [gHome UTF8String]);
    return 2;
  }

  gDir = StoreDirectory();
  gFileDir = [gHome stringByAppendingPathComponent:@"files"];
  [[NSFileManager defaultManager] createDirectoryAtPath:gFileDir
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:NULL];

#ifdef PE_TEST_SMALL_LIMITS
  TestRejectsOversizedFile();
  TestOversizedLearningBlobIsIgnored();
#else
  TestExportImportRoundTrip();
  TestRejectsFileWithoutHeader();
  TestRejectsMissingFile();
  TestSkipsCommentsAndBlankLines();
  TestHandlesCRLF();
  TestLegacyImportRenormalizesProbability();
#endif

  if (failures) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  printf("TestPhraseEditorImport: OK\n");
  [pool release];
  return 0;
}
