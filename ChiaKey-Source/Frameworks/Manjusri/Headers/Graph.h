/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef Graph_h
#define Graph_h

#include "Node.h"

namespace Manjusri {
using namespace std;

typedef pair<StringScorePair, NodeSet::const_iterator>
    StringScoreNodeSetIteratorPair;
typedef vector<StringScoreNodeSetIteratorPair> Path;

class PathNode {
 public:
  PathNode(const string& pText, double pScore, NodeSet::const_iterator pPointer)
      : text(pText), score(pScore), nodePointer(pPointer) {}

  string text;
  double score;
  NodeSet::const_iterator nodePointer;
};

typedef vector<PathNode> FastPath;

ostream& operator<<(ostream& stream, const FastPath& path);

inline ostream& operator<<(ostream& stream, const FastPath& path) {
  for (vector<PathNode>::const_iterator iter = path.begin(); iter != path.end();
       ++iter) {
    Location loc = (*(*iter).nodePointer).location();

    stream << (*iter).text << " (" << loc.first << ", " << loc.second << ", "
           << (*iter).score << ") -> ";
  }
  stream << "END";
  return stream;
}

typedef pair<pair<string, size_t>, NodeSet::const_iterator> Candidate;
typedef vector<Candidate> CandidateVector;

const string PathAsString(const Path& path);

inline const string PathAsString(const Path& path) {
  string result;
  for (Path::const_iterator iter = path.begin(); iter != path.end(); ++iter)
    result += (*iter).first.first;

  return result;
}

inline const string FastPathAsString(const FastPath& path) {
  string result;
  for (FastPath::const_iterator iter = path.begin(); iter != path.end(); ++iter)
    result += (*iter).text;

  return result;
}

inline const vector<string> FastPathAsTextSegments(const FastPath& path) {
  vector<string> results;
  for (FastPath::const_iterator iter = path.begin(); iter != path.end();
       ++iter) {
    const string& s = (*iter).text;
    if (s.length()) results.push_back(s);
  }

  return results;
}

ostream& operator<<(ostream& stream, const Candidate& candi);
ostream& operator<<(ostream& stream, const CandidateVector& cvec);

inline ostream& operator<<(ostream& stream, const Candidate& candi) {
  Location location = (*(candi.second)).location();
  string qstring = (*(candi.second)).queryString();
  stream << "candidate: " << candi.first.first << ", is the index "
         << candi.first.second << " of node at (" << location.first << ", "
         << location.second << "), qstring: " << qstring;
  return stream;
}

inline ostream& operator<<(ostream& stream, const CandidateVector& cvec) {
  for (CandidateVector::const_iterator iter = cvec.begin(); iter != cvec.end();
       ++iter) {
    stream << endl << *iter;
  }

  return stream;
}

class PathCompare {
 public:
  int operator()(const Path& p1, const Path& p2) const {
    double p1s = 0.0, p2s = 0.0;
    if (p1.size()) p1s = p1[0].first.second;

    if (p2.size()) p2s = p2[0].first.second;

    // sort in descending order
    return p1s > p2s;
  }
};

class NodeSetIteratorCompareByLength {
 public:
  int operator()(const NodeSet::const_iterator& i1,
                 const NodeSet::const_iterator& i2) const {
    return (*i1).location().second > (*i2).location().second;
  }
};

ostream& operator<<(ostream& stream, const Path& path);

inline ostream& operator<<(ostream& stream, const Path& path) {
  stream << "path: ";
  for (Path::const_iterator piter = path.begin(); piter != path.end();) {
    const StringScoreNodeSetIteratorPair& ssnsip = *piter;
    stream << ssnsip.first.first << "(" << ssnsip.first.second << ") at ("
           << (*(ssnsip.second)).location().first << ", "
           << (*(ssnsip.second)).location().second << ")";
    if (++piter != path.end()) stream << " <- ";
  }

  return stream;
}

ostream& operator<<(ostream& stream, const vector<Path>& paths);

inline ostream& operator<<(ostream& stream, const vector<Path>& paths) {
  size_t counter = 0;

  for (vector<Path>::const_iterator piter = paths.begin(); piter != paths.end();
       ++piter) {
    stream << endl << counter++ << ": " << *piter;
  }
  return stream;
}

class SubblockBuilder {
 public:
  SubblockBuilder() {
    map<string, string>& m = m_aliasMap;
#include "MandarinAlias.h"
  }

