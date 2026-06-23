//
// OVOFFullWidthCharacter.h
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

#ifndef OVOFFullWidthCharacter_h
#define OVOFFullWidthCharacter_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#ifndef OVOFFULLWIDTHCHARACTER_IDENTIFIER
#define OVOFFULLWIDTHCHARACTER_IDENTIFIER "OVOFFullWidthCharacter"
#endif

namespace OpenVanilla {
using namespace std;

class OVOFFullWidthCharacterContext : public OVEventHandlingContext {
 public:
  virtual const string filterText(const string& inputText,
                                  OVLoaderService* loaderService);
};

class OVOFFullWidthCharacter : public OVOutputFilter {
 public:
  virtual const string localizedName(const string& locale) {
#ifndef _MSC_VER
    string tcname = "使用全形英數字";
    string scname = "使用全角英数字";
#else
    string tcname =
        "\xe4\xbd\xbf\xe7\x94\xa8\xe5\x85\xa8\xe5\xbd\xa2\xe8\x8b\xb1\xe6\x95"
        "\xb8\xe5\xad\x97";
    string scname =
        "\xe4\xbd\xbf\xe7\x94\xa8\xe5\x85\xa8\xe8\xa7\x92\xe8\x8b\xb1\xe6\x95"
        "\xb0\xe5\xad\x97";
#endif

    if (locale == "zh_TW" || locale == "zh-Hant")
      return tcname;
    else if (locale == "zh_CN" || locale == "zh-Hans")
      return scname;
    else
      return string("Use Full-Width Characters");
  }

  virtual OVEventHandlingContext* createContext() {
    return new OVOFFullWidthCharacterContext;
  }

  virtual const string identifier() const;
};

};  // namespace OpenVanilla

#endif