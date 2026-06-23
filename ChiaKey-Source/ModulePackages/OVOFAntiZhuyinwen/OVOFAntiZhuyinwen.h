//
// OVOFAntiZhuyinwen.h
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

#ifndef OVOFAntiZhuyinwen_h
#define OVOFAntiZhuyinwen_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include <vector>

namespace OpenVanilla {
using namespace std;

class OVOFAntiZhuyinwenContext : public OVEventHandlingContext {
 public:
  OVOFAntiZhuyinwenContext() {
#ifndef _MSC_VER
    m_bopomofo.push_back("ㄅ");
    m_bopomofo.push_back("ㄆ");
    m_bopomofo.push_back("ㄇ");
    m_bopomofo.push_back("ㄈ");
    m_bopomofo.push_back("ㄉ");
    m_bopomofo.push_back("ㄊ");
    m_bopomofo.push_back("ㄋ");
    m_bopomofo.push_back("ㄌ");
    m_bopomofo.push_back("ㄍ");
    m_bopomofo.push_back("ㄎ");
    m_bopomofo.push_back("ㄏ");
    m_bopomofo.push_back("ㄐ");
    m_bopomofo.push_back("ㄑ");
    m_bopomofo.push_back("ㄒ");
    m_bopomofo.push_back("ㄓ");
    m_bopomofo.push_back("ㄔ");
    m_bopomofo.push_back("ㄕ");
    m_bopomofo.push_back("ㄖ");
    m_bopomofo.push_back("ㄗ");
    m_bopomofo.push_back("ㄘ");
    m_bopomofo.push_back("ㄙ");
    m_bopomofo.push_back("ㄧ");
    m_bopomofo.push_back("ㄨ");
    m_bopomofo.push_back("ㄩ");
    m_bopomofo.push_back("ㄚ");
    m_bopomofo.push_back("ㄛ");
    m_bopomofo.push_back("ㄜ");
    m_bopomofo.push_back("ㄝ");
    m_bopomofo.push_back("ㄞ");
    m_bopomofo.push_back("ㄟ");
    m_bopomofo.push_back("ㄠ");
    m_bopomofo.push_back("ㄡ");
    m_bopomofo.push_back("ㄢ");
    m_bopomofo.push_back("ㄣ");
    m_bopomofo.push_back("ㄤ");
    m_bopomofo.push_back("ㄥ");
    m_bopomofo.push_back("ㄦ");
#else
    m_bopomofo.push_back("\xE3\x84\x85");
    m_bopomofo.push_back("\xE3\x84\x86");
    m_bopomofo.push_back("\xE3\x84\x87");
    m_bopomofo.push_back("\xE3\x84\x88");
    m_bopomofo.push_back("\xE3\x84\x89");
    m_bopomofo.push_back("\xE3\x84\x8A");
    m_bopomofo.push_back("\xE3\x84\x8B");
    m_bopomofo.push_back("\xE3\x84\x8C");
    m_bopomofo.push_back("\xE3\x84\x8D");
    m_bopomofo.push_back("\xE3\x84\x8E");
    m_bopomofo.push_back("\xE3\x84\x8F");
    m_bopomofo.push_back("\xE3\x84\x90");
    m_bopomofo.push_back("\xE3\x84\x91");
    m_bopomofo.push_back("\xE3\x84\x92");
    m_bopomofo.push_back("\xE3\x84\x93");
    m_bopomofo.push_back("\xE3\x84\x94");
    m_bopomofo.push_back("\xE3\x84\x95");
    m_bopomofo.push_back("\xE3\x84\x96");
    m_bopomofo.push_back("\xE3\x84\x97");
    m_bopomofo.push_back("\xE3\x84\x98");
    m_bopomofo.push_back("\xE3\x84\x99");
    m_bopomofo.push_back("\xE3\x84\xA7");
    m_bopomofo.push_back("\xE3\x84\xA8");
    m_bopomofo.push_back("\xE3\x84\xA9");
    m_bopomofo.push_back("\xE3\x84\x9A");
    m_bopomofo.push_back("\xE3\x84\x9B");
    m_bopomofo.push_back("\xE3\x84\x9C");
    m_bopomofo.push_back("\xE3\x84\x9D");
    m_bopomofo.push_back("\xE3\x84\x9E");
    m_bopomofo.push_back("\xE3\x84\x9F");
    m_bopomofo.push_back("\xE3\x84\xA0");
    m_bopomofo.push_back("\xE3\x84\xA1");
    m_bopomofo.push_back("\xE3\x84\xA2");
    m_bopomofo.push_back("\xE3\x84\xA3");
    m_bopomofo.push_back("\xE3\x84\xA4");
    m_bopomofo.push_back("\xE3\x84\xA5");
    m_bopomofo.push_back("\xE3\x84\xA6");
#endif
  }
  virtual const string filterText(const string& inputText,
                                  OVLoaderService* loaderService);
  virtual bool checkText(string theText);

