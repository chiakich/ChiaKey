// Checks for importing a KeyKey/Manjusri user phrase export, in particular the
// <database> block that carries the bigram and candidate-override caches.
//
// Old Yahoo! KeyKey builds encrypted that block with SQLite SEE; ChiaKey has no
// SEE codec and writes it in the clear, so Import has to recognise both. The
// golden vectors below come from real KeyKey exports (SQLite headers, table
// definitions and random nonces -- no phrase data) and pin the cipher: if the
// key derivation, mode, IV location or counter rule is ever changed, they fail.
// One of them is a page whose nonce makes the counter carry out of byte 4,
// which most pages never do (about three in four nonces stay clear of it).
//
// The source file is included rather than linked so the test can drive
// Import() directly.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "BPMFUserPhraseHelper.cpp"
#include "MJSRExportCipher.h"

using namespace std;
using namespace Manjusri;

static int failures = 0;
static string g_tempDir = "/tmp";

#define CHECK(cond)                                         \
  do {                                                      \
    if (!(cond)) {                                          \
      cerr << "FAIL " << __LINE__ << ": " << #cond << endl; \
      failures++;                                           \
    }                                                       \
  } while (0)

static const size_t kPageSize = 1024;
static const size_t kReserved = 32;

static string TempPath(const string& name) { return g_tempDir + "/" + name; }

// ---------------------------------------------------------------- fixtures

// A user phrase DB in the shape Import expects to write into.
static OVSQLiteConnection* OpenUserDB(const string& path) {
  remove(path.c_str());
  OVSQLiteConnection* db = OVSQLiteConnection::Open(path);
  if (!db) return 0;
  db->execute(
      "CREATE TABLE user_unigrams (qstring, current, probability, backoff)");
  db->execute(
      "CREATE TABLE user_bigram_cache (qstring, previous, current, "
      "probability)");
  db->execute("CREATE TABLE user_candidate_override_cache (qstring, current)");
  // The newer learning stores, with the unique keys the IME's incremental
  // saves rely on -- a restore has to survive them.
  db->execute("CREATE TABLE user_context_override_cache (qstring, current)");
  db->execute(
      "CREATE UNIQUE INDEX user_context_override_cache_qstring_unique "
      "ON user_context_override_cache (qstring)");
  db->execute(
      "CREATE TABLE user_learning_stats "
      "(store, qstring, selection_count, last_used)");
  db->execute(
      "CREATE UNIQUE INDEX user_learning_stats_key "
      "ON user_learning_stats (store, qstring)");
  return db;
}

static int CountRows(OVSQLiteConnection* db, const string& table) {
  OVSQLiteStatementRef statement =
      db->prepare("SELECT COUNT(*) FROM %s", table.c_str());
  if (!statement) return -1;
  int count = -1;
  if (statement->step() == SQLITE_ROW) count = statement->intOfColumn(0);
  return count;
}

static string FirstTextValue(OVSQLiteConnection* db, const string& sql) {
  OVSQLiteStatementRef statement = db->prepare("%s", sql.c_str());
  if (!statement) return string();
  string result;
  if (statement->step() == SQLITE_ROW) {
    const char* text = statement->textOfColumn(0);
    if (text) result = text;
  }
  return result;
}

