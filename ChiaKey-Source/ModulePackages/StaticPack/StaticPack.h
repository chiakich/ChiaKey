//
// Developed for Yahoo! Taiwan by Lithoglyph Inc.
// Copyright (c) 2007-2010 Yahoo! Taiwan.
//

#ifndef StaticPack_h
#define StaticPack_h

#ifndef OV_USE_SQLITE
#define OV_USE_SQLITE
#endif

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace OpenVanilla {
using namespace std;

class StaticPack {
 public:
  static const vector<pair<OVModulePackage*, string> >
  StaticPack::InitializedModulePackages(OVPathInfo* pathInfo,
                                        OVLoaderService* loaderService);
};
};  // namespace OpenVanilla

#endif