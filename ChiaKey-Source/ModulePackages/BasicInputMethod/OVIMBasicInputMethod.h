//
// OVIMBasicInputMethod.h
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

#ifndef BasicInputMethod_h
#define BasicInputMethod_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace OpenVanilla {
class OVIMBasicInputMethodContext : public OVEventHandlingContext {
 public:
  virtual void startSession(OVTextBuffer* composingText,
                            OVCandidateService* candidateService,
                            OVLoaderService* loaderService) {
    loaderService->logger() << "Session started" << endl;
  }

  virtual void stopSession(OVLoaderService* loaderService) {
    loaderService->logger() << "Session ended" << endl;
  }

  virtual void pauseSession(OVTextBuffer* composingText,
                            OVCandidateService* candidateService,
                            OVLoaderService* loaderService) {}

  virtual void resumeSession(OVTextBuffer* composingText,
                             OVCandidateService* candidateService,
                             OVLoaderService* loaderService) {}

  virtual bool handleKey(OVKey* key, OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService) {
    loaderService->logger()
        << "Received key, code = " << key->keyCode() << endl;

    if (key->keyCode() == OVKeyCode::Enter) {
      if (!composingText->isEmpty()) {
        composingText->commit();
        candidateService->plainTextCandidatePanel()->hide();
        return true;
      } else {
        return true;
      }
    }

    if (key->receivedString().length()) {
      composingText->appendText(key->receivedString());
      composingText->updateDisplay();

      stringstream sstr;
      sstr << "Entered: " << key->receivedString();

      candidateService->plainTextCandidatePanel()->textStorage()->setContent(
          sstr.str());
      candidateService->plainTextCandidatePanel()->updateDisplay();
      candidateService->plainTextCandidatePanel()->show();
      return true;
    }

    if (!composingText->isEmpty()) {
      return true;
    }

    return false;
  }

  virtual bool handleDirectText(const string&, OVTextBuffer* composingText,
                                OVCandidateService* candidateService,
                                OVLoaderService* loaderService) {
    return false;
  }

  virtual void candidateCanceled(OVCandidateService* candidateService,
                                 OVLoaderService* loaderService) {}

  virtual bool candidateSelected(OVCandidateService* candidateService,
                                 const string& text, size_t index,
                                 OVLoaderService* loaderService) {
    return true;
  }

  virtual bool candidateNonPanelKeyReceived(
      OVCandidateService* candidateService, const OVKey* key,
      const OVTextBuffer* composingText, OVLoaderService* loaderService) {
    return false;
  }
};

class OVIMBasicInputMethod : public OVInputMethod {
 public:
  virtual OVEventHandlingContext* createContext() {
    return new OVIMBasicInputMethodContext;
  }

  virtual string identifier() const { return string("OVIMBasicInputMethod"); }

  virtual void finalize(OVKeyValueMap* moduleConfig, OVPathInfo* pathInfo,
                        OVLoaderService* loaderService) {
    loaderService->logger() << "Module finalized" << endl;
  }

  virtual void handleUpdatedConfig(OVKeyValueMap* moduleConfig,
                                   OVPathInfo* pathInfo,
                                   OVLoaderService* loaderService) {
    loaderService->logger() << "Module config updated" << endl;
  }

 protected:
  virtual bool privateInitialize(OVKeyValueMap* moduleConfig,
                                 OVPathInfo* pathInfo,
                                 OVLoaderService* loaderService) {
    loaderService->logger() << "Module initialized" << endl;
    return true;
  }
};

};  // namespace OpenVanilla

#endif
