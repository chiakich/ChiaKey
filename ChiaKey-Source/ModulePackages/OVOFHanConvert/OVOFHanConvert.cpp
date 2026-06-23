//
// OVOFHanConvert.cpp
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

#include "OVOFHanConvert.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace OpenVanilla;

const size_t vxSC2TCTableSize = 8189;
extern "C" unsigned short vxSC2TCTable[];

const size_t vxTC2SCTableSize = 3059;
extern "C" unsigned short vxTC2SCTable[];

struct VXHCData {
  unsigned short key, value;
};

int VXHCCompare(const void* a, const void* b) {
  unsigned short x = ((const struct VXHCData*)a)->key,
                 y = ((const struct VXHCData*)b)->key;
  if (x == y) return 0;
  if (x < y) return -1;
  return 1;
}

unsigned short VXHCFind(unsigned key, unsigned short* table, size_t size) {
  struct VXHCData k;
  k.key = key;
  struct VXHCData* d = (struct VXHCData*)bsearch(
      &k, table, size, sizeof(struct VXHCData), VXHCCompare);
  if (!d) return 0;
  return d->value;
}

unsigned short VXUCS2TradToSimpChinese(unsigned short c) {
  return VXHCFind(c, vxTC2SCTable, vxTC2SCTableSize);
}

unsigned short VXUCS2SimpToTradChinese(unsigned short c) {
  return VXHCFind(c, vxSC2TCTable, vxSC2TCTableSize);
}

const string OVOFTC2SCContext::filterText(const string& inputText,
                                          OVLoaderService* loaderService) {
  wstring wideText = OVUTF16::FromUTF8(inputText);

  for (wstring::iterator iter = wideText.begin(); iter != wideText.end();
       ++iter) {
    unsigned short value = VXUCS2TradToSimpChinese((unsigned short)*iter);
    *iter = value ? value : *iter;
  }

  return OVUTF8::FromUTF16(wideText);
}

const string OVOFSC2TCContext::filterText(const string& inputText,
                                          OVLoaderService* loaderService) {
  wstring wideText = OVUTF16::FromUTF8(inputText);

  for (wstring::iterator iter = wideText.begin(); iter != wideText.end();
       ++iter) {
    unsigned short value = VXUCS2SimpToTradChinese((unsigned short)*iter);
    *iter = value ? value : *iter;
  }

  return OVUTF8::FromUTF16(wideText);
}

const string OVOFHanConvert::identifier() const {
  return m_tc2sc ? OVOFHANCONVERT_IDENTIFIER_PREFIX "-TC2SC"
                 : OVOFHANCONVERT_IDENTIFIER_PREFIX "-SC2TC";
}
