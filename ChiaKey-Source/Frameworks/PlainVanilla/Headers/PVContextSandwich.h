//
// PVContextSandwich.h
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

#ifndef PVContextSandwich_h
#define PVContextSandwich_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace OpenVanilla {
using namespace std;

struct PVContextSandwich : public OVBase {
  ~PVContextSandwich();

  void startAllContexts(OVLoaderService* loaderService);
  void stopAllContexts(OVLoaderService* loaderService);
  void clearAllContexts(OVLoaderService* loaderService);

  vector<OVEventHandlingContext*> preprocessors;
  vector<OVEventHandlingContext*> aroundFilters;
  vector<OVEventHandlingContext*> inputMethods;
  vector<OVEventHandlingContext*> outputFilters;

  void prepareShortcutContexts();
  vector<OVEventHandlingContext*> keyHandlingContexts;

  bool isAroundFilterContext(OVEventHandlingContext* context);

 protected:
  static void DeleteEach(vector<OVEventHandlingContext*>& contexts);
  static void StartEach(vector<OVEventHandlingContext*>& contexts,
                        OVLoaderService* loaderService);
  static void StopEach(vector<OVEventHandlingContext*>& contexts,
                       OVLoaderService* loaderService);
  static void ClearEach(vector<OVEventHandlingContext*>& contexts,
                        OVLoaderService* loaderService);
};

inline PVContextSandwich::~PVContextSandwich() {
  DeleteEach(preprocessors);
  DeleteEach(aroundFilters);
  DeleteEach(inputMethods);
  DeleteEach(outputFilters);
}

inline void PVContextSandwich::startAllContexts(
    OVLoaderService* loaderService) {
  StartEach(preprocessors, loaderService);
  StartEach(aroundFilters, loaderService);
  StartEach(inputMethods, loaderService);
  StartEach(outputFilters, loaderService);
}

inline void PVContextSandwich::stopAllContexts(OVLoaderService* loaderService) {
  StopEach(preprocessors, loaderService);
  StopEach(aroundFilters, loaderService);
  StopEach(inputMethods, loaderService);
  StopEach(outputFilters, loaderService);
}

inline void PVContextSandwich::clearAllContexts(
    OVLoaderService* loaderService) {
  ClearEach(preprocessors, loaderService);
  ClearEach(aroundFilters, loaderService);
  ClearEach(inputMethods, loaderService);
  ClearEach(outputFilters, loaderService);
}

inline void PVContextSandwich::prepareShortcutContexts() {
  keyHandlingContexts.clear();
  keyHandlingContexts.insert(keyHandlingContexts.end(), preprocessors.begin(),
                             preprocessors.end());

  // @todo reverse the order of around filter
  keyHandlingContexts.insert(keyHandlingContexts.end(), aroundFilters.begin(),
                             aroundFilters.end());
  keyHandlingContexts.insert(keyHandlingContexts.end(), inputMethods.begin(),
                             inputMethods.end());
}

inline void PVContextSandwich::DeleteEach(
    vector<OVEventHandlingContext*>& contexts) {
  for (vector<OVEventHandlingContext*>::iterator iter = contexts.begin();
       iter != contexts.end(); ++iter)
    if (*iter) delete *iter;
  contexts.clear();
}

inline void PVContextSandwich::StartEach(
    vector<OVEventHandlingContext*>& contexts, OVLoaderService* loaderService) {
  for (vector<OVEventHandlingContext*>::iterator iter = contexts.begin();
       iter != contexts.end(); ++iter)
    if (*iter) (*iter)->startSession(loaderService);
}

inline void PVContextSandwich::StopEach(
    vector<OVEventHandlingContext*>& contexts, OVLoaderService* loaderService) {
  for (vector<OVEventHandlingContext*>::iterator iter = contexts.begin();
       iter != contexts.end(); ++iter)
    if (*iter) (*iter)->stopSession(loaderService);
}

inline void PVContextSandwich::ClearEach(
    vector<OVEventHandlingContext*>& contexts, OVLoaderService* loaderService) {
  for (vector<OVEventHandlingContext*>::iterator iter = contexts.begin();
       iter != contexts.end(); ++iter)
    if (*iter) (*iter)->clear(loaderService);
}

inline bool PVContextSandwich::isAroundFilterContext(
    OVEventHandlingContext* context) {
  for (vector<OVEventHandlingContext*>::iterator iter = aroundFilters.begin();
       iter != aroundFilters.end(); ++iter) {
    if (context == *iter) return true;
  }

  return false;
}
};  // namespace OpenVanilla

#endif