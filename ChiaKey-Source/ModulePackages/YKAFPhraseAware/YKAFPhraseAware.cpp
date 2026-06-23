/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#include "YKAFPhraseAware.h"

#include <sstream>

#ifdef WIN32
#define snprintf sprintf_s
#endif

namespace OpenVanilla {

bool YKAFPhraseAwareContext::handleDirectText(
    const vector<string>& segments, OVTextBuffer* readingText,
    OVTextBuffer* composingText, OVCandidateService* candidateService,
    OVLoaderService* loaderService) {
  m_module->collect(segments);
  return false;
}

bool YKAFPhraseAwareContext::handleKey(OVKey* key, OVTextBuffer* readingText,
                                       OVTextBuffer* composingText,
                                       OVCandidateService* candidateService,
                                       OVLoaderService* loaderService) {
  if (key->isCtrlPressed() && key->keyCode() == '3') {
    composingText->showToolTip("Writing statistics to temp");
    m_module->writeToTemp();
    return true;
  }

  return false;
}

};  // namespace OpenVanilla