  vector<string> buildSubblockWithAlias(vector<string>::const_iterator begin,
                                        vector<string>::const_iterator end) {
    vector<string> results;

    if (begin == end) return results;

    string r1;
    string r2;

    // we only alias the first char
    r1 += *begin;
    if (m_aliasMap.find(*begin) != m_aliasMap.end()) {
      r2 += m_aliasMap[*begin];
    } else {
      r2 += *begin;
    }
    ++begin;

    for (; begin != end; ++begin) {
      r1 += *begin;
      r2 += *begin;
    }

    if (r1 != r2) {
      results.push_back(r1);
      results.push_back(r2);
      // cerr << "query has alias, r1 = " << r1 << ", r2 = " << r2 << endl;
    } else {
      results.push_back(r1);
    }

    return results;
  }

 protected:
  map<string, string> m_aliasMap;
};

class Graph {
 public:
  // does not own the language model
  Graph(LanguageModel* lm, size_t queryGenerationSpan = 8);

  void clear();
  void build(StringFilter* filter = 0);
  vector<Path> walk(const string& previous = "",
                    const Location& location = Location(0, 0),
                    bool aggressive = false);
  FastPath fastWalk(const string& previous = "",
                    const Location& location = Location(0, 0));

  void setSource(const StringVector& source);

  double lastBuildTime();
  double lastWalkTime();

  StringVector queryBlocks() const;

  // if cursorAtEndOfBlock == false (default), cursor must be in or just before
  // the block (Microsoft-style); set it to true for Apple-style candidate
  // search
  CandidateVector candidatesAtIndex(size_t atIndex,
                                    bool cursorAtEndOfBlock = false) const;

  bool overrideNodeCandidate(const Node& queryNode, const string& currentText,
                             bool cacheSelection = true);

  bool insertQueryBlockAndBuild(const string& newBlock, size_t atIndex,
                                StringFilter* filter = 0);
  bool removeQueryBlockAndBuild(size_t atIndex, StringFilter* filter = 0);

  // this is used to "pop" the head node, be sure you pop the one in the path
  // with highest score
  pair<bool, string> shiftNodeAndMaintainPathWalk(const Node& targetNode);
  pair<bool, string> shiftNodeAndMaintainPathWalk(const Node& targetNode,
                                                  const Node& refNextNode);

  friend ostream& operator<<(ostream& stream, const Graph& graph);

 protected:
  void resetSource();

  LanguageModel* m_LM;

  // the building blocks
  StringVector m_source;
  NodeSet m_nodes;
  size_t m_queryGenerationSpan;

  double m_buildTime;
  double m_walkTime;

