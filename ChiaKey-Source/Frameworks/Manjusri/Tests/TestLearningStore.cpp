// Standalone checks for the LearningStore rewrite: eviction policy, read
// recency, and the incremental SQLite round trip (including a table in the old
// 4-column / 2-column shape).
#include <cassert>
#include <cstdio>
#include <iostream>

#include "LanguageModel.h"

using namespace std;
using namespace Manjusri;

static int failures = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      cerr << "FAIL " << __LINE__ << ": " << #cond << endl;               \
      failures++;                                                         \
    }                                                                     \
  } while (0)

static void TestEvictionPrefersLeastUsed() {
  LearningStore<string> store(3);
  store.learn("a", "1");
  store.learn("b", "2");
  store.learn("b", "2");  // b now has two selections
  store.learn("c", "3");

  store.learn("d", "4");  // full: must drop a single-selection entry

  CHECK(store.peek("b") != 0);          // twice-selected survives
  CHECK(store.peek("d") != 0);
  CHECK(store.size() == 3);
  // a was the least recently used single-selection entry
  CHECK(store.peek("a") == 0);
  CHECK(store.pendingDeletes().count("a") == 1);
}

static void TestReadRefreshesRecency() {
  LearningStore<string> store(2);
  store.learn("a", "1");
  store.learn("b", "2");
  CHECK(store.fetch("a") != 0);  // a is now the most recent

  store.learn("c", "3");
  CHECK(store.peek("a") != 0);  // survived because the read refreshed it
  CHECK(store.peek("b") == 0);
}

static void TestSelectionCountAndDirtyTracking() {
  LearningStore<string> store(10);
  CHECK(!store.hasPendingWrites());

  store.learn("a", "1");
  CHECK(store.hasPendingWrites());
  CHECK(store.dirtyKeys().size() == 1);
  CHECK(store.peek("a")->selectionCount == 1);

  store.learn("a", "2");
  CHECK(store.peek("a")->selectionCount == 2);
  CHECK(store.peek("a")->value == "2");

  store.markClean();
  CHECK(!store.hasPendingWrites());
  CHECK(store.dirtyKeys().empty());

  // a read must not make the entry dirty again
  store.fetch("a");
  CHECK(!store.hasPendingWrites());

  store.remove("a");
  CHECK(store.peek("a") == 0);
  CHECK(store.pendingDeletes().count("a") == 1);
}

static void TestLoadDoesNotClobberNewerMemory() {
  LearningStore<string> store(10);
  store.learn("a", "fresh");
  CHECK(!store.loadEntry("a", "stale", 9, 1));
  CHECK(store.peek("a")->value == "fresh");
  CHECK(store.peek("a")->selectionCount == 1);
}

// ---- persistence -----------------------------------------------------------

static string g_tempDir = "/tmp";

static const string TempPath(const string& name) {
  return g_tempDir + "/" + name;
}

static int CountRows(OVSQLiteConnection* db, const char* table) {
  OVSQLiteStatement* s = db->prepare("SELECT COUNT(*) FROM %s", table);
  if (!s) return -1;

  int n = -1;
  while (s->step() == SQLITE_ROW) n = s->intOfColumn(0);
  delete s;
  return n;
}

static OVSQLiteConnection* MakeLegacyUserDB(const string& path) {
  remove(path.c_str());
  OVSQLiteConnection* db = OVSQLiteConnection::Open(path);
  assert(db);
  // the pre-migration shapes, as created by older ChiaKey and by the Phrase
  // Editor's CREATE TABLE IF NOT EXISTS
  db->execute("CREATE TABLE user_bigram_cache (qstring, previous, current, probability)");
  db->execute("CREATE TABLE user_candidate_override_cache (qstring, current)");
  db->execute("CREATE TABLE user_unigrams (qstring, current, probability, backoff)");
  db->execute("CREATE TABLE unigrams (qstring, current, probability, backoff)");
  db->execute("CREATE TABLE bigrams (qstring, previous, current, probability)");
  db->execute("INSERT INTO unigrams VALUES('x', 'X', -1.0, -0.5)");
  // a legacy row with no selection_count/last_used, plus a duplicate qstring
  db->execute("INSERT INTO user_candidate_override_cache VALUES('legacy', 'L1')");
  db->execute("INSERT INTO user_candidate_override_cache VALUES('legacy', 'L2')");
  return db;
}

// An older ChiaKey build writes these tables positionally; that must keep
// working after the migration, or its learning silently stops persisting.
static void TestOldBuildStillWrites(OVSQLiteConnection* db) {
  CHECK(db->execute("DELETE FROM user_bigram_cache") == SQLITE_OK);
  CHECK(db->execute("INSERT INTO user_bigram_cache VALUES('oq','op','oc',-1.5)") ==
        SQLITE_OK);
  CHECK(db->execute("DELETE FROM user_candidate_override_cache") == SQLITE_OK);
  CHECK(db->execute("INSERT INTO user_candidate_override_cache VALUES('oq','oc')") ==
        SQLITE_OK);
}

