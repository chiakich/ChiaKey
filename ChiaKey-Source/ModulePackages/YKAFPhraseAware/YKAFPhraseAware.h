/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef YKAFPhraseAwareSearch_h
#define YKAFPhraseAwareSearch_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace OpenVanilla {
using namespace std;

class YKAFPhraseAware;

class YKAFPhraseAwareContext : public OVEventHandlingContext {
 public:
  YKAFPhraseAwareContext(YKAFPhraseAware* module) : m_module(module) {}

  virtual bool handleDirectText(const vector<string>& segments,
                                OVTextBuffer* readingText,
                                OVTextBuffer* composingText,
                                OVCandidateService* candidateService,
                                OVLoaderService* loaderService);
  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService);

 protected:
  YKAFPhraseAware* m_module;
};

class YKAFPhraseAware : public OVAroundFilter {
 public:
  virtual const string localizedName(const string& locale) {
    return "Phrase Awareness";
  }

  virtual OVEventHandlingContext* createContext() {
    return new YKAFPhraseAwareContext(this);
  }

  virtual bool initialize(OVPathInfo* pathInfo,
                          OVLoaderService* loaderService) {
    return true;
  }

  virtual const string identifier() const { return "YKAFPhraseAware"; }

  virtual int suggestedOrder() const { return -20000; }

  virtual void collectRecent(bool flush = true) {
    size_t from = m_recent.size() >= 4 ? 4 : m_recent.size();

    for (size_t p = from; p >= 2; p--) {
      vector<string> s;
      s.insert(s.begin(), m_recent.begin() + (4 - p), m_recent.end());
      string ph = OVStringHelper::Join(s);

      size_t count = m_stats[ph];
      m_stats[ph] = count + 1;
    }

    if (flush) {
      m_recent.clear();
    }
  }

  virtual void collect(const vector<string>& segments) {
    cerr << "collecting: " << OVStringHelper::Join(segments, ", ") << endl;

    for (vector<string>::const_iterator si = segments.begin();
         si != segments.end(); ++si) {
      vector<string> cps = OVUTF8Helper::SplitStringByCodePoint(*si);
      if (cps.size() > 1) {
        size_t count = m_stats[*si];
        m_stats[*si] = count + 1;

        // flush
        m_recent.clear();
      } else {
        // we only collect Unihan
        unsigned int cp = OVUTF8Helper::CodePointFromSingleUTF8String(cps[0]);
        if (cp < 0x4e00 || cp > 0x9fff) {
          m_recent.clear();
          continue;
        }

        m_recent.push_back(*si);
        if (m_recent.size() > 4) {
          m_recent.erase(m_recent.begin(),
                         m_recent.begin() + (m_recent.size() - 4));
          collectRecent(false);
          cerr << "now recent: " << OVStringHelper::Join(m_recent, ", ")
               << endl;

          collectRecent(false);
        }
      }
    }

    decimate();
  }

  virtual void decimate() {
    if (m_stats.size() < 50) {
      return;
    }

    vector<pair<string, size_t> > collected;
    for (map<string, size_t>::const_iterator si = m_stats.begin();
         si != m_stats.end(); ++si) {
      collected.push_back(*si);
    }
    sort(collected.begin(), collected.end(), SortPair());

    size_t c = collected.size();
    for (size_t i = c / 2; i < c; i++) {
      map<string, size_t>::iterator it = m_stats.find(collected[i].first);
      if (it != m_stats.end()) {
        m_stats.erase(it);
      }
    }
  }

  virtual void writeToTemp() {
    string tmpFile = OVPathHelper::PathCat(OVDirectoryHelper::TempDirectory(),
                                           "PhraseAwareTemp.txt");

    vector<pair<string, size_t> > collected;
    for (map<string, size_t>::const_iterator si = m_stats.begin();
         si != m_stats.end(); ++si) {
      collected.push_back(*si);
    }

    sort(collected.begin(), collected.end(), SortPair());

    FILE* f = OVFileHelper::OpenStream(tmpFile, "w");
    if (f) {
      for (vector<pair<string, size_t> >::const_iterator pi = collected.begin();
           pi != collected.end(); ++pi) {
        fprintf(f, "%08d %s\n", (int)(*pi).second, (*pi).first.c_str());
      }
      fclose(f);
    }
  }

 protected:
  class SortPair {
   public:
    int operator()(const pair<string, size_t>& a,
                   const pair<string, size_t>& b) {
      return a.second > b.second;
    }
  };

  map<string, size_t> m_stats;
  vector<string> m_recent;

  friend class YKAFPhraseAwareContext;
};

};  // namespace OpenVanilla

#endif
