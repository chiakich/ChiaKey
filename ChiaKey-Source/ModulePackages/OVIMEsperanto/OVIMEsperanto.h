//
// OVIMEsperanto.h
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

#ifndef OVIMEsperanto_h
#define OVIMEsperanto_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include <string>
#include <vector>

namespace OpenVanilla {
const size_t EsperantoXTranslationCharCount = 12;

class EsperantoKeyBuffer {
 public:
  EsperantoKeyBuffer() { reset(); }

  void reset() { m_value = -1; }

  bool buffering() {
    if (m_value > -1) return true;

    return false;
  }

  bool detect(int key) {
    int tmp = isEsperantoXTranslationChar(key);
    if (tmp > -1) {
      m_value = tmp;
      return true;
    }
    return false;
  }

  int value() { return m_value; }

 protected:
  int isEsperantoXTranslationChar(int key) {
    for (int i = 0; i < EsperantoXTranslationCharCount; i++) {
      if (key == EsperantoXTranslationChars[i]) return i;
    }

    return -1;
  }

  static char EsperantoXTranslationChars[EsperantoXTranslationCharCount];
  int m_value;
};

char EsperantoKeyBuffer::EsperantoXTranslationChars
    [EsperantoXTranslationCharCount] = {'c', 'g', 'h', 'j', 's', 'u',
                                        'C', 'G', 'H', 'J', 'S', 'U'};

class OVIMEsperantoContext : public OVEventHandlingContext {
 public:
  OVIMEsperantoContext() {
#ifndef _MSC_VER
    m_translationStrings.push_back(string("ĉ"));
    m_translationStrings.push_back(string("ĝ"));
    m_translationStrings.push_back(string("ĥ"));
    m_translationStrings.push_back(string("ĵ"));
    m_translationStrings.push_back(string("ŝ"));
    m_translationStrings.push_back(string("ŭ"));
    m_translationStrings.push_back(string("Ĉ"));
    m_translationStrings.push_back(string("Ĝ"));
    m_translationStrings.push_back(string("Ĥ"));
    m_translationStrings.push_back(string("Ĵ"));
    m_translationStrings.push_back(string("Ŝ"));
    m_translationStrings.push_back(string("Ŭ"));
#else
    m_translationStrings.push_back(string("\xC4\x89"));
    m_translationStrings.push_back(string("\xC4\x9D"));
    m_translationStrings.push_back(string("\xC4\xA5"));
    m_translationStrings.push_back(string("\xC4\xB5"));
    m_translationStrings.push_back(string("\xC5\x9D"));
    m_translationStrings.push_back(string("\xC5\xAD"));
    m_translationStrings.push_back(string("\xC4\x88"));
    m_translationStrings.push_back(string("\xC4\x9C"));
    m_translationStrings.push_back(string("\xC4\xA4"));
    m_translationStrings.push_back(string("\xC4\xB4"));
    m_translationStrings.push_back(string("\xC5\x9C"));
    m_translationStrings.push_back(string("\xC5\xAC"));

#endif
  }

  virtual void startSession(OVLoaderService* loaderService) {
    loaderService->logger() << "Session started" << endl;
    m_keyBuffer.reset();
  }

  virtual void stopSession(OVLoaderService* loaderService) {
    loaderService->logger() << "Session ended" << endl;
    m_keyBuffer.reset();
  }

  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService) {
    loaderService->logger()
        << "Received key, code = " << key->keyCode() << endl;

    if (!key->receivedString().length()) {
      if (key->keyCode() == OVKeyCode::Delete ||
          key->keyCode() == OVKeyCode::Backspace) {
        if (m_keyBuffer.buffering()) {
          composingText->clear();
          composingText->updateDisplay();
          m_keyBuffer.reset();
          return true;
        }
        return false;
      }

      // if not delete or backspace, we empty the buffer and commit what we have
      if (m_keyBuffer.buffering()) {
        composingText->commit();
        m_keyBuffer.reset();
      }

      return false;  // let the application handle the key
    }

    if (m_keyBuffer.buffering()) {
      if (key->keyCode() == 'x' || key->keyCode() == 'X') {
        composingText->setText(m_translationStrings[m_keyBuffer.value()]);
        composingText->commit();
      } else {
        composingText->appendText(key->receivedString());
        composingText->commit();
      }
      m_keyBuffer.reset();
    } else if (m_keyBuffer.detect(key->keyCode())) {
      composingText->appendText(key->receivedString());
      composingText->updateDisplay();
    } else {
      composingText->setText(key->receivedString());
      composingText->commit();
    }
    return true;
  }

 protected:
  EsperantoKeyBuffer m_keyBuffer;
  vector<string> m_translationStrings;
};

class OVIMEsperanto : public OVInputMethod {
 public:
  virtual OVEventHandlingContext* createContext() {
    return new OVIMEsperantoContext;
  }

  virtual const string identifier() const { return string("OVIMEsperanto"); }

  virtual const string localizedName(const string& locale) {
#ifndef _MSC_VER
    string tcname = "世界語";
    string scname = "世界语";
    string jname = "エスペラント";
#else
    string tcname = "\xE4\xB8\x96\xE7\x95\x8C\xE8\xAA\x9E";
    string scname = "\xE4\xB8\x96\xE7\x95\x8C\xE8\xAF\xAD";
    string jname =
        "\xE3\x82\xA8\xE3\x82\xB9\xE3\x83\x9A\xE3\x83\xA9\xE3\x83\xB3\xE3\x83"
        "\x88";
#endif

    if (locale == "zh_TW" || locale == "zh-hant")
      return tcname;
    else if (locale == "zh_CN" || locale == "zh-hans")
      return scname;
    else if (locale == "ja")
      return jname;
    else
      return string("Esperano Keyboard");
  }
};
};  // namespace OpenVanilla

#endif
