//
// OVAFReverseLookupPackage.h
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

#ifndef OVAFReverseLookupPackage_h
#define OVAFReverseLookupPackage_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "OVAFReverseLookup.h"
#include "OVAFReverseLookupConfig.h"

namespace OpenVanilla {
using namespace std;

class OVAFReverseLookupPackage : public OVModulePackage {
 protected:
  map<string, OVDatabaseService*> m_tableMap;
  vector<string> m_tableNames;

 public:
  virtual size_t numberOfModules(OVLoaderService*) {
    return m_tableNames.size();
  }

  virtual OVModule* moduleAtIndex(size_t index, OVLoaderService*) {
    if (index >= m_tableNames.size()) return 0;

    string name = m_tableNames[index];
    return new OVAFReverseLookup(name, m_tableMap[name]);
  }

  virtual bool initialize(OVPathInfo*, OVLoaderService* loaderService);
};

};  // namespace OpenVanilla

#endif