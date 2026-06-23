//
// OVAFHomophone.h
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

#ifndef OVAFHomophone_h
#define OVAFHomophone_h

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

class OVAFHomophone;

class OVAFHomophoneContext : public OVEventHandlingContext {
 public:
  OVAFHomophoneContext(OVAFHomophone* module);
  ~OVAFHomophoneContext();

  virtual void startSession(OVLoaderService* loaderService);
  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService);
  virtual bool handleDirectText(const string&, OVTextBuffer* readingText,
                                OVTextBuffer* composingText,
                                OVCandidateService* candidateService,
                                OVLoaderService* loaderService);
  virtual void showCandidatePanel(const string& text,
                                  OVCandidateService* candidateService,
                                  OVLoaderService* loaderService);
  virtual void showPhonePanel(OVCandidateService* candidateService,
                              OVLoaderService* loaderService);
  virtual bool candidateSelected(OVCandidateService* candidateService,
                                 const string& text, size_t index,
                                 OVTextBuffer* readingText,
                                 OVTextBuffer* composingText,
                                 OVLoaderService* loaderService);

 protected:
  OVAFHomophone* m_module;
  OVSQLiteStatement* m_selectPhone;
  OVSQLiteStatement* m_selectCandidates;
  vector<string> m_phones;
  string m_selectedPhone;
  vector<string> m_candidates;
  bool m_logging;
  bool m_isSelectingPhone;
  bool m_isSelectingCandidate;
};

class OVAFHomophone : public OVAroundFilter {
 public:
  OVAFHomophone();
  ~OVAFHomophone();

  virtual OVEventHandlingContext* createContext();
  virtual const string identifier() const;
  virtual const string localizedName(const string& locale);
  virtual bool initialize(OVPathInfo* pathInfo, OVLoaderService* loaderService);
  virtual void loadConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService);
  virtual void saveConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService);
  virtual void finalize();
  friend class OVAFHomophoneContext;

 protected:
  OVSQLiteConnection* m_bpmfDB;
  string m_cfgUseCharactersSupportedByEncoding;
};

};  // namespace OpenVanilla

#endif
