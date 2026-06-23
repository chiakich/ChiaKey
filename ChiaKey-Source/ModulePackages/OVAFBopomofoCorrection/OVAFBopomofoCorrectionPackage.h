//
// OVAFBopomofoCorrectionPackage.h
//
// Copyright (c) 2004-2010 The OpenVanilla Project (http://openvanilla.org)
// All rights reserved.
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//

#ifndef OVAFBopomofoCorrectionPackage_h
#define OVAFBopomofoCorrectionPackage_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "OVAFBopomofoCorrection.h"

namespace OpenVanilla {
using namespace std;

class OVAFBopomofoCorrectionPackage : public OVModulePackage {
 public:
  OVAFBopomofoCorrectionPackage() {
    m_moduleVector.push_back(new OVModuleClassWrapper<OVAFBopomofoCorrection>);
  }

  virtual bool initialize(OVPathInfo*, OVLoaderService* loaderService) {
    // loaderService->logger("OVAFBopomofoCorrection") << "package inited." <<
    // endl; in your derived class, add class wrappers to m_moduleVector
    return true;
  }
};

};  // namespace OpenVanilla

#endif