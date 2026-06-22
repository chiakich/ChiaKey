//
// OVIMTraditionalMandarin.cpp
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

#include "OVIMTraditionalMandarin.h"

namespace OpenVanilla {

using namespace Formosa::Mandarin;

OVIMTraditionalMandarinContext::OVIMTraditionalMandarinContext(
    OVIMTraditionalMandarin* module)
    : m_module(module), m_readingBuffer(0) {}

const BopomofoKeyboardLayout*
OVIMTraditionalMandarinContext::currentKeyboardLayout() {
  const BopomofoKeyboardLayout* layout =
      BopomofoKeyboardLayout::LayoutForName(m_module->m_cfgKeyboardLayout);

  if (!layout) layout = BopomofoKeyboardLayout::StandardLayout();

  return layout;
}

const string OVIMTraditionalMandarinContext::readingQueryString() {
#ifndef OVIMTRADITIONALMANDARIN_USE_ABSOLUTE_ORDER_QUERY_STRING
  return m_readingBuffer.standardLayoutQueryString();
#else
  if (m_module->m_useStandardLayoutQueryString)
    return m_readingBuffer.standardLayoutQueryString();

  return m_readingBuffer.absoluteOrderQueryString();
#endif
}

void OVIMTraditionalMandarinContext::startSession(
    OVLoaderService* loaderService) {
  m_readingBuffer.setKeyboardLayout(currentKeyboardLayout());
  m_readingBuffer.clear();
}

void OVIMTraditionalMandarinContext::stopSession(
    OVLoaderService* loaderService) {
  m_readingBuffer.clear();
}

bool OVIMTraditionalMandarinContext::queryAndCompose(
    OVKeyValueDataTableInterface* dataTable, const string& queryString,
    OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVCandidateService* candidateService, OVLoaderService* loaderService) {
  vector<string> rawResults = dataTable->valuesForKey(queryString);

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
    // if m_readingBuffer is empty, it's not querying a BPMF
    if (m_readingBuffer.isEmpty()) return false;

    loaderService->beep();
    readingText->updateDisplay();
    return true;
  } else if (results.size() == 1) {
    m_readingBuffer.clear();
    readingText->clear();
    readingText->updateDisplay();
    composingText->setText(results[0]);
    composingText->commit();
    return true;
  } else {
    readingText->setText(results[0]);
    readingText->updateDisplay();

    OVOneDimensionalCandidatePanel* panel =
        candidateService->useOneDimensionalCandidatePanel();
    OVCandidateList* candidates = panel->candidateList();
    candidates->setCandidates(results);

    OVKeyVector nextPageKeys = panel->defaultNextPageKeys();
    OVKeyVector previousPageKeys = panel->defaultPreviousPageKeys();
    nextPageKeys.push_back(
        loaderService->makeOVKey('>', false, false, false, true));
    previousPageKeys.push_back(
        loaderService->makeOVKey('<', false, false, false, true));
    panel->setNextPageKeys(nextPageKeys);
    panel->setPreviousPageKeys(previousPageKeys);

    if (m_module->m_cfgCandidateSelectionKeys.size()) {
      panel->setCandidateKeys(m_module->m_cfgCandidateSelectionKeys,
                              loaderService);
      panel->setCandidatesPerPage(m_module->m_cfgCandidateSelectionKeys.size());
    } else {
      const BopomofoKeyboardLayout* layout =
          BopomofoKeyboardLayout::LayoutForName(m_module->m_cfgKeyboardLayout);

      if (layout == BopomofoKeyboardLayout::HsuLayout()) {
        panel->setCandidateKeys("asdfzxcv", loaderService);
        panel->setCandidatesPerPage(8);
      } else if (layout == BopomofoKeyboardLayout::ETen26Layout()) {
        panel->setCandidateKeys("asdfjkl;", loaderService);
        panel->setCandidatesPerPage(8);
      } else {
        panel->setCandidateKeys("123456789", loaderService);
        panel->setCandidatesPerPage(9);
      }
    }

    panel->updateDisplay();
    panel->show();
    panel->yieldToCandidateEventHandler();
    return true;
  }

  return false;
}

bool OVIMTraditionalMandarinContext::handleBackspace(
    OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVCandidateService* candidateService, OVLoaderService* loaderService) {
  if (m_readingBuffer.isEmpty()) {
    if (!readingText->isEmpty()) {
      // no BPMF reading, but composing buffer not empty... it's punctuation
      readingText->clear();
      readingText->updateDisplay();
      composingText->clear();
      composingText->updateDisplay();
    }

    return false;
  }

  m_readingBuffer.backspace();
  readingText->setText(m_readingBuffer.composedString());
  readingText->updateDisplay();
  return true;
}

