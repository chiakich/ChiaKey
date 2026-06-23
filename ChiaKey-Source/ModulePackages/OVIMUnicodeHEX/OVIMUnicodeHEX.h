//
// OVIMUnicodeHEX.h
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
#define strcasecmp stricmp
#else
#include "OpenVanilla.h"
#endif

#include <string>
#include <vector>

namespace OpenVanilla {
const size_t ebMaxKeySeq = 4;

class UnicodeHEXKeyBuffer {
 public:
  UnicodeHEXKeyBuffer() { clear(); }
  void add(char c) {
    if (len == ebMaxKeySeq) return;
    buf[len++] = c;
    buf[len] = 0;
  }
  void remove() {
    if (!len) return;
    buf[--len] = 0;
  }
  void clear() {
    len = 0;
    buf[0] = 0;
  }
  char buf[ebMaxKeySeq];
  size_t len;
};

class OVIMUnicodeHEXContext : public OVEventHandlingContext {
 public:
  OVIMUnicodeHEXContext() {}

  virtual void startSession(OVLoaderService* loaderService) {
    loaderService->logger() << "Session started" << endl;
    m_keyBuffer.clear();
  }

  virtual void stopSession(OVLoaderService* loaderService) {
    loaderService->logger() << "Session ended" << endl;
    m_keyBuffer.clear();
  }

  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService) {
    loaderService->logger()
        << "Received key, code = " << key->keyCode() << endl;

    if (key->keyCode() == OVKeyCode::Return ||
        key->keyCode() == OVKeyCode::Space) {
      if (!(strlen(m_keyBuffer.buf))) {
        return false;
      }
      unsigned u = strtol(m_keyBuffer.buf, (char**)NULL, 16);
      if (u > 65535) {
        readingText->clear();
        m_keyBuffer.clear();
        loaderService->beep();
        return true;
      }
      string bufferString = OVUTF8Helper::SingleUTF8StringFromCodePoint(u);
      composingText->setText(bufferString);
      composingText->commit();
      readingText->clear();
      m_keyBuffer.clear();
      return true;
    }

    if (!key->receivedString().length()) {
      if (key->keyCode() == OVKeyCode::Esc) {
        composingText->clear();
        m_keyBuffer.clear();
        return true;
      }

      if (key->keyCode() == OVKeyCode::Delete ||
          key->keyCode() == OVKeyCode::Backspace) {
        if (!(strlen(m_keyBuffer.buf))) {
          return false;
        }
        m_keyBuffer.remove();

        if (!strlen(m_keyBuffer.buf)) {
          readingText->clear();
          readingText->updateDisplay();
        } else {
          string bufferString =
              string("U+") + StringToUpper(string(m_keyBuffer.buf));
          readingText->setText(bufferString);
          readingText->updateDisplay();
        }
        return true;
      }
      return false;
    }

    if (strlen(m_keyBuffer.buf) >= ebMaxKeySeq)
      return true;  // Max to 4 letters.

    if ((key->keyCode() >= 'a' && key->keyCode() <= 'f') ||
        (key->keyCode() >= '0' && key->keyCode() <= '9')) {
      m_keyBuffer.add(key->keyCode());
      string bufferString =
          string("U+") + StringToUpper(string(m_keyBuffer.buf));
      readingText->setText(bufferString);
      readingText->updateDisplay();
    } else {
      loaderService->beep();
    }

    return true;
  }

  string StringToUpper(
      string strToConvert) {  // change each element of the string to upper case
    for (unsigned int i = 0; i < strToConvert.length(); i++) {
      strToConvert[i] = toupper(strToConvert[i]);
    }
    return strToConvert;  // return the converted string
  }

 protected:
  UnicodeHEXKeyBuffer m_keyBuffer;
};

class OVIMUnicodeHEX : public OVInputMethod {
 public:
  virtual OVEventHandlingContext* createContext() {
    return new OVIMUnicodeHEXContext;
  }

  virtual const string identifier() const { return string("OVIMUnicodeHEX"); }

  virtual const string localizedName(const string& locale) {
    return string("Unicode HEX");
  }
};
};  // namespace OpenVanilla

#endif
