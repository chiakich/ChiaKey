/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef YKAFWordCount_h
#define YKAFWordCount_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace OpenVanilla {
using namespace std;

class YKAFWordCount;

class YKAFWordCountContext : public OVEventHandlingContext {
 public:
  YKAFWordCountContext(YKAFWordCount* module)
      : m_module(module), m_sessionStartTime(0), m_sessionCharCount(0) {}

  virtual void stopSession(OVLoaderService* loaderService);
  virtual void startSession(OVLoaderService* loaderService);
  // virtual bool handleKey(OVKey* key, OVTextBuffer* readingText, OVTextBuffer*
  // composingText, OVCandidateService* candidateService, OVLoaderService*
  // loaderService);
  virtual bool handleDirectText(const string& text, OVTextBuffer* readingText,
                                OVTextBuffer* composingText,
                                OVCandidateService* candidateService,
                                OVLoaderService* loaderService);

 protected:
  YKAFWordCount* m_module;
  // size_t m_count;

  time_t m_sessionStartTime;
  size_t m_sessionCharCount;
};

class YKAFWordCount : public OVAroundFilter {
 public:
  YKAFWordCount() : m_latestSessionTimestamp(0), m_latestSessionCharCount(0) {}

  virtual const string localizedName(const string& locale) {
#ifndef _MSC_VER
    string tcname = "字數統計";
    string scname = "字数统计";
#else
    string tcname = "\xE5\xAD\x97\xE6\x95\xB8\xE7\xB5\xB1\xE8\xA8\x88";
    string scname = "\xE5\xAD\x97\xE6\x95\xB0\xE7\xBB\x9F\xE8\xAE\xA1";
#endif
    if (locale == "zh_TW" || locale == "zh-Hant")
      return tcname;
    else if (locale == "zh_CN" || locale == "zh-Hans")
      return scname;
    else
      return string("Word Count");
  }

  virtual OVEventHandlingContext* createContext() {
    return new YKAFWordCountContext(this);
  }

  virtual bool initialize(OVPathInfo* pathInfo,
                          OVLoaderService* loaderService) {
    return true;
  }

  virtual const string identifier() const;

  virtual int suggestedOrder() const { return 10000; }

  virtual void addSessionCharCount(time_t timestamp, size_t count) {
    if (timestamp > m_latestSessionTimestamp) {
      m_latestSessionTimestamp = timestamp;
    }

    m_latestSessionCharCount += count;
  }

  virtual void saveConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService);

 protected:
  friend class YKAFWordCountContext;

  time_t m_latestSessionTimestamp;
  size_t m_latestSessionCharCount;
};

};  // namespace OpenVanilla

#endif
