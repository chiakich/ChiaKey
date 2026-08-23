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

#define CHECK(cond)                                         \
  do {                                                      \
    if (!(cond)) {                                          \
      cerr << "FAIL " << __LINE__ << ": " << #cond << endl; \
      failures++;                                           \
    }                                                       \
  } while (0)

static void TestEvictionPrefersLeastUsed() {
  LearningStore<string> store(3);
  store.learn("a", "1");
  store.learn("b", "2");
  store.learn("b", "2");  // b now has two selections
  store.learn("c", "3");

  store.learn("d", "4");  // full: must drop a single-selection entry

  CHECK(store.peek("b") != 0);  // twice-selected survives
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

// With nothing left that was picked only once, the victim is still the entry
// with the fewest selections rather than simply the oldest.
static void TestEvictionRanksByCountWhenNothingIsSingle() {
  LearningStore<string> store(3);
  store.learn("a", "1");  // ends up at three selections
  store.learn("a", "1");
  store.learn("a", "1");
  store.learn("b", "2");  // two
  store.learn("b", "2");
  store.learn("c", "3");  // one

  store.learn("d", "4");
  CHECK(store.peek("c") == 0);  // the single-selection entry goes first
  CHECK(store.peek("a") != 0);
  CHECK(store.peek("b") != 0);

  // now a(3), b(2), d(1): the fresh single-selection entry is the cheapest
  // again
  store.learn("e", "5");
  CHECK(store.peek("d") == 0);
  CHECK(store.peek("a") != 0);
  CHECK(store.peek("b") != 0);

  // a(3), b(2), e(1) -> promote e past b, so b becomes the weakest
  store.learn("e", "5");
  store.learn("e", "5");
  store.learn("f", "6");
  CHECK(store.peek("b") == 0);
  CHECK(store.peek("a") != 0);
  CHECK(store.peek("e") != 0);
}

// Lowering the capacity has to shed the excess immediately, not one entry per
// subsequent insertion.
static void TestCapacityShrinkEvictsNow() {
  LearningStore<string> store(10);
  for (int i = 0; i < 8; i++) {
    char key[8];
    snprintf(key, sizeof(key), "k%d", i);
    store.learn(key, "v");
  }
  CHECK(store.size() == 8);

  store.setCapacity(3);
  CHECK(store.capacity() == 3);
  CHECK(store.size() == 3);
  // the survivors are the most recent, and every drop is queued for the disk
  CHECK(store.peek("k7") != 0);
  CHECK(store.peek("k0") == 0);
  CHECK(store.pendingDeletes().count("k0") == 1);

  store.setCapacity(0);  // ignored: a zero-capacity store could hold nothing
  CHECK(store.capacity() == 3);
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
  OVSQLiteStatementRef s = db->prepare("SELECT COUNT(*) FROM %s", table);
  if (!s) return -1;

  int n = -1;
  while (s->step() == SQLITE_ROW) n = s->intOfColumn(0);
  return n;
}

static OVSQLiteConnection* MakeLegacyUserDB(const string& path) {
  remove(path.c_str());
  OVSQLiteConnection* db = OVSQLiteConnection::Open(path);
  assert(db);
  // the pre-migration shapes, as created by older ChiaKey and by the Phrase
  // Editor's CREATE TABLE IF NOT EXISTS
  db->execute(
      "CREATE TABLE user_bigram_cache (qstring, previous, current, "
      "probability)");
  db->execute("CREATE TABLE user_candidate_override_cache (qstring, current)");
  db->execute(
      "CREATE TABLE user_unigrams (qstring, current, probability, backoff)");
  db->execute("CREATE TABLE unigrams (qstring, current, probability, backoff)");
  db->execute("CREATE TABLE bigrams (qstring, previous, current, probability)");
  db->execute("INSERT INTO unigrams VALUES('x', 'X', -1.0, -0.5)");
  // a legacy row with no selection_count/last_used, plus a duplicate qstring
  db->execute(
      "INSERT INTO user_candidate_override_cache VALUES('legacy', 'L1')");
  db->execute(
      "INSERT INTO user_candidate_override_cache VALUES('legacy', 'L2')");
  return db;
}

// An older ChiaKey build writes these tables positionally; that must keep
// working after the migration, or its learning silently stops persisting.
static void TestOldBuildStillWrites(OVSQLiteConnection* db) {
  CHECK(db->execute("DELETE FROM user_bigram_cache") == SQLITE_OK);
  CHECK(db->execute(
            "INSERT INTO user_bigram_cache VALUES('oq','op','oc',-1.5)") ==
        SQLITE_OK);
  CHECK(db->execute("DELETE FROM user_candidate_override_cache") == SQLITE_OK);
  CHECK(db->execute(
            "INSERT INTO user_candidate_override_cache VALUES('oq','oc')") ==
        SQLITE_OK);
}

// A database that an interim build left with inline stat columns has to come
// back to the original shape, keeping both the rows and the statistics.
static void TestRollsBackInlineStatColumns() {
  const string path = TempPath("learningstore-rollback.db");
  remove(path.c_str());
  OVSQLiteConnection* db = OVSQLiteConnection::Open(path);
  assert(db);
  db->execute(
      "CREATE TABLE user_candidate_override_cache "
      "(qstring, current, selection_count DEFAULT 1, last_used DEFAULT 0)");
  db->execute(
      "CREATE TABLE user_bigram_cache "
      "(qstring, previous, current, probability, "
      "selection_count DEFAULT 1, last_used DEFAULT 0)");
  db->execute(
      "INSERT INTO user_candidate_override_cache VALUES('q','V',7,1234)");

  LanguageModel::MigrateUserLearningTables(db);

  CHECK(!UserTableHasColumn(db, "user_candidate_override_cache",
                            "selection_count"));
  CHECK(!UserTableHasColumn(db, "user_bigram_cache", "selection_count"));
  CHECK(CountRows(db, "user_candidate_override_cache") == 1);

  // the statistics moved to the side table rather than being dropped
  {
    OVSQLiteStatementRef s = db->prepare(
        "SELECT selection_count, last_used FROM user_learning_stats "
        "WHERE store = 'override' AND qstring = 'q'");
    CHECK(s != 0);
    if (s) {
      CHECK(s->step() == SQLITE_ROW);
      CHECK(s->intOfColumn(0) == 7);
      CHECK(s->intOfColumn(1) == 1234);
    }
  }

  // Scoped so the statements it prepared are finalized before the
  // borrowed connection is closed below.
  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.loadUserCandidateOverrideCache();
    CHECK(lm.fetchCachedOverrideSelection("q") == "V");
  }

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

// A correction is trusted in the context it was made in immediately, but only
// applies context-free once it has proved general.
static void TestContextKeyedOverrides() {
  const string path = TempPath("learningstore-context.db");
  OVSQLiteConnection* db = MakeLegacyUserDB(path);
  LanguageModel::MigrateUserLearningTables(db);

  {
    LanguageModel lm(db, 0, false, false, false, true, true);

    lm.cacheContextOverrideSelection("p1", "q", "A");
    CHECK(lm.fetchCachedContextOverrideSelection("p1", "q") == "A");
    // a different preceding reading must not inherit it
    CHECK(lm.fetchCachedContextOverrideSelection("p2", "q") == "");
    // and one context is not enough to generalise
    CHECK(!lm.overrideGeneralizesAcrossContexts("q"));

    lm.cacheContextOverrideSelection("p2", "q", "A");
    CHECK(!lm.overrideGeneralizesAcrossContexts("q"));

    // third distinct context: the correction has proved itself
    lm.cacheContextOverrideSelection("p3", "q", "A");
    CHECK(lm.overrideGeneralizesAcrossContexts("q"));

    // re-learning a context already seen must not inflate the breadth
    lm.cacheContextOverrideSelection("p1", "other", "B");
    lm.cacheContextOverrideSelection("p1", "other", "B");
    lm.cacheContextOverrideSelection("p1", "other", "B");
    CHECK(!lm.overrideGeneralizesAcrossContexts("other"));

    lm.saveUserBigramCacheAndCandidateOverrideCache(true, true);
  }

  // breadth and context entries have to survive a reload
  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.loadUserCandidateOverrideCache();
    CHECK(lm.fetchCachedContextOverrideSelection("p1", "q") == "A");
    CHECK(lm.fetchCachedContextOverrideSelection("p9", "q") == "");
    CHECK(lm.overrideGeneralizesAcrossContexts("q"));
    CHECK(!lm.overrideGeneralizesAcrossContexts("other"));

    // removing the context entry leaves the reading's breadth alone: the user
    // did make those corrections, whatever is currently resident
    lm.removeCachedContextSelection("p1", "q");
    CHECK(lm.fetchCachedContextOverrideSelection("p1", "q") == "");
    CHECK(lm.overrideGeneralizesAcrossContexts("q"));
    lm.saveUserBigramCacheAndCandidateOverrideCache(true, true);
  }

  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.loadUserCandidateOverrideCache();
    CHECK(lm.fetchCachedContextOverrideSelection("p1", "q") == "");
    CHECK(lm.fetchCachedContextOverrideSelection("p2", "q") == "A");

    lm.flushUserCache();
    CHECK(CountRows(db, "user_context_override_cache") == 0);
    CHECK(!lm.overrideGeneralizesAcrossContexts("q"));
  }

  delete db;
  remove(path.c_str());
}

