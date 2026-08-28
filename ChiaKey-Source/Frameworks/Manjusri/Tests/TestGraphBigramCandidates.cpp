// Graph::annotatedCandidatesAtIndex(): with a matching previous text, bigram
// candidates come first and are tagged as such; with an unmatched (or empty)
// previous the list degrades to exactly the unigram order that
// candidatesAtIndex() returns. This is the contract the iOS host relies on to
// show context-aware picks in the candidate bar.
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
      cerr << "FAIL " << __LINE__ << ": " << #cond << endl;   \
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

  db->execute("INSERT INTO unigrams VALUES('*', '*', -8.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('!', '!', 0.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('$', '$', 0.0, 0.0)");

  db->execute("INSERT INTO unigrams VALUES('R1', 'A', -1.0, 0.0)");
  // R2's unigram order is X then Y; the bigram below promotes Y after A
  db->execute("INSERT INTO unigrams VALUES('R2', 'X', -2.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('R2', 'Y', -3.0, 0.0)");
  db->execute("INSERT INTO bigrams VALUES('R1 R2', 'A', 'Y', -0.5)");

  return db;
}

int main(int argc, char** argv) {
  const string tempDir = argc > 1 ? argv[1] : "/tmp";
  const string path = tempDir + "/graph-bigram-candidates.db";
  OVSQLiteConnection* db = BuildFixture(path);
  if (!db) {
    cerr << "cannot open fixture" << endl;
    return 1;
  }

  // scoped so the model finalizes its statements before the connection closes
  {
  LanguageModel lm(db, 0, false, false, false, true, true);
  Node::SetUNK(lm.UNKUnigram().probability, lm.UNKUnigram().backoff);

  Graph graph(&lm);
  graph.clear();
  graph.insertQueryBlockAndBuild("R1", 1);
  graph.insertQueryBlockAndBuild("R2", 2);

  // baseline: the unigram-only path is untouched
  {
    CandidateVector plain = graph.candidatesAtIndex(2);
    CHECK(plain.size() == 2);
    CHECK(plain[0].first.first == "X");
    CHECK(plain[1].first.first == "Y");
  }

  // matching previous: the bigram pick leads, tagged, and is deduped from the
  // unigram tail
  {
    AnnotatedCandidateVector annotated =
        graph.annotatedCandidatesAtIndex(2, "A");
    CHECK(annotated.size() == 2);
    CHECK(annotated[0].first.first.first == "Y");
    CHECK(annotated[0].second == kCandidateOriginBigram);
    CHECK(annotated[1].first.first.first == "X");
    CHECK(annotated[1].second == kCandidateOriginUnigram);
  }

  // unmatched previous: same order and tags as the unigram-only path
  {
    AnnotatedCandidateVector annotated =
        graph.annotatedCandidatesAtIndex(2, "ZZZ");
    CHECK(annotated.size() == 2);
    CHECK(annotated[0].first.first.first == "X");
    CHECK(annotated[0].second == kCandidateOriginUnigram);
    CHECK(annotated[1].first.first.first == "Y");
    CHECK(annotated[1].second == kCandidateOriginUnigram);
  }

  // empty previous behaves like unmatched
  {
    AnnotatedCandidateVector annotated =
        graph.annotatedCandidatesAtIndex(2, "");
    CHECK(annotated.size() == 2);
    CHECK(annotated[0].second == kCandidateOriginUnigram);
  }
  }

  delete db;
  remove(path.c_str());

  if (failures) {
    cerr << failures << " check(s) failed" << endl;
    return 1;
  }
  cout << "TestGraphBigramCandidates: OK" << endl;
  return 0;
}