bool OVIMTraditionalMandarinContext::handleKey(
    OVKey* key, OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVCandidateService* candidateService, OVLoaderService* loaderService) {
  // loaderService->logger("OVIMTraditionalMandarinContext") << "received key
  // code: " << key->keyCode()
  //     << ", string: " << key->receivedString() << endl;

  // handles ctrl-opt-[x] or ctrl-alt-[x]
  if (key->isCtrlPressed() && (key->isOptPressed() || key->isAltPressed()) &&
      readingText->isEmpty())
    return queryAndCompose(m_module->m_punctuationTable,
                           string("_ctrl_opt_") + key->receivedString(),
                           readingText, composingText, candidateService,
                           loaderService);

  // handles ctrl-0 and ctrl-1
  if ((key->receivedString() == "0" || key->receivedString() == "1") &&
      key->isCtrlPressed() && readingText->isEmpty())
    return queryAndCompose(m_module->m_punctuationTable,
                           string("_punctuation_list"), readingText,
                           composingText, candidateService, loaderService);

  // if it's non-printable key and the composing text is empty, we return the
  // event
  if (!key->receivedString().size() && readingText->isEmpty()) {
    return false;
  }

  if ((!key->keyCode() || key->isDirectTextKey()) && readingText->isEmpty() &&
      key->receivedString().size()) {
    composingText->setText(key->receivedString());
    composingText->commit();
    return true;
  }

  // filter out combined function keys
  if (key->isCombinedFunctionKey() || key->isNumLockOn()) {
    if (readingText->isEmpty())
      return false;
    else {
      loaderService->beep();
      return true;
    }
  }

  // CAPS LOCK or SHIFT
  if (key->isCapsLockOn() || key->isNumLockOn() ||
      (key->isShiftPressed() && key->isKeyCodeAlpha())) {
    if (!readingText->isEmpty()) {
      readingText->clear();
      readingText->updateDisplay();
      m_readingBuffer.clear();
    }

    if (key->receivedString().size() > 1) {
      composingText->setText(key->receivedString());
    } else {
      // reverse the case that is sent in
      char c = key->keyCode();

      if (c >= 'A' && c <= 'Z')
        c = tolower(c);
      else
        c = toupper(c);

      // OS X doesn't send lower case letters when CAPS LOCK + SHIFT are both
      // on, so here
      if (key->isCapsLockOn() && key->isShiftPressed()) c = toupper(c);

      composingText->setText(string(1, c));
    }
    composingText->commit();
    return true;
  }

  // so, if we're here, only two conditions are possible:
  // 1. composing text is not empty and it's a non-printable key (return,
  // backspace, escape, and everything else)
  // 2. it's a printable key (regardless the composing text state)

  OVOneDimensionalCandidatePanel* panel =
      candidateService->useVerticalCandidatePanel();

  // see if it's the compose key
  if (key->keyCode() == OVKeyCode::Space ||
      key->keyCode() == OVKeyCode::Return) {
    if (m_readingBuffer.isEmpty()) {
      if (key->keyCode() == OVKeyCode::Space) {
        composingText->setText(key->receivedString());
        composingText->commit();
        return true;
      }

      return false;
    }

    return queryAndCompose(m_module->m_BPMFTable, readingQueryString(),
                           readingText, composingText, candidateService,
                           loaderService);
  }

  if (key->keyCode() == OVKeyCode::Esc) {
    if (m_readingBuffer.isEmpty()) return false;

    m_readingBuffer.clear();
    readingText->clear();
    readingText->updateDisplay();
    return true;
  }

  if (key->keyCode() == OVKeyCode::Backspace)
    return handleBackspace(readingText, composingText, candidateService,
                           loaderService);

  if (key->keyCode() && m_readingBuffer.combineKey(key->keyCode()) &&
      !key->isDirectTextKey()) {
    readingText->setText(m_readingBuffer.composedString());
    readingText->updateDisplay();

    if (m_readingBuffer.hasToneMarker())
      return queryAndCompose(m_module->m_BPMFTable, readingQueryString(),
                             readingText, composingText, candidateService,
                             loaderService);

    return true;
  }

  if (key->isDirectTextKey() && key->receivedString().size()) {
    m_readingBuffer.clear();
    readingText->clear();
    readingText->updateDisplay();
    composingText->setText(key->receivedString());
    composingText->commit();
    return true;
  }

  if (!m_readingBuffer.isEmpty()) {
    loaderService->beep();
    return true;
  }

  pair<bool, string> hasPunctuation;
  hasPunctuation = findPunctuationKey(key->receivedString());
  if (hasPunctuation.first)
    return queryAndCompose(m_module->m_punctuationTable, hasPunctuation.second,
                           readingText, composingText, candidateService,
                           loaderService);

  if (key->receivedString().size()) {
    composingText->setText(key->receivedString());
    composingText->commit();
    return true;
  }

  return false;
}

