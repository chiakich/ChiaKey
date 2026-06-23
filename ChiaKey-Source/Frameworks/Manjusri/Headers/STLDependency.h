/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef STLDependency
#define STLDependency

#include <set>
#include <vector>

namespace Manjusri {
using namespace std;

typedef double Score;
typedef pair<size_t, size_t> Location;
typedef pair<Location, string> LocationStringPair;
typedef pair<string, Score> StringScorePair;
typedef vector<StringScorePair> StringScorePairVector;
typedef map<string, Score> StringScoreMap;
};  // namespace Manjusri

#endif
