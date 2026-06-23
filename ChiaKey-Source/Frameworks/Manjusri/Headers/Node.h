/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef Node_h
#define Node_h

#include "LanguageModel.h"
#include "StringVectorHelper.h"

namespace Manjusri {
using namespace std;

class Node {
 protected:
  static Score c_defaultUNKProbability;
  static Score c_defaultUNKBackoff;
  static Score c_defaultOverrideScore;

 public:
  static void SetUNK(Score probability, Score backoff);
  static void SetDefaultOverrideScore(Score override);

 public:
  Node(const Location& location = Location(0, 0),
       const string& queryStirng = "");
  const Location location() const;
  const string queryString() const;

  bool isEmpty() const;
  void clear();
  bool isOverridden() const;
  bool isBypassableOverridden() const;
  void relocate(const Location& newLocation);

  void setOverridden();
  void overrideWithText(const string& text);
  void overrideWithSelection(size_t index);
  // bool isSelectedCandidateScoreHigherThanFirst(size_t index);
  bool isTextFirstInUnigramCurrents(const string& text);

  void adjustScoreWithSelection(const string& currentText);
  void cancelOverride();
  const vector<string> candidates() const;
  const StringScorePair findHighestScorePair(const string& previous = "") const;

  void addSortedBigrams(const BigramVector& bigrams);
  void addSortedUnigrams(const UnigramVector& unigrams);
  void addUnigramBackoffs(const UnigramVector& unigrams);

  bool operator==(const Node& another) const;
  bool operator<(const Node& another) const;

  bool isPreceding(const Location& location) const;
  bool isFollowing(const Location& location) const;
  bool isOverlapping(const Location& location) const;

  friend ostream& operator<<(ostream& stream, const Node& node);

 protected:
  Location m_location;
  string m_queryString;

  bool m_overridden;
  size_t m_overriddenSelection;

  bool m_hasOverriddenText;
  string m_overriddenText;

