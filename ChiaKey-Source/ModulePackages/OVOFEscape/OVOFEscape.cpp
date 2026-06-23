//
// OVOFEscape.cpp
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

#include "OVOFEscape.h"

#ifdef WIN32
#define snprintf sprintf_s
#endif

namespace OpenVanilla {

OVOFEscapeCStringContext::OVOFEscapeCStringContext(string cfgCase)
    : m_cfgCase(cfgCase) {
  m_cfgCase = cfgCase;
}

const string OVOFEscapeCStringContext::filterText(
    const string& inputText, OVLoaderService* loaderService) {
  vector<string> words = OVUTF8Helper::SplitStringByCodePoint(inputText);
  string returnString = "";
  size_t i;
  for (i = 0; i < words.size(); i++) {
    string word = words[i];
    if (word.length() == 1) {
      returnString += word;
    } else {
      size_t j;
      for (j = 0; j < word.length(); j++) {
        char buf[16];
        unsigned char c = word[j];
        if (m_cfgCase == "Lower")
          snprintf(buf, 16, "\\x%2x", c);
        else
          snprintf(buf, 16, "\\x%2X", c);
        returnString += string(buf);
      }
    }
  }
  return returnString;
}

void OVOFEscapeCString::loadConfig(OVKeyValueMap* moduleConfig,
                                   OVLoaderService* loaderService) {
  if (moduleConfig->hasKey("Case"))
    m_cfgCase = moduleConfig->stringValueForKey("Case");
  else
    m_cfgCase = "Upper";
}
void OVOFEscapeCString::saveConfig(OVKeyValueMap* moduleConfig,
                                   OVLoaderService* loaderService) {
  moduleConfig->setKeyStringValue("Case", m_cfgCase);
}

const string OVOFEscapeURLContext::filterText(const string& inputText,
                                              OVLoaderService* loaderService) {
  // http://www.ietf.org/rfc/rfc2396.txt
  vector<string> words = OVUTF8Helper::SplitStringByCodePoint(inputText);
  string returnString = "";
  size_t i;
  for (i = 0; i < words.size(); i++) {
    string word = words[i];
    if (word.length() == 1) {
      if (word == ">" || word == "<" || word == "#" || word == "%" ||
          word == "\"" || word == "{" || word == "}" || word == "|" ||
          word == "\\" || word == "^" || word == "[" || word == "]" ||
          word == "`") {
        char buf[16];
        unsigned char c = word[0];
        snprintf(buf, 16, "%%%2X", c);
        returnString += string(buf);
      } else {
        returnString += word;
      }
    } else {
      size_t j;
      for (j = 0; j < word.length(); j++) {
        char buf[16];
        unsigned char c = word[j];
        snprintf(buf, 16, "%%%2X", c);
        returnString += string(buf);
      }
    }
  }
  return returnString;
}

const string OVOFEscapeC99Context::filterText(const string& inputText,
                                              OVLoaderService* loaderService) {
  vector<string> words = OVUTF8Helper::SplitStringByCodePoint(inputText);
  string returnString = "";
  size_t i;
  for (i = 0; i < words.size(); i++) {
    string word = words[i];
    if (word.length() == 1) {
      returnString += word;
    } else {
      wstring wideText = OVUTF16::FromUTF8(word);
      for (wstring::iterator iter = wideText.begin(); iter != wideText.end();
           ++iter) {
        char buf[16];
        if ((unsigned int)*iter <= 0xFFFF) {
          snprintf(buf, 16, "\\u%04x", (unsigned int)*iter);
        } else {
          snprintf(buf, 16, "\\U%08x", (unsigned int)*iter);
        }
        returnString += string(buf);
      }
    }
  }
  return returnString;
}

};  // namespace OpenVanilla