 private:
  vector<string> m_bopomofo;
};

class OVOFAntiZhuyinwen : public OVOutputFilter {
 public:
  virtual const string localizedName(const string& locale) {
#ifndef _MSC_VER
    string tcname = "注音文過濾";
    string scname = "注音文过滤";
#else
    string tcname =
        "\xE6\xB3\xA8\xE9\x9F\xB3\xE6\x96\x87\xE9\x81\x8E\xE6\xBF\xBE";
    string scname =
        "\xE6\xB3\xA8\xE9\x9F\xB3\xE6\x96\x87\xE8\xBF\x87\xE6\xBB\xA4";
#endif
    if (locale == "zh_TW" || locale == "zh-hant")
      return tcname;
    else if (locale == "zh_CN" || locale == "zh-hans")
      return scname;
    else
      return string("Anti-Zhuyinwen");
  }

  virtual const string identifier() const { return "OVOFAntiZhuyinwen"; }

  virtual OVEventHandlingContext* createContext() {
    return new OVOFAntiZhuyinwenContext();
  }
};

class OVOFAntiZhuyinwenReselectContext : public OVEventHandlingContext {
 public:
  OVOFAntiZhuyinwenReselectContext(OVKeyValueDataTableInterface* dataTable)
      : m_dataTable(dataTable) {}

  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService) {
    return false;
  }

  virtual bool handleDirectText(const string& text, OVTextBuffer* readingText,
                                OVTextBuffer* composingText,
                                OVCandidateService* candidateService,
                                OVLoaderService* loaderService) {
    if (!text.size()) return false;

    m_characters = OVUTF8Helper::SplitStringByCodePoint(text);
    m_currentChar = m_characters.end();

    for (vector<string>::iterator iter = m_characters.begin();
         iter != m_characters.end(); ++iter) {
      string current = *iter;
      vector<string> results;

      results = m_dataTable->valuesForKey(current);
      loaderService->logger("process")
          << current << ": " << results.size() << endl;

      if (results.size()) {
        m_currentChar = iter;

        OVOneDimensionalCandidatePanel* panel =
            candidateService->useVerticalCandidatePanel();
        OVCandidateList* list = panel->candidateList();

        list->addCandidates(results);

        panel->setCandidatesPerPage(6);
        panel->show();
        panel->updateDisplay();
        panel->yieldToCandidateEventHandler();

        composingText->setText(text);
        composingText->updateDisplay();
        composingText->setCursorPosition((size_t)(iter - m_characters.begin()));

        string locale = loaderService->locale();
#ifndef _MSC_VER
        if (locale == "zh_TW" || locale == "zh-hant")
          loaderService->notify("啟動注音文校正功能");
        else if (locale == "zh_CN" || locale == "zh-hans")
          loaderService->notify("启动注音文校正功能");
#else
        if (locale == "zh_TW" || locale == "zh-hant")
          loaderService->notify(
              "\xE5\x95\x9F\xE5\x8B\x95\xE6\xB3\xA8\xE9\x9F\xB3\xE6\x96\x87\xE6"
              "\xA0\xA1\xE6\xAD\xA3\xE5\x8A\x9F\xE8\x83\xBD");
        else if (locale == "zh_CN" || locale == "zh-hans")
          loaderService->notify(
              "\xE5\x90\xAF\xE5\x8A\xA8\xE6\xB3\xA8\xE9\x9F\xB3\xE6\x96\x87\xE6"
              "\xA0\xA1\xE6\xAD\xA3\xE5\x8A\x9F\xE8\x83\xBD");
#endif
        else
          loaderService->notify("Zhuyinwen correction activated");

        return true;
      }
    }

    composingText->setText(text);
    composingText->commit();
    return true;
  }