// A database that an interim build left with inline stat columns has to come
// back to the original shape, keeping both the rows and the statistics.
static void TestRollsBackInlineStatColumns() {
  const string path = TempPath("learningstore-rollback.db");
  remove(path.c_str());
  OVSQLiteConnection* db = OVSQLiteConnection::Open(path);
  assert(db);
  db->execute("CREATE TABLE user_candidate_override_cache "
              "(qstring, current, selection_count DEFAULT 1, last_used DEFAULT 0)");
  db->execute("CREATE TABLE user_bigram_cache "
              "(qstring, previous, current, probability, "
              "selection_count DEFAULT 1, last_used DEFAULT 0)");
  db->execute("INSERT INTO user_candidate_override_cache VALUES('q','V',7,1234)");

  LanguageModel::MigrateUserLearningTables(db);

  CHECK(!UserTableHasColumn(db, "user_candidate_override_cache", "selection_count"));
  CHECK(!UserTableHasColumn(db, "user_bigram_cache", "selection_count"));
  CHECK(CountRows(db, "user_candidate_override_cache") == 1);

  // the statistics moved to the side table rather than being dropped
  OVSQLiteStatement* s = db->prepare(
      "SELECT selection_count, last_used FROM user_learning_stats "
      "WHERE store = 'override' AND qstring = 'q'");
  CHECK(s != 0);
  if (s) {
    CHECK(s->step() == SQLITE_ROW);
    CHECK(s->intOfColumn(0) == 7);
    CHECK(s->intOfColumn(1) == 1234);
    delete s;
  }

  LanguageModel lm(db, 0, false, false, false, true, true);
  lm.loadUserCandidateOverrideCache();
  CHECK(lm.fetchCachedOverrideSelection("q") == "V");

  TestOldBuildStillWrites(db);

  delete db;
  remove(path.c_str());
}

static void TestMigrationAndRoundTrip() {
  const string path = TempPath("learningstore-roundtrip.db");
  OVSQLiteConnection* db = MakeLegacyUserDB(path);
  LanguageModel::MigrateUserLearningTables(db);

  // dedupe kept exactly one row for the duplicated qstring
  CHECK(CountRows(db, "user_candidate_override_cache") == 1);

  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.loadUserCandidateOverrideCache();

    // a legacy row survives migration and is readable
    CHECK(lm.fetchCachedOverrideSelection("legacy") == "L2");

    lm.cacheOverrideSelection("q1", "A");
    lm.cacheOverrideSelection("q2", "B");
    lm.cacheUserBigram("p q1", "P", "A");
    lm.saveUserBigramCacheAndCandidateOverrideCache(true, true);
  }

  // reopen against the same tables: learning must come back
  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.loadUserCandidateOverrideCache();
    lm.loadUserBigramCache();
    CHECK(lm.fetchCachedOverrideSelection("q1") == "A");
    CHECK(lm.fetchCachedOverrideSelection("q2") == "B");
    CHECK(lm.fetchCachedOverrideSelection("legacy") == "L2");

    BigramVector bv = lm.findBigrams("p q1");
    CHECK(bv.size() == 1);
    if (bv.size()) CHECK(bv[0].current == "A");

    // re-learning the same key must update in place, not duplicate the row
    lm.cacheOverrideSelection("q1", "C");
    lm.saveUserBigramCacheAndCandidateOverrideCache(true, true);
    CHECK(CountRows(db, "user_candidate_override_cache") == 3);

    // a removal has to reach the table
    lm.removeCachedSelection("q2");
    lm.saveUserBigramCacheAndCandidateOverrideCache(true, true);
    CHECK(CountRows(db, "user_candidate_override_cache") == 2);
  }

  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.loadUserCandidateOverrideCache();
    CHECK(lm.fetchCachedOverrideSelection("q1") == "C");
    CHECK(lm.fetchCachedOverrideSelection("q2") == "");

    // flushing user learning must empty the tables, not just memory
    lm.flushUserCache();
    CHECK(CountRows(db, "user_candidate_override_cache") == 0);
    CHECK(CountRows(db, "user_bigram_cache") == 0);
  }

  delete db;
  remove(path.c_str());
}

int main(int argc, char** argv) {
  if (argc > 1) g_tempDir = argv[1];

  TestEvictionPrefersLeastUsed();
  TestReadRefreshesRecency();
  TestSelectionCountAndDirtyTracking();
  TestLoadDoesNotClobberNewerMemory();
  TestMigrationAndRoundTrip();
  TestRollsBackInlineStatColumns();

  if (failures) {
    cerr << failures << " check(s) failed" << endl;
    return 1;
  }
  cout << "TestLearningStore: OK" << endl;
  return 0;
}
