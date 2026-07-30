// Walker-level check for adjustScoreWithSelection: a learned rare candidate has
// to survive the walk, not just sit at the front of one node's candidate list.
//
// The corpus is built so that the two readings R1 R2 can be covered either by
// two single-syllable nodes or by one two-syllable phrase. The phrase is scored
// so it loses to the single-character split by a small margin, but wins by a
// wide margin if the split's first node collapses to its rare candidate's own
// probability -- which is exactly what the old reorder-only behaviour did.
#include <cstdio>
#include <iostream>

#include "Graph.h"
#include "LanguageModel.h"

using namespace std;
using namespace Manjusri;

static int failures = 0;

#define CHECK(cond)                                           \
  do {                                                        \
    if (!(cond)) {                                            \
      cerr << "FAIL " << __LINE__ << ": " << #cond << endl;    \
      failures++;                                             \
    }                                                         \
  } while (0)

static OVSQLiteConnection* BuildFixture(const string& path) {
  remove(path.c_str());
  OVSQLiteConnection* db = OVSQLiteConnection::Open(path);
  if (!db) return 0;

  db->execute("CREATE TABLE unigrams (qstring, current, probability, backoff)");
  db->execute("CREATE TABLE bigrams (qstring, previous, current, probability)");
  db->execute("CREATE TABLE user_bigram_cache (qstring, previous, current, probability)");
  db->execute("CREATE TABLE user_candidate_override_cache (qstring, current)");
  db->execute("CREATE TABLE user_learning_stats (store, qstring, selection_count, last_used)");
  db->execute("CREATE UNIQUE INDEX user_learning_stats_key ON user_learning_stats (store, qstring)");
  db->execute("CREATE UNIQUE INDEX ovr_u ON user_candidate_override_cache (qstring)");
  db->execute("CREATE UNIQUE INDEX big_u ON user_bigram_cache (qstring)");

  // Zero backoffs keep the arithmetic below readable: a node then scores
  // exactly its unigram probability plus any length prior.
  db->execute("INSERT INTO unigrams VALUES('*', '*', -8.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('!', '!', 0.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('$', '$', 0.0, 0.0)");

  // R1 alone: a common candidate and a rare one
  db->execute("INSERT INTO unigrams VALUES('R1', 'COMMON', -2.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('R1', 'RARE', -4.5, 0.0)");
  // R2 alone
  db->execute("INSERT INTO unigrams VALUES('R2', 'X', -2.0, 0.0)");
  // R1R2 as one phrase. With a 1.0 length prior it scores -4.2, so it loses to
  // COMMON + X (-4.0) by a hair -- but beats RARE + X (-6.5) comfortably.
  db->execute("INSERT INTO unigrams VALUES('R1R2', 'PHRASE', -5.2, 0.0)");

  return db;
}

// Mirrors ManjusriComposer: the graph seeds BOS/EOS itself, so readings start
// at index 1.
static const string WalkText(LanguageModel* lm) {
  Graph graph(lm);
  graph.clear();
  graph.insertQueryBlockAndBuild("R1", 1);
  graph.insertQueryBlockAndBuild("R2", 2);

  FastPath path = graph.fastWalk("", Location(0, 0));
  return FastPathAsString(path);
}

int main(int argc, char** argv) {
  const string tempDir = argc > 1 ? argv[1] : "/tmp";
  const string path = tempDir + "/learned-pick-walk.db";
  OVSQLiteConnection* db = BuildFixture(path);
  if (!db) {
    cerr << "cannot open fixture" << endl;
    return 1;
  }

  Node::SetPhraseLengthBonus(1.0);

  // Baseline: nothing learned, the two-character split should win.
  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    Node::SetUNK(lm.UNKUnigram().probability, lm.UNKUnigram().backoff);
    string text = WalkText(&lm);
    cout << "no learning:      " << text << endl;
    CHECK(text.find("COMMON") != string::npos);
    CHECK(text.find("PHRASE") == string::npos);
  }

  // After the user picks the rare candidate for R1, the walk must still take
  // the two-character split and must show RARE. Before the scoring fix the node
  // fell to -4.5 and PHRASE took over the whole span.
  {
    LanguageModel lm(db, 0, false, false, false, true, true);
    Node::SetUNK(lm.UNKUnigram().probability, lm.UNKUnigram().backoff);
    lm.cacheOverrideSelection("R1", "RARE");

    string text = WalkText(&lm);
    cout << "after learning:   " << text << endl;
    CHECK(text.find("RARE") != string::npos);
    CHECK(text.find("PHRASE") == string::npos);
    CHECK(text.find("COMMON") == string::npos);
  }

  delete db;
  remove(path.c_str());

  if (failures) {
    cerr << failures << " check(s) failed" << endl;
    return 1;
  }
  cout << "TestLearnedPickSurvivesWalk: OK" << endl;
  return 0;
}
