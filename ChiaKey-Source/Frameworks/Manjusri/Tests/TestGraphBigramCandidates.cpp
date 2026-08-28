// Graph::annotatedCandidatesAtIndex(): the list matches candidatesAtIndex()
// exactly and only flags entries the context outscores, gated the same way
// findHighestScorePair() gates the walk. The FastPath form resolves each
// node's own preceding text, including inside a phrase the walk chose.
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
  // the phrase outscores the A + X split, so the walk covers both blocks
  db->execute("INSERT INTO unigrams VALUES('R1R2', 'AB', -2.0, 0.0)");

  // after A: Z passes the gate but has no unigram to flag; X is already the
  // top; Y passes and is flagged; W loses to the unigram top
  db->execute("INSERT INTO bigrams VALUES('R1 R2', 'A', 'Z', -0.2)");
  db->execute("INSERT INTO bigrams VALUES('R1 R2', 'A', 'X', -0.4)");
  db->execute("INSERT INTO bigrams VALUES('R1 R2', 'A', 'Y', -0.5)");
  db->execute("INSERT INTO bigrams VALUES('R1 R2', 'A', 'W', -10.0)");

  return db;
}

static const vector<string> TextsOf(const AnnotatedCandidateVector& annotated) {
  vector<string> texts;
  for (AnnotatedCandidateVector::const_iterator iter = annotated.begin();
       iter != annotated.end(); ++iter)
    texts.push_back((*iter).text);
  return texts;
}

static const vector<string> TextsOf(const CandidateVector& plain) {
  vector<string> texts;
  for (CandidateVector::const_iterator iter = plain.begin();
       iter != plain.end(); ++iter)
    texts.push_back((*iter).first.first);
  return texts;
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

    const CandidateVector plain = graph.candidatesAtIndex(2);
    CHECK(TextsOf(plain) ==
          vector<string>({"AB", "X", "Y", "W"}));

    // flags never reorder: same texts, same order, same node-relative indexes
    {
      AnnotatedCandidateVector annotated =
          graph.annotatedCandidatesAtIndex(2, "A");
      CHECK(TextsOf(annotated) == TextsOf(plain));
      CHECK(annotated.size() == plain.size());
      for (size_t i = 0; i < annotated.size(); i++) {
        CHECK(annotated[i].indexInNode == plain[i].first.second);
        CHECK(annotated[i].node == plain[i].second);
      }

      // only Y is promoted: Z has no unigram, X is already the top, W fails
      for (size_t i = 0; i < annotated.size(); i++)
        CHECK((annotated[i].origin == kCandidateOriginBigram) ==
              (annotated[i].text == "Y"));
    }

    // unmatched and empty previous flag nothing at all
    {
      const char* others[] = {"ZZZ", ""};
      for (size_t o = 0; o < 2; o++) {
        AnnotatedCandidateVector annotated =
            graph.annotatedCandidatesAtIndex(2, others[o]);
        CHECK(TextsOf(annotated) == TextsOf(plain));
        for (size_t i = 0; i < annotated.size(); i++)
          CHECK(annotated[i].origin == kCandidateOriginUnigram);
      }
    }

    // the FastPath form derives each node's own previous. The walk covers
    // both blocks with the phrase AB, so the R2 node's context is the "A"
    // inside it -- a single caller-supplied previous cannot express this.
    {
      FastPath path = graph.fastWalk("", Location(0, 0));
      CHECK(FastPathAsString(path).find("AB") != string::npos);

      AnnotatedCandidateVector annotated =
          graph.annotatedCandidatesAtIndex(2, path);
      CHECK(TextsOf(annotated) == TextsOf(plain));
      for (size_t i = 0; i < annotated.size(); i++)
        CHECK((annotated[i].origin == kCandidateOriginBigram) ==
              (annotated[i].text == "Y"));
    }

    // an overridden node is never flagged under the user's pick
    {
      graph.overrideNodeCandidate(*(plain[1].second), "X", false);

      AnnotatedCandidateVector annotated =
          graph.annotatedCandidatesAtIndex(2, "A");
      CHECK(TextsOf(annotated) == TextsOf(plain));
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
