//
// OVIMAlphanumeric.h
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

#ifndef OVIMAlphanumeric_h
#define OVIMAlphanumeric_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace OpenVanilla {
class OVIMAlphanumericContext : public OVEventHandlingContext {
 public:
  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService) {
    if (key->isCombinedFunctionKey() || !key->receivedString().size())
      return false;

    if (key->isPrintable()) {
      if (key->isCapsLockOn()) {
        if (key->keyCode() >= 'a' && key->keyCode() <= 'z') {
          if (key->isShiftPressed()) {
            composingText->setText(string(1, tolower(key->keyCode())));
          } else {
            composingText->setText(string(1, toupper(key->keyCode())));
          }
          composingText->commit();
          return true;
        }
      }
      composingText->setText(key->receivedString());
      composingText->commit();
      return true;
    }

    return false;
  }
};

class OVIMAlphanumeric : public OVInputMethod {
 public:
  virtual OVEventHandlingContext* createContext() {
    return new OVIMAlphanumericContext;
  }

  virtual const string identifier() const { return string("OVIMAlphanumeric"); }

  virtual const string localizedName(const string& locale) {
#ifndef _MSC_VER
    string tcname = "英文/數字";
    string scname = "英文/数字";
#else
    string tcname = "\xe8\x8b\xb1\xe6\x96\x87/\xe6\x95\xb8\xe5\xad\x97";
    string scname = "\xe8\x8b\xb1\xe6\x96\x87/\xe6\x95\xb0\xe5\xad\x97";
#endif

    if (locale == "zh_TW" || locale == "zh-hant")
      return tcname;
    else if (locale == "zh_CN" || locale == "zh-hans")
      return scname;
    else
      return string("Alphanumeric");
  }
};
};  // namespace OpenVanilla

#endif