void OVIMTraditionalMandarinContext::candidateCanceled(
    OVCandidateService* candidateService, OVTextBuffer* readingText,
    OVTextBuffer* composingText, OVLoaderService* loaderService) {
  readingText->clear();
  readingText->updateDisplay();
  m_readingBuffer.clear();
}

bool OVIMTraditionalMandarinContext::candidateSelected(
    OVCandidateService* candidateService, const string& text, size_t index,
    OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVLoaderService* loaderService) {
  m_readingBuffer.clear();
  readingText->clear();
  readingText->updateDisplay();
  composingText->setText(text);
  composingText->commit();
  return true;
}

bool OVIMTraditionalMandarinContext::candidateNonPanelKeyReceived(
    OVCandidateService* candidateService, const OVKey* key,
    OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVLoaderService* loaderService) {
  OVOneDimensionalCandidatePanel* panel =
      candidateService->useOneDimensionalCandidatePanel();
  pair<bool, string> hasPunctuation;

  OVCandidateList* candidates = panel->candidateList();
  string currentString = candidates->candidateAtIndex(
      panel->currentHightlightIndexInCandidateList());

  if (key->keyCode() == OVKeyCode::Backspace) {
    panel->hide();
    panel->cancelEventHandler();
    return handleBackspace(readingText, composingText, candidateService,
                           loaderService);
  }

  if (currentString.length()) {
    // is it a valid key or a valid puncuation?
    int state = 0;

    if (m_readingBuffer.isValidKey(key->keyCode()) && !key->isDirectTextKey())
      state = 1;
    else {
      hasPunctuation = findPunctuationKey(key->receivedString());
      if (hasPunctuation.first) state = 2;
    }

    if (state) {
      // commit the key first
      composingText->setText(currentString);
      composingText->commit();
      panel->hide();
      panel->cancelEventHandler();

      if (state == 1) {
        m_readingBuffer.clear();
        m_readingBuffer.combineKey(key->keyCode());
        readingText->setText(m_readingBuffer.composedString());
        readingText->updateDisplay();
      } else if (state == 2) {
        m_readingBuffer.clear();
        readingText->clear();
        readingText->updateDisplay();
        queryAndCompose(m_module->m_punctuationTable, hasPunctuation.second,
                        readingText, composingText, candidateService,
                        loaderService);
      }

      return true;
    }
  }

  return false;
}

pair<bool, string> OVIMTraditionalMandarinContext::findPunctuationKey(
    const string& keyString) {
  string prefix = "_punctuation_";
  string withLayout =
      prefix + currentKeyboardLayout()->name() + "_" + keyString;
  string punctuation = prefix + keyString;

  vector<string> results =
      m_module->m_punctuationTable->valuesForKey(withLayout);
  if (results.size()) return pair<bool, string>(true, withLayout);

  results = m_module->m_punctuationTable->valuesForKey(punctuation);
  if (results.size()) return pair<bool, string>(true, punctuation);

  return pair<bool, string>(false, "");
}

OVIMTraditionalMandarin::OVIMTraditionalMandarin()
    : m_punctuationTable(0),
      m_BPMFTable(0),
      m_useStandardLayoutQueryString(false) {}

OVIMTraditionalMandarin::~OVIMTraditionalMandarin() {
  if (m_punctuationTable && m_punctuationTable != m_BPMFTable)
    delete m_punctuationTable;

  if (m_BPMFTable) delete m_BPMFTable;
}

OVEventHandlingContext* OVIMTraditionalMandarin::createContext() {
  return new OVIMTraditionalMandarinContext(this);
}

const string OVIMTraditionalMandarin::identifier() const {
  return string(OVIMTRADITIONALMANDARIN_IDENTIFIER);
}

const string OVIMTraditionalMandarin::localizedName(const string& locale) {
#ifndef _MSC_VER
  string tcname = "傳統注音";
  string scname = "传统繁体注音";
#else
  string tcname = "\xe5\x82\xb3\xe7\xb5\xb1\xe6\xb3\xa8\xe9\x9f\xb3";
  string scname =
      "\xe4\xbc\xa0\xe7\xbb\x9f\xe7\xb9\x81\xe4\xbd\x93\xe6\xb3\xa8\xe9\x9f"
      "\xb3";
#endif

  if (locale == "zh_TW" || locale == "zh-Hant")
    return tcname;
  else if (locale == "zh_CN" || locale == "zh-Hans")
    return scname;
  else
    return string("Traditional Mandarin (Bopomofo)");
}

