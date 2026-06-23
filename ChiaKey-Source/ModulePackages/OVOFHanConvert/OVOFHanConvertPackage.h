//
// OVOFHanConvertPackage.h
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

#ifndef OVOFHanConvertPackage_h
#define OVOFHanConvertPackage_h

#include "OVOFHanConvert.h"

namespace OpenVanilla {
using namespace std;

// we encourage people to do the real initialization in initialize
class OVOFHanConvertPackage : public OVModulePackage {
 public:
  virtual size_t numberOfModules(OVLoaderService*) {
#ifdef OVOFHANCONVERT_USE_TC2SCONLY
    return 1;
#else
    return 2;
#endif
  }

  virtual OVModule* moduleAtIndex(size_t index, OVLoaderService*) {
    switch (index) {
      case 0:
        return new OVOFHanConvert(true);

#ifndef OVOFHANCONVERT_USE_TC2SCONLY
      case 1:
        return new OVOFHanConvert(false);
#endif
    }
    return 0;
  }
};

};  // namespace OpenVanilla

#endif