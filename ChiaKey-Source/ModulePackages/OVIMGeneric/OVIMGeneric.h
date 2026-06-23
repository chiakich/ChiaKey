//
// OVIMGeneric.h
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

#ifndef OVIMGeneric_h
#define OVIMGeneric_h

#ifndef OV_USE_SQLITE
#define OV_USE_SQLITE
#endif

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace OpenVanilla {
using namespace std;

class OVIMGeneric;

class OVIMGenericContext : public OVEventHandlingContext {
 public:
  OVIMGenericContext(OVIMGeneric* module);

  virtual void startSession(OVLoaderService* loaderService);
  virtual void stopSession(OVLoaderService* loaderService);
  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService);

  virtual void candidateCanceled(OVCandidateService* candidateService,
                                 OVTextBuffer* readingText,
                                 OVTextBuffer* composingText,
                                 OVLoaderService* loaderService);
  virtual bool candidateSelected(OVCandidateService* candidateService,
                                 const string& text, size_t index,
                                 OVTextBuffer* readingText,
                                 OVTextBuffer* composingText,
                                 OVLoaderService* loaderService);
  virtual bool candidateNonPanelKeyReceived(
      OVCandidateService* candidateService, const OVKey* key,
      OVTextBuffer* readingText, OVTextBuffer* composingText,
      OVLoaderService* loaderService);

 protected:
  virtual bool handleBackspace(OVTextBuffer* readingText,
                               OVTextBuffer* composingText,
                               OVCandidateService* candidateService,
                               OVLoaderService* loaderService);
  virtual bool compose(OVTextBuffer* readingText, OVTextBuffer* composingText,
                       OVCandidateService* candidateService,
                       OVLoaderService* loaderService,
                       bool sendIfOnlyOneCandidate = true);
  virtual bool isValidKey(const string& keystr);
  virtual bool isEndKey(const OVKey* key);
  virtual const string combineReading();
  virtual const string combineQueryString();

  virtual bool stringContainsWildcard(const string& str);
  virtual const string keynameForKeyString(const string& keystr);

  vector<string> m_components;
  OVIMGeneric* m_module;

  // for user frequency table
  OVSQLiteStatement* m_selectStatement;
  OVSQLiteStatement* m_insertStatement;
  OVSQLiteStatement* m_updateStatement;
  OVSQLiteStatement* m_findOrderStatement;

  // last committed string
  string m_lastCommittedString;
};

class OVIMGeneric : public OVInputMethod {
 public:
  OVIMGeneric(const string& name, OVDatabaseService* databaseService,
              const string& tableName = "");
  ~OVIMGeneric();

  virtual OVEventHandlingContext* createContext();
  virtual const string identifier() const;
  virtual const string localizedName(const string& locale);
  virtual bool initialize(OVPathInfo* pathInfo, OVLoaderService* loaderService);
  virtual void finalize();
  virtual void loadConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService);
  virtual void saveConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService);

 protected:
  friend class OVIMGenericContext;

  string m_name;
  string m_tableName;
  OVKeyValueDataTableInterface* m_dataTable;
  OVKeyValueDataTableInterface* m_overrideTable;
  OVDatabaseService* m_databaseService;
  bool m_caseSensitive;

  map<string, string> m_localizedNames;

  string m_userDatabasePath;
  OVSQLiteConnection* m_userDatabaseConnection;

 protected:
  // configurable items
  size_t m_cfgMaximumRadicalLength;
  bool m_cfgShouldCommitAtMaximumRadicalLength;
  bool m_cfgClearReadingBufferAtCompositionError;
  bool m_cfgUseDynamicFrequency;
  bool m_cfgUseSpaceAsFirstCandidateSelectionKey;
  bool m_cfgComposeWhileTyping;
  bool m_cfgSendFirstCandidateWithSpaceWithOnePageList;
  char m_cfgMatchOneChar;
  char m_cfgMatchZeroOrMoreChar;
  string m_cfgCandidateSelectionKeys;
  string m_cfgUseCharactersSupportedByEncoding;
  string m_cfgUseOverrideTable;

  // @todo: more options (ShouldCommitAsPhrase requires lots of
  // rearchitecturing) bool m_cfgShouldCommitAsPhrase; bool
  // m_cfgDisplayRadicalsOnCandidate; bool m_cfgOverrideSelectionKey;
};
};  // namespace OpenVanilla

#endif
