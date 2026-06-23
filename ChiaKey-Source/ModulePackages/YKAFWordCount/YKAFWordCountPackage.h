/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef YKAFWordCountPackage_h
#define YKAFWordCountPackage_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "YKAFWordCount.h"

namespace OpenVanilla {
using namespace std;

class YKAFWordCountPackage : public OVModulePackage {
 public:
  YKAFWordCountPackage() {
    m_moduleVector.push_back(new OVModuleClassWrapper<YKAFWordCount>);
  }

  virtual bool initialize(OVPathInfo*, OVLoaderService* loaderService) {
    return true;
  }
};

};  // namespace OpenVanilla

#endif
