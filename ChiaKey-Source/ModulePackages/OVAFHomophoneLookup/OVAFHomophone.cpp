//
// OVAFHomophone.cpp
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

#include "OVAFHomophone.h"

namespace OpenVanilla {

OVAFHomophoneContext::OVAFHomophoneContext(OVAFHomophone* module)
    : m_module(module) {
  m_selectPhone = m_module->m_bpmfDB->prepare(
      "SELECT key from 'Manjusri-bpmf-cin' where value = ?");
  m_selectCandidates = m_module->m_bpmfDB->prepare(
      "SELECT value from 'Manjusri-bpmf-cin' where key = ?");
  m_logging = false;
}

OVAFHomophoneContext::~OVAFHomophoneContext() {
  if (m_selectPhone) delete m_selectPhone;
  if (m_selectCandidates) delete m_selectCandidates;
}

void OVAFHomophoneContext::startSession(OVLoaderService* loaderService) {
  m_candidates.clear();
}

bool OVAFHomophoneContext::handleKey(OVKey* key, OVTextBuffer* readingText,
                                     OVTextBuffer* composingText,
                                     OVCandidateService* candidateService,
                                     OVLoaderService* loaderService) {
  if (key->keyCode() == '`' && !m_logging) {
    loaderService->notify("Start Logging");
    m_logging = true;
    m_selectedPhone = "";
    m_isSelectingPhone = false;
    m_isSelectingCandidate = false;
    return true;
  }
  return false;
}

bool OVAFHomophoneContext::candidateSelected(
    OVCandidateService* candidateService, const string& text, size_t index,
    OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVLoaderService* loaderService) {
  if (m_isSelectingPhone) {
    m_selectedPhone = m_phones[index];
    string bpmf =
        BPMF::FromAbsoluteOrderString(m_selectedPhone).composedString();
    composingText->setText(bpmf);
    composingText->commit();
    m_isSelectingPhone = false;
    return true;
  }
  if (m_isSelectingCandidate) {
    string s = m_candidates[index];
    composingText->setText(s);
    composingText->commit();
    m_isSelectingCandidate = false;
    m_logging = false;
    return true;
  }
  return true;
}

void OVAFHomophoneContext::showCandidatePanel(
    const string& text, OVCandidateService* candidateService,
    OVLoaderService* loaderService) {
  m_logging = false;

  OVOneDimensionalCandidatePanel* panel =
      candidateService->useVerticalCandidatePanel();

  m_candidates.clear();
  m_selectCandidates->reset();
  m_selectCandidates->bindTextToColumn(text, 1);

  OVEncodingService* encodingService = loaderService->encodingService();

  if (m_module->m_cfgUseCharactersSupportedByEncoding.length()) {
    while (m_selectCandidates->step() == SQLITE_ROW) {
      string candidate = m_selectCandidates->textOfColumn(0);
      if (encodingService->stringSupportedByEncoding(
              candidate, m_module->m_cfgUseCharactersSupportedByEncoding))
        m_candidates.push_back(candidate);
    }
  } else {
    while (m_selectCandidates->step() == SQLITE_ROW) {
      string candidate = m_selectCandidates->textOfColumn(0);
      if (encodingService->stringSupportedBySystem(candidate))
        m_candidates.push_back(candidate);
    }
  }

  if (m_candidates.size()) {
    panel->candidateList()->clear();
    panel->candidateList()->addCandidates(m_candidates);
    panel->setCandidatesPerPage(9);
    panel->updateDisplay();
    panel->show();
    panel->yieldToCandidateEventHandler();
    m_isSelectingCandidate = true;
  }
}

void OVAFHomophoneContext::showPhonePanel(OVCandidateService* candidateService,
                                          OVLoaderService* loaderService) {
  OVOneDimensionalCandidatePanel* panel =
      candidateService->useVerticalCandidatePanel();
  if (m_phones.size()) {
    panel->candidateList()->clear();
    int i;
    for (i = 0; i < m_phones.size(); i++) {
      string bpmf = BPMF::FromAbsoluteOrderString(m_phones[i]).composedString();
      panel->candidateList()->addCandidate(bpmf);
    }
    panel->setCandidatesPerPage(8);
    panel->updateDisplay();
    panel->show();
    panel->yieldToCandidateEventHandler();
    m_isSelectingPhone = true;
  }
}

bool OVAFHomophoneContext::handleDirectText(
    const string& text, OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVCandidateService* candidateService, OVLoaderService* loaderService) {
  if (m_logging) {
    if (m_selectedPhone.size()) {
      showCandidatePanel(m_selectedPhone, candidateService, loaderService);
      m_selectedPhone = "";
      composingText->setText(text);
      composingText->updateDisplay();
      return true;
    }

    vector<string> chrs = OVUTF8Helper::SplitStringByCodePoint(text);

    loaderService->logger("OVAFHomophone")
        << "handleDirectText: " << text << " (" << chrs.size() << ")" << endl;

    if (!chrs.size()) return false;

    string chr = chrs[chrs.size() - 1];
    m_phones.clear();
    m_selectPhone->reset();
    m_selectPhone->bindTextToColumn(chr, 1);

#warning This will cause memory leak
    while (m_selectPhone->step() == SQLITE_ROW) {
      string phone = m_selectPhone->textOfColumn(0);
      if (OVWildcard::Match(phone, "_punctuation_*")) {
        m_logging = false;
        m_selectedPhone = "";
        m_isSelectingPhone = false;
        m_isSelectingCandidate = false;
        return false;
      }
      m_phones.push_back(phone);
    }
    if (m_phones.size()) {
      if (m_phones.size() == 1) {
        showCandidatePanel(m_phones[0], candidateService, loaderService);
      } else {
        showPhonePanel(candidateService, loaderService);
      }
    }
  }

  composingText->setText(text);
  composingText->commit();
  return true;
}

OVAFHomophone::OVAFHomophone() : m_bpmfDB(0) {}

OVAFHomophone::~OVAFHomophone() {
  if (m_bpmfDB) delete m_bpmfDB;
}

OVEventHandlingContext* OVAFHomophone::createContext() {
  return new OVAFHomophoneContext(this);
}

const string OVAFHomophone::identifier() const {
  return string("OVAFHomophone");
}

const string OVAFHomophone::localizedName(const string& locale) {
#ifndef _MSC_VER
  string tcname = "同音字查詢";
  string scname = "同音字查询";
#else
  string tcname =
      "\xE5\x90\x8C\xE9\x9F\xB3\xE5\xAD\x97\xE6\x9F\xA5\xE8\xA9\xA2";
  string scname =
      "\xE5\x90\x8C\xE9\x9F\xB3\xE5\xAD\x97\xE6\x9F\xA5\xE8\xAF\xA2";
#endif
  if (locale == "zh_TW" || locale == "zh-hant")
    return tcname;
  else if (locale == "zh_CN" || locale == "zh-hans")
    return scname;
  else
    return string("Homophone");
}

bool OVAFHomophone::initialize(OVPathInfo* pathInfo,
                               OVLoaderService* loaderService) {
  string dbPath;
  dbPath = OVPathHelper::PathCat(pathInfo->resourcePath, "BPMF.db");
  if (!OVPathHelper::PathExists(dbPath)) {
    loaderService->logger("OVAFHomophone")
        << "db not found: " << dbPath << endl;
    return false;
  }
  m_bpmfDB = OVSQLiteConnection::Open(dbPath);
  return true;
}

void OVAFHomophone::loadConfig(OVKeyValueMap* moduleConfig,
                               OVLoaderService* loaderService) {
  loaderService->logger("OVAFHomophone") << "Load config" << endl;
  if (moduleConfig->hasKey("UseCharactersSupportedByEncoding")) {
    m_cfgUseCharactersSupportedByEncoding =
        moduleConfig->stringValueForKey("UseCharactersSupportedByEncoding");
  }
}

void OVAFHomophone::saveConfig(OVKeyValueMap* moduleConfig,
                               OVLoaderService* loaderService) {
  loaderService->logger("OVAFHomophone") << "Save config" << endl;
  moduleConfig->setKeyStringValue("UseCharactersSupportedByEncoding",
                                  m_cfgUseCharactersSupportedByEncoding);
}

void OVAFHomophone::finalize() {}

};  // namespace OpenVanilla
