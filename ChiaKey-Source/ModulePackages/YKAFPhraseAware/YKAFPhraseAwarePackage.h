/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef YKAFPhraseAwarePackage_h
#define YKAFPhraseAwarePackage_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "YKAFPhraseAware.h"

namespace OpenVanilla {
using namespace std;

class YKAFPhraseAwarePackage : public OVModulePackage {
 public:
  YKAFPhraseAwarePackage() {
    m_moduleVector.push_back(new OVModuleClassWrapper<YKAFPhraseAware>);
  }

  virtual bool initialize(OVPathInfo*, OVLoaderService* loaderService) {
    return true;
  }
};

};  // namespace OpenVanilla

#endif
