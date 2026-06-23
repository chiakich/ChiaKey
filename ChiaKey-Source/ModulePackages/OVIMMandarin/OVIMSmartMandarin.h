//
// OVIMSmartMandarin.h
//
// Copyright (c) 2004-2010 The OpenVanilla Project (http://openvanilla.org)
// All rights reserved.
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//

#ifndef OVIMSmartMandarin_h
#define OVIMSmartMandarin_h

#ifndef OV_USE_SQLITE
#define OV_USE_SQLITE
#endif

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "Mandarin.h"
#include "Manjusri.h"

namespace OpenVanilla {
using namespace std;
using namespace Formosa::Mandarin;
using namespace Manjusri;

class OVIMSmartMandarin;

class CandidateFilter {
 public:
  virtual bool shouldPass(const string& text) = 0;
};

class ManjusriComposer {
 public:
  // we don't own lm
  ManjusriComposer(LanguageModel* lm)
      : m_graph(lm /* , 20 */ /* pinyin */),
        m_LM(lm),
        m_cursorLeftBound(0),
        m_cursorRightBound(0) {}

  void clear() {
    m_graph.clear();
    m_latestCandidate.clear();
    update();
  }

  bool insertAt(size_t index, const string qstring, StringFilter* filter = 0) {
    // cerr << "insertAt " << (filter ? "has filter" : "has no filter") << endl;
    return m_graph.insertQueryBlockAndBuild(qstring, index, filter);
  }

  void deleteAt(size_t index, StringFilter* filter = 0) {
    m_graph.removeQueryBlockAndBuild(index, filter);
  }

  void backspaceAt(size_t index, StringFilter* filter = 0) {
    m_graph.removeQueryBlockAndBuild(index - 1, filter);
  }

  size_t cursorLeftBound() { return m_cursorLeftBound; }

  size_t cursorRightBound() { return m_cursorRightBound; }

  const string composedString() { return m_composedString; }

  const vector<string> composedStringAsTextSegments() {
    return FastPathAsTextSegments(m_latestFastPath);
  }

  const vector<pair<size_t, size_t> > wordSegments() {
    vector<pair<size_t, size_t> > results;

    if (m_latestFastPath.size() < 3) return results;

    for (FastPath::const_iterator iter = m_latestFastPath.begin() + 1;
         iter != (m_latestFastPath.end() - 2); ++iter) {
      Location loc = (*(*iter).nodePointer).location();
      results.push_back(
          pair<size_t, size_t>(loc.first - m_cursorLeftBound, loc.second));
    }

    /*
                if (m_latestPath.size() < 3)
                    return results;

                for (Path::const_iterator iter = m_latestPath.begin() + 1; iter
       != (m_latestPath.end() - 2); ++iter) { Location loc =
       (*(*iter).second).location();

                    results.push_back(pair<size_t, size_t>(loc.first -
       m_cursorLeftBound, loc.second));
                }
    */

    return results;
  }

  const string shift() {
    if (m_latestFastPath.size() < 3) return string();

    const Node& node = *(m_latestFastPath[1].nodePointer);
    const Node& node2 = *(m_latestFastPath[2].nodePointer);

    /*
                if (m_latestPath.size() < 3)
                    return string();

                const StringScoreNodeSetIteratorPair& ssnsip = m_latestPath[1];
                const Node& node = *(ssnsip.second);
                Location loc = node.location(); */

    pair<bool, string> rsp = m_graph.shiftNodeAndMaintainPathWalk(node, node2);
    return rsp.second;
  }

  void update() {
    StringVector qblocks = m_graph.queryBlocks();
    if (!qblocks.size()) {
      clear();
      return;
    }

    /*
    // use aggressive walk
    vector<Path> paths = m_graph.walk("", Location(0, 0), true);
    if (!paths.size()) {
        clear();
        return;
    }

    m_latestPath = paths[0];

    */

    m_latestFastPath = m_graph.fastWalk("", Location(0, 0));
    if (!m_latestFastPath.size()) {
      clear();
      return;
    }

    m_cursorLeftBound = 1;
    m_cursorRightBound = qblocks.size() - 1;

    m_composedString = FastPathAsString(m_latestFastPath);

    // cerr << "update result: " << m_latestFastPath << endl;
  }

  vector<string> collectCandidates(size_t cursor,
                                   bool candidateCursorAtEndOfTargetBlock) {
    vector<string> results;

    m_latestCandidate =
        m_graph.candidatesAtIndex(cursor, candidateCursorAtEndOfTargetBlock);

    for (CandidateVector::iterator iter = m_latestCandidate.begin();
         iter != m_latestCandidate.end(); ++iter)
      results.push_back((*iter).first.first);

    return results;
  }

  size_t chooseCandidate(size_t index, bool shouldUpdate = true) {
    if (index >= m_latestCandidate.size()) return 0;

    bool shouldCacheSelection = true;
    Candidate& candi = m_latestCandidate[index];
    const Node& node = *(candi.second);

    // we disallow BPMF symbols
    BPMF bpmf = BPMF::FromComposedString(candi.first.first);
    if (!bpmf.isEmpty()) shouldCacheSelection = false;

    m_graph.overrideNodeCandidate(node /*.location() */, candi.first.first,
                                  shouldCacheSelection);

    if (shouldUpdate) update();

    // remember MSC is more strict on undefined iterator pointer, so we can't go
    // to this path's end, whose node pointer points to graph's m_set's end(),
    // which is a "bad ptr" for MSC
    for (FastPath::iterator fpiter = m_latestFastPath.begin();
         fpiter + 1 != m_latestFastPath.end(); ++fpiter) {
      if ((*(*fpiter).nodePointer).isPreceding(node.location())) {
        string previous = (*(*fpiter).nodePointer).queryString();

        if (previous != m_LM->BOSQueryString()) {
          // cerr << "caching user bigram, preceeding text = " << (*fpiter).text
          // << ", our text = " << candi.first.first << ", so qstring: "
          //     << (*(*fpiter).nodePointer).queryString()  << " + "
          //     << node.queryString() << endl;

          // don't cache the bigram if it's a punctuation symbol
          if (!OVWildcard::Match(node.queryString(), "_punctuation_*") &&
              !OVWildcard::Match(node.queryString(), "_passthru_*") &&
              !OVWildcard::Match(node.queryString(), "_ctrl_*")) {
            m_LM->cacheUserBigram(
                m_LM->combineBigramQueryString(
                    (*(*fpiter).nodePointer).queryString(), node.queryString()),
                (*fpiter).text, candi.first.first);
          }
        }
      }
    }

    return node.location().first + node.location().second;
  }

