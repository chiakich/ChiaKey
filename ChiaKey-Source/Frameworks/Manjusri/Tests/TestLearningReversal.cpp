// The learn/forget decision, driven through ManjusriComposer the way the IME
// drives it.
//
// Both call sites ask Node whether the user's pick is what the lexicon would
// have offered anyway; if it is, the correction is retracted rather than
// recorded. That question has to be answered against the lexicon's own
// ordering. adjustScoreWithSelection() moves a learned pick to the front of
// m_unigramCurrents, so asking the live vector instead means that once an
// override exists the answer is "yes" for the overridden text and never for
// the lexicon's real top candidate -- which inverts both directions:
// reinforcing a learned pick deletes it (taking the generalization breadth
// with it), and reverting writes an inverse override instead of forgetting.
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "Graph.h"
#include "LanguageModel.h"
#include "OVIMSmartMandarin.h"

using namespace std;
using namespace Manjusri;
using namespace OpenVanilla;

static int failures = 0;

#define CHECK(cond)                                         \
  do {                                                      \
    if (!(cond)) {                                          \
      cerr << "FAIL " << __LINE__ << ": " << #cond << endl; \
      failures++;                                           \
    }                                                       \
  } while (0)

// COMMON is the lexicon's answer for reading Z; RARE is what the user wants.
// P and Q are unambiguous readings, used only as preceding context.
static OVSQLiteConnection* BuildFixture(const string& path) {
  remove(path.c_str());
  OVSQLiteConnection* db = OVSQLiteConnection::Open(path);
  if (!db) return 0;

  db->execute("CREATE TABLE unigrams (qstring, current, probability, backoff)");
  db->execute("CREATE TABLE bigrams (qstring, previous, current, probability)");
  db->execute(
      "CREATE TABLE user_bigram_cache (qstring, previous, current, "
      "probability)");
  db->execute("CREATE TABLE user_candidate_override_cache (qstring, current)");
  db->execute("CREATE TABLE user_context_override_cache (qstring, current)");
  db->execute(
      "CREATE TABLE user_learning_stats (store, qstring, selection_count, "
      "last_used)");
  db->execute(
      "CREATE UNIQUE INDEX user_learning_stats_key ON user_learning_stats "
      "(store, qstring)");
  db->execute(
      "CREATE UNIQUE INDEX ovr_u ON user_candidate_override_cache (qstring)");
  db->execute(
      "CREATE UNIQUE INDEX ctx_u ON user_context_override_cache (qstring)");
  db->execute("CREATE UNIQUE INDEX big_u ON user_bigram_cache (qstring)");

  db->execute("INSERT INTO unigrams VALUES('*', '*', -8.0, 0.0)");
  // Empty marker text so composedString() is just the sentence.
  db->execute("INSERT INTO unigrams VALUES('!', '', 0.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('$', '', 0.0, 0.0)");

  db->execute("INSERT INTO unigrams VALUES('P', 'P', -2.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('Q', 'Q', -2.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('Z', 'COMMON', -2.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('Z', 'RARE', -3.5, 0.0)");

  return db;
}

// Types the readings, then picks `want` at the last position if it is not
// already what came out. Returns what the walk produced before any correction.
static const string Type(LanguageModel* lm, const vector<string>& readings,
                         const string& want) {
  ManjusriComposer composer(lm);
  composer.clear();
  for (size_t i = 0; i < readings.size(); i++)
    composer.insertAt(i + 1, readings[i]);
  composer.update();

  const string got = composer.composedString();
  if (want.empty()) return got;

  const size_t at = readings.size() - 1;
  vector<string> candidates =
      composer.collectCandidates(at + composer.cursorLeftBound(), false);
  for (size_t i = 0; i < candidates.size(); i++)
    if (candidates[i] == want) {
      composer.chooseCandidate(i);
      break;
    }

  return got;
}

static size_t RowCount(OVSQLiteConnection* db, const char* sql) {
  size_t count = 0;
  OVSQLiteStatementRef statement = db->prepare("%s", sql);
  while (statement && statement->step() == SQLITE_ROW) count++;
  return count;
}

static const string OverrideFor(OVSQLiteConnection* db, const char* table,
                                const char* qstring) {
  string result;
  OVSQLiteStatementRef statement =
      db->prepare("SELECT current FROM %s WHERE qstring = %Q", table, qstring);
  if (statement && statement->step() == SQLITE_ROW)
    result = SafeColumnText(statement.get(), 0);
  return result;
}

