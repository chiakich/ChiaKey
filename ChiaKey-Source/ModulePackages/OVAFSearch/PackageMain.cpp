//
// Developed for Yahoo! Taiwan by Lithoglyph Inc.
// Copyright (c) 2007-2010 Yahoo! Taiwan.
//

#include "OVAFSearchPackage.h"

using namespace OpenVanilla;

extern "C" OVEXPORT OVModulePackage* OVModulePackageMain() {
  return new OVAFSearchPackage;
}

extern "C" OVEXPORT unsigned int OVModulePackageFrameworkVersion() {
  return OVFrameworkInfo::Version();
}
