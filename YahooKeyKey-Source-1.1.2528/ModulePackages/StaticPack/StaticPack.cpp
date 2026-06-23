//
// Developed for Yahoo! Taiwan by Lithoglyph Inc.
// Copyright (c) 2007-2010 Yahoo! Taiwan.
//

#include "StaticPack.h"

#include <OVAFBopomofoCorrectionPackage.h>
#include <OVAFEvalPackage.h>
#include <OVIMGenericPackage.h>
#include <OVIMMandarinPackage.h>
#include <OVOFFullWidthCharacterPackage.h>
#include <OVOFHanConvertPackage.h>

namespace OpenVanilla {

const vector<pair<OVModulePackage*, string> >
StaticPack::InitializedModulePackages(OVPathInfo* pathInfo,
                                      OVLoaderService* loaderService) {
  vector<pair<OVModulePackage*, string> > results;
  OVModulePackage* pkg;
#define ADD(type, name)                                           \
  pkg = new type;                                                 \
  if (pkg->initialize(pathInfo, loaderService)) {                 \
    results.push_back(pair<OVModulePackage*, string>(pkg, name)); \
  } else {                                                        \
    delete pkg;                                                   \
  }

  ADD(OVAFEvalPackage, "OVAFEvalPackage");
  ADD(OVAFBopomofoCorrectionPackage, "OVAFBopomofoCorrectionPackage");
  ADD(OVIMGenericPackage, "OVIMGenericPackage");
  ADD(OVIMMandarinPackage, "OVIMMandarinPackage");
  ADD(OVOFFullWidthCharacterPackage, "OVOFFullWidthCharacterPackage");
  ADD(OVOFHanConvertPackage, "OVOFHanConvertPackage");

#undef ADD

  return results;
}

};  // namespace OpenVanilla
