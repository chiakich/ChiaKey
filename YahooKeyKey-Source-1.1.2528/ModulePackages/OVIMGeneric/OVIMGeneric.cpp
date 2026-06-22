//
// OVIMGeneric.cpp
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

#include "OVIMGeneric.h"

#include <ctype.h>

namespace OpenVanilla {

const string OVIMGenericContext::keynameForKeyString(const string& keystr) {
  string result = m_module->m_dataTable->valueForProperty(
      OVKeynamePropertyHelper::KeynameToProperty(keystr));
  if (!result.size() && keystr.size() == 1) {
    if (keystr[0] == m_module->m_cfgMatchOneChar) {
      result = string(1, m_module->m_cfgMatchOneChar);
    } else if (keystr[0] == m_module->m_cfgMatchZeroOrMoreChar) {
      result = string(1, m_module->m_cfgMatchZeroOrMoreChar);
    }
  }

  return result;
}

bool OVIMGenericContext::isValidKey(const string& keystr) {
  return keynameForKeyString(keystr).length() != 0;
}

bool OVIMGenericContext::isEndKey(const OVKey* key) {
  string endkey = m_module->m_dataTable->valueForProperty("endkey");
  for (string::iterator iter = endkey.begin(); iter != endkey.end(); ++iter)
    if (key->keyCode() == *iter) return true;

  return false;
}

const string OVIMGenericContext::combineReading() {
  // cerr << "enter combine reading" << endl;

  string reading;
  for (vector<string>::iterator iter = m_components.begin();
       iter < m_components.end(); ++iter) {
    // cerr << "iter = " << (size_t)(iter - m_components.begin()) << endl;
    for (vector<string>::iterator back = m_components.end(); back > iter;
         --back) {
      // cerr << "back = " << (size_t)(back - m_components.begin()) << endl;

      string newKey;

      for (vector<string>::iterator j = iter; j < back; ++j) {
        // cerr << "j = " << (size_t)(j - m_components.begin()) << endl;

        newKey += *j;
      }

      // cerr << "combining new key: " << newKey << endl;
      string nr = keynameForKeyString(newKey);
      if (nr.size()) {
        reading += nr;
        iter = back;
        iter--;  // because later there is a ++
        break;
      }
    }

    // reading +=
    // m_module->m_dataTable->valueForProperty(OVKeynamePropertyHelper::KeynameToProperty(*iter));
  }

  // cerr << "leave combine reading" << endl;
  return reading;
}

bool OVIMGenericContext::stringContainsWildcard(const string& str) {
  char o = m_module->m_cfgMatchOneChar;
  char z = m_module->m_cfgMatchZeroOrMoreChar;

  for (string::const_iterator iter = str.begin(); iter != str.end(); ++iter)
    if (*iter == o || *iter == z) return true;

  return false;
}

const string OVIMGenericContext::combineQueryString() {
  string query;
  for (vector<string>::iterator iter = m_components.begin();
       iter != m_components.end(); ++iter)
    query += *iter;

  return query;
}

OVIMGenericContext::OVIMGenericContext(OVIMGeneric* module)
    : m_module(module),
      m_selectStatement(0),
      m_insertStatement(0),
      m_updateStatement(0),
      m_findOrderStatement(0) {}

void OVIMGenericContext::startSession(OVLoaderService* loaderService) {
  // loaderService->logger(OVIMGENERIC_IDENTIFIER_PREFIX) << "session started"
  // << endl;
  m_lastCommittedString = "";
  m_components.clear();

  if (m_module->m_userDatabaseConnection) {
    // loaderService->logger(OVIMGENERIC_IDENTIFIER_PREFIX) << "using database"
    // << endl;

    bool hasTable = false;

    hasTable = m_module->m_userDatabaseConnection->hasTable("user_frequency");
    // loaderService->logger(OVIMGENERIC_IDENTIFIER_PREFIX) << "has table? " <<
    // hasTable << endl;

    if (!hasTable) {
      if (m_module->m_userDatabaseConnection->createTable(
              "user_frequency", "key, value, count")) {
        if (m_module->m_userDatabaseConnection->createIndexOnTable(
                "user_frequency_index", "user_frequency", "key")) {
          hasTable = true;
        }
      }
    }

    if (hasTable) {
      int result;
      if ((result = m_module->m_userDatabaseConnection->execute("BEGIN")) ==
          SQLITE_OK) {
        // loaderService->logger(OVIMGENERIC_IDENTIFIER_PREFIX) << "Preparing
        // user frequency database" << endl;

        m_selectStatement = m_module->m_userDatabaseConnection->prepare(
            "SELECT count FROM user_frequency WHERE key = ? and value = ?");
        m_insertStatement = m_module->m_userDatabaseConnection->prepare(
            "INSERT INTO user_frequency VALUES (?, ?, ?)");
        m_updateStatement = m_module->m_userDatabaseConnection->prepare(
            "UPDATE user_frequency SET count = ? WHERE key = ? and value = ?");
        m_findOrderStatement = m_module->m_userDatabaseConnection->prepare(
            "SELECT value FROM user_frequency WHERE key = ? ORDER BY count "
            "DESC");
        // loaderService->logger(OVIMGENERIC_IDENTIFIER_PREFIX) << "has
        // m_insertStatement? " << (m_insertStatement != 0) << endl;
      } else {
        // loaderService->logger(OVIMGENERIC_IDENTIFIER_PREFIX) << "Cannot
        // BEGIN: " << result << endl;
      }
    }
  }
}

void OVIMGenericContext::stopSession(OVLoaderService* loaderService) {
  // loaderService->logger(OVIMGENERIC_IDENTIFIER_PREFIX) << "session ended" <<
  // endl;
  m_lastCommittedString = "";
  m_components.clear();

  if (m_module->m_userDatabaseConnection) {
    if (m_selectStatement) {
      delete m_selectStatement;
      m_selectStatement = 0;
    }

    if (m_insertStatement) {
      delete m_insertStatement;
      m_insertStatement = 0;
    }

    if (m_findOrderStatement) {
      delete m_findOrderStatement;
      m_findOrderStatement = 0;
    }

    if (m_updateStatement) {
      delete m_updateStatement;
      m_updateStatement = 0;
    }

    m_module->m_userDatabaseConnection->execute("COMMIT");
  }
}

bool OVIMGenericContext::compose(OVTextBuffer* readingText,
                                 OVTextBuffer* composingText,
                                 OVCandidateService* candidateService,
                                 OVLoaderService* loaderService,
                                 bool sendIfOnlyOneCandidate) {
  string queryString = combineQueryString();

  vector<string> rawResults;

  if (stringContainsWildcard(queryString) && queryString.size() > 1) {
    OVWildcard expression(queryString, m_module->m_cfgMatchOneChar,
                          m_module->m_cfgMatchZeroOrMoreChar);
    vector<pair<string, string> > wcresults =
        m_module->m_dataTable->valuesForKey(expression);

    for (vector<pair<string, string> >::iterator iter = wcresults.begin();
         iter != wcresults.end(); ++iter)
      rawResults.push_back((*iter).second);
  } else {
    if (m_module->m_overrideTable) {
      rawResults = m_module->m_overrideTable->valuesForKey(queryString);
      if (!rawResults.size())
        rawResults = m_module->m_dataTable->valuesForKey(queryString);
    } else
      rawResults = m_module->m_dataTable->valuesForKey(queryString);
  }

  // now we filter out the characters/strings not supported by the current
  // encoding
  vector<string> results;
  OVEncodingService* encodingService = loaderService->encodingService();

  if (m_module->m_cfgUseCharactersSupportedByEncoding.length()) {
    for (vector<string>::iterator rsi = rawResults.begin();
         rsi != rawResults.end(); ++rsi)
      if (encodingService->stringSupportedByEncoding(
              *rsi, m_module->m_cfgUseCharactersSupportedByEncoding))
        results.push_back(*rsi);
  } else {
    for (vector<string>::iterator rsi = rawResults.begin();
         rsi != rawResults.end(); ++rsi)
      if (encodingService->stringSupportedBySystem(*rsi))
        results.push_back(*rsi);
  }

  if (!results.size()) {
    if (sendIfOnlyOneCandidate) loaderService->beep();

    if (m_module->m_cfgClearReadingBufferAtCompositionError &&
        !m_module->m_cfgComposeWhileTyping) {
      m_components.clear();
      readingText->clear();
      readingText->updateDisplay();
      return false;
    }
  } else if (results.size() == 1 && sendIfOnlyOneCandidate) {
    // close candidate window (might be open because of auto-compose mode)
    OVOneDimensionalCandidatePanel* panel =
        candidateService->useOneDimensionalCandidatePanel();

    if (panel->isVisible()) {
      panel->hide();
      panel->cancelEventHandler();
    }

    m_components.clear();
    readingText->clear();
    readingText->updateDisplay();
    m_lastCommittedString = results[0];
    composingText->setText(m_lastCommittedString);
    composingText->commit();
    return true;
  } else {
    // sort the results
    if (m_findOrderStatement) {
      m_findOrderStatement->bindTextToColumn(queryString, 1);
      map<string, bool> orderedResult;
      vector<string> userResults;

      while (m_findOrderStatement->step() == SQLITE_ROW) {
        orderedResult[m_findOrderStatement->textOfColumn(0)] = true;
        userResults.push_back(m_findOrderStatement->textOfColumn(0));
      }

      vector<string> newResults;
      newResults.insert(newResults.end(), userResults.begin(),
                        userResults.end());

      for (vector<string>::iterator iter = results.begin();
           iter != results.end(); ++iter) {
        if (orderedResult.find(*iter) == orderedResult.end())
          newResults.push_back(*iter);
      }

      results = newResults;

      m_findOrderStatement->reset();
    }

    OVOneDimensionalCandidatePanel* panel =
        candidateService->useOneDimensionalCandidatePanel();

    OVCandidateList* candidates = panel->candidateList();
    candidates->setCandidates(results);

    string candidateKeys;
    if (m_module->m_cfgUseSpaceAsFirstCandidateSelectionKey)
      candidateKeys = string(" ") + m_module->m_cfgCandidateSelectionKeys;
    else
      candidateKeys = m_module->m_cfgCandidateSelectionKeys;

    // truncate the candidate keys if such is the case
    if (results.size() < candidateKeys.length()) {
      candidateKeys = candidateKeys.substr(0, results.size());
    }

    panel->setCandidateKeys(candidateKeys, loaderService);

    OVKeyVector nextPageKeys;

    // filters out the space key if the number of candidates exceeds one page
    if (results.size() <= candidateKeys.length() &&
        (m_module->m_cfgUseSpaceAsFirstCandidateSelectionKey ||
         m_module->m_cfgSendFirstCandidateWithSpaceWithOnePageList)) {
      OVKeyVector kvec = panel->defaultNextPageKeys();
      OVKey spaceKey = loaderService->makeOVKey(
          OVKeyCode::Space, false, false, false, false, false, false, false);
      for (OVKeyVector::iterator kvi = kvec.begin(); kvi != kvec.end(); ++kvi)
        if (!(*kvi == spaceKey)) nextPageKeys.push_back(*kvi);
    } else {
      nextPageKeys = panel->defaultNextPageKeys();
    }

    OVKeyVector previousPageKeys = panel->defaultPreviousPageKeys();
    nextPageKeys.push_back(
        loaderService->makeOVKey('>', false, false, false, true));
    previousPageKeys.push_back(
        loaderService->makeOVKey('<', false, false, false, true));
    panel->setNextPageKeys(nextPageKeys);
    panel->setPreviousPageKeys(previousPageKeys);

    if (results.size() <= candidateKeys.size())
      panel->setCandidatesPerPage(results.size());

    panel->updateDisplay();
    panel->show();
    panel->yieldToCandidateEventHandler();
    return true;
  }

  return false;
}

bool OVIMGenericContext::handleBackspace(OVTextBuffer* readingText,
                                         OVTextBuffer* composingText,
                                         OVCandidateService* candidateService,
                                         OVLoaderService* loaderService) {
  if (!m_components.size()) return false;
  m_components.pop_back();

  readingText->setText(combineReading());
  readingText->updateDisplay();

  if (m_module->m_cfgComposeWhileTyping) {
    if (!compose(readingText, composingText, candidateService, loaderService,
                 false)) {
      candidateService->useOneDimensionalCandidatePanel()->cancelEventHandler();
    }
  }

  return true;
}

bool OVIMGenericContext::handleKey(OVKey* key, OVTextBuffer* readingText,
                                   OVTextBuffer* composingText,
                                   OVCandidateService* candidateService,
                                   OVLoaderService* loaderService) {
  // // supports CTRL-ALT-g
  //     if (key->keyCode() == 'g' && key->isAltPressed() &&
  //     key->isCtrlPressed()) {
  //         if (readingText->isEmpty()) {
  //             composingText->setText(m_lastCommittedString);
  //             composingText->commit();
  //             return true;
  //         }
  //     }

  // if it's non-printable key and the composing text is empty, we return the
  // event
  if ((!key->receivedString().size() || key->isNumLockOn() ||
       key->isCombinedFunctionKey()) &&
      readingText->isEmpty())
    return false;

  if ((!key->keyCode() || key->isDirectTextKey()) && readingText->isEmpty() &&
      key->receivedString().size()) {
    m_lastCommittedString = key->receivedString();
    composingText->setText(m_lastCommittedString);
    composingText->commit();
    return true;
  }

  if (key->isCapsLockOn() || key->isNumLockOn() ||
      (key->isShiftPressed() && key->isKeyCodeAlpha())) {
    if (!readingText->isEmpty()) {
      readingText->clear();
      readingText->updateDisplay();
      m_components.clear();
    }

    if (key->receivedString().size() > 1) {
      m_lastCommittedString = key->receivedString();
    } else {
      if (!key->isPrintable()) {
        m_lastCommittedString = "";
        composingText->clear();
        return true;
      }

      // reverse the case that is sent in
      char c = key->keyCode();

      if (c >= 'A' && c <= 'Z')
        c = tolower(c);
      else
        c = toupper(c);

      // OS X doesn't send lower case letters when CAPS LOCK + SHIFT are both
      // on, so here
      if (key->isCapsLockOn() && key->isShiftPressed()) c = toupper(c);

      m_lastCommittedString = string(1, c);
    }
    composingText->setText(m_lastCommittedString);
    composingText->commit();
    return true;
  }

  // so, if we're here, only two conditions are possible:
  // 1. composing text is not empty and it's a non-printable key (return,
  // backspace, escape, and everything else)
  // 2. it's a printable key (regardless the composing text state)

  OVOneDimensionalCandidatePanel* panel =
      candidateService->useOneDimensionalCandidatePanel();

  if (key->receivedString().size() && key->keyCode() != 32 &&
      !key->isDirectTextKey()) {
    if (isValidKey(string(1, key->keyCode()))) {
      if (m_components.size() >= m_module->m_cfgMaximumRadicalLength) {
        loaderService->beep();
        return true;
      }

      m_components.push_back(string(1, key->keyCode()));
    } else {
      if (readingText->isEmpty()) {
        if (key->receivedString().size()) {
          m_lastCommittedString = key->receivedString();
          composingText->setText(m_lastCommittedString);
          composingText->commit();
          return true;
        }

        return false;
      } else {
        readingText->setText(combineReading());
        readingText->updateDisplay();
        loaderService->beep();
        return true;
      }
    }
  } else if (key->keyCode() == OVKeyCode::Return ||
             key->keyCode() == OVKeyCode::Space) {
    if (readingText->isEmpty()) {
      if (key->keyCode() == OVKeyCode::Space) {
        m_lastCommittedString = key->receivedString();
        composingText->setText(m_lastCommittedString);
        composingText->commit();
        return true;
      }

      return false;
    }

    if (compose(readingText, composingText, candidateService, loaderService))
      return true;
  } else if (key->keyCode() == OVKeyCode::Esc) {
    m_components.clear();
    readingText->clear();
    readingText->updateDisplay();
    return true;
  } else if (key->keyCode() == OVKeyCode::Backspace) {
    return handleBackspace(readingText, composingText, candidateService,
                           loaderService);
  } else if (key->isDirectTextKey() && key->receivedString().size()) {
    m_components.clear();
    readingText->clear();
    readingText->updateDisplay();
    m_lastCommittedString = key->receivedString();
    composingText->setText(m_lastCommittedString);
    composingText->commit();
    return true;
  } else {
    loaderService->beep();
  }

  readingText->setText(combineReading());
  readingText->updateDisplay();

  bool excludeEndKey = (m_components.size() > 1) &&
                       (key->keyCode() == m_module->m_cfgMatchOneChar ||
                        key->keyCode() == m_module->m_cfgMatchZeroOrMoreChar);

  if ((isEndKey(key) && !excludeEndKey) ||
      (m_module->m_cfgShouldCommitAtMaximumRadicalLength &&
       m_components.size() >= m_module->m_cfgMaximumRadicalLength)) {
    compose(readingText, composingText, candidateService, loaderService);
  } else {
    if (m_module->m_cfgComposeWhileTyping) {
      compose(readingText, composingText, candidateService, loaderService,
              false);
    }
  }

  return true;
}

void OVIMGenericContext::candidateCanceled(OVCandidateService* candidateService,
                                           OVTextBuffer* readingText,
                                           OVTextBuffer* composingText,
                                           OVLoaderService* loaderService) {
  readingText->clear();
  readingText->updateDisplay();

  m_components.clear();
}

bool OVIMGenericContext::candidateSelected(OVCandidateService* candidateService,
                                           const string& text, size_t index,
                                           OVTextBuffer* readingText,
                                           OVTextBuffer* composingText,
                                           OVLoaderService* loaderService) {
  int count = 0;

  // we increase the dynamic frequency only when the candidate is not the first
  // (what's the use of increasing a candidate's count then it's already on
  // top?)
  if (m_selectStatement && index != 0) {
    m_selectStatement->bindTextToColumn(combineQueryString(), 1);
    m_selectStatement->bindTextToColumn(text, 2);
    if (m_selectStatement->step() == SQLITE_ROW) {
      count = m_selectStatement->intOfColumn(0);
      while (m_selectStatement->step() == SQLITE_ROW)
        ;
    }
    m_selectStatement->reset();

    if (!count && m_insertStatement) {
      m_insertStatement->bindTextToColumn(combineQueryString(), 1);
      m_insertStatement->bindTextToColumn(text, 2);
      m_insertStatement->bindIntToColumn(1, 3);
      int result = m_insertStatement->step();
      m_insertStatement->reset();
    } else if (count && m_updateStatement) {
      count++;
      m_updateStatement->bindIntToColumn(count, 1);
      m_updateStatement->bindTextToColumn(combineQueryString(), 2);
      m_updateStatement->bindTextToColumn(text, 3);
      int result = m_updateStatement->step();
      m_updateStatement->reset();
    }
  }

  m_components.clear();
  readingText->clear();
  readingText->updateDisplay();

  m_lastCommittedString = text;
  composingText->setText(m_lastCommittedString);
  composingText->commit();
  return true;
}

bool OVIMGenericContext::candidateNonPanelKeyReceived(
    OVCandidateService* candidateService, const OVKey* key,
    OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVLoaderService* loaderService) {
  OVOneDimensionalCandidatePanel* panel =
      candidateService->useOneDimensionalCandidatePanel();

  OVCandidateList* candidates = panel->candidateList();
  string currentString = candidates->candidateAtIndex(
      panel->currentHightlightIndexInCandidateList());

  if (key->keyCode() == OVKeyCode::Backspace) {
    panel->hide();
    panel->cancelEventHandler();
    return handleBackspace(readingText, composingText, candidateService,
                           loaderService);
  }

  if (key->keyCode() == OVKeyCode::Space) {
    // if a space key reaches here, it's must because space is not a candidate
    // key or a page-down key
    if (m_module->m_cfgSendFirstCandidateWithSpaceWithOnePageList) {
      panel->hide();
      panel->cancelEventHandler();

      m_components.clear();
      readingText->clear();
      readingText->updateDisplay();
      m_lastCommittedString = panel->candidateList()->candidateAtIndex(0);
      composingText->setText(m_lastCommittedString);
      composingText->commit();
      return true;
    }
  }

  if (currentString.length() && isValidKey(string(1, key->keyCode())) &&
      !key->isDirectTextKey()) {
    if (m_module->m_cfgComposeWhileTyping) {
      if (isValidKey(string(1, key->keyCode())) &&
          m_components.size() < m_module->m_cfgMaximumRadicalLength) {
        m_components.push_back(string(1, key->keyCode()));
        readingText->setText(combineReading());
        readingText->updateDisplay();

        // autocompose
        bool excludeEndKey =
            (m_components.size() > 1) &&
            (key->keyCode() == m_module->m_cfgMatchOneChar ||
             key->keyCode() == m_module->m_cfgMatchZeroOrMoreChar);
        bool hasCandidates = false;

        if ((isEndKey(key) && !excludeEndKey) ||
            (m_module->m_cfgShouldCommitAtMaximumRadicalLength &&
             m_components.size() >= m_module->m_cfgMaximumRadicalLength)) {
          hasCandidates = compose(readingText, composingText, candidateService,
                                  loaderService);
        } else {
          if (m_module->m_cfgComposeWhileTyping) {
            hasCandidates = compose(readingText, composingText,
                                    candidateService, loaderService, false);
          }
        }

        if (!hasCandidates)
          candidateService->useOneDimensionalCandidatePanel()
              ->cancelEventHandler();

        return true;
      } else {
        // skip this part
      }
    }

    m_lastCommittedString = currentString;
    composingText->setText(m_lastCommittedString);
    composingText->commit();

    panel->hide();
    panel->cancelEventHandler();
    m_components.clear();
    m_components.push_back(string(1, key->keyCode()));

    readingText->setText(combineReading());
    readingText->updateDisplay();

    bool excludeEndKey = (m_components.size() > 1) &&
                         (key->keyCode() == m_module->m_cfgMatchOneChar ||
                          key->keyCode() == m_module->m_cfgMatchZeroOrMoreChar);
    bool hasCandidates = false;

    // compose if reaches end key
    if ((isEndKey(key) && !excludeEndKey) ||
        (m_module->m_cfgShouldCommitAtMaximumRadicalLength &&
         m_components.size() >= m_module->m_cfgMaximumRadicalLength)) {
      hasCandidates =
          compose(readingText, composingText, candidateService, loaderService);
    }

    return true;

    /*
            if (!m_module->m_cfgComposeWhileTyping) {
                m_lastCommittedString = currentString;
                composingText->setText(m_lastCommittedString);
                composingText->commit();

                panel->hide();
                panel->cancelEventHandler();
                m_components.clear();
                m_components.push_back(string(1, key->keyCode()));


                readingText->setText(combineReading());
                readingText->updateDisplay();

                bool excludeEndKey = (m_components.size() > 1) &&
       (key->keyCode() == m_module->m_cfgMatchOneChar || key->keyCode() ==
       m_module->m_cfgMatchZeroOrMoreChar); bool hasCandidates = false;

                // compose if reaches end key
                if ((isEndKey(key) && !excludeEndKey) ||
       (m_module->m_cfgShouldCommitAtMaximumRadicalLength && m_components.size()
       >= m_module->m_cfgMaximumRadicalLength)) { hasCandidates =
       compose(readingText, composingText, candidateService, loaderService);
                }

                return true;
            }
            else {
                if (isValidKey(string(1, key->keyCode())) && m_components.size()
       < m_module->m_cfgMaximumRadicalLength) { m_components.push_back(string(1,
       key->keyCode())); readingText->setText(combineReading());
                    readingText->updateDisplay();

                    // autocompose
                    bool excludeEndKey = (m_components.size() > 1) &&
       (key->keyCode() == m_module->m_cfgMatchOneChar || key->keyCode() ==
       m_module->m_cfgMatchZeroOrMoreChar); bool hasCandidates = false;

                    if ((isEndKey(key) && !excludeEndKey) ||
       (m_module->m_cfgShouldCommitAtMaximumRadicalLength && m_components.size()
       >= m_module->m_cfgMaximumRadicalLength)) { hasCandidates =
       compose(readingText, composingText, candidateService, loaderService);
                    }
                    else {
                        if (m_module->m_cfgComposeWhileTyping) {
                            hasCandidates = compose(readingText, composingText,
       candidateService, loaderService, false);
                        }
                    }

                    if (!hasCandidates)
                        candidateService->useOneDimensionalCandidatePanel()->cancelEventHandler();

                    return true;
                }
                else {
                    readingText->setText(combineReading());
                    readingText->updateDisplay();
                    loaderService->beep();
                    return true;
                }
            }
    */
  }

  return false;
}

OVIMGeneric::OVIMGeneric(const string& name, OVDatabaseService* databaseService,
                         const string& tableName)
    : m_name(name),
      m_tableName(tableName.size() ? tableName : name),
      m_databaseService(databaseService),
      m_dataTable(0),
      m_caseSensitive(false),
      m_userDatabaseConnection(0),
      m_cfgMaximumRadicalLength(128),
      m_cfgShouldCommitAtMaximumRadicalLength(false),
      m_cfgClearReadingBufferAtCompositionError(false),
      m_cfgMatchOneChar(0),
      m_cfgMatchZeroOrMoreChar(0),
      m_cfgUseDynamicFrequency(true),
      m_cfgUseSpaceAsFirstCandidateSelectionKey(false),
      m_cfgComposeWhileTyping(false),
      m_cfgSendFirstCandidateWithSpaceWithOnePageList(false),
      m_cfgCandidateSelectionKeys("12345678990"),
      m_overrideTable(0) {
  // set the defaults

  if (OVWildcard::Match(name, "*cj*cin")) {
    m_cfgMaximumRadicalLength = 5;
    m_cfgShouldCommitAtMaximumRadicalLength = false;
    m_cfgClearReadingBufferAtCompositionError = true;
    m_cfgMatchOneChar = '?';
    m_cfgMatchZeroOrMoreChar = '*';
    m_cfgSendFirstCandidateWithSpaceWithOnePageList = true;
  } else if (OVWildcard::Match(name, "*simplex*cin")) {
    m_cfgMaximumRadicalLength = 2;
    m_cfgShouldCommitAtMaximumRadicalLength = true;
    m_cfgClearReadingBufferAtCompositionError = true;
    m_cfgUseDynamicFrequency = false;
  } else if (OVWildcard::Match(name, "*bpmf*cin")) {
    m_cfgMaximumRadicalLength = 4;
    m_cfgShouldCommitAtMaximumRadicalLength = true;
    m_cfgClearReadingBufferAtCompositionError = false;
  }

  string selkeys;
  selkeys = databaseService->valueForPropertyInTable("selkey", m_tableName);
  if (selkeys.length()) m_cfgCandidateSelectionKeys = selkeys;

  string ename, cname, tcname, scname;

  ename = databaseService->valueForPropertyInTable("ename", m_tableName);
  if (!ename.length()) ename = m_name;

  cname = databaseService->valueForPropertyInTable("cname", m_tableName);
  if (!cname.length()) cname = ename;

  tcname = databaseService->valueForPropertyInTable("tcname", m_tableName);
  if (!tcname.length()) tcname = cname;

  scname = databaseService->valueForPropertyInTable("scname", m_tableName);
  if (!scname.length()) scname = cname;

  m_localizedNames["en"] = ename;
  m_localizedNames["zh_TW"] = tcname;
  m_localizedNames["zh-Hant"] = tcname;
  m_localizedNames["zh_CN"] = scname;
  m_localizedNames["zh-Hans"] = scname;
}

OVIMGeneric::~OVIMGeneric() {
  if (m_dataTable) delete m_dataTable;

  if (m_overrideTable) delete m_overrideTable;
}

OVEventHandlingContext* OVIMGeneric::createContext() {
  return m_dataTable ? new OVIMGenericContext(this) : 0;
}

const string OVIMGeneric::identifier() const { return m_name; }

const string OVIMGeneric::localizedName(const string& locale) {
  map<string, string>::iterator iter = m_localizedNames.find(locale);
  return iter == m_localizedNames.end() ? m_localizedNames["en"]
                                        : (*iter).second;
}

bool OVIMGeneric::initialize(OVPathInfo* pathInfo,
                             OVLoaderService* loaderService) {
  bool caseSensitive = OVWildcard::Match(m_name, "*-casesensitive*");
  m_dataTable = m_databaseService->createKeyValueDataTableInterface(
      m_tableName, caseSensitive);

  if (m_dataTable) {
    // loaderService->logger(OVIMGENERIC_IDENTIFIER_PREFIX) << "successfully got
    // the table, cname = " << m_dataTable->valueForProperty("cname") << endl;
  }

  if (OVDirectoryHelper::CheckDirectory(pathInfo->writablePath)) {
    m_userDatabasePath = OVPathHelper::PathCat(
        pathInfo->writablePath,
        string(identifier()) + "-DynamicCandidateOrder.sqlite3");
    // loaderService->logger(OVIMGENERIC_IDENTIFIER_PREFIX) << "wrtiteable table
    // at: " << m_userDatabasePath << endl;
  }

  return !!m_dataTable;
}

void OVIMGeneric::finalize() {}

void OVIMGeneric::loadConfig(OVKeyValueMap* moduleConfig,
                             OVLoaderService* loaderService) {
  // loaderService->logger(OVIMGENERIC_IDENTIFIER_PREFIX) << "Load config" <<
  // endl;

  if (moduleConfig->hasKey("MaximumRadicalLength")) {
    size_t length =
        (size_t)moduleConfig->intValueForKey("MaximumRadicalLength");
    m_cfgMaximumRadicalLength = length ? length : m_cfgMaximumRadicalLength;
  }

  if (moduleConfig->hasKey("ShouldCommitAtMaximumRadicalLength"))
    m_cfgShouldCommitAtMaximumRadicalLength =
        moduleConfig->isKeyTrue("ShouldCommitAtMaximumRadicalLength");

  if (moduleConfig->hasKey("ClearReadingBufferAtCompositionError"))
    m_cfgClearReadingBufferAtCompositionError =
        moduleConfig->isKeyTrue("ClearReadingBufferAtCompositionError");

  if (moduleConfig->hasKey("MatchOneChar"))
    m_cfgMatchOneChar = moduleConfig->stringValueForKey("MatchOneChar")[0];

  if (moduleConfig->hasKey("MatchZeroOrMoreChar"))
    m_cfgMatchZeroOrMoreChar =
        moduleConfig->stringValueForKey("MatchZeroOrMoreChar")[0];

  if (moduleConfig->hasKey("UseDynamicFrequency")) {
    m_cfgUseDynamicFrequency = moduleConfig->isKeyTrue("UseDynamicFrequency");
  }

  if (moduleConfig->hasKey("UseSpaceAsFirstCandidateSelectionKey")) {
    m_cfgUseSpaceAsFirstCandidateSelectionKey =
        moduleConfig->isKeyTrue("UseSpaceAsFirstCandidateSelectionKey");
  }

  if (moduleConfig->hasKey("ComposeWhileTyping")) {
    m_cfgComposeWhileTyping = moduleConfig->isKeyTrue("ComposeWhileTyping");
  }

  if (moduleConfig->hasKey("SendFirstCandidateWithSpaceWithOnePageList")) {
    m_cfgSendFirstCandidateWithSpaceWithOnePageList =
        moduleConfig->isKeyTrue("SendFirstCandidateWithSpaceWithOnePageList");
  }

  if (moduleConfig->hasKey("UseCharactersSupportedByEncoding")) {
    m_cfgUseCharactersSupportedByEncoding =
        moduleConfig->stringValueForKey("UseCharactersSupportedByEncoding");
  }

  if (OVWildcard::Match(m_name, "*ehq*cin")) {
    // ehq requires encoding override, so we do this
    m_cfgUseCharactersSupportedByEncoding = "UTF-8";
  }

  if (!loaderService->encodingService()->isEncodingSupported(
          m_cfgUseCharactersSupportedByEncoding))
    m_cfgUseCharactersSupportedByEncoding = "";

  if (m_cfgUseDynamicFrequency && !m_userDatabaseConnection) {
    m_userDatabaseConnection = OVSQLiteConnection::Open(m_userDatabasePath);
    // loaderService->logger(OVIMGENERIC_IDENTIFIER_PREFIX) << "opening database
    // connection: " << (m_userDatabasePath.size() ? "done" : "failed") << endl;
  } else if (!m_cfgUseDynamicFrequency && m_userDatabaseConnection) {
    delete m_userDatabaseConnection;
    m_userDatabaseConnection = 0;
  }

  if (moduleConfig->hasKey("UseOverrideTable")) {
    string tableName = moduleConfig->stringValueForKey("UseOverrideTable");

    if (!tableName.size()) {
      if (m_overrideTable) {
        delete m_overrideTable;
        m_overrideTable = 0;
      }
    } else if (tableName.size() && tableName != m_cfgUseOverrideTable) {
      bool caseSensitive = OVWildcard::Match(m_name, "*-casesensitive*");
      OVKeyValueDataTableInterface* table =
          m_databaseService->createKeyValueDataTableInterface(tableName,
                                                              caseSensitive);

      if (m_overrideTable) {
        delete m_overrideTable;
        m_overrideTable = 0;
      }

      if (table) {
        m_overrideTable = table;
      } else {
        tableName = "";
      }
    }

    m_cfgUseOverrideTable = tableName;
  }
}

void OVIMGeneric::saveConfig(OVKeyValueMap* moduleConfig,
                             OVLoaderService* loaderService) {
  // loaderService->logger(OVIMGENERIC_IDENTIFIER_PREFIX) << "Save config" <<
  // endl;

  moduleConfig->setKeyIntValue("MaximumRadicalLength",
                               (int)m_cfgMaximumRadicalLength);
  moduleConfig->setKeyBoolValue("ShouldCommitAtMaximumRadicalLength",
                                m_cfgShouldCommitAtMaximumRadicalLength);
  moduleConfig->setKeyBoolValue("ClearReadingBufferAtCompositionError",
                                m_cfgClearReadingBufferAtCompositionError);
  moduleConfig->setKeyStringValue(
      "MatchOneChar",
      m_cfgMatchOneChar ? string(1, m_cfgMatchOneChar) : string());
  moduleConfig->setKeyStringValue("MatchZeroOrMoreChar",
                                  m_cfgMatchZeroOrMoreChar
                                      ? string(1, m_cfgMatchZeroOrMoreChar)
                                      : string());
  moduleConfig->setKeyBoolValue("UseDynamicFrequency",
                                m_cfgUseDynamicFrequency);
  moduleConfig->setKeyStringValue("UseCharactersSupportedByEncoding",
                                  m_cfgUseCharactersSupportedByEncoding);
  moduleConfig->setKeyBoolValue("UseSpaceAsFirstCandidateSelectionKey",
                                m_cfgUseSpaceAsFirstCandidateSelectionKey);
  moduleConfig->setKeyBoolValue("ComposeWhileTyping", m_cfgComposeWhileTyping);
  moduleConfig->setKeyStringValue("UseOverrideTable", m_cfgUseOverrideTable);
  moduleConfig->setKeyBoolValue(
      "SendFirstCandidateWithSpaceWithOnePageList",
      m_cfgSendFirstCandidateWithSpaceWithOnePageList);
}

};  // namespace OpenVanilla