  const string currentlyMarkedUnigram(size_t from, size_t to) {
    return SVH::Join(
        SVH::SubVector(OVUTF8Helper::SplitStringByCodePoint(m_composedString),
                       from - m_cursorLeftBound, to - from));
  }

  const pair<bool, string> addUserUnigram(size_t from, size_t to) {
    string qstring =
        SVH::Join(SVH::SubVector(m_graph.queryBlocks(), from, to - from));
    string current = SVH::Join(
        SVH::SubVector(OVUTF8Helper::SplitStringByCodePoint(m_composedString),
                       from - m_cursorLeftBound, to - from));

    // cerr << "adding unigram, qstring: " << qstring << ", current: " <<
    // current << endl;
    bool result = m_LM->addUserUnigram(qstring, current);
    return pair<bool, string>(result, current);
  }

  void logStats(OVLoaderService* loaderService) {
    static double accuBuildTime = 0.0;
    static double accuBuildCount = 0;

    // loaderService->logger("ManjusriComposer") << "LM SQL query count: " <<
    // m_LM->queryCount() << ", cached query: " << m_LM->cachedQueryCount() <<
    // ", hit rate: " << (double)m_LM->cachedQueryCount() /
    // (double)(m_LM->queryCount() + m_LM->cachedQueryCount()) * 100.0 << "%" <<
    // endl; loaderService->logger("ManjusriComposer") << "Last build time: " <<
    // m_graph.lastBuildTime() << " secs, walk time: " << m_graph.lastWalkTime()
    // << " secs" << ", build/walk ratio = " << m_graph.lastBuildTime() /
    // m_graph.lastWalkTime() << endl;

    accuBuildTime += m_graph.lastBuildTime();
    accuBuildCount += 1.0;
    // loaderService->logger("ManjusriComposer") << "Avg build time: " <<
    // accuBuildTime / accuBuildCount << " secs" << endl;

    m_LM->resetQueryCount();
  }

 protected:
  Graph m_graph;
  //      Path m_latestPath;
  FastPath m_latestFastPath;

  CandidateVector m_latestCandidate;
  string m_composedString;

  LanguageModel* m_LM;

  size_t m_cursorLeftBound;
  size_t m_cursorRightBound;
};

class OVIMSmartMandarinContext : public OVEventHandlingContext {
 public:
  OVIMSmartMandarinContext(OVIMSmartMandarin* module);

  virtual void startSession(OVLoaderService* loaderService);
  virtual void stopSession(OVLoaderService* loaderService);
  virtual void clear(OVLoaderService* loaderService);
  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService);

  virtual void candidateCanceled(OVCandidateService* candidateService,
                                 OVTextBuffer* readingText,
                                 OVTextBuffer* composingText,
                                 OVLoaderService* loaderService);
  virtual bool candidateSelected(OVCandidateService* candidateService,
                                 const string& text, size_t index,
                                 OVTextBuffer* readingText,
                                 OVTextBuffer* composingText,
                                 OVLoaderService* loaderService);
  virtual bool candidateNonPanelKeyReceived(
      OVCandidateService* candidateService, const OVKey* key,
      OVTextBuffer* readingText, OVTextBuffer* composingText,
      OVLoaderService* loaderService);

 protected:
  const BopomofoKeyboardLayout* currentKeyboardLayout();

  OVIMSmartMandarin* m_module;

  BopomofoReadingBuffer m_BPMFReading;
  ManjusriComposer m_manjusri;
  size_t m_cursor;

  bool m_markMode;
  size_t m_markCursor;
};

class OVIMSmartMandarin : public OVInputMethod {
 public:
  OVIMSmartMandarin();
  ~OVIMSmartMandarin();

  virtual OVEventHandlingContext* createContext();
  virtual const string identifier() const;
  virtual const string localizedName(const string& locale);
  virtual bool initialize(OVPathInfo* pathInfo, OVLoaderService* loaderService);
  virtual void finalize();
  virtual void loadConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService);
  virtual void saveConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService);

 protected:
  friend class OVIMSmartMandarinContext;

 protected:
  LanguageModel* m_LM;
  // OVSQLiteConnection* m_BPMFDB;

  size_t m_cacheFlushKeyCounter;

  // configurable items
  string m_cfgKeyboardLayout;
  string m_cfgCandidateSelectionKeys;
  string m_cfgUseCharactersSupportedByEncoding;
  bool m_cfgShowCandidateListWithSpace;
  bool m_cfgClearComposingTextWithEsc;
  bool m_cfgCandidateCursorAtEndOfTargetBlock;
  bool m_cfgShiftKeyAlwaysCommitUppercaseCharacters;

  size_t m_cfgComposingTextBufferSize;
};
};  // namespace OpenVanilla

#endif