  map<string, StringScorePairVector> m_bigramMap;
  StringScorePairVector m_unigramCurrents;
  StringScoreMap m_unigramPreviousBackoffs;
};

typedef set<Node> NodeSet;

ostream& operator<<(ostream& stream, const StringScorePairVector& vec);
NodeSet::iterator FindNodeAtLocation(NodeSet& nodeset,
                                     const Location& location);
NodeSet::iterator FindNodeWithNode(NodeSet& nodeset, const Node& queryNode);
const vector<NodeSet::const_iterator> FindNodesPreceding(
    const NodeSet& nodeset, const Location& location);
const vector<NodeSet::const_iterator> FindNodesFollowing(
    const NodeSet& nodeset, const Location& location);
const vector<NodeSet::const_iterator> FindNodesOverlapping(
    const NodeSet& nodeset, const Location& location);

inline NodeSet::iterator FindNodeAtLocation(NodeSet& nodeset,
                                            const Location& location) {
  return nodeset.find(Node(location));
}

inline NodeSet::iterator FindNodeWithNode(NodeSet& nodeset,
                                          const Node& queryNode) {
  return nodeset.find(queryNode);
}

inline const vector<NodeSet::const_iterator> FindNodesPreceding(
    const NodeSet& nodeset, const Location& location) {
  vector<NodeSet::const_iterator> results;
  for (NodeSet::const_iterator iter = nodeset.begin(); iter != nodeset.end();
       ++iter)
    if ((*iter).isPreceding(location)) results.push_back(iter);

  return results;
}

inline const vector<NodeSet::const_iterator> FindNodesFollowing(
    const NodeSet& nodeset, const Location& location) {
  vector<NodeSet::const_iterator> results;
  for (NodeSet::const_iterator iter = nodeset.begin(); iter != nodeset.end();
       ++iter)
    if ((*iter).isFollowing(location)) results.push_back(iter);

  return results;
}

inline const vector<NodeSet::const_iterator> FindNodesOverlapping(
    const NodeSet& nodeset, const Location& location) {
  vector<NodeSet::const_iterator> results;
  for (NodeSet::const_iterator iter = nodeset.begin(); iter != nodeset.end();
       ++iter)
    if ((*iter).isOverlapping(location)) results.push_back(iter);

  return results;
}

inline Node::Node(const Location& location, const string& queryStirng)
    : m_location(location),
      m_queryString(queryStirng),
      m_overridden(false),
      m_overriddenSelection(0),
      m_hasOverriddenText(false) {}

inline const Location Node::location() const { return m_location; }

inline const string Node::queryString() const { return m_queryString; }

inline bool Node::isEmpty() const {
  return m_bigramMap.size() == 0 && m_unigramCurrents.size() == 0 &&
         m_unigramPreviousBackoffs.size() == 0;
}

inline void Node::clear() {
  m_location = Location(0, 0);
  m_queryString = "";
  m_overridden = false;
  m_overriddenSelection = 0;
  m_overriddenText = "";
  m_hasOverriddenText = false;

  m_bigramMap.clear();
  m_unigramCurrents.clear();
  m_unigramPreviousBackoffs.clear();
}

inline void Node::relocate(const Location& newLocation) {
  m_location = newLocation;
}

inline bool Node::isOverridden() const { return m_overridden; }

inline bool Node::isBypassableOverridden() const {
  // strlen("_punctuation_") == 13

  if (m_overridden) {
    if (m_queryString.length() > 13 &&
        m_queryString.substr(0, 13) == "_punctuation_") {
      return true;
    }
  }

  return false;
}

inline void Node::setOverridden() {
  if (!m_overridden) {
    m_overridden = true;
    m_overriddenSelection = 0;
    m_hasOverriddenText = false;
  }
}

inline void Node::overrideWithText(const string& text) {
  m_overridden = true;
  m_overriddenText = text;
  m_hasOverriddenText = true;
  m_overriddenSelection = 0;
}

inline void Node::overrideWithSelection(size_t index) {
  if (index < m_unigramCurrents.size()) {
    m_overridden = true;
    m_overriddenSelection = index;
    m_hasOverriddenText = false;
  } else {
    m_overridden = false;
  }
}

inline bool Node::isTextFirstInUnigramCurrents(const string& text) {
  if (!m_unigramCurrents.size()) return false;

  return m_unigramCurrents[0].first == text;
}

// inline bool Node::isSelectedCandidateScoreHigherThanFirst(size_t index)
// {
//     if (index >= m_unigramCurrents.size())
//         return false;
//
//     StringScorePair ssp1 = m_unigramCurrents[0];
//     StringScorePair ssp2 = m_unigramCurrents[index];
//
//     return (ssp2.second >= ssp1.second);
// }

inline void Node::adjustScoreWithSelection(const string& currentText) {
  // cerr << "adjusting score, currentText = " << currentText << endl;

  StringScorePairVector::iterator sspi;
  for (sspi = m_unigramCurrents.begin(); sspi != m_unigramCurrents.end();
       ++sspi) {
    if ((*sspi).first == currentText) break;
  }

  if (sspi == m_unigramCurrents.end()) return;

  StringScorePair ssp = *sspi;  // m_unigramCurrents[index];
  // cerr << "string = " << ssp.first << ", score =" << ssp.second << endl;

  m_unigramCurrents.erase(sspi);
  // if (m_unigramCurrents.size())
  // ssp.second = m_unigramCurrents[0].second;
  // ssp.second = 0.0;

  m_unigramCurrents.insert(m_unigramCurrents.begin(), ssp);

  // heuristics: add a bigram for empty text (BOS)

  StringScorePairVector& sspv = m_bigramMap[""];

  // cerr << "dumping the adjusted node (before bigram adjustment): " << *this
  // << endl;

  if (sspv.size()) {
    StringScorePair oldPair = sspv[0];
    if (oldPair.first != ssp.first)
      sspv.insert(sspv.begin(), StringScorePair(ssp.first, oldPair.second));
  } else {
    // m_bigramMap[""].push_back(StringScorePair(ssp.first,
    // c_defaultOverrideScore));
    m_bigramMap.erase("");
  }

  // m_bigramMap.clear();

  // cerr << "dumping the adjusted node: " << *this << endl;
}

inline void Node::cancelOverride() {
  m_overridden = false;
  m_overriddenSelection = 0;
  m_hasOverriddenText = false;
  m_overriddenText = "";
}

inline const vector<string> Node::candidates() const {
  vector<string> results;
  for (StringScorePairVector::const_iterator iter = m_unigramCurrents.begin();
       iter != m_unigramCurrents.end(); ++iter)
    results.push_back((*iter).first);

  return results;
}

inline const StringScorePair Node::findHighestScorePair(
    const string& previous) const {
  if (m_overridden) {
    if (m_hasOverriddenText)
      return StringScorePair(m_overriddenText, c_defaultOverrideScore);

    return StringScorePair(m_unigramCurrents[m_overriddenSelection].first,
                           c_defaultOverrideScore);
  }

  map<string, StringScorePairVector>::const_iterator miter =
      m_bigramMap.find(previous);
  StringScorePair bigramResult;
  bool hasBigramResult = false;

  if (miter != m_bigramMap.end()) {
    bigramResult = (*miter).second.at(0);
    hasBigramResult = true;
  }

  Score backoffWeight = Node::c_defaultUNKBackoff;
  StringScoreMap::const_iterator siter =
      m_unigramPreviousBackoffs.find(previous);
  if (siter != m_unigramPreviousBackoffs.end()) backoffWeight = (*siter).second;

  StringScorePair result;

  if (m_unigramCurrents.size()) {
    result = m_unigramCurrents[0];
    result.second += backoffWeight;
  } else {
    result.first = "";
    result.second = backoffWeight + Node::c_defaultUNKProbability;
  }

  if (hasBigramResult)
    if (bigramResult.second > result.second) return bigramResult;

  return result;
}

inline void Node::addSortedBigrams(const BigramVector& bigrams) {
  for (BigramVector::const_iterator iter = bigrams.begin();
       iter != bigrams.end(); ++iter)
    m_bigramMap[(*iter).previous].push_back(
        StringScorePair((*iter).current, (*iter).probability));
}

inline void Node::addSortedUnigrams(const UnigramVector& unigrams) {
  for (UnigramVector::const_iterator iter = unigrams.begin();
       iter != unigrams.end(); ++iter)
    m_unigramCurrents.push_back(
        StringScorePair((*iter).current, (*iter).probability));
}

inline void Node::addUnigramBackoffs(const UnigramVector& unigrams) {
  for (UnigramVector::const_iterator iter = unigrams.begin();
       iter != unigrams.end(); ++iter)
    m_unigramPreviousBackoffs[(*iter).current] = (*iter).backoff;
}

inline bool Node::operator==(const Node& another) const {
  if (m_location == another.m_location)
    return m_queryString == another.m_queryString;
  else
    return false;
}

inline bool Node::operator<(const Node& another) const {
  if (m_location == another.m_location)
    return m_queryString < another.m_queryString;
  else
    return m_location < another.m_location;
}

inline bool Node::isPreceding(const Location& location) const {
  return m_location.first + m_location.second == location.first;
}

inline bool Node::isFollowing(const Location& location) const {
  return m_location.first == location.first + location.second;
}

inline bool Node::isOverlapping(const Location& location) const {
  size_t p1 = m_location.first, p2 = p1 + m_location.second;
  size_t q1 = location.first, q2 = q1 + location.second;

  return q1 >= p1 && q1 < p2 && q2 >= p1 && q2 <= p2;
}

inline void Node::SetUNK(Score probability, Score backoff) {
  c_defaultUNKProbability = probability;
  c_defaultUNKBackoff = backoff;
}

inline ostream& operator<<(ostream& stream, const Node& node) {
  stream << "Node (" << node.m_location.first << ", " << node.m_location.second
         << "), queryString: '" << node.m_queryString << "'";
  stream << ", overridden: " << (node.m_overridden ? "true" : "false");
  if (node.m_overridden) {
    if (node.m_hasOverriddenText)
      stream << ", overridden text: " << node.m_overriddenText;
    else
      stream << ", overridden index: " << node.m_overriddenSelection;
  }
  stream << endl;

  for (map<string, StringScorePairVector>::const_iterator miter =
           node.m_bigramMap.begin();
       miter != node.m_bigramMap.end(); ++miter)
    stream << "bigram " << (*miter).first << " -> " << (*miter).second << endl;

  stream << "unigrams: " << node.m_unigramCurrents << endl;

  stream << "backoffs:";
  for (StringScoreMap::const_iterator siter =
           node.m_unigramPreviousBackoffs.begin();
       siter != node.m_unigramPreviousBackoffs.end();) {
    stream << "BOW(" << (*siter).first << ") = " << (*siter).second;
    if (++siter != node.m_unigramPreviousBackoffs.end()) stream << ", ";
  }

  return stream;
}

inline ostream& operator<<(ostream& stream, const StringScorePairVector& vec) {
  for (StringScorePairVector::const_iterator iter = vec.begin();
       iter != vec.end(); ++iter) {
    stream << (*iter).first << " (" << (*iter).second << ")";
    if (iter + 1 != vec.end()) stream << ", ";
  }

  return stream;
}

};  // namespace Manjusri

#endif
