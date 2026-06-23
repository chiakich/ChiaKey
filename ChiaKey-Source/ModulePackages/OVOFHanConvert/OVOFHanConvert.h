//
// OVOFHanConvert.h
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

#ifndef OVOFHanConvert_h
#define OVOFHanConvert_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#ifndef OVOFHANCONVERT_IDENTIFIER_PREFIX
#define OVOFHANCONVERT_IDENTIFIER_PREFIX "OVOFHanConvert"
#endif

namespace OpenVanilla {
using namespace std;

class OVOFTC2SCContext : public OVEventHandlingContext {
 public:
  virtual const string filterText(const string& inputText,
                                  OVLoaderService* loaderService);
};

class OVOFSC2TCContext : public OVEventHandlingContext {
 public:
  virtual const string filterText(const string& inputText,
                                  OVLoaderService* loaderService);
};

class OVOFHanConvert : public OVOutputFilter {
 public:
  OVOFHanConvert(bool tc2sc) : m_tc2sc(tc2sc) {}

  virtual const string localizedName(const string& locale) {
    if (m_tc2sc) {
      if (locale == "zh_TW" || locale == "zh-Hant")
        return "\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87\xE8\xBD\x89"
               "\xE7\xB0\xA1\xE9\xAB\x94";
      if (locale == "zh_CN" || locale == "zh-Hans")
        return "\xE7\xB9\x81\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87\xE8\xBD\xAC"
               "\xE7\xAE\x80\xE4\xBD\x93";
      return "Traditional Chinese to Simpified Chinese";
    }

    if (locale == "zh_TW" || locale == "zh-Hant")
      return "\xE7\xB0\xA1\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87\xE8\xBD\x89\xE7"
             "\xB9\x81\xE9\xAB\x94";
    if (locale == "zh_CN" || locale == "zh-Hans")
      return "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87\xE8\xBD\xAC\xE7"
             "\xB9\x81\xE4\xBD\x93";
    return "Simplified Chinese to Traditional Chinese";
  }

  virtual OVEventHandlingContext* createContext() {
    if (m_tc2sc)
      return new OVOFTC2SCContext;
    else
      return new OVOFSC2TCContext;
  }

  virtual const string identifier() const;

 protected:
  bool m_tc2sc;
};

};  // namespace OpenVanilla

#endif