// Overrides that predate context keying are credited to one short of the gate:
// dormant on the day of the upgrade, so a stray one cannot force itself on
// every context, but one confirmation away from the reach they used to have.
// The credit has to be a one-off -- entries learned afterwards earn it.
static void TestPreExistingOverridesAreGrandfathered() {
  const string path = TempPath("learningstore-grandfather.db");
  OVSQLiteConnection* db = MakeLegacyUserDB(path);
  db->execute("INSERT INTO user_candidate_override_cache VALUES('old', 'O')");

  LanguageModel::MigrateUserLearningTables(db);

  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.loadUserCandidateOverrideCache();
    CHECK(lm.fetchCachedOverrideSelection("old") == "O");
    // Not forced everywhere yet...
    CHECK(!lm.overrideGeneralizesAcrossContexts("old"));

    // ...but the next confirmation in any context gets it there.
    lm.cacheContextOverrideSelection("p1", "old", "O");
    CHECK(lm.overrideGeneralizesAcrossContexts("old"));

    // something learned now starts from one context, not from credit
    lm.cacheOverrideSelection("fresh", "F");
    lm.cacheContextOverrideSelection("p1", "fresh", "F");
    CHECK(!lm.overrideGeneralizesAcrossContexts("fresh"));
    lm.saveUserBigramCacheAndCandidateOverrideCache(true, true);
  }

  // a second migration pass must not hand out free breadth
  LanguageModel::MigrateUserLearningTables(db);
  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.loadUserCandidateOverrideCache();
    CHECK(!lm.overrideGeneralizesAcrossContexts("fresh"));
    CHECK(lm.overrideGeneralizesAcrossContexts("old"));
  }

  delete db;
  remove(path.c_str());
}

