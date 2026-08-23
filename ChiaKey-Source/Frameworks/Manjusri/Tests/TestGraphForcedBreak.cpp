// Tab is a toggle: the second press at the same boundary takes the break back.
// It has to restore the walk *and* the candidate list, because a break is a
// hard veto -- the phrase it spans stops being built at all, so a user who
// broke by mistake cannot select their way out of it.
#include <iostream>

#include "Graph.h"
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

static OVSQLiteConnection* BuildFixture() {
  OVSQLiteConnection* db = OVSQLiteConnection::Open(":memory:");
  if (!db) return 0;

  db->execute("CREATE TABLE unigrams (qstring, current, probability, backoff)");
  db->execute("CREATE TABLE bigrams (qstring, previous, current, probability)");

  db->execute("INSERT INTO unigrams VALUES('*', '*', -8.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('!', '!', 0.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('$', '$', 0.0, 0.0)");

  db->execute("INSERT INTO unigrams VALUES('R1', 'ONE', -2.0, 0.0)");
  db->execute("INSERT INTO unigrams VALUES('R2', 'TWO', -2.0, 0.0)");
  // Covers R1 R2 as one word, and beats ONE + TWO comfortably.
  db->execute("INSERT INTO unigrams VALUES('R1R2', 'PHRASE', -2.0, 0.0)");

  return db;
}

static const bool CandidatesInclude(const Graph& graph, size_t atIndex,
                                    const string& text) {
  CandidateVector candidates = graph.candidatesAtIndex(atIndex);
  for (size_t i = 0; i < candidates.size(); ++i)
    if (candidates[i].first.first == text) return true;

  return false;
}

int main() {
  OVSQLiteConnection* db = BuildFixture();
  if (!db) {
    cerr << "cannot open fixture" << endl;
    return 1;
  }

  Node::SetPhraseLengthBonus(1.0);
  // Scoped so the statements it prepared are finalized before the
  // borrowed connection is closed below.
  {
    LanguageModel lm(db, 0, false, false, false, false, false);
    Node::SetUNK(lm.UNKUnigram().probability, lm.UNKUnigram().backoff);

    // The graph seeds BOS/EOS itself, so readings start at index 1 and the
    // boundary between them is index 2.
    Graph graph(&lm);
    graph.clear();
    graph.insertQueryBlockAndBuild("R1", 1);
    graph.insertQueryBlockAndBuild("R2", 2);

    CHECK(FastPathAsString(graph.fastWalk("", Location(0, 0))) == "!PHRASE$");
    CHECK(CandidatesInclude(graph, 1, "PHRASE"));

    CHECK(graph.toggleForcedBreakAt(2));
    CHECK(FastPathAsString(graph.fastWalk("", Location(0, 0))) == "!ONETWO$");
    CHECK(!CandidatesInclude(graph, 1, "PHRASE"));

    CHECK(graph.toggleForcedBreakAt(2));
    CHECK(FastPathAsString(graph.fastWalk("", Location(0, 0))) == "!PHRASE$");
    CHECK(CandidatesInclude(graph, 1, "PHRASE"));

    // And it keeps toggling rather than latching either way.
    CHECK(graph.toggleForcedBreakAt(2));
    CHECK(FastPathAsString(graph.fastWalk("", Location(0, 0))) == "!ONETWO$");

    // Breaks at other boundaries are independent.
    graph.clear();
    graph.insertQueryBlockAndBuild("R1", 1);
    graph.insertQueryBlockAndBuild("R2", 2);
    CHECK(graph.toggleForcedBreakAt(2));
    CHECK(FastPathAsString(graph.fastWalk("", Location(0, 0))) == "!ONETWO$");
    // Out of bounds: index 0 is BOS, and the last block is EOS.
    CHECK(!graph.toggleForcedBreakAt(0));
    CHECK(!graph.toggleForcedBreakAt(3));
    CHECK(FastPathAsString(graph.fastWalk("", Location(0, 0))) == "!ONETWO$");
  }
  delete db;

  if (failures) {
    cerr << failures << " check(s) failed" << endl;
    return 1;
  }

  cout << "TestGraphForcedBreak: OK" << endl;
  return 0;
}
