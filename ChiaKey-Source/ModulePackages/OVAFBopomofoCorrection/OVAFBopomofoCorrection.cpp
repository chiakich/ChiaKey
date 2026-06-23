//
// OVAFBopomofoCorrection.cpp
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

#include "OVAFBopomofoCorrection.h"

namespace OpenVanilla {

OVAFBopomofoCorrectionContext::OVAFBopomofoCorrectionContext(
    OVKeyValueDataTableInterface* dataTable)
    : m_dataTable(dataTable) {}

bool OVAFBopomofoCorrectionContext::handleKey(
    OVKey* key, OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVCandidateService* candidateService, OVLoaderService* loaderService) {
  return false;
}

bool OVAFBopomofoCorrectionContext::handleDirectText(
    const string& text, OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVCandidateService* candidateService, OVLoaderService* loaderService) {
  /*
#ifndef _MSC_VER
const char* tcMsg = "啟動注音文校正功能";
const char* scMsg = "启动注音文校正功能";
#else
const char* tcMsg =
"\xE5\x95\x9F\xE5\x8B\x95\xE6\xB3\xA8\xE9\x9F\xB3\xE6\x96\x87\xE6\xA0\xA1\xE6\xAD\xA3\xE5\x8A\x9F\xE8\x83\xBD";
const char* scMsg =
"\xE5\x90\xAF\xE5\x8A\xA8\xE6\xB3\xA8\xE9\x9F\xB3\xE6\x96\x87\xE6\xA0\xA1\xE6\xAD\xA3\xE5\x8A\x9F\xE8\x83\xBD";
#endif
  */

  if (!readingText->isEmpty() || !composingText->isEmpty()) return false;

  if (!text.size()) return false;

  // loaderService->logger("OVAFBopomofoCorrectionContext") << "direct text: "
  // << text << endl;

  m_characters = OVUTF8Helper::SplitStringByCodePoint(text);
  m_currentChar = m_characters.end();

  for (vector<string>::iterator iter = m_characters.begin();
       iter != m_characters.end(); ++iter) {
    string current = *iter;
    vector<string> results;

    results = m_dataTable->valuesForKey(current);

    // loaderService->logger("OVAFBopomofoCorrectionContext") << "current: " <<
    // current << ", results: " << results.size() << endl;

    if (results.size()) {
      m_currentChar = iter;

      OVOneDimensionalCandidatePanel* panel =
          candidateService->useVerticalCandidatePanel();
      OVCandidateList* list = panel->candidateList();

      list->addCandidates(results);

      panel->setCandidatesPerPage(9);
      panel->show();
      panel->updateDisplay();
      panel->yieldToCandidateEventHandler();

      composingText->setText(text);
      composingText->setCursorPosition((size_t)(iter - m_characters.begin()));
      composingText->updateDisplay();

      /*
      string locale = loaderService->locale();

      if (locale == "zh_TW" || locale == "zh-hant")
              loaderService->notify(tcMsg);
      else if (locale == "zh_CN" || locale == "zh-hans")
              loaderService->notify(scMsg);
      else
              loaderService->notify("Bopomofo correction activated");
      */

      return true;
    }
  }

  composingText->setText(text);
  composingText->commit();
  return true;
}

void OVAFBopomofoCorrectionContext::candidateCanceled(
    OVCandidateService* candidateService, OVTextBuffer* readingText,
    OVTextBuffer* composingText, OVLoaderService* loaderService) {
  composingText->setText(OVUTF8Helper::CombineCodePoints(m_characters));
  composingText->commit();
}

bool OVAFBopomofoCorrectionContext::candidateSelected(
    OVCandidateService* candidateService, const string& text, size_t index,
    OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVLoaderService* loaderService) {
  *m_currentChar = text;
  composingText->setText(OVUTF8Helper::CombineCodePoints(m_characters));
  composingText->commit();
  return true;
}

// bool
// OVAFBopomofoCorrectionContext::candidateNonPanelKeyReceived(OVCandidateService*
// candidateService, const OVKey* key, OVTextBuffer* readingText, OVTextBuffer*
// composingText, OVLoaderService* loaderService)
//{
//     return false;
// }

const string OVAFBopomofoCorrection::localizedName(const string& locale) {
#ifndef _MSC_VER
  string tcname = "注音文校正";
  string scname = "注音文校正";
#else
  string tcname =
      "\xE6\xB3\xA8\xE9\x9F\xB3\xE6\x96\x87\xE6\xA0\xA1\xE6\xAD\xA3";
  string scname =
      "\xE6\xB3\xA8\xE9\x9F\xB3\xE6\x96\x87\xE6\xA0\xA1\xE6\xAD\xA3";
#endif

  if (locale == "zh_TW" || locale == "zh-Hant")
    return tcname;
  else if (locale == "zh_CN" || locale == "zh-Hans")
    return scname;
  else
    return string("Bopomofo Correction");
}

const string OVAFBopomofoCorrection::identifier() const {
  return OVAFBOPOMOFOCORRECTION_IDENTIFIER;
}

OVEventHandlingContext* OVAFBopomofoCorrection::createContext() {
  return new OVAFBopomofoCorrectionContext(m_dataTable);
}

bool OVAFBopomofoCorrection::initialize(OVPathInfo* pathInfo,
                                        OVLoaderService* loaderService) {
  string tableName =
      OVAFBOPOMOFOCORRECTION_IDENTIFIER "-bopomofo-correction-cin";
  m_dataTable =
      loaderService->CINDatabaseService()->createKeyValueDataTableInterface(
          tableName);
  if (!m_dataTable) {
    m_dataTable = loaderService->SQLiteDatabaseService()
                      ->createKeyValueDataTableInterface(tableName);
    if (!m_dataTable) {
      // loaderService->logger(identifier()) << "Cannot find table: " <<
      // tableName << endl;
      return false;
    }
  }

  // loaderService->logger(identifier()) << "bopomofo correction inited." <<
  // endl;

  return true;
}

void OVAFBopomofoCorrection::finalize() { delete m_dataTable; }

};  // namespace OpenVanilla
