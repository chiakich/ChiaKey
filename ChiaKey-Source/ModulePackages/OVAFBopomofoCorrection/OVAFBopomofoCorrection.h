//
// OVAFBopomofoCorrection.h
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

#ifndef OVAFBopomofoCorrection_h
#define OVAFBopomofoCorrection_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#ifndef OVAFBOPOMOFOCORRECTION_IDENTIFIER
#define OVAFBOPOMOFOCORRECTION_IDENTIFIER "OVAFBopomofoCorrection"
#endif

namespace OpenVanilla {
using namespace std;

class OVAFBopomofoCorrectionContext : public OVEventHandlingContext {
 public:
  OVAFBopomofoCorrectionContext(OVKeyValueDataTableInterface* dataTable);
  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService);
  virtual bool handleDirectText(const string& text, OVTextBuffer* readingText,
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
  //        virtual bool candidateNonPanelKeyReceived(OVCandidateService*
  //        candidateService, const OVKey* key, OVTextBuffer* readingText,
  //        OVTextBuffer* composingText, OVLoaderService* loaderService);

 protected:
  vector<string> m_characters;
  vector<string>::iterator m_currentChar;
  OVKeyValueDataTableInterface* m_dataTable;
};

class OVAFBopomofoCorrection : public OVAroundFilter {
 public:
  virtual const string localizedName(const string& locale);
  virtual const string identifier() const;
  virtual OVEventHandlingContext* createContext();
  virtual bool initialize(OVPathInfo* pathInfo, OVLoaderService* loaderService);
  virtual void finalize();

  virtual int suggestedOrder() const { return -5; }

 protected:
  OVKeyValueDataTableInterface* m_dataTable;
};
};  // namespace OpenVanilla

#endif