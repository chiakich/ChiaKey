//
// OVOFFullWidthCharacter.cpp
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

#include "OVOFFullWidthCharacter.h"

namespace OpenVanilla {

const string OVOFFullWidthCharacterContext::filterText(
    const string& inputText, OVLoaderService* loaderService) {
  unsigned short fullWidthChars[95] = {
      0x3000, 0xff01, 0x201d, 0xff03, 0xff04, 0xff05, 0xff06, 0x2019, 0xff08,
      0xff09, 0xff0a, 0xff0b, 0xff0c, 0xff0d, 0xff0e, 0xff0f, 0xff10, 0xff11,
      0xff12, 0xff13, 0xff14, 0xff15, 0xff16, 0xff17, 0xff18, 0xff19, 0xff1a,
      0xff1b, 0xff1c, 0xff1d, 0xff1e, 0xff1f, 0xff20, 0xff21, 0xff22, 0xff23,
      0xff24, 0xff25, 0xff26, 0xff27, 0xff28, 0xff29, 0xff2a, 0xff2b, 0xff2c,
      0xff2d, 0xff2e, 0xff2f, 0xff30, 0xff31, 0xff32, 0xff33, 0xff34, 0xff35,
      0xff36, 0xff37, 0xff38, 0xff39, 0xff3a, 0x3014, 0xff3c, 0x3015, 0xff3e,
      0xff3f, 0x2018, 0xff41, 0xff42, 0xff43, 0xff44, 0xff45, 0xff46, 0xff47,
      0xff48, 0xff49, 0xff4a, 0xff4b, 0xff4c, 0xff4d, 0xff4e, 0xff4f, 0xff50,
      0xff51, 0xff52, 0xff53, 0xff54, 0xff55, 0xff56, 0xff57, 0xff58, 0xff59,
      0xff5a, 0xff5b, 0xff5c, 0xff5d, 0xff5e};

  const char* halfWidthChars =
      " !\"#$%&'()*+,-./0123456789:;<=>?"
      "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
      "abcdefghijklmnopqrstuvwxyz{|}~";

  wstring wInputText = OVUTF16::FromUTF8(inputText);
  for (wstring::iterator iter = wInputText.begin(); iter != wInputText.end();
       ++iter) {
    if (*iter > 0x7f) continue;

    const char* ptr = strchr(halfWidthChars, (char)*iter);
    if (ptr) {
      *iter = fullWidthChars[(size_t)(ptr - halfWidthChars)];
    }
  }

  return OVUTF8::FromUTF16(wInputText);
}

const string OVOFFullWidthCharacter::identifier() const {
  return OVOFFULLWIDTHCHARACTER_IDENTIFIER;
}

};  // namespace OpenVanilla