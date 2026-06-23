//
// OVAFPhraseConverter.cpp
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

#include "OVAFPhraseConverter.h"

#include <ctype.h>

#include <sstream>

namespace OpenVanilla {

class PhraseConverterStringTable : public OVLocalizationStringTable {
 public:
  PhraseConverterStringTable() {
#ifndef _MSC_VER
    add("en", "->", " → ");
    add("zh_TW", "->", " → ");
    add("zh_CN", "->", " → ");

    add("en", "Simplified Chinese Phrase Converter",
        "Simplified Chinese Phrase Converter");
    add("zh_TW", "Simplified Chinese Phrase Converter", "簡體中文常用詞彙轉換");
    add("zh_CN", "Simplified Chinese Phrase Converter", "简体中文常用词汇转换");
#else
    add("en", "->", " \xe2\x86\x92 ");
    add("zh_TW", "->", " \xe2\x86\x92 ");
    add("zh_CN", "->", " \xe2\x86\x92 ");

    add("en", "Simplified Chinese Phrase Converter",
        "Simplified Chinese Phrase Converter");
    add("zh_TW", "Simplified Chinese Phrase Converter",
        "\xe7\xb0\xa1\xe9\xab\x94\xe4\xb8\xad\xe6\x96\x87\xe5\xb8\xb8\xe7\x94"
        "\xa8\xe8\xa9\x9e\xe5\xbd\x99\xe8\xbd\x89\xe6\x8f\x9b");
    add("zh_CN", "Simplified Chinese Phrase Converter",
        "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87\xe5\xb8\xb8\xe7\x94"
        "\xa8\xe8\xaf\x8d\xe6\xb1\x87\xe8\xbd\xac\xe6\x8d\xa2");
#endif
  }
};

typedef OVLocalization<PhraseConverterStringTable> PCL;

OVAFPhraseConverterContext::OVAFPhraseConverterContext(
    OVAFPhraseConverter* module)
    : m_module(module) {}

bool OVAFPhraseConverterContext::handleDirectText(
    const vector<string>& textSegments, OVTextBuffer* readingText,
    OVTextBuffer* composingText, OVCandidateService* candidateService,
    OVLoaderService* loaderService) {
  // loaderService->logger(m_module->identifier()) << "Received word segments: "
  // << OVStringHelper::Join(textSegments, ", ") << endl;

  vector<string> messages;

  for (vector<string>::const_iterator tsiter = textSegments.begin();
       tsiter != textSegments.end(); ++tsiter) {
    // loaderService->logger(m_module->identifier()) << "segment: " << *tsiter;

    vector<string> values = m_module->m_dataTable->findChardef(*tsiter);
    if (values.size()) {
      // loaderService->logger(m_module->identifier()) << ", converted: " <<
      // values[0];
      composingText->commitAsTextSegment(values[0]);

      stringstream sst;
      sst << *tsiter << PCL::S("->") << values[0];
      messages.push_back(sst.str());
    } else {
      composingText->commitAsTextSegment(*tsiter);
    }

    // loaderService->logger(m_module->identifier()) << endl;
  }

  if (messages.size()) {
    composingText->showToolTip(OVStringHelper::Join(messages, ", "));
  }

  return true;
}

OVAFPhraseConverter::~OVAFPhraseConverter() {
  if (m_dataTable) {
    delete m_dataTable;
  }
}

OVEventHandlingContext* OVAFPhraseConverter::createContext() {
  return new OVAFPhraseConverterContext(this);
}

const string OVAFPhraseConverter::identifier() const {
  return string(OVAFPHRASECONVERTER_IDENTIFIER);
}

const string OVAFPhraseConverter::localizedName(const string& locale) {
  return PCL::S(locale, "Simplified Chinese Phrase Converter");
}

bool OVAFPhraseConverter::initialize(OVPathInfo* pathInfo,
                                     OVLoaderService* loaderService) {
  // loaderService->logger(identifier()) << *pathInfo << endl;

  string dataFilePath =
      OVPathHelper::PathCat(pathInfo->resourcePath, "Phrases.cin");
  OVCINDataTableParser parser;
  m_dataTable = parser.CINDataTableFromFileName(dataFilePath);
  if (!m_dataTable) {
    loaderService->logger(identifier())
        << "Cannot open: " << dataFilePath << endl;
    return false;
  }

  PCL::SetDefaultLocale(loaderService->locale());

  return true;
}

};  // namespace OpenVanilla
