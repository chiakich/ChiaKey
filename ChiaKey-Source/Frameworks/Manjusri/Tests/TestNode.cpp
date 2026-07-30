// Coverage for NodeSet lookups (FindNodeAtLocation / FindNodesPreceding /
// FindNodesOverlapping) -- ported from the old UnitTest++-based TestMajusri.cpp,
// which had rotted against the current Node.h API (LocationPair -> Location,
// FindNode -> FindNodeAtLocation, FindNodesPreceeding -> FindNodesPreceding).
#include <cassert>
#include <cstdio>
#include <iostream>

#include "Graph.h"

using namespace std;
using namespace Manjusri;

static int failures = 0;

#define CHECK(cond)                                          \
  do {                                                        \
    if (!(cond)) {                                            \
      cerr << "FAIL " << __LINE__ << ": " << #cond << endl;   \
      failures++;                                             \
    }                                                          \
  } while (0)

static void TestNodeSetLookups() {
  Node a(Location(0, 1));
  Node b(Location(1, 2));
  Node c(Location(3, 1));

  NodeSet s;
  s.insert(a);
  s.insert(b);
  s.insert(c);

  NodeSet::iterator i = FindNodeAtLocation(s, Location(1, 2));
  CHECK(i != s.end());
  CHECK(*i == b);

  vector<NodeSet::const_iterator> preceding =
      FindNodesPreceding(s, Location(1, 2));
  CHECK(preceding.size() == 1);
  CHECK(*preceding[0] == a);

  vector<NodeSet::const_iterator> overlapping =
      FindNodesOverlapping(s, Location(1, 1));
  CHECK(overlapping.size() == 1);
  CHECK(*overlapping[0] == b);
}

int main() {
  TestNodeSetLookups();

  if (failures) {
    cerr << failures << " check(s) failed" << endl;
    return 1;
  }
  cout << "TestNode: OK" << endl;
  return 0;
}
