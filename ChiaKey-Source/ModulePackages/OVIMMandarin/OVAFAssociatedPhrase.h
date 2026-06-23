//
// OVAFAssociatedPhrase.h
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

#ifndef OVAFAssociatedPhrase_h
#define OVAFAssociatedPhrase_h

#ifndef OV_USE_SQLITE
#define OV_USE_SQLITE
#endif

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#ifndef OVAFASSOCIATEDPHRASE_IDENTIFIER
#define OVAFASSOCIATEDPHRASE_IDENTIFIER "OVAFAssociatedPhrase"
#endif

namespace OpenVanilla {
using namespace std;

class OVAFAssociatedPhrase;

class OVAFAssociatedPhraseContext : public OVEventHandlingContext {
 public:
  OVAFAssociatedPhraseContext(OVAFAssociatedPhrase* module);
  ~OVAFAssociatedPhraseContext();

  virtual void startSession(OVLoaderService* loaderService);
  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService);
  virtual bool handleDirectText(const string&, OVTextBuffer* readingText,
                                OVTextBuffer* composingText,
                                OVCandidateService* candidateService,
                                OVLoaderService* loaderService);

 protected:
  OVAFAssociatedPhrase* m_module;
  OVSQLiteStatement* m_selectStatement;
  vector<string> m_candidates;
  bool m_keyHandled;
};

class OVAFAssociatedPhrase : public OVAroundFilter {
 public:
  OVAFAssociatedPhrase();
  ~OVAFAssociatedPhrase();

  virtual OVEventHandlingContext* createContext();
  virtual const string identifier() const;
  virtual const string localizedName(const string& locale);
  virtual bool initialize(OVPathInfo* pathInfo, OVLoaderService* loaderService);
  virtual void finalize();
  friend class OVAFAssociatedPhraseContext;

  virtual int suggestedOrder() const { return -1; }

 protected:
  OVSQLiteConnection* m_phraseDB;
};

};  // namespace OpenVanilla

#endif