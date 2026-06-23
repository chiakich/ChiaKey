//
// TestPlainVanillaModulePackageMain.cpp
//
// Copyright (c) 2007-2010 Lukhnos D. Liu (http://lukhnos.org)
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

#ifndef __APPLE__
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

using namespace OpenVanilla;

class TestEventHandlingContext : public OVEventHandlingContext {
 public:
  ~TestEventHandlingContext() {
    cerr << "NOTE: event handling context destructed" << endl;
  }

  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService) {
    loaderService->logger("context")
        << "Received key, code = " << key->keyCode() << endl;
    return false;
  }
};

class TestModule : public OVModule {
 public:
  ~TestModule() { cout << "module destruted!" << endl; }

  virtual OVEventHandlingContext* createContext() {
    return new TestEventHandlingContext;
  }

  virtual bool initialize(OVPathInfo* pathInfo,
                          OVLoaderService* loaderService) {
    loaderService->logger("test module")
        << "module initialized! loaded path = " << pathInfo->loadedPath << endl;
    return true;
  }

  virtual void finalize() { cout << "module finalized!" << endl; }

  virtual void loadConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService) {
    loaderService->logger("test module") << "loading module config" << endl;
  }

  virtual void saveConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService) {
    loaderService->logger("test module") << "saving module config" << endl;
  }

  bool isInputMethod() const { return true; }

  const string identifier() const { return string("TestModule"); }
};

class TestModulePackage : public OVModulePackage {
 public:
  virtual bool initialize(OVPathInfo*, OVLoaderService* loaderService) {
    loaderService->logger("TestModulePackage")
        << "module package initialized!" << endl;
    m_moduleVector.push_back(new OVModuleClassWrapper<TestModule>());
    return true;
  }

  virtual void finalize() { cerr << "module package finalized!" << endl; }
};

extern "C" OVModulePackage* OVModulePackageMain() {
  return new TestModulePackage;
}

extern "C" unsigned int OVModulePackageFrameworkVersion() {
  return OVFrameworkInfo::Version();
}
