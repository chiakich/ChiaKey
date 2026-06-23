//
// Developed for Yahoo! Taiwan by Lithoglyph Inc.
// Copyright (c) 2007-2010 Yahoo! Taiwan.
//

#ifndef OVAFSearchPackage_h
#define OVAFSearchPackage_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "OVAFSearch.h"

namespace OpenVanilla {
using namespace std;

class OVAFSearchPackage : public OVModulePackage {
 public:
  OVAFSearchPackage() {
    m_moduleVector.push_back(new OVModuleClassWrapper<OVAFSearch>);
  }

  virtual bool initialize(OVPathInfo*, OVLoaderService* loaderService) {
    // loaderService->logger("OVOFSearchPacakge") << "Initialized" << endl;
    return true;
  }
};

};  // namespace OpenVanilla

#endif