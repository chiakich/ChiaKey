//
// OVAFReverseLookup.cpp
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

#include "OVAFReverseLookup.h"

#include <ctype.h>

#include <set>

#include "Mandarin.h"
#include "OVAFReverseLookupConfig.h"

namespace OpenVanilla {

OVAFReverseLookupContext::OVAFReverseLookupContext(OVAFReverseLookup* module)
    : m_module(module) {}

bool OVAFReverseLookupContext::handleDirectText(
    const string& inputText, OVTextBuffer* readingText,
    OVTextBuffer* composingText, OVCandidateService* candidateService,
    OVLoaderService* loaderService) {
  if (m_module->m_dataTable) {
    // ad hoc stuff
    bool bpmfAbsoluteStringLookup =
        OVWildcard::Match(m_module->m_tableName, "*Mandarin-bpmf*");
    bool pinyinLookup = OVWildcard::Match(
        m_module->m_tableName,
        "*" OVAFREVERSELOOKUP_GENERATE_PINYIN_LOOKUP_TABLE_NAME_SUFFIX);

    vector<string> results;
    set<string> codepointsLookedUp;

    vector<string> values = OVUTF8Helper::SplitStringByCodePoint(inputText);
    for (vector<string>::iterator viter = values.begin(); viter != values.end();
         ++viter) {
      if (codepointsLookedUp.find(*viter) != codepointsLookedUp.end()) {
        continue;
      }

      unsigned int cp = OVUTF8Helper::CodePointFromSingleUTF8String(*viter);
      if (cp < 0x2e80 || (cp >= 0x3000 && cp <= 0x3040) ||
          (cp >= 0xff00 && cp <= 0xffff)) {
        // skip non-CJK chars and punctuation symbols
        continue;
      }

      codepointsLookedUp.insert(*viter);

      vector<string> keys = m_module->m_dataTable->keysForValue(*viter);

      if (!keys.size()) {
        continue;
      }

      vector<string> namedKeys;
      set<string> foundRadicals;

      // combine the keyname
      for (vector<string>::iterator kiter = keys.begin(); kiter != keys.end();
           ++kiter) {
        string namestr;

        // ad hoc stuff
        if (bpmfAbsoluteStringLookup) {
          if (pinyinLookup) {
            namestr = Formosa::Mandarin::BPMF::FromAbsoluteOrderString(*kiter)
                          .HanyuPinyinString(false, true);
          } else {
            namestr = Formosa::Mandarin::BPMF::FromAbsoluteOrderString(*kiter)
                          .composedString();
          }
        } else {
          string& kstr = *kiter;
          for (size_t ki = 0; ki < kstr.length(); ki++) {
            namestr += m_module->m_dataTable->valueForProperty(
                OVKeynamePropertyHelper::KeynameToProperty(
                    string(1, kstr[ki])));
          }
        }

        if (namestr.size()) {
          if (foundRadicals.find(namestr) == foundRadicals.end()) {
            foundRadicals.insert(namestr);
            namedKeys.push_back(namestr);
          }
        }
      }

      if (!namedKeys.size()) {
        continue;
      }

      string text = *viter + ": " + OVStringHelper::Join(namedKeys, ", ");
      // loaderService->logger(m_module->identifier()) << text << endl;
      results.push_back(text);
    }

    if (results.size()) {
      composingText->showToolTip(OVStringHelper::Join(results, "\n"));
    }
  }

  // loaderService->logger(m_module->identifier()) << "filtering: " << inputText
  // << endl;
  return false;
}

OVAFReverseLookup::OVAFReverseLookup(const string& name,
                                     OVDatabaseService* databaseService)
    : m_tableName(name), m_databaseService(databaseService), m_dataTable(0) {
  // set the defaults

  string ename, cname, tcname, scname;

  ename = databaseService->valueForPropertyInTable("ename", name);
  if (!ename.length()) ename = m_tableName;

  cname = databaseService->valueForPropertyInTable("cname", name);
  if (!cname.length()) cname = ename;

  tcname = databaseService->valueForPropertyInTable("tcname", name);
  if (!tcname.length()) tcname = cname;

  scname = databaseService->valueForPropertyInTable("scname", name);
  if (!scname.length()) scname = cname;

#ifndef _MSC_VER
  string cnamePrefix = "字根反查-";
#else
  string cnamePrefix = "\xe5\xad\x97\xe6\xa0\xb9\xe5\x8f\x8d\xe6\x9f\xa5-";
#endif

  if (OVWildcard::Match(
          name,
          "*" OVAFREVERSELOOKUP_GENERATE_PINYIN_LOOKUP_TABLE_NAME_SUFFIX)) {
    ename = "Hanyu Pinyin";
#ifndef _MSC_VER
    tcname = "漢語拼音";
    scname = "汉语拼音";
#else
    tcname = "\xe6\xbc\xa2\xe8\xaa\x9e\xe6\x8b\xbc\xe9\x9f\xb3";
    scname = "\xe6\xb1\x89\xe8\xaf\xad\xe6\x8b\xbc\xe9\x9f\xb3";
#endif
  }

  m_localizedNames["en"] = string("Reverse Lookup for ") + ename;
  m_localizedNames["zh_TW"] = cnamePrefix + tcname;
  m_localizedNames["zh-Hant"] = cnamePrefix + tcname;
  m_localizedNames["zh_CN"] = cnamePrefix + scname;
  m_localizedNames["zh-Hans"] = cnamePrefix + scname;
}

OVAFReverseLookup::~OVAFReverseLookup() {
  if (m_dataTable) delete m_dataTable;
}

OVEventHandlingContext* OVAFReverseLookup::createContext() {
  return m_dataTable ? new OVAFReverseLookupContext(this) : 0;
}

const string OVAFReverseLookup::identifier() const {
  return string(OVAFREVERSELOOKUP_IDENTIFIER_PREFIX "-") + m_tableName;
}

const string OVAFReverseLookup::localizedName(const string& locale) {
  map<string, string>::iterator iter = m_localizedNames.find(locale);
  return iter == m_localizedNames.end() ? m_localizedNames["en"]
                                        : (*iter).second;
}

bool OVAFReverseLookup::initialize(OVPathInfo* pathInfo,
                                   OVLoaderService* loaderService) {
  string tableName = m_tableName;
  if (OVWildcard::Match(
          tableName,
          "*" OVAFREVERSELOOKUP_GENERATE_PINYIN_LOOKUP_TABLE_NAME_SUFFIX)) {
    tableName = OVStringHelper::StringByReplacingOccurrencesOfStringWithString(
        m_tableName, OVAFREVERSELOOKUP_GENERATE_PINYIN_LOOKUP_TABLE_NAME_SUFFIX,
        "");
  }

  bool caseSensitive = OVWildcard::Match(tableName, "*-casesensitive*");
  m_dataTable = m_databaseService->createKeyValueDataTableInterface(
      tableName, caseSensitive);
  return !!m_dataTable;
}

};  // namespace OpenVanilla
