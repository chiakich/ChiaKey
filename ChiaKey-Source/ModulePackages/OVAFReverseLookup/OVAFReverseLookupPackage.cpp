//
// OVAFReverseLookupPackage.cpp
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

#include "OVAFReverseLookupPackage.h"

namespace OpenVanilla {

bool OVAFReverseLookupPackage::initialize(OVPathInfo*,
                                          OVLoaderService* loaderService) {
  vector<string> matchPatterns = OVStringHelper::Split(
      OVAFREVERSELOOKUP_SOURCE_TABLE_PATTERNS_SEMICOLON_SEPARATED, ';');

  //    loaderService->logger(OVAFREVERSELOOKUP_IDENTIFIER_PREFIX) << "Reverse
  //    table source patterns: " <<
  //    OVAFREVERSELOOKUP_SOURCE_TABLE_PATTERNS_SEMICOLON_SEPARATED << " (" <<
  //    matchPatterns.size() << " patterns)" << endl;

  vector<OVDatabaseService*> services;

  if (loaderService->SQLiteDatabaseService())
    services.push_back(loaderService->SQLiteDatabaseService());

  if (loaderService->CINDatabaseService())
    services.push_back(loaderService->CINDatabaseService());

  for (vector<OVDatabaseService*>::iterator iter = services.begin();
       iter != services.end(); ++iter) {
    OVDatabaseService* service = *iter;
    vector<string> tables = service->tables();

    for (vector<string>::iterator siter = tables.begin(); siter != tables.end();
         ++siter) {
      if (OVWildcard::MultiWildcardMatchAny(*siter, matchPatterns) &&
          service->tableSupportsValueToKeyLookup(*siter)) {
        //    			loaderService->logger(OVAFREVERSELOOKUP_IDENTIFIER_PREFIX)
        //    << "Found table: " << *siter << endl;
        m_tableMap[*siter] = *iter;

        if (OVWildcard::Match(
                *siter,
                OVAFREVERSELOOKUP_GENERATE_PINYIN_LOOKUP_IF_ENCOUNTERED_TABLE_NAME)) {
          m_tableMap
              [*siter +
               OVAFREVERSELOOKUP_GENERATE_PINYIN_LOOKUP_TABLE_NAME_SUFFIX] =
                  *iter;
        }
      }
    }
  }

  for (map<string, OVDatabaseService*>::iterator miter = m_tableMap.begin();
       miter != m_tableMap.end(); ++miter) {
    m_tableNames.push_back((*miter).first);
  }

  return true;
}

};  // namespace OpenVanilla