// The cache database as KeyKey/ChiaKey build it before exporting: 1024-byte
// pages with 32 reserved bytes per page, so the encrypted layout below matches
// what SEE produced.
static string BuildCacheDatabaseFile() {
  string path = TempPath("export-cache.db");
  remove(path.c_str());

  sqlite3* raw = 0;
  if (sqlite3_open(path.c_str(), &raw) != SQLITE_OK) return string();

  int reserved = (int)kReserved;
  sqlite3_exec(raw, "PRAGMA page_size=1024", 0, 0, 0);
  sqlite3_file_control(raw, "main", SQLITE_FCNTL_RESERVE_BYTES, &reserved);
  sqlite3_exec(raw,
               "CREATE TABLE user_bigram_cache (qstring, previous, current, "
               "probability);"
               "CREATE TABLE user_candidate_override_cache (qstring, current);"
               "INSERT INTO user_bigram_cache VALUES "
               "('5_n? w[', '\xe4\xb8\x80\xe7\x9b\xb4', '\xe6\x83\xb3', "
               "'-1.174761');"
               "INSERT INTO user_bigram_cache VALUES "
               "('aJ wl', '\xe6\xac\x8a', '\xe9\x87\x8d', '-1.174761');"
               "INSERT INTO user_candidate_override_cache VALUES "
               "('aJ', '\xe6\xac\x8a');",
               0, 0, 0);
  sqlite3_close(raw);

  ifstream ifs(path.c_str(), ios::binary);
  string data((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
  ifs.close();
  remove(path.c_str());

  if (data.size() % kPageSize || (unsigned char)data[20] != kReserved) {
    return string();
  }
  return data;
}

// A cache database with the schema and rows a caller passes in, so a test can
// forge an export from an older build or one with a drifted schema.
static string BuildCacheDatabaseFileWithSQL(const string& sql) {
  string path = TempPath("export-cache-custom.db");
  remove(path.c_str());

  sqlite3* raw = 0;
  if (sqlite3_open(path.c_str(), &raw) != SQLITE_OK) return string();

  int reserved = (int)kReserved;
  sqlite3_exec(raw, "PRAGMA page_size=1024", 0, 0, 0);
  sqlite3_file_control(raw, "main", SQLITE_FCNTL_RESERVE_BYTES, &reserved);
  sqlite3_exec(raw, sql.c_str(), 0, 0, 0);
  sqlite3_close(raw);

  ifstream ifs(path.c_str(), ios::binary);
  string data((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
  ifs.close();
  remove(path.c_str());
  return data;
}

// The inverse of DecryptExportDatabase, used to forge a legacy-shaped export.
// CTR is symmetric, so this is the same keystream walk.
static string EncryptCacheDatabase(const string& plain) {
  unsigned char key[16];
  const char* phrase = MANJUSRI_EXPORT_KEY;
  size_t phraseSize = strlen(phrase);
  for (size_t i = 0; i < sizeof(key); ++i)
    key[i] = (unsigned char)phrase[i % phraseSize];
  AES128Encryptor cipher(key);

  string result(plain);
  for (size_t offset = 0; offset < result.size(); offset += kPageSize) {
    unsigned char* page = (unsigned char*)&result[offset];

    // A fixed, obviously synthetic nonce; the real exporter randomises it.
    // Byte 4 starts high on purpose so the round trip crosses the counter
    // rollover that most real pages avoid.
    unsigned char counter[16];
    for (size_t i = 0; i < sizeof(counter); ++i) {
      counter[i] = (unsigned char)(0x10 + i + offset / kPageSize);
    }
    counter[4] = 0xf5;
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

static string WriteExportFile(const string& name, const string& block) {
  string path = TempPath(name);
  ofstream ofs(path.c_str(), ios::binary);
  ofs << "MJSR version 1.0.0\n";
  // current, bopomofo, probability, backoff
  ofs << "\xe6\xb8\xac\xe8\xa9\xa6\t\xe3\x84\x98\xe3\x84\x9c\xcb\x8b,"
         "\xe3\x84\x95\xcb\x8b\t-3.0\t0.0\n";
  ofs << "\xe6\xac\x8a\xe9\x87\x8d\t\xe3\x84\xa9\xe3\x84\xa2\xcb\x87,"
         "\xe3\x84\x93\xe3\x84\xa8\xe3\x84\xa5\xcb\x8b\t-4.0\t0.0\n";
  ofs << "\n# What follows is the \"Automatic Learning\" database, do not "
         "remove this\n";
  ofs << "<database>";

  static const char* kHexDigits = "0123456789abcdef";
  for (size_t i = 0; i < block.size(); ++i) {
    if (!(i % 30)) ofs << "\n";
    unsigned char c = (unsigned char)block[i];
    ofs << kHexDigits[c >> 4] << kHexDigits[c & 0x0f];
  }
  ofs << "\n</database>\n";
  ofs.close();
  return path;
}

// ------------------------------------------------------------------- tests

static void TestAESKnownAnswer() {
  // FIPS-197 C.1
  unsigned char key[16], in[16], out[16];
  for (size_t i = 0; i < 16; ++i) key[i] = (unsigned char)i;
  static const unsigned char kPlain[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
                                           0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb,
                                           0xcc, 0xdd, 0xee, 0xff};
  static const unsigned char kCipher[16] = {0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b,
                                            0x04, 0x30, 0xd8, 0xcd, 0xb7, 0x80,
                                            0x70, 0xb4, 0xc5, 0x5a};
  memcpy(in, kPlain, sizeof(in));
  AES128Encryptor(key).encryptBlock(in, out);
  CHECK(!memcmp(out, kCipher, sizeof(out)));
}

// Bytes lifted from a real Yahoo! KeyKey export: the encrypted SQLite header,
// the page-size/format fields it leaves in the clear, the all-zero stretch at
// offset 64 (a freshly created database), and the page's nonce. Decrypting
// these correctly means the key, mode, IV and counter rule all match KeyKey.
static void TestRealKeyKeyVector() {
  static const unsigned char kHeader[24] = {
      0xa8, 0xcd, 0xc7, 0xa5, 0x08, 0x3f, 0x90, 0x21, 0x60, 0x91, 0xb1, 0x93,
      0x2f, 0xc0, 0xb3, 0x5d, 0x04, 0x00, 0x01, 0x01, 0x20, 0x40, 0x20, 0x20};
  static const unsigned char kAtOffset64[16] = {
      0xfa, 0x7a, 0x2c, 0x17, 0x3b, 0x06, 0x2e, 0xa3,
      0xbc, 0x5a, 0x92, 0xda, 0x67, 0x98, 0x2d, 0x9f};
  static const unsigned char kNonce[16] = {0x66, 0x6b, 0xae, 0x84, 0xbe, 0x2e,
                                           0x6a, 0x64, 0xae, 0xbf, 0x3d, 0xd1,
                                           0x51, 0xcc, 0x37, 0xdd};

  string page(kPageSize, '\0');
  memcpy(&page[0], kHeader, sizeof(kHeader));
  memcpy(&page[64], kAtOffset64, sizeof(kAtOffset64));
  memcpy(&page[kPageSize - 16], kNonce, sizeof(kNonce));

  CHECK(DecryptExportDatabase(page));
  CHECK(!memcmp(page.data(), "SQLite format 3\x00", 16));
  CHECK(!memcmp(page.data() + 16, kHeader + 16, 8));  // kept in the clear
  CHECK(page.compare(64, 16, string(16, '\0')) == 0);
}

// A second real page, chosen because its nonce forces the counter to carry out
// of byte 4: IV[4] is 0xe0, so block 32 (offset 512) rolls over into byte 5.
// Everything the assertions below look at -- the schema records sitting near
// the end of the page -- is decrypted on the far side of that rollover, which
// no other vector here reaches. This is page 1 of a cache database, so it
// carries table definitions only, no phrases.
static const char kRealKeyKeyCarryPage[] =
    "2b2b3ef68a94a12c2d6bc189c795ca870400010120402020c4ca5aaa574d0b8da8849609"
    "15b32d4a2c366ce44cf05ddd1bfe825d4f0ab14829fa049cd272f89103126ff7fe6887f5"
    "0bbd5046d6d88043ef01a7fef1409e1ef7013b8da75b04611539ab7728babe651f403c83"
    "dc140363edc10fcd090283615ef8e262a8b2fd615a855aeb250f7548c3d4e78be258e597"
    "13da42aab43d44a27c315a56ed16e56597a5e273e457dbdfea5bfb61008ca7cecb38736c"
    "cb57f76082fc42fd7a9673b5f78c1cde87e528fb249b5e47bfbc987d3f6ff2a871ef2f84"
    "ca3dee942da0c5f3bd880c4664a86ba88b79fa1a5cd7cc6e7d844bce902e8bb013d2d6fb"
    "4e4de058476b65c9918873127177e86765ecd4d8586442d9b5c0fe9ee3706aba1faadccc"
    "b051fa592f54468bec9948f3aa025f4c4a84938e7525334c3cff48ebe73e8a8bdacabb3f"
    "a313662b50c7086386530f9d0eb238912e7a8427acbc83e30efb2e65520008ec8bdaba16"
    "f4f5acdbba62db771f7ff1a41836083f9423d314dc2d9fb0ab29a5e45cb7b77951dda441"
    "d33c4ab0ad214d8f522793d5e6fafa34908dae41b8addf9234db8af206ecd1eb679ada23"
    "6ff80289afef8a39993fa050c3a0119314eae3e11661806073dbafb2d34a049043a71a55"
    "ca9622a7bb62082c1c5aa65f5430e74d6ce440f722d6319bce1275d28f3de549ac6bf929"
    "2d7e3ca0a5e79a66905fddd3eb7bb129472d39e396879e2a24c7385e6ea2091018b6583e"
    "e8c9499a0eb571e3445fe0899a2309d74fd0c26263b7319873280f14d51bc85c0516eee9"
    "d233bd3e983a7c6a328ed8e4ca8412787120977c4cd9273572745a9a1ea6a5c33ec94d99"
    "6a836e81ea4780e231b6aa4707ebc1ec4ac2e4e97fe42cd9be16e199e613f207851563e9"
    "910be189e75c12a55feecf39ea93bd7dbe2fcd1d670d886012affb12670c13506ea17b12"
    "3b630283cf437f0eb4c4a317d11bd530f5613eca9c632798d63a9c240fa000ed1b27f546"
    "8449a5d4f7723661c68166fed87ab219036126281ed9da6384d8986e99d6b757d6364841"
    "a9b8a53785f5dd152207e7894128c9aca8abd095dc0f41d0f86923ba89fa33161bb8aab6"
    "6cdce895f3e22364017e1f6ccdbe9365ff6821133e6f2c404379bd3a6489d9429897d1c2"
    "5ba3731e57a6450f05d2e89fb65bcddca4e6f89d16a4da5577ef2e8175aba504575c1d42"
    "9c58604ec29fbce263c16340a13120268427f5366b1fac5d40ac1a1622c5147988509920"
    "b58d6d9faced44ccd8ae18d9eda8d719d088da7dfeffe23721e4626335d637dea7919ea9"
    "8d2b5606c515040bee9c56c35f9acbc048e11c449f578ca151c8058fac13860b02d5daa0"
    "5c1d6161248498e4c3fc62a1daa6dcefbf816c2190cc51961136386dfa153cc27a8ff577"
    "97488ecfe04d95483e11c62c47b82a1d";

static string HexToBinary(const string& hex) {
  string result;
  for (size_t i = 0; i + 1 < hex.size(); i += 2) {
    result += (char)strtol(hex.substr(i, 2).c_str(), 0, 16);
  }
  return result;
}

static void TestRealKeyKeyCarryVector() {
  string page = HexToBinary(kRealKeyKeyCarryPage);
  CHECK(page.size() == kPageSize);

  CHECK(DecryptExportDatabase(page));
  CHECK(!memcmp(page.data(), "SQLite format 3\x00", 16));
  CHECK(page.find("CREATE TABLE user_bigram_cache (qstring, previous, "
                  "current, probability)") != string::npos);
  CHECK(page.find("CREATE TABLE user_candidate_override_cache (qstring, "
                  "current)") != string::npos);
}

static void TestPlaintextBlockIsPassedThrough() {
  string plain = BuildCacheDatabaseFile();
  CHECK(!plain.empty());
  string data = plain;
  CHECK(DecryptExportDatabase(data));
  CHECK(data == plain);  // ChiaKey's own export is not encrypted
}

// Each page's reserved bytes carry the nonce and are never encrypted, so a
// round trip is only expected to restore the page bodies.
static string PageBodies(const string& data) {
  string result;
  for (size_t offset = 0; offset < data.size(); offset += kPageSize) {
    result += data.substr(offset, kPageSize - kReserved);
  }
  return result;
}

static void TestEncryptedBlockRoundTrip() {
  string plain = BuildCacheDatabaseFile();
  CHECK(!plain.empty());
  string data = EncryptCacheDatabase(plain);
  CHECK(PageBodies(data) != PageBodies(plain));
  CHECK(DecryptExportDatabase(data));
  CHECK(PageBodies(data) == PageBodies(plain));
}

static void TestUnreadableBlockIsRejected() {
  string data(kPageSize, '\x5a');
  data[20] = 32;  // claims the legacy reserved size, but decrypts to nothing
  CHECK(!DecryptExportDatabase(data));

  string tooShort("SQLite");
  CHECK(!DecryptExportDatabase(tooShort));

  string notAPageMultiple = EncryptCacheDatabase(BuildCacheDatabaseFile());
  notAPageMultiple.resize(notAPageMultiple.size() - 7);
  CHECK(!DecryptExportDatabase(notAPageMultiple));
}

// End to end: a legacy-shaped export file lands both the phrases and the
// caches.
static void TestImportsLegacyExport() {
  string path = WriteExportFile("legacy-export.txt",
                                EncryptCacheDatabase(BuildCacheDatabaseFile()));

  OVSQLiteConnection* db = OpenUserDB(TempPath("import-legacy.db"));
  CHECK(db != 0);
  if (!db) return;

  CHECK(BPMFUserPhraseHelper::Import(db, path));
  CHECK(CountRows(db, "user_unigrams") == 2);
  CHECK(CountRows(db, "user_bigram_cache") == 2);
  CHECK(CountRows(db, "user_candidate_override_cache") == 1);
  CHECK(FirstTextValue(db,
                       "SELECT current FROM user_bigram_cache WHERE previous = "
                       "'\xe6\xac\x8a'") == "\xe9\x87\x8d");
  delete db;
}

static void TestImportsPlaintextExport() {
  string path = WriteExportFile("plain-export.txt", BuildCacheDatabaseFile());

  OVSQLiteConnection* db = OpenUserDB(TempPath("import-plain.db"));
  CHECK(db != 0);
  if (!db) return;

  CHECK(BPMFUserPhraseHelper::Import(db, path));
  CHECK(CountRows(db, "user_unigrams") == 2);
  CHECK(CountRows(db, "user_bigram_cache") == 2);
  CHECK(CountRows(db, "user_candidate_override_cache") == 1);
  delete db;
}

// A block we cannot read costs the caller its cache, not the phrases: the
// import still succeeds and whatever was already learned stays put.
static void TestUnreadableBlockKeepsPhrasesAndCache() {
  string garbage(kPageSize, '\x5a');
  garbage[20] = 32;
  string path = WriteExportFile("garbage-export.txt", garbage);

  OVSQLiteConnection* db = OpenUserDB(TempPath("import-garbage.db"));
  CHECK(db != 0);
  if (!db) return;
  db->execute("INSERT INTO user_bigram_cache VALUES ('x', 'y', 'z', '-1.0')");

  CHECK(BPMFUserPhraseHelper::Import(db, path));
  CHECK(CountRows(db, "user_unigrams") == 2);
  CHECK(CountRows(db, "user_bigram_cache") == 1);
  delete db;
}

// The newer stores travel with a backup too, so restoring one brings them back.
static void TestImportsAllLearningTables() {
  string block = BuildCacheDatabaseFileWithSQL(
      "CREATE TABLE user_bigram_cache (qstring, previous, current, "
      "probability);"
      "CREATE TABLE user_candidate_override_cache (qstring, current);"
      "CREATE TABLE user_context_override_cache (qstring, current);"
      "CREATE TABLE user_learning_stats "
      "(store, qstring, selection_count, last_used);"
      "INSERT INTO user_bigram_cache VALUES ('aJ wl', 'a', 'b', '-1.0');"
      "INSERT INTO user_candidate_override_cache VALUES ('aJ', 'c');"
      "INSERT INTO user_context_override_cache VALUES ('aJ wl', 'd');"
      "INSERT INTO user_learning_stats VALUES ('context', 'aJ wl', 3, 12345);");
  CHECK(!block.empty());
  string path = WriteExportFile("all-tables-export.txt", block);

  OVSQLiteConnection* db = OpenUserDB(TempPath("import-all-tables.db"));
  CHECK(db != 0);
  if (!db) return;

  CHECK(BPMFUserPhraseHelper::Import(db, path));
  CHECK(CountRows(db, "user_bigram_cache") == 1);
  CHECK(CountRows(db, "user_candidate_override_cache") == 1);
  CHECK(CountRows(db, "user_context_override_cache") == 1);
  CHECK(CountRows(db, "user_learning_stats") == 1);
  CHECK(FirstTextValue(db, "SELECT current FROM user_context_override_cache") ==
        "d");
  delete db;
}

// An export from an older build carries only the two original tables. What it
// says nothing about must be left alone, not wiped.
static void TestOlderExportLeavesNewerTablesAlone() {
  string path = WriteExportFile("older-export.txt", BuildCacheDatabaseFile());

  OVSQLiteConnection* db = OpenUserDB(TempPath("import-older.db"));
  CHECK(db != 0);
  if (!db) return;
  db->execute("INSERT INTO user_context_override_cache VALUES ('aJ wl', 'd')");
  db->execute(
      "INSERT INTO user_learning_stats VALUES ('context', 'aJ wl', 3, 12345)");

  CHECK(BPMFUserPhraseHelper::Import(db, path));
  CHECK(CountRows(db, "user_bigram_cache") == 2);
  CHECK(CountRows(db, "user_context_override_cache") == 1);
  CHECK(CountRows(db, "user_learning_stats") == 1);
  delete db;
}

// Duplicate keys used to abort the restore after the tables had been emptied,
// and the import still reported success.
static void TestDuplicateKeysDoNotEmptyCaches() {
  string block = BuildCacheDatabaseFileWithSQL(
      "CREATE TABLE user_bigram_cache (qstring, previous, current, "
      "probability);"
      "CREATE TABLE user_candidate_override_cache (qstring, current);"
      "CREATE TABLE user_context_override_cache (qstring, current);"
      "INSERT INTO user_context_override_cache VALUES ('aJ wl', 'd');"
      "INSERT INTO user_context_override_cache VALUES ('aJ wl', 'e');");
  CHECK(!block.empty());
  string path = WriteExportFile("duplicate-export.txt", block);

  OVSQLiteConnection* db = OpenUserDB(TempPath("import-duplicate.db"));
  CHECK(db != 0);
  if (!db) return;

  CHECK(BPMFUserPhraseHelper::Import(db, path));
  CHECK(CountRows(db, "user_context_override_cache") == 1);
  delete db;
}

// A restore that cannot complete must leave every cache as it was, and say so.
static void TestFailedRestoreRollsBackAndReportsFailure() {
  // user_learning_stats without last_used: the restore's SELECT names a column
  // this file does not have, and that failure comes after the earlier tables
  // have already been emptied and refilled.
  string block = BuildCacheDatabaseFileWithSQL(
      "CREATE TABLE user_bigram_cache (qstring, previous, current, "
      "probability);"
      "CREATE TABLE user_candidate_override_cache (qstring, current);"
      "CREATE TABLE user_learning_stats (store, qstring, selection_count);"
      "INSERT INTO user_bigram_cache VALUES ('aJ wl', 'a', 'b', '-1.0');");
  CHECK(!block.empty());
  string path = WriteExportFile("broken-export.txt", block);

  OVSQLiteConnection* db = OpenUserDB(TempPath("import-broken.db"));
  CHECK(db != 0);
  if (!db) return;
  db->execute("INSERT INTO user_bigram_cache VALUES ('x', 'y', 'z', '-1.0')");

  CHECK(!BPMFUserPhraseHelper::Import(db, path));
  CHECK(CountRows(db, "user_unigrams") == 2);
  CHECK(CountRows(db, "user_bigram_cache") == 1);
  CHECK(FirstTextValue(db, "SELECT current FROM user_bigram_cache") == "z");
  delete db;
}

int main(int argc, char** argv) {
  if (argc > 1) g_tempDir = argv[1];

  TestAESKnownAnswer();
  TestRealKeyKeyVector();
  TestRealKeyKeyCarryVector();
  TestPlaintextBlockIsPassedThrough();
  TestEncryptedBlockRoundTrip();
  TestUnreadableBlockIsRejected();
  TestImportsLegacyExport();
  TestImportsPlaintextExport();
  TestUnreadableBlockKeepsPhrasesAndCache();
  TestImportsAllLearningTables();
  TestOlderExportLeavesNewerTablesAlone();
  TestDuplicateKeysDoNotEmptyCaches();
  TestFailedRestoreRollsBackAndReportsFailure();

  if (failures) {
    cerr << failures << " check(s) failed" << endl;
    return 1;
  }
  cout << "TestUserPhraseImport: OK" << endl;
  return 0;
}
