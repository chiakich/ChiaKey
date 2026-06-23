// [AUTO_HEADER]

#ifndef OVMPChewingPackge_h
#define OVMPChewingPackage_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "OVIMChewing.h"

namespace OpenVanilla {
using namespace std;

class OVIMChewingPackage : public OVModulePackage {
 public:
  OVIMChewingPackage() {
    // add OVIMChewing to our module package
    m_moduleVector.push_back(new OVModuleClassWrapper<OVIMChewing>);
  }

  virtual bool initialize(OVPathInfo*, OVLoaderService* loaderService) {
    loaderService->logger("OVIMChewingPacakge") << "Initialized" << endl;
    return true;
  }
};

};  // namespace OpenVanilla

#endif