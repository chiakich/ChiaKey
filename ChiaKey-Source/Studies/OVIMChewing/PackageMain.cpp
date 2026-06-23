// [AUTO_HEADER]

#include "OVIMChewingPackage.h"

using namespace OpenVanilla;

extern "C" OVModulePackage* OVModulePackageMain() {
  return new OVIMChewingPackage;
}

extern "C" unsigned int OVModulePackageFrameworkVersion() {
  return OVFrameworkInfo::Version();
}
