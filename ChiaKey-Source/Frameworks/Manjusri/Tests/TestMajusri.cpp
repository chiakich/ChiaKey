/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#include <UnitTest++.h>

#include <iostream>

#include "Graph.h"
#include "LanguageModel.h"

using namespace std;
using namespace Manjusri;

TEST(NodeSet) {
  Node a(LocationPair(0, 1));
  Node b(LocationPair(1, 2));
  Node c(LocationPair(3, 1));

  NodeSet s;
  s.insert(a);
  s.insert(b);
  s.insert(c);

  NodeSet::iterator i;
  i = FindNode(s, LocationPair(1, 2));
  CHECK_EQUAL(b, *i);

  vector<NodeSet::iterator> v;
  v = FindNodesPreceeding(s, LocationPair(1, 2));
  CHECK_EQUAL(1, v.size());
  CHECK_EQUAL(a, *v[0]);

  v = FindNodesOverlapping(s, LocationPair(1, 1));
  CHECK_EQUAL(1, v.size());
  CHECK_EQUAL(b, *v[0]);
}
