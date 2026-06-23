/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#include "YKAFWordCountPackage.h"

using namespace OpenVanilla;

extern "C" OVEXPORT OVModulePackage* OVModulePackageMain() {
  return new YKAFWordCountPackage;
}

extern "C" OVEXPORT unsigned int OVModulePackageFrameworkVersion() {
  return OVFrameworkInfo::Version();
}
