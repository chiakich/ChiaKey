//
//  TestPhraseEditorImport.mm
//
//  Runs against a temporary home (CFFIXED_USER_HOME), as TestLegacyImport
//  does; main() refuses to run if the store resolves anywhere else.
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

// Not inside gDir: ResetStoreDirectory() wipes that between tests.
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

// Everything a backup carries comes back, learning tables included.
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

  // Clean, so anything present afterwards came from the file.
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

// Hex for the <database> section, as the exporter writes it.
static NSString *HexEncode(NSData *data) {
  const unsigned char *bytes = (const unsigned char *)[data bytes];
  NSMutableString *hex = [NSMutableString stringWithCapacity:[data length] * 2];
  for (NSUInteger i = 0; i < [data length]; i++)
    [hex appendFormat:@"%02x", bytes[i]];
  return hex;
}

// An export whose learning blob holds exactly the given schema and rows.
static NSString *WriteExportWithBlobSQL(NSString *name, const char *sql) {
  NSString *dbPath = TempPath([name stringByAppendingString:@".db"]);
  [[NSFileManager defaultManager] removeItemAtPath:dbPath error:NULL];
  sqlite3 *db = NULL;
  if (sqlite3_open([dbPath UTF8String], &db) != SQLITE_OK) return nil;
  sqlite3_exec(db, sql, NULL, NULL, NULL);
  sqlite3_close(db);

  NSData *blob = [NSData dataWithContentsOfFile:dbPath];
  NSString *contents = [NSString
      stringWithFormat:@"MJSR version 1.0.0\n%@<database>\n%@\n</database>\n",
                       kPhraseLines, HexEncode(blob)];
  return WriteFile(name, contents);
}

// A restore that fails part way must roll back, not leave the caches emptied.
static void TestFailedRestoreKeepsCurrentCaches(void) {
  ResetStoreDirectory();
  PEUserPhraseStore *store = [[PEUserPhraseStore alloc] init];
  ExecOnUserDB(
      "INSERT INTO user_bigram_cache VALUES ('aJ wl', 'a', 'b', '-1.0');"
      "INSERT INTO user_learning_stats VALUES ('bigram', 'aJ wl', 2, 42);");

  // One column where four are expected: the INSERT ... SELECT fails after
  // the DELETE has already run inside the transaction.
  NSString *path = WriteExportWithBlobSQL(
      @"bad-schema.txt", "CREATE TABLE user_bigram_cache (qstring);"
                         "INSERT INTO user_bigram_cache VALUES ('x');");
  // The phrases land, so the import succeeded; only the blob is reported lost.
  BOOL learningDataRestored = YES;
  CHECK([store importUserPhraseDBFromFile:path
                     learningDataRestored:&learningDataRestored]);
  CHECK(!learningDataRestored);
  // The phrases still land; the learning tables are untouched.
  CHECK(CountRows("user_unigrams") == 2);
  CHECK(CountRows("user_bigram_cache") == 1);
  CHECK([FirstTextValue("SELECT current FROM user_bigram_cache")
      isEqualToString:@"b"]);
  CHECK(CountRows("user_learning_stats") == 1);
  [store release];
}

// A file from an older ChiaKey has no context/stats tables; restoring it
// must not drop what is here.
static void TestOlderExportKeepsNewerStores(void) {
  ResetStoreDirectory();
  PEUserPhraseStore *store = [[PEUserPhraseStore alloc] init];
  ExecOnUserDB(
      "INSERT INTO user_bigram_cache VALUES ('old', 'a', 'b', '-1.0');"
      "INSERT INTO user_context_override_cache VALUES ('aJ wl', 'd');"
      "INSERT INTO user_learning_stats VALUES ('context', 'aJ wl', 3, 99);");

  NSString *path = WriteExportWithBlobSQL(
      @"older-export.txt",
      "CREATE TABLE user_bigram_cache (qstring, previous, current, "
      "probability);"
      "INSERT INTO user_bigram_cache VALUES ('new', 'c', 'd', '-1.0');");
  CHECK([store importUserPhraseDBFromFile:path]);
  // The table the file carries is replaced; the ones it lacks are kept.
  CHECK(CountRows("user_bigram_cache") == 1);
  CHECK([FirstTextValue("SELECT qstring FROM user_bigram_cache")
      isEqualToString:@"new"]);
  CHECK(CountRows("user_context_override_cache") == 1);
  CHECK(CountRows("user_learning_stats") == 1);
  [store release];
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

// The phrases have to land either way; the line walk sees one break per line.
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

// Estimated against another lexicon, so an imported phrase is renormalized.
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

// Built with the limits lowered, so the guards are cheap to reach.
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

  // And through a symlink, whose own size is a few dozen bytes.
  NSString *link = TempPath(@"oversized-link.txt");
  [[NSFileManager defaultManager] removeItemAtPath:link error:NULL];
  CHECK([[NSFileManager defaultManager] createSymbolicLinkAtPath:link
                                            withDestinationPath:path
                                                          error:NULL]);
  CHECK(![store importUserPhraseDBFromFile:link]);
  CHECK(CountRows("user_unigrams") == 0);
  [store release];
}

// A real export, not filler: arbitrary hex would be rejected as unreadable
// anyway and the test would pass without the size guard running.
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

  // Skipping the block for size is reported, but the phrases still import.
  BOOL learningDataRestored = YES;
  CHECK([store importUserPhraseDBFromFile:path
                     learningDataRestored:&learningDataRestored]);
  CHECK(!learningDataRestored);
  // The marker survives, so the block was skipped rather than restored.
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
  TestFailedRestoreKeepsCurrentCaches();
  TestOlderExportKeepsNewerStores();
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
