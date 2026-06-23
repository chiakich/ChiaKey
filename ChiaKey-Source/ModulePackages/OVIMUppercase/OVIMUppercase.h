//
// OVIMUppercase.h
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

#ifndef OVIMUpperCase_h
#define OVIMUpperCase_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace OpenVanilla {
using namespace std;

class OVIMUppercaseContext : public OVEventHandlingContext {
 public:
  OVIMUppercaseContext();

  virtual void startSession(OVLoaderService* loaderService);
  virtual void stopSession(OVLoaderService* loaderService);
  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService);

  virtual void candidateCanceled(OVCandidateService* candidateService,
                                 OVTextBuffer* readingText,
                                 OVTextBuffer* composingText,
                                 OVLoaderService* loaderService) {
    loaderService->logger("OVIMUppercase") << "candidate canceled!" << endl;
  }

  virtual bool candidateSelected(OVCandidateService* candidateService,
                                 const string& text, size_t index,
                                 OVTextBuffer* readingText,
                                 OVTextBuffer* composingText,
                                 OVLoaderService* loaderService) {
    m_strings.clear();
    composingText->setText(text);
    composingText->commit();
    candidateService->useVerticalCandidatePanel()->hide();
    loaderService->logger("OVIMUppercase") << "candidate selected!" << endl;
    return true;
  }

  virtual bool candidateNonPanelKeyReceived(
      OVCandidateService* candidateService, const OVKey* key,
      OVTextBuffer* readingText, OVTextBuffer* composingText,
      OVLoaderService* loaderService) {
    loaderService->logger("OVIMUppercase")
        << "non panel key received, code = " << key->keyCode() << endl;
    if (key->receivedString().size()) {
      m_strings.push_back(key->receivedString());
      composingText->setText(combineString());
      composingText->updateDisplay();
    }
    return false;
  }

 protected:
  virtual const string combineString(bool convert = false);
  vector<string> m_strings;
};

class OVIMUppercase : public OVInputMethod {
 public:
  virtual OVEventHandlingContext* createContext();
  virtual const string identifier() const;
  virtual const string localizedName(const string& locale);
  virtual bool initialize(OVPathInfo* pathInfo, OVLoaderService* loaderService);
  virtual void finalize();
  virtual void loadConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService);
  virtual void saveConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService);
};
};  // namespace OpenVanilla

#endif