bool OVIMTraditionalMandarin::initialize(OVPathInfo* pathInfo,
                                         OVLoaderService* loaderService) {
  vector<OVDatabaseService*> databases;

  if (loaderService->CINDatabaseService())
    databases.push_back(loaderService->CINDatabaseService());

  if (loaderService->SQLiteDatabaseService())
    databases.push_back(loaderService->SQLiteDatabaseService());

  for (vector<OVDatabaseService*>::iterator iter = databases.begin();
       iter != databases.end(); ++iter) {
    vector<string> tables;

    tables = (*iter)->tables();

    if (!m_punctuationTable) {
      tables = (*iter)->tables(
          string(OVIMMANDARIN_PUNCTUATIONS_TABLE_PREFIX "-punctuation*"));
      if (tables.size())
        m_punctuationTable =
            (*iter)->createKeyValueDataTableInterface(tables[0]);
    }

    if (!m_BPMFTable) {
      tables = (*iter)->tables(string(OVIMMANDARIN_DATA_TABLE_PREFIX "-bpmf*"));
      if (tables.size())
        m_BPMFTable = (*iter)->createKeyValueDataTableInterface(tables[0]);
    }
  }

  for (vector<OVDatabaseService*>::iterator iter = databases.begin();
       iter != databases.end(); ++iter) {
    vector<string> tables;

    if (!m_punctuationTable) {
      tables = (*iter)->tables(string("bpmf-punctuation*"));
      if (tables.size())
        m_punctuationTable =
            (*iter)->createKeyValueDataTableInterface(tables[0]);
    }

    if (!m_BPMFTable) {
      tables = (*iter)->tables(string("bpmf-cin"));
      if (!tables.size()) tables = (*iter)->tables(string("bpmf*"));

      for (vector<string>::iterator table = tables.begin();
           table != tables.end() && !m_BPMFTable; ++table) {
        if ((*table).find("punctuation") != string::npos) continue;
        m_BPMFTable = (*iter)->createKeyValueDataTableInterface(*table);
        if (m_BPMFTable) m_useStandardLayoutQueryString = true;
      }
    }
  }

  if (!m_punctuationTable) m_punctuationTable = m_BPMFTable;

  if (!m_punctuationTable || !m_BPMFTable) {
    // loaderService->logger("OVIMTraditionalMandarin") << "Cannot get the
    // proper table." << endl;
    return false;
  }

  // loaderService->logger("OVIMTraditionalMandarin") <<
  // "OVIMTraditionalMandarin initialized." << endl;
  return true;
}

void OVIMTraditionalMandarin::finalize() {}

void OVIMTraditionalMandarin::loadConfig(OVKeyValueMap* moduleConfig,
                                         OVLoaderService* loaderService) {
  if (moduleConfig->hasKey("KeyboardLayout"))
    m_cfgKeyboardLayout = moduleConfig->stringValueForKey("KeyboardLayout");
  else
    m_cfgKeyboardLayout = "Standard";

  if (moduleConfig->hasKey("CandidateSelectionKeys"))
    m_cfgCandidateSelectionKeys =
        moduleConfig->stringValueForKey("CandidateSelectionKeys");
  else
    m_cfgCandidateSelectionKeys = "";

  // loaderService->logger("OVIMTraditionalMandarin") << "Using keyboard layout
  // " << m_cfgKeyboardLayout << endl;

  if (moduleConfig->hasKey("UseCharactersSupportedByEncoding")) {
    m_cfgUseCharactersSupportedByEncoding =
        moduleConfig->stringValueForKey("UseCharactersSupportedByEncoding");
  }

  if (m_cfgUseCharactersSupportedByEncoding.length())
    if (!loaderService->encodingService()->isEncodingSupported(
            m_cfgUseCharactersSupportedByEncoding))
      m_cfgUseCharactersSupportedByEncoding = "";
}

void OVIMTraditionalMandarin::saveConfig(OVKeyValueMap* moduleConfig,
                                         OVLoaderService* loaderService) {
  moduleConfig->setKeyStringValue("KeyboardLayout", m_cfgKeyboardLayout);
  moduleConfig->setKeyStringValue("UseCharactersSupportedByEncoding",
                                  m_cfgUseCharactersSupportedByEncoding);

  if (m_cfgCandidateSelectionKeys.size())
    moduleConfig->setKeyStringValue("CandidateSelectionKeys",
                                    m_cfgCandidateSelectionKeys);
}

};  // namespace OpenVanilla