  SubblockBuilder m_subblockBuilder;
};

inline ostream& operator<<(ostream& stream, const Graph& graph) {
  stream << "graph: " << graph.m_nodes.size() << " nodes";

  for (NodeSet::const_iterator ni = graph.m_nodes.begin();
       ni != graph.m_nodes.end(); ++ni) {
    stream << endl << *ni;
    stream << endl << string(60, '=');
  }
  return stream;
}

inline Graph::Graph(LanguageModel* lm, size_t queryGenerationSpan)
    : m_LM(lm),
      m_queryGenerationSpan(queryGenerationSpan),
      m_buildTime(0.0),
      m_walkTime(0.0) {
  resetSource();
}

inline void Graph::clear() {
  m_nodes.clear();
  m_source.clear();
  m_buildTime = 0.0;
  m_walkTime = 0.0;
  resetSource();
}

inline void Graph::resetSource() {
  StringVector emptyVector;
  setSource(emptyVector);
}

inline void Graph::setSource(const StringVector& source) {
  m_source.clear();
  m_source.push_back(m_LM->BOSQueryString());
  m_source.insert(m_source.end(), source.begin(), source.end());
  m_source.push_back(m_LM->EOSQueryString());
}

inline double Graph::lastBuildTime() { return m_buildTime; }

inline double Graph::lastWalkTime() { return m_walkTime; }

inline void Graph::build(StringFilter* filter) {
  // cerr << "build " << (filter ? "has filter" : "has no filter") << endl;

  m_buildTime = 0.0;
  OVBenchmark benchmark;
  benchmark.start();
  size_t builtBlocks = 0;

  StringVector& qblocks = m_source;
  vector<LocationStringPair> subblocks;

  for (StringVector::const_iterator qiter = qblocks.begin();
       qiter != qblocks.end(); ++qiter) {
    for (StringVector::const_iterator siter = qiter + 1;;) {
      size_t length = (size_t)(siter - qiter);
      if (length > m_queryGenerationSpan) break;

      size_t location = (size_t)(qiter - qblocks.begin());

      vector<string> sbs =
          m_subblockBuilder.buildSubblockWithAlias(qiter, siter);
      for (vector<string>::const_iterator si = sbs.begin(); si != sbs.end();
           ++si)
        subblocks.push_back(
            LocationStringPair(Location(location, length), *si));

      // subblocks.push_back(LocationStringPair(Location(location, length),
      // StringVectorHelper::CombineIntoString(SVH::SubVector(qiter, siter))));

      // we can't put ++siter in for, because if siter == qblocks.end() and we
      // ++siter, diaster ensues in MSC++
      if (siter != qblocks.end())
        ++siter;
      else
        break;
    }
  }

  // cerr << subblocks.size() << " subblocks to build" << endl;

  set<Location> aliasToIgnore;

  for (vector<LocationStringPair>::iterator sbiter = subblocks.begin();
       sbiter != subblocks.end(); ++sbiter) {
    Location& location = (*sbiter).first;

    if (aliasToIgnore.find(location) != aliasToIgnore.end()) continue;

    string& subblock = (*sbiter).second;

    // cerr << "building " << subblock << endl;

    // we only build subblocks with length > 2 only if the subblock can be found
    // in the LM
    if (location.second > 1 && !m_LM->isInDictionary(subblock, true, filter)) {
      // cerr << "ignored 1" << endl;
      continue;
    }

    // and determine if the node already exists
    NodeSet::const_iterator existingNode;
    if ((existingNode = FindNodeAtLocation(m_nodes, location)) != m_nodes.end())
      if ((*existingNode).queryString() == subblock) {
        // cerr << "ignored 2" << endl;
        continue;
      }

    vector<NodeSet::const_iterator> precedingNodes =
        FindNodesPreceding(m_nodes, location);
    if (!precedingNodes.size()) {
      Node node(location, subblock);
      node.addSortedUnigrams(m_LM->findUnigrams(subblock, true, filter));

      // candidate cache
      string cachedCurrentText = m_LM->fetchCachedOverrideSelection(subblock);
      if (cachedCurrentText.size()) {
        node.adjustScoreWithSelection(cachedCurrentText);

        // because alias subblock comes next (with same location with aliased
        // qstring), we want to ignore it
        aliasToIgnore.insert(location);
      }

      m_nodes.insert(node);
      builtBlocks++;
      // cerr << "built 1" << endl;
      continue;
    }

    // now we build the blocks in relation to the preceding nodes
    for (vector<NodeSet::const_iterator>::iterator pniter =
             precedingNodes.begin();
         pniter != precedingNodes.end(); ++pniter) {
      const Node& pnode = **pniter;
      Node node(location, subblock);

      // builds the bigram
      BigramVector bivec = m_LM->findBigrams(
          m_LM->combineBigramQueryString(pnode.queryString(), subblock),
          filter);
      // cerr << "found bigrams:" << endl;
      // cerr << bivec;
      // cerr << endl;

      node.addSortedBigrams(m_LM->findBigrams(
          m_LM->combineBigramQueryString(pnode.queryString(), subblock),
          filter));

      // build the unigram backoffs
      node.addUnigramBackoffs(
          m_LM->findUnigrams(pnode.queryString(), true, filter));

      // then the unigrams
      node.addSortedUnigrams(m_LM->findUnigrams(subblock, true, filter));

      // candidate cache
      string cachedCurrentText = m_LM->fetchCachedOverrideSelection(subblock);

      if (cachedCurrentText.size()) {
        node.adjustScoreWithSelection(cachedCurrentText);

        // because alias subblock comes next (with same location with aliased
        // qstring), we want to ignore it
        aliasToIgnore.insert(location);
      }

      // put in the node
      m_nodes.insert(node);
      builtBlocks++;
      // cerr << "built 2" << endl;
    }
  }

  // cerr << builtBlocks << " blocks actually built" << endl;
  m_buildTime += benchmark.elapsedSeconds();
}

inline FastPath Graph::fastWalk(const string& previous,
                                const Location& location) {
  m_walkTime = 0.0;
  OVBenchmark benchmark;
  benchmark.start();

  vector<NodeSet::const_iterator> currentNodes =
      FindNodesFollowing(m_nodes, location);

  // end of a path
  if (!currentNodes.size()) {
    FastPath result;
    result.push_back(PathNode("", 0.0, m_nodes.end()));
    m_walkTime += benchmark.elapsedSeconds();
    return result;
  }

  // scan if any path has user override
  bool hasOverride = false;
  NodeSet::const_iterator override = m_nodes.end();

  for (vector<NodeSet::const_iterator>::const_iterator cniter =
           currentNodes.begin();
       cniter != currentNodes.end(); ++cniter) {
    const Node& node = **cniter;
    if (node.isOverridden()) {
      hasOverride = true;
      override = *cniter;
      break;
    }
  }

  if (hasOverride) {
    currentNodes.clear();
    currentNodes.push_back(override);
  }

  vector<FastPath> nextPaths;
  for (vector<NodeSet::const_iterator>::const_iterator cniter =
           currentNodes.begin();
       cniter != currentNodes.end(); ++cniter) {
    const Node& node = **cniter;

    StringScorePair ssp = node.findHighestScorePair(previous);
    FastPath nextPath;

    if (node.location().second)
      nextPath =
          fastWalk(ssp.first,
                   Location(node.location().first + node.location().second, 0));

    nextPath.insert(
        nextPath.begin(),
        PathNode(ssp.first, ssp.second + nextPath[0].score, *cniter));
    nextPaths.push_back(nextPath);
  }

  if (nextPaths.size()) {
    vector<FastPath>::iterator npi = nextPaths.begin();
    vector<FastPath>::iterator bestPath = npi;

    double currentScore =
        ((*bestPath).size() != 0) ? (*bestPath)[0].score : 0.0;
    for (; npi != nextPaths.end(); ++npi) {
      if ((*npi).size() != 0) {
        double newScore = (*npi)[0].score;

        if (newScore > currentScore) {
          currentScore = newScore;
          bestPath = npi;
        }
      }
    }

    m_walkTime += benchmark.elapsedSeconds();
    return (*bestPath);
  }

  m_walkTime += benchmark.elapsedSeconds();
  return FastPath();
}

inline vector<Path> Graph::walk(const string& previous,
                                const Location& location, bool aggressive) {
  m_walkTime = 0.0;
  OVBenchmark benchmark;
  benchmark.start();
  vector<Path> results;
  vector<NodeSet::const_iterator> currentNodes =
      FindNodesFollowing(m_nodes, location);

  // end of a path
  if (!currentNodes.size()) {
    Path end;
    end.push_back(StringScoreNodeSetIteratorPair(StringScorePair("", 0.0),
                                                 m_nodes.end()));
    results.push_back(end);

    m_walkTime += benchmark.elapsedSeconds();
    return results;
  }

  // scan if any path has user override
  bool hasOverride = false;
  NodeSet::const_iterator override = m_nodes.end();

  for (vector<NodeSet::const_iterator>::const_iterator cniter =
           currentNodes.begin();
       cniter != currentNodes.end(); ++cniter) {
    const Node& node = **cniter;
    if (node.isOverridden()) {
      hasOverride = true;
      override = *cniter;
      break;
    }
  }

  if (hasOverride) {
    currentNodes.clear();

    currentNodes.push_back(override);
  }

  for (vector<NodeSet::const_iterator>::const_iterator cniter =
           currentNodes.begin();
       cniter != currentNodes.end(); ++cniter) {
    const Node& node = **cniter;

    StringScorePair ssp = node.findHighestScorePair(previous);
    vector<Path> nextPaths;

    if (node.location().second)
      nextPaths =
          walk(ssp.first,
               Location(node.location().first + node.location().second, 0),
               aggressive);

    for (vector<Path>::iterator npiter = nextPaths.begin();
         npiter != nextPaths.end(); ++npiter) {
      Path& path = *npiter;

      // path[0].first.second = the first node's score (StringScorePair::second)
      // of the path
      path.insert(
          path.begin(),
          StringScoreNodeSetIteratorPair(
              StringScorePair(ssp.first, ssp.second + path[0].first.second),
              *cniter));

      // if aggressive, we assume the fist path has the highest score, so we
      // don't care the rest
      if (aggressive) break;
    }

    if (aggressive && (nextPaths.begin() != nextPaths.end()))
      results.insert(results.end(), nextPaths.begin(), nextPaths.begin() + 1);
    else
      results.insert(results.end(), nextPaths.begin(), nextPaths.end());
  }

  if (aggressive && results.size() > 1)
    sort(results.begin(), results.end(), PathCompare());

  m_walkTime += benchmark.elapsedSeconds();
  return results;
}

inline StringVector Graph::queryBlocks() const { return m_source; }

inline CandidateVector Graph::candidatesAtIndex(size_t atIndex,
                                                bool cursorAtEndOfBlock) const {
  CandidateVector results;
  vector<NodeSet::const_iterator> nodes;

  if (!cursorAtEndOfBlock)
    nodes = FindNodesOverlapping(m_nodes, Location(atIndex, 0));
  else
    nodes = FindNodesPreceding(m_nodes, Location(atIndex, 0));

  sort(nodes.begin(), nodes.end(), NodeSetIteratorCompareByLength());

  // cerr << nodes.size() << " results" << endl;

  set<string> strset;
  for (vector<NodeSet::const_iterator>::iterator iter = nodes.begin();
       iter != nodes.end(); ++iter) {
    vector<string> candidates = (**iter).candidates();
    for (vector<string>::iterator citer = candidates.begin();
         citer != candidates.end(); ++citer) {
      if (strset.find(*citer) == strset.end()) {
        strset.insert(*citer);
        results.push_back(Candidate(
            pair<string, size_t>(*citer, (size_t)(citer - candidates.begin())),
            *iter));
      }
    }
  }
  return results;
}

inline bool Graph::overrideNodeCandidate(const Node& queryNode,
                                         const string& currentText,
                                         bool cacheSelection) {
  NodeSet::iterator niter = FindNodeWithNode(m_nodes, queryNode);
  if (niter == m_nodes.end())
    return false;
  else {
    // cancel override of the overlapping nodes
    vector<NodeSet::const_iterator> nodes =
        FindNodesOverlapping(m_nodes, Location(queryNode.location().first, 0));
    for (vector<NodeSet::const_iterator>::iterator iter = nodes.begin();
         iter != nodes.end(); ++iter) {
      Node& existing = const_cast<Node&>(**iter);
      existing.cancelOverride();
    }

    Node& node = const_cast<Node&>(*niter);
    // if (!node.isTextFirstInUnigramCurrents(currentText))
    node.overrideWithText(currentText);

    // put the stuff in cache
    // cerr << "caching qstring: " << node.queryString() << ", with candidate
    // text: " << currentText << endl;
    if (!OVWildcard::Match(node.queryString(), "_punctuation_*") &&
        !OVWildcard::Match(node.queryString(), "_passthru_*") &&
        !OVWildcard::Match(node.queryString(), "_ctrl_*") && cacheSelection) {
      // cerr << "can be cached" << endl;

      if (!node.isTextFirstInUnigramCurrents(currentText))
        m_LM->cacheOverrideSelection(node.queryString(), currentText);
      else
        m_LM->removeCachedSelection(node.queryString());
    }
  }
  return true;
}

inline bool Graph::insertQueryBlockAndBuild(const string& newBlock,
                                            size_t atIndex,
                                            StringFilter* filter) {
  // cerr << "insertQueryBlockAndBuild " << (filter ? "has filter" : "has no
  // filter") << endl;

  if (atIndex > m_source.size()) return false;

  if (!m_LM->isInDictionary(newBlock, true, filter)) return false;

  OVBenchmark benchmark;
  benchmark.start();

  // when we insert a new query block, we clear all longer-than-one nodes within
  // the range of (atIndex - 3, atIndex + 3), then we relocate every node whose
  // location.first >= atIndex by +1

  NodeSet newNodes;
  // size_t removeLength = m_queryGenerationSpan <= 1 ? 1 :
  // m_queryGenerationSpan - 1;
  size_t removeLength = 0;
  size_t from = atIndex < removeLength ? 0 : atIndex - removeLength;

  removeLength++;
  size_t to = atIndex + removeLength >= m_source.size()
                  ? m_source.size()
                  : atIndex + removeLength;

  // cerr << "Removed nodes within the range (" << from << ", " << (to - from)
  // << ")" << endl;

  for (NodeSet::iterator iter = m_nodes.begin(); iter != m_nodes.end();
       ++iter) {
    Location location = (*iter).location();
    size_t x1 = location.first;
    size_t x2 = location.first + location.second;

    // cerr << "examing (" << location.first << ", " << location.second << ")"
    // << endl;
    if (((x1 >= from && x1 <= to) || (x2 >= from && x2 <= to) ||
         (from >= x1 && to <= x2)) &&
        !(*iter).isOverridden()) {
      // cerr << "removed node at (" << location.first << ", " <<
      // location.second << ")" << endl;
      continue;
    }

    Node newNode(*iter);
    if (location.first >= atIndex) {
      // cerr << "relocating old node at (" << location.first << ", " <<
      // location.second << ")";
      ++location.first;
      // cerr << " to (" << location.first << ", " << location.second << ")" <<
      // endl;
      newNode.relocate(location);
    }
    newNodes.insert(newNode);
  }

  m_nodes = newNodes;
  m_source.insert(m_source.begin() + atIndex, newBlock);
  build(filter);
  m_buildTime = benchmark.elapsedSeconds();
  return true;
}

inline pair<bool, string> Graph::shiftNodeAndMaintainPathWalk(
    const Node& targetNode, const Node& refNextNode) {
  Location location = targetNode.location();
  NodeSet::iterator ni = FindNodeWithNode(m_nodes, targetNode);
  if (ni == m_nodes.end()) return pair<bool, string>(false, "");

  // override text: we assume the preceding text is always ""
  string otext = (*ni).findHighestScorePair("").first;

  vector<NodeSet::const_iterator> pnodes =
      FindNodesFollowing(m_nodes, location);

  //		cerr << "ref node: " << refNextNode << endl;

  for (vector<NodeSet::const_iterator>::const_iterator pni = pnodes.begin();
       pni != pnodes.end(); ++pni) {
    Node& node = const_cast<Node&>(**pni);

    //			cerr << "current node: " << node << endl;

    if (node == refNextNode) {
      // overriding using the next node information

      //				cerr << "filling following node at (" <<
      // node.location().first << ", " << node.location().second<< ") with
      // override text" << endl; 				cerr << "overriding
      // the following node with previous text: " << otext << ", so current is
      // now: " << node.findHighestScorePair(otext).first << endl;

      // node.adjustScoreWithSelection(node.findHighestScorePair(otext).first);

      // determine if the node has a bigram result
      StringScorePair ssp = node.findHighestScorePair(otext);

      node.setOverridden();
      StringScorePair afterOverrideSSP = node.findHighestScorePair(otext);
      if (ssp.first != afterOverrideSSP.first) {
        node.overrideWithText(ssp.first);
      }

      break;
    }
  }

  // remove all overlapping nodes at location
  NodeSet newNodes;
  for (NodeSet::iterator iter = m_nodes.begin(); iter != m_nodes.end();
       ++iter) {
    Location iloc = (*iter).location();

    if (iloc.first >= location.first &&
        iloc.first < (location.first + location.second)) {
      // cerr << "removed node (" << iloc.first << ", " << iloc.second << ")" <<
      // endl;
      continue;
    }

    Node newNode(*iter);

    if (iloc.first >= location.first + location.second) {
      // cerr << "relocating old node at (" << iloc.first << ", " << iloc.second
      // << ")";
      iloc.first -= location.second;
      newNode.relocate(iloc);
      // cerr << " to (" << iloc.first << ", " << iloc.second << ")" << endl;
    }

    newNodes.insert(newNode);
  }

  m_nodes = newNodes;
  m_source.erase(m_source.begin() + location.first,
                 m_source.begin() + location.first + location.second);
  return pair<bool, string>(true, otext);
}

// this is used to "pop" the head node, be sure you pop the one in the path with
// highest score
inline pair<bool, string> Graph::shiftNodeAndMaintainPathWalk(
    const Node& targetNode) {
  Location location = targetNode.location();
  NodeSet::iterator ni = FindNodeWithNode(m_nodes, targetNode);
  if (ni == m_nodes.end()) return pair<bool, string>(false, "");

  // override text: we assume the preceding text is always ""
  string otext = (*ni).findHighestScorePair("").first;

  // cerr << "poping up node at (" << location.first << ", " << location.second
  // << "), override text: " << otext << endl;

  // set the override text -- this is all wrong
  // vector<NodeSet::const_iterator> pnodes = FindNodesPreceding(m_nodes,
  // location); for (vector<NodeSet::const_iterator>::const_iterator pni =
  // pnodes.begin() ; pni != pnodes.end() ; ++pni) {
  //     Node& node = const_cast<Node&>(**pni);
  //     node.overrideWithText(otext);
  //
  //     cerr << "filling preceding node at (" << node.location().first << ", "
  //     << node.location().second<< ") with override text" << endl;
  // }

  vector<NodeSet::const_iterator> pnodes =
      FindNodesFollowing(m_nodes, location);

  // vector<NodeSet::const_iterator>::const_iterator highestScoreNode =
  // pnodes.end();

  for (vector<NodeSet::const_iterator>::const_iterator pni = pnodes.begin();
       pni != pnodes.end(); ++pni) {
    Node& node = const_cast<Node&>(**pni);

    // cerr << "filling following node at (" << node.location().first << ", " <<
    // node.location().second<< ") with override text" << endl; cerr <<
    // "overriding the following node with previous text: " << otext << ", so
    // current is now: " << node.findHighestScorePair(otext).first << endl;
    // node.overrideWithText(node.findHighestScorePair(otext).first);
    node.adjustScoreWithSelection(node.findHighestScorePair(otext).first);

    // StringScorePair sspsc = node.findHighestScorePair(otext);
    //             cerr << "score: " << sspsc.second << endl;

    // if (highestScoreNode == pnodes.end())
    //  highestScoreNode = pni;
    // else {
    //  StringScorePair ssp1 = node.findHighestScorePair(otext);
    //  StringScorePair ssp2 = (**highestScoreNode).findHighestScorePair(otext);
    //
    //  if (ssp1.second > ssp2.second)
    //      highestScoreNode = pni;
    // }
  }

  // if (highestScoreNode != pnodes.end()) {
  //             cerr << "override result:" << endl;
  //  Node& node = const_cast<Node&>(**highestScoreNode);
  //  node.overrideWithText(node.findHighestScorePair(otext).first);
  //
  //             cerr << "filling following node at (" << node.location().first
  //             << ", " << node.location().second<< ") with override text" <<
  //             endl; cerr << "overriding the following node with previous
  //             text: " << otext << ", so current is now: " <<
  //             node.findHighestScorePair(otext).first << endl;
  // }

  // remove all overlapping nodes at location
  NodeSet newNodes;
  for (NodeSet::iterator iter = m_nodes.begin(); iter != m_nodes.end();
       ++iter) {
    Location iloc = (*iter).location();

    if (iloc.first >= location.first &&
        iloc.first < (location.first + location.second)) {
      // cerr << "removed node (" << iloc.first << ", " << iloc.second << ")" <<
      // endl;
      continue;
    }

    Node newNode(*iter);

    if (iloc.first >= location.first + location.second) {
      // cerr << "relocating old node at (" << iloc.first << ", " << iloc.second
      // << ")";
      iloc.first -= location.second;
      newNode.relocate(iloc);
      // cerr << " to (" << iloc.first << ", " << iloc.second << ")" << endl;
    }

    newNodes.insert(newNode);
  }

  m_nodes = newNodes;
  m_source.erase(m_source.begin() + location.first,
                 m_source.begin() + location.first + location.second);
  return pair<bool, string>(true, otext);
}

inline bool Graph::removeQueryBlockAndBuild(size_t atIndex,
                                            StringFilter* filter) {
  if (atIndex >= m_source.size()) return false;

  OVBenchmark benchmark;
  benchmark.start();

  // when we delete new query block, we clear all longer-than-one nodes within
  // the range of (atIndex - 3, atIndex + 3), then we relocate every node whose
  // location.first >= atIndex by -1

  NodeSet newNodes;
  // size_t removeLength = m_queryGenerationSpan <= 1 ? 1 :
  // m_queryGenerationSpan - 1;
  size_t removeLength = 1;
  size_t from = atIndex < removeLength ? 0 : atIndex - removeLength;
  size_t to = atIndex + removeLength >= m_source.size()
                  ? m_source.size()
                  : atIndex + removeLength;

  // cerr << "Removed nodes within the range (" << from << ", " << to << ")" <<
  // endl;

  for (NodeSet::iterator iter = m_nodes.begin(); iter != m_nodes.end();
       ++iter) {
    Location location = (*iter).location();
    size_t x1 = location.first;
    size_t x2 = location.first + location.second;

    // cerr << "examing (" << location.first << ", " << location.second << ")"
    // << endl;
    if (((x1 >= from && x1 <= to) || (x2 >= from && x2 <= to)) &&
        !(*iter).isBypassableOverridden()) {
      // cerr << "removed node at (" << location.first << ", " <<
      // location.second << ")" << endl;
      continue;
    }

    if (location.first == atIndex) continue;

    Node newNode(*iter);
    if (location.first > atIndex) {
      // cerr << "relocating old node at (" << location.first << ", " <<
      // location.second << ")";

      if (location.first) --location.first;
      // cerr << " to (" << location.first << ", " << location.second << ")" <<
      // endl;
      newNode.relocate(location);
    }
    newNodes.insert(newNode);
  }

  m_nodes = newNodes;
  m_source.erase(m_source.begin() + atIndex);
  build(filter);
  m_buildTime = benchmark.elapsedSeconds();
  return true;
}
};  // namespace Manjusri

#endif