// A save that cannot reach the disk must leave the stores dirty so the next one
// retries. Marking them clean on a failed commit loses the correction outright:
// nothing on disk, and nothing in memory saying anything was owed.
static void TestFailedSaveKeepsLearningDirty() {
  const string path = TempPath("learningstore-busy.db");
  OVSQLiteConnection* db = MakeLegacyUserDB(path);
  LanguageModel::MigrateUserLearningTables(db);

  // A second connection holding a write lock: the Phrase Editor, or any other
  // process sharing this database.
  sqlite3* blocker = 0;
  CHECK(sqlite3_open(path.c_str(), &blocker) == SQLITE_OK);
  CHECK(sqlite3_exec(blocker, "BEGIN IMMEDIATE", 0, 0, 0) == SQLITE_OK);

  // Scoped so the statements it prepared are finalized before the
  // borrowed connection is closed below.
  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.cacheOverrideSelection("q1", "A");
    lm.cacheUserBigram("p q1", "P", "A");

    CHECK(!lm.saveUserBigramCacheAndCandidateOverrideCache(true, true));
    CHECK(CountRows(db, "user_candidate_override_cache") ==
          1);  // only 'legacy'
    CHECK(CountRows(db, "user_bigram_cache") == 0);

    sqlite3_exec(blocker, "COMMIT", 0, 0, 0);
    sqlite3_close(blocker);

    // the retry finds the work still outstanding and lands it
    CHECK(lm.saveUserBigramCacheAndCandidateOverrideCache(true, true));
    CHECK(CountRows(db, "user_candidate_override_cache") == 2);
    CHECK(CountRows(db, "user_bigram_cache") == 1);
    CHECK(lm.fetchCachedOverrideSelection("q1") == "A");
  }

  delete db;
  remove(path.c_str());
}

// Flushing has to be all or nothing. Clearing memory while the tables survive
// just means the next load brings the discarded learning back, and the IME
// would have told the user it was gone.
static void TestFlushRefusedWhileEditorHoldsLock() {
  const string path = TempPath("learningstore-flushlock.db");
  const string lockPath = TempPath("learningstore-flushlock.lock");
  OVSQLiteConnection* db = MakeLegacyUserDB(path);
  LanguageModel::MigrateUserLearningTables(db);

  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.cacheOverrideSelection("q1", "A");
    CHECK(lm.saveUserBigramCacheAndCandidateOverrideCache(true, true));
  }

  FILE* lock = fopen(lockPath.c_str(), "w");
  CHECK(lock != 0);
  if (lock) fclose(lock);

  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.setUserPhraseEditingLockPath(lockPath);
    lm.loadUserCandidateOverrideCache();

    CHECK(!lm.flushUserCache());
    // neither the tables nor the resident entries were touched
    CHECK(CountRows(db, "user_candidate_override_cache") == 2);
    CHECK(lm.fetchCachedOverrideSelection("q1") == "A");
  }

  remove(lockPath.c_str());

  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.setUserPhraseEditingLockPath(lockPath);
    lm.loadUserCandidateOverrideCache();

    CHECK(lm.flushUserCache());
    CHECK(CountRows(db, "user_candidate_override_cache") == 0);
    CHECK(lm.fetchCachedOverrideSelection("q1") == "");
  }

  delete db;
  remove(path.c_str());
}

