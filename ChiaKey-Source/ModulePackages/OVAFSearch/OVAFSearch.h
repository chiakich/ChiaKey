//
// Developed for Yahoo! Taiwan by Lithoglyph Inc.
// Copyright (c) 2007-2010 Yahoo! Taiwan.
//

#ifndef OVAFSearch_h
#define OVAFSearch_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#ifndef OVAFSEARCH_IDENTIFIER
#define OVAFSEARCH_IDENTIFIER "OVAFSearch"
#endif

namespace OpenVanilla {
using namespace std;

class OVAFSearch;

class OVAFSearchContext : public OVEventHandlingContext {
 public:
  OVAFSearchContext(OVAFSearch* module)
      : m_logging(false), m_searchType(0), m_module(module) {}

  virtual void stopSession(OVLoaderService* loaderService);
  virtual void startSession(OVLoaderService* loaderService);
  virtual string searchWeb(const string& text, OVLoaderService* loaderService);
  virtual string searchStock(const string& text,
                             OVLoaderService* loaderService);
  virtual string searchWretch(const string& text,
                              OVLoaderService* loaderService);
  virtual string searchAuction(const string& text,
                               OVLoaderService* loaderService);
  virtual string searchMap(const string& text, OVLoaderService* loaderService);

  virtual void showStopMessage(OVLoaderService* loaderService);
  virtual void setPrompt(OVTextBuffer* composingText,
                         OVLoaderService* loaderService);
  virtual void setLog(OVTextBuffer* composingText,
                      OVLoaderService* loaderService);
  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService);
  virtual bool handleDirectText(const string& text, OVTextBuffer* readingText,
                                OVTextBuffer* composingText,
                                OVCandidateService* candidateService,
                                OVLoaderService* loaderService);
  virtual bool candidateNonPanelKeyReceived(
      OVCandidateService* candidateService, const OVKey* key,
      OVTextBuffer* readingText, OVTextBuffer* composingText,
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

 protected:
  OVAFSearch* m_module;

  bool m_logging;
  size_t m_searchType;
  string m_searchString;
  vector<string> m_localizedSearchActionNames;
  vector<string> m_seachedText;
  size_t m_searchedCursor;

  static const string EscapeURLString(const string& text);
};

class OVAFSearch : public OVAroundFilter {
 public:
  OVAFSearch() : m_cfgShortcutKey('`') {}

  virtual const string localizedName(const string& locale) {
#ifndef _MSC_VER
    string tcname = "Internet 搜尋整合";
    string scname = "Internet 搜寻整合";
#else
    string tcname = "Internet \xE6\x90\x9C\xE5\xB0\x8B\xE6\x95\xB4\xE5\x90\x88";
    string scname = "Internet \xE6\x90\x9C\xE5\xAF\xBB\xE6\x95\xB4\xE5\x90\x88";
#endif
    if (locale == "zh_TW" || locale == "zh-Hant")
      return tcname;
    else if (locale == "zh_CN" || locale == "zh-Hans")
      return scname;
    else
      return string("Internet Search Integration");
  }

  virtual OVEventHandlingContext* createContext() {
    return new OVAFSearchContext(this);
  }

  virtual bool initialize(OVPathInfo* pathInfo,
                          OVLoaderService* loaderService) {
    return true;
  }

  virtual const string identifier() const;

  virtual int suggestedOrder() const { return 100; }

  virtual void loadConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService);
  virtual void saveConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService);

 protected:
  friend class OVAFSearchContext;
  char m_cfgShortcutKey;
};

};  // namespace OpenVanilla

#endif