  virtual void candidateCanceled(OVCandidateService* candidateService,
                                 OVTextBuffer* readingText,
                                 OVTextBuffer* composingText,
                                 OVLoaderService* loaderService) {
    composingText->setText(OVUTF8Helper::CombineCodePoints(m_characters));
    composingText->commit();
  }

  virtual bool candidateSelected(OVCandidateService* candidateService,
                                 const string& text, size_t index,
                                 OVTextBuffer* readingText,
                                 OVTextBuffer* composingText,
                                 OVLoaderService* loaderService) {
    *m_currentChar = text;
    composingText->setText(OVUTF8Helper::CombineCodePoints(m_characters));
    composingText->commit();
    return true;
  }

  virtual bool candidateNonPanelKeyReceived(
      OVCandidateService* candidateService, const OVKey* key,
      OVTextBuffer* readingText, OVTextBuffer* composingText,
      OVLoaderService* loaderService) {
    return false;
  }

 protected:
  vector<string> m_characters;
  vector<string>::iterator m_currentChar;
  OVKeyValueDataTableInterface* m_dataTable;
};

class OVOFAntiZhuyinwenReselect : public OVAroundFilter {
 public:
  virtual const string localizedName(const string& locale) {
#ifndef _MSC_VER
    string tcname = "注音文校正";
    string scname = "注音文校正";
#else
    string tcname =
        "\xE6\xB3\xA8\xE9\x9F\xB3\xE6\x96\x87\xE6\xA0\xA1\xE6\xAD\xA3";
    string scname =
        "\xE6\xB3\xA8\xE9\x9F\xB3\xE6\x96\x87\xE6\xA0\xA1\xE6\xAD\xA3";
#endif
    if (locale == "zh_TW" || locale == "zh-hant")
      return tcname;
    else if (locale == "zh_CN" || locale == "zh-hans")
      return scname;
    else
      return string("Zhuyinwen Correction");
  }

  virtual const string identifier() const {
    return "OVOFAntiZhuyinwenReselect";
  }

  virtual OVEventHandlingContext* createContext() {
    return new OVOFAntiZhuyinwenReselectContext(m_dataTable);
  }

  virtual bool initialize(OVPathInfo* pathInfo,
                          OVLoaderService* loaderService) {
    m_dataTable =
        loaderService->CINDatabaseService()->createKeyValueDataTableInterface(
            "OVOFAntiZhuyinwen-zhuyinwen-reverse-lookup-cin");
    if (!m_dataTable) {
      loaderService->logger(identifier())
          << "Cannot find table: "
          << "OVOFAntiZhuyinwen-zhuyinwen-reverse-lookup-cin" << endl;
      return false;
    }

    loaderService->logger(identifier())
        << "Table loaded: "
        << "OVOFAntiZhuyinwen-zhuyinwen-reverse-lookup-cin" << endl;
    return true;
  }

  virtual void finalize() { delete m_dataTable; }

 protected:
  OVKeyValueDataTableInterface* m_dataTable;
};

};  // namespace OpenVanilla

#endif