// Taking a reading back to the lexicon's own answer drops the breadth with it,
// so a later correction has to earn its way to being general again.
static void TestRevertingAnOverrideDropsItsBreadth() {
  const string path = TempPath("learningstore-breadth.db");
  OVSQLiteConnection* db = MakeLegacyUserDB(path);
  LanguageModel::MigrateUserLearningTables(db);

  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.cacheOverrideSelection("q1", "A");
    lm.cacheContextOverrideSelection("p1", "q1", "A");
    lm.cacheContextOverrideSelection("p2", "q1", "A");
    lm.cacheContextOverrideSelection("p3", "q1", "A");
    CHECK(lm.overrideGeneralizesAcrossContexts("q1"));
    CHECK(lm.saveUserBigramCacheAndCandidateOverrideCache(true, true));

    lm.removeCachedSelection("q1");
    CHECK(!lm.overrideGeneralizesAcrossContexts("q1"));

    // a reload in between (loadConfig() does this on every preference change)
    // must not resurrect the row we just dropped
    lm.loadUserCandidateOverrideCache();
    CHECK(!lm.overrideGeneralizesAcrossContexts("q1"));
    CHECK(lm.saveUserBigramCacheAndCandidateOverrideCache(true, true));
  }

  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    lm.loadUserCandidateOverrideCache();
    CHECK(!lm.overrideGeneralizesAcrossContexts("q1"));
    // re-learning starts over rather than generalising for free
    lm.cacheOverrideSelection("q1", "B");
    lm.cacheContextOverrideSelection("p1", "q1", "B");
    CHECK(!lm.overrideGeneralizesAcrossContexts("q1"));
  }

  delete db;
  remove(path.c_str());
}

// A cache hit is handed straight back now, so what goes into the cache has to
// be sorted; otherwise the walk would read whatever row SQLite happened to
// yield first.
static void TestCachedBigramsComeBackSorted() {
  const string path = TempPath("learningstore-bigramsort.db");
  remove(path.c_str());
  OVSQLiteConnection* db = OVSQLiteConnection::Open(path);
  assert(db);
  db->execute("CREATE TABLE unigrams (qstring, current, probability, backoff)");
  db->execute("CREATE TABLE bigrams (qstring, previous, current, probability)");
  db->execute(
      "CREATE TABLE user_bigram_cache "
      "(qstring, previous, current, probability)");
  db->execute("CREATE TABLE user_candidate_override_cache (qstring, current)");
  // deliberately inserted worst-first
  db->execute("INSERT INTO bigrams VALUES('p q', 'P', 'WORST', -3.0)");
  db->execute("INSERT INTO bigrams VALUES('p q', 'P', 'BEST', -0.5)");
  db->execute("INSERT INTO bigrams VALUES('p q', 'P', 'MIDDLE', -1.5)");
  LanguageModel::MigrateUserLearningTables(db);

  // Scoped so the statements it prepared are finalized before the
  // borrowed connection is closed below.
  {
    LanguageModel lm(db, 0, false, false, false, true, true);

    BigramVector first = lm.findBigrams("p q");
    CHECK(first.size() == 3);
    if (first.size() == 3) CHECK(first[0].current == "BEST");

    // second call comes from m_bigramCache
    BigramVector cached = lm.findBigrams("p q");
    CHECK(cached.size() == 3);
    if (cached.size() == 3) CHECK(cached[0].current == "BEST");

    // and a learned entry still outranks the lexicon on a cache hit
    lm.cacheUserBigram("p q", "P", "LEARNED");
    BigramVector merged = lm.findBigrams("p q");
    CHECK(merged.size() == 4);
    if (merged.size() == 4) CHECK(merged[0].current == "LEARNED");
  }

  delete db;
  remove(path.c_str());
}

int main(int argc, char** argv) {
  if (argc > 1) g_tempDir = argv[1];

  TestEvictionPrefersLeastUsed();
  TestReadRefreshesRecency();
  TestEvictionRanksByCountWhenNothingIsSingle();
  TestCapacityShrinkEvictsNow();
  TestSelectionCountAndDirtyTracking();
  TestLoadDoesNotClobberNewerMemory();
  TestMigrationAndRoundTrip();
  TestRollsBackInlineStatColumns();
  TestContextKeyedOverrides();
  TestPreExistingOverridesAreGrandfathered();
  TestFailedSaveKeepsLearningDirty();
  TestFlushRefusedWhileEditorHoldsLock();
  TestRevertingAnOverrideDropsItsBreadth();
  TestCachedBigramsComeBackSorted();

  if (failures) {
    cerr << failures << " check(s) failed" << endl;
    return 1;
  }
  cout << "TestLearningStore: OK" << endl;
  return 0;
}
