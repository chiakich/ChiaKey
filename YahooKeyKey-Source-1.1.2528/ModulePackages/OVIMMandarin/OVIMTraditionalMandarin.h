//
// OVIMTraditionalMandarin.h
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

#ifndef OVIMTraditionalMandarin_h
#define OVIMTraditionalMandarin_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "Mandarin.h"
#include "OVIMMandarinConfig.h"

namespace OpenVanilla {
using namespace std;
using namespace Formosa::Mandarin;

class OVIMTraditionalMandarin;

class OVIMTraditionalMandarinContext : public OVEventHandlingContext {
 public:
  OVIMTraditionalMandarinContext(OVIMTraditionalMandarin* module);

  virtual void startSession(OVLoaderService* loaderService);
  virtual void stopSession(OVLoaderService* loaderService);
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
  virtual bool queryAndCompose(OVKeyValueDataTableInterface* dataTable,
                               const string& queryString,
                               OVTextBuffer* readingText,
                               OVTextBuffer* composingText,
                               OVCandidateService* candidateService,
                               OVLoaderService* loaderService);
  virtual bool handleBackspace(OVTextBuffer* readingText,
                               OVTextBuffer* composingText,
                               OVCandidateService* candidateService,
                               OVLoaderService* loaderService);

  pair<bool, string> findPunctuationKey(const string& keyString);
  const BopomofoKeyboardLayout* currentKeyboardLayout();
  const string readingQueryString();

  BopomofoReadingBuffer m_readingBuffer;
  OVIMTraditionalMandarin* m_module;
};

class OVIMTraditionalMandarin : public OVInputMethod {
 public:
  OVIMTraditionalMandarin();
  ~OVIMTraditionalMandarin();

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
  friend class OVIMTraditionalMandarinContext;

  OVKeyValueDataTableInterface* m_punctuationTable;
  OVKeyValueDataTableInterface* m_BPMFTable;
  bool m_useStandardLayoutQueryString;

 protected:
  // configurable items
  string m_cfgKeyboardLayout;
  string m_cfgCandidateSelectionKeys;
  string m_cfgUseCharactersSupportedByEncoding;
};
};  // namespace OpenVanilla

#endif
