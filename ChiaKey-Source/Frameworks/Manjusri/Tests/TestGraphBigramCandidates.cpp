// Graph::annotatedCandidatesAtIndex(): context-promoted candidates lead and
// are tagged, gated the same way findHighestScorePair() gates the walk, and
// grams merged from every preceding reading are reachable.
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

static OVSQLiteConnection* BuildFixture() {
  OVSQLiteConnection* db = OVSQLiteConnection::Open(":memory:");
  if (!db) return 0;

  db->execute("CREATE TABLE unigrams (qstring, current, probability, backoff)");
  db->execute("CREATE TABLE bigrams (qstring, previous, current, probability)");

  db->execute("INSERT INTO unigrams VALUES('*', '*', -8.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('!', '!', 0.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('$', '$', 0.0, 0.0)");

  db->execute("INSERT INTO unigrams VALUES('R1', 'A', -1.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('R2', 'X', -2.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('R2', 'Y', -3.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('R2', 'W', -3.5, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('R3', 'M', -2.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('R3', 'N', -3.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('R1R2', 'P', -2.5, 0.0)");

  // after A: Z passes the gate but has no unigram; X is already the top;
  // Y passes and should be promoted; W loses to the unigram top
  db->execute("INSERT INTO bigrams VALUES('R1 R2', 'A', 'Z', -0.2)");
  db->execute("INSERT INTO bigrams VALUES('R1 R2', 'A', 'X', -0.4)");
  db->execute("INSERT INTO bigrams VALUES('R1 R2', 'A', 'Y', -0.5)");
  db->execute("INSERT INTO bigrams VALUES('R1 R2', 'A', 'W', -10.0)");

  // R3 is preceded by both the R2 node and the R1R2 phrase node; the R2-keyed
  // bigram only survives if build() merges grams across preceding readings
  db->execute("INSERT INTO bigrams VALUES('R2 R3', 'X', 'N', -0.1)");

  return db;
}

int main() {
  OVSQLiteConnection* db = BuildFixture();
  if (!db) {
    cerr << "cannot open fixture" << endl;
    return 1;
  }

  {
    LanguageModel lm(db, 0, false, false, false, false, false);
    Node::SetUNK(lm.UNKUnigram().probability, lm.UNKUnigram().backoff);

    Graph graph(&lm);
    graph.clear();
    graph.insertQueryBlockAndBuild("R1", 1);
    graph.insertQueryBlockAndBuild("R2", 2);
    graph.insertQueryBlockAndBuild("R3", 3);

    // the unigram-only path is untouched; index 2 is covered by the R1R2
    // phrase node (longest first) and the R2 node
    {
      CandidateVector plain = graph.candidatesAtIndex(2);
      CHECK(plain.size() == 4);
      CHECK(plain[0].first.first == "P");
      CHECK(plain[1].first.first == "X");
      CHECK(plain[2].first.first == "Y");
      CHECK(plain[3].first.first == "W");
    }

    // within the R2 node: Y is promoted and tagged; X stays first-of-unigrams
    // untagged; W failed the gate; Z has no unigram and must not appear
    {
      AnnotatedCandidateVector annotated =
          graph.annotatedCandidatesAtIndex(2, "A");
      CHECK(annotated.size() == 4);
      CHECK(annotated[0].text == "P");
      CHECK(annotated[0].origin == kCandidateOriginUnigram);
      CHECK(annotated[1].text == "Y");
      CHECK(annotated[1].origin == kCandidateOriginBigram);
      CHECK(annotated[2].text == "X");
      CHECK(annotated[2].origin == kCandidateOriginUnigram);
      CHECK(annotated[3].text == "W");
      CHECK(annotated[3].origin == kCandidateOriginUnigram);
    }

    // unmatched and empty previous both degrade to the plain order
    {
      AnnotatedCandidateVector annotated =
          graph.annotatedCandidatesAtIndex(2, "ZZZ");
      CHECK(annotated.size() == 4);
      CHECK(annotated[0].text == "P");
      CHECK(annotated[1].text == "X");
      CHECK(annotated[1].origin == kCandidateOriginUnigram);

      annotated = graph.annotatedCandidatesAtIndex(2, "");
      CHECK(annotated.size() == 4);
      CHECK(annotated[1].text == "X");
      CHECK(annotated[1].origin == kCandidateOriginUnigram);
    }

    // R3's node kept the bigram keyed by the R2 reading's text even though
    // the R1R2 phrase node is enumerated first among its predecessors
    {
      AnnotatedCandidateVector annotated =
          graph.annotatedCandidatesAtIndex(3, "X");
      CHECK(annotated.size() >= 2);
      CHECK(annotated[0].text == "N");
      CHECK(annotated[0].origin == kCandidateOriginBigram);
    }

    // the walk sees the merged bigram too: after committing X, N beats M
    {
      const Node& r3node = *(graph.annotatedCandidatesAtIndex(3, "X")[0].node);
      CHECK(r3node.findHighestScorePair("X").first == "N");
    }

    // an overridden node is never reordered under the user's pick
    {
      CandidateVector plain = graph.candidatesAtIndex(2);
      graph.overrideNodeCandidate(*(plain[1].second), "X", false);

      AnnotatedCandidateVector annotated =
          graph.annotatedCandidatesAtIndex(2, "A");
      CHECK(annotated.size() == 4);
      CHECK(annotated[1].text == "X");
      for (size_t i = 0; i < annotated.size(); i++)
        CHECK(annotated[i].origin == kCandidateOriginUnigram);
    }
  }

  delete db;

  if (failures) {
    cerr << failures << " check(s) failed" << endl;
    return 1;
  }
  cout << "TestGraphBigramCandidates: OK" << endl;
  return 0;
}