static size_t BreadthFor(OVSQLiteConnection* db, const char* qstring) {
  size_t result = 0;
  OVSQLiteStatementRef statement = db->prepare(
      "SELECT selection_count FROM user_learning_stats WHERE store = "
      "'override_breadth' AND qstring = %Q",
      qstring);
  if (statement && statement->step() == SQLITE_ROW)
    result = (size_t)statement->intOfColumn(0);
  return result;
}

int main(int argc, char** argv) {
  const string tempDir = argc > 1 ? argv[1] : "/tmp";

  vector<string> z, pz, qz;
  z.push_back("Z");
  pz.push_back("P");
  pz.push_back("Z");
  qz.push_back("Q");
  qz.push_back("Z");

  // ---- reverting to the lexicon's own answer retracts the correction ----
  {
    const string path = tempDir + "/learning-reversal-revert.db";
    OVSQLiteConnection* db = BuildFixture(path);
    if (!db) {
      cerr << "cannot open fixture" << endl;
      return 1;
    }

    // Scoped so the statements it prepared are finalized before the
    // borrowed connection is closed below.
    {
      LanguageModel lm(db, 0, false, false, false, true, true);
      Node::SetUNK(lm.UNKUnigram().probability, lm.UNKUnigram().backoff);
      lm.loadUserBigramCache();
      lm.loadUserCandidateOverrideCache();

      CHECK(Type(&lm, pz, "RARE") == "PCOMMON");
      lm.saveUserBigramCacheAndCandidateOverrideCache(true, true);
      CHECK(OverrideFor(db, "user_candidate_override_cache", "Z") == "RARE");
      CHECK(BreadthFor(db, "Z") == 1);

      // The correction took effect...
      CHECK(Type(&lm, pz, "") == "PRARE");

      // ...and now the user takes it back.
      Type(&lm, pz, "COMMON");
      lm.saveUserBigramCacheAndCandidateOverrideCache(true, true);

      CHECK(RowCount(db, "SELECT * FROM user_candidate_override_cache") == 0);
      CHECK(RowCount(db, "SELECT * FROM user_context_override_cache") == 0);
      // The breadth ratchet goes with it: an abandoned correction must not let
      // a later one generalise on evidence the user has disowned.
      CHECK(BreadthFor(db, "Z") == 0);
      CHECK(Type(&lm, pz, "") == "PCOMMON");
    }
    delete db;
  }

  // ---- re-picking an already-learned text reinforces, never retracts ----
  {
    const string path = tempDir + "/learning-reversal-reinforce.db";
    OVSQLiteConnection* db = BuildFixture(path);
    if (!db) {
      cerr << "cannot open fixture" << endl;
      return 1;
    }

    // Scoped so the statements it prepared are finalized before the
    // borrowed connection is closed below.
    {
      LanguageModel lm(db, 0, false, false, false, true, true);
      Node::SetUNK(lm.UNKUnigram().probability, lm.UNKUnigram().backoff);
      lm.loadUserBigramCache();
      lm.loadUserCandidateOverrideCache();

      // Three distinct preceding contexts (BOS, P, Q) reach the breadth the
      // context-free override needs to apply everywhere.
      Type(&lm, z, "RARE");
      Type(&lm, pz, "RARE");
      Type(&lm, qz, "RARE");
      lm.saveUserBigramCacheAndCandidateOverrideCache(true, true);

      CHECK(BreadthFor(db, "Z") ==
            LanguageModel::c_overrideGeneralizationContexts);
      CHECK(OverrideFor(db, "user_candidate_override_cache", "Z") == "RARE");
      CHECK(RowCount(db, "SELECT * FROM user_context_override_cache") == 3);

      // RARE is already what comes out here, so picking it again is the user
      // confirming, not correcting.
      CHECK(Type(&lm, pz, "RARE") == "PRARE");
      lm.saveUserBigramCacheAndCandidateOverrideCache(true, true);

      CHECK(OverrideFor(db, "user_candidate_override_cache", "Z") == "RARE");
      CHECK(OverrideFor(db, "user_context_override_cache", "P Z") == "RARE");
      CHECK(BreadthFor(db, "Z") ==
            LanguageModel::c_overrideGeneralizationContexts);
    }
    delete db;
  }

  if (failures) {
    cerr << "TestLearningReversal: " << failures << " failure(s)" << endl;
    return 1;
  }

  cout << "TestLearningReversal: OK" << endl;
  return 0;
}
