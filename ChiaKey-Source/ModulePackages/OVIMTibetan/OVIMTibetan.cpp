//
// OVIMTibetan.cpp
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

#include "OVIMTibetan.h"

#include <ctype.h>

#include "OVIMTibetanData.h"

namespace OpenVanilla {

OVIMTibetanContext::OVIMTibetanContext(int currentKeyboard)
    : m_currentKeyboard(currentKeyboard) {
  m_currentKeyboard = currentKeyboard;
  m_tibetanNumbers.push_back(string("\xE0\xBC\xA0"));
  m_tibetanNumbers.push_back(string("\xE0\xBC\xA1"));
  m_tibetanNumbers.push_back(string("\xE0\xBC\xA2"));
  m_tibetanNumbers.push_back(string("\xE0\xBC\xA3"));
  m_tibetanNumbers.push_back(string("\xE0\xBC\xA4"));
  m_tibetanNumbers.push_back(string("\xE0\xBC\xA5"));
  m_tibetanNumbers.push_back(string("\xE0\xBC\xA6"));
  m_tibetanNumbers.push_back(string("\xE0\xBC\xA7"));
  m_tibetanNumbers.push_back(string("\xE0\xBC\xA8"));
  m_tibetanNumbers.push_back(string("\xE0\xBC\xA9"));

  m_tibetanFinalAdd.push_back(string("\xE0\xBE\x83"));
  m_tibetanFinalAdd.push_back(string("\xE0\xBD\xBE"));

  m_tibetanSymbols.push_back(string("\xE0\xBC\x80"));
  m_tibetanSymbols.push_back(string("\xE0\xBC\x8D"));
  m_tibetanSymbols.push_back(string("\xE0\xBC\x84"));
  m_tibetanSymbols.push_back(string("\xE0\xBC\x85"));
  m_tibetanSymbols.push_back(string("\xE0\xBC\xBC"));
  m_tibetanSymbols.push_back(string("\xE0\xBC\xBD"));
  m_tibetanSymbols.push_back(string("\xE0\xBE\x85"));
  m_tibetanSymbols.push_back(string("\xE0\xBD\xBF"));
  m_tibetanSymbols.push_back(string("\xE0\xBC\x94"));
  m_tibetanSymbols.push_back(string("\xE0\xBC\x91"));
  m_tibetanSymbols.push_back(string("\xE0\xBC\x88"));

  m_tibetanHTransform.push_back(string("\xE0\xBD\x83"));
  m_tibetanHTransform.push_back(string("\xE0\xBD\x92"));
  m_tibetanHTransform.push_back(string("\xE0\xBD\x97"));
  m_tibetanHTransform.push_back(string("\xE0\xBD\x9C"));
  m_tibetanHTransform.push_back(string("\xE0\xBD\x8D"));

  m_tibetanVowel.push_back(string(""));
  m_tibetanVowel.push_back(string("\xE0\xBD\xB2"));
  m_tibetanVowel.push_back(string("\xE0\xBD\xB4"));
  m_tibetanVowel.push_back(string("\xE0\xBD\xBC"));
  m_tibetanVowel.push_back(string("\xE0\xBD\xBA"));
  m_tibetanVowel.push_back(string("\xE0\xBD\xBB"));
  m_tibetanVowel.push_back(string("\xE0\xBD\xBD"));
  m_tibetanVowel.push_back(string("\xE0\xBE\x80"));

  m_tibetanConsonants.push_back(string("\xE0\xBD\x80"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x81"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x82"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x84"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x85"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x86"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x87"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x89"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x8F"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x90"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x91"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x93"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x94"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x95"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x96"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x98"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x99"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x9A"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x9B"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x9D"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x9E"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x9F"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\xA0"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\xA1"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\xA2"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\xA3"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\xA4"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\xA6"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\xA7"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\xA8"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x8A"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x8B"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x8C"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\x8D"));
  m_tibetanConsonants.push_back(string("\xE0\xBD\xA5"));

  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\x90"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\x91"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\x92"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\x94"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\x95"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\x96"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\x97"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\x99"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\x9F"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xA0"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xA1"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xA3"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xA4"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xA5"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xA6"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xA8"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xA9"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xAA"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xAB"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xAD"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xAE"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xAF"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xB0"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xB1"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xB2"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xB3"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xB4"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xB6"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xB7"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xB8"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\x9A"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\x9B"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\x9C"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\x9D"));
  m_tibetanLowerConsonants.push_back(string("\xE0\xBE\xB5"));
}

void OVIMTibetanContext::startSession(OVLoaderService* loaderService) {
  loaderService->logger("OVIMTibetanContext") << "session started" << endl;
  m_buffer.clear();
  m_buffer.lastIsOther();
}

void OVIMTibetanContext::stopSession(OVLoaderService* loaderService) {
  loaderService->logger("OVIMTibetanContext") << "session ended" << endl;
  m_buffer.clear();
  m_buffer.lastIsOther();
}

string OVIMTibetanContext::lowerString(string from) {
  string tmp = "";
  size_t i;
  for (i = 0; i < from.length(); i++) {
    char c = from[i];
    c = tolower(c);
    tmp += c;
  }
  return tmp;
}

string OVIMTibetanContext::upperString(string from) {
  string tmp = "";
  size_t i;
  for (i = 0; i < from.length(); i++) {
    char c = from[i];
    c = toupper(c);
    tmp += c;
  }
  return tmp;
}

bool OVIMTibetanContext::handleKey(OVKey* key, OVTextBuffer* readingText,
                                   OVTextBuffer* composingText,
                                   OVCandidateService* candidateService,
                                   OVLoaderService* loaderService) {
  loaderService->logger("OVIMTibetanContext")
      << "received key, code = " << key->keyCode()
      << ", string = " << key->receivedString() << endl;

  if (key->isAltPressed() || key->isOptPressed() || key->isCommandPressed() ||
      key->isCtrlPressed()) {
    return false;
  }

  if (key->isCapsLockOn()) {
    m_buffer.clear();
    m_buffer.lastIsOther();
    if (key->receivedString().length()) {
      string tmp;
      if (key->isShiftPressed()) {
        tmp = upperString(key->receivedString());
      } else {
        tmp = lowerString(key->receivedString());
      }
      composingText->appendText(tmp);
      composingText->commit();
      return true;
    } else {
      return false;
    }
  }

  if (key->keyCode() == OVKeyCode::Up || key->keyCode() == OVKeyCode::Down ||
      key->keyCode() == OVKeyCode::Left || key->keyCode() == OVKeyCode::Right) {
    m_buffer.lastIsOther();
    if (m_buffer.len) {
      composingText->commit();
    }
    return false;
  }

  if (key->keyCode() == OVKeyCode::Return ||
      key->keyCode() == OVKeyCode::Enter) {
    if (!m_buffer.len) return false;
    composingText->commit();
    m_buffer.clear();
    m_buffer.lastIsOther();
    return true;
  }

  if (key->keyCode() == OVKeyCode::Space) {
    composingText->appendText(string("\340\274\213"), true);
    composingText->commit();
    m_buffer.clear();
    m_buffer.lastIsOther();
  }

  if (key->keyCode() == OVKeyCode::Delete ||
      key->keyCode() == OVKeyCode::Backspace) {
    if (!m_buffer.len) return false;
    composingText->clear();
    composingText->updateDisplay();
    m_buffer.clear();
    m_buffer.lastIsOther();
    return true;
  }

  char keycode = key->keyCode();

  if (keycode >= '0' && keycode <= '9') {
    composingText->appendText(m_tibetanNumbers[keycode - '0'], true);
    composingText->commit();
    m_buffer.clear();
    m_buffer.lastIsOther();
    return true;
  }

  if (keycode == SpaceKey[m_currentKeyboard]) {
    composingText->appendText(" ", true);
    composingText->commit();
    m_buffer.clear();
    m_buffer.lastIsOther();
    return true;
  }

  if (keycode == ComposeKey[m_currentKeyboard]) {
    composingText->commit();
    if (m_buffer.buf[0] == ComposeKey[m_currentKeyboard])  // End Stacking
    {
      m_buffer.clear();
      m_buffer.lastIsOther();
    } else {  // Begin Composing
      m_buffer.clear();
      m_buffer.lastIsOther();
      m_buffer.add(keycode);
      loaderService->notify("Start stacking characters..");
    }
    composingText->updateDisplay();
    return true;
  }

  short int i;

  if ((i = isFinalAddKey(keycode)) > -1) {  // Final M or N
    if (m_buffer.buf[0] == ComposeKey[m_currentKeyboard] && m_buffer.len == 1)
      return true;
    if (m_buffer.last || m_buffer.len) {
      composingText->appendText(m_tibetanFinalAdd[i], true);
      composingText->commit();
      m_buffer.clear();
      m_buffer.lastIsOther();
    }
    return true;
  }

  if ((i = isSymbolKey(keycode)) > -1) {  // Symbols
    composingText->appendText(m_tibetanSymbols[i], true);
    composingText->commit();
    m_buffer.clear();
    m_buffer.lastIsOther();

    return true;  // key processed
  }

  if ((i = isVowelKey(keycode)) > -1) {  // Vowels
    if (m_buffer.last == 1 ||
        (m_buffer.buf[0] == ComposeKey[m_currentKeyboard] &&
         m_buffer.len > 3)) {
      composingText->appendText(m_tibetanVowel[i], true);
      composingText->commit();
      m_buffer.clear();
      m_buffer.lastIsVowel();
    }
    return true;  // key processed
  }

  if (m_currentKeyboard == 1) {  // Sambhota Keymap #2
    if (keycode == 'D') keycode = 'm';
    if (keycode == 'F') keycode = '\'';
  }

  if ((i = isConsonantKey(keycode)) > -1) {  // Consonant
    string tmpString = m_tibetanConsonants[i];
    int k;
    m_buffer.lastIsConsonant();
    if (m_buffer.buf[0] == ComposeKey[m_currentKeyboard] &&
        m_buffer.len < MAX_COMPOSE) {     // Stacking
      if (i == 34 && m_buffer.len > 1) {  // kb transform
        if (isConsonantKey(m_buffer.buf[m_buffer.len - 1]) == 0) {
          composingText->clear();
          composingText->updateDisplay();
          if (m_buffer.len < 3) {
            composingText->appendText(string("\xE0\xBD\xA9"), true);
            composingText->updateDisplay();
          } else {
            if (((k = isHtransform(isConsonantKey(m_buffer.buf[1]))) > -1) &&
                isConsonantKey(m_buffer.buf[2]) == htransform) {
              composingText->appendText(m_tibetanHTransform[k], true);
            } else if (isConsonantKey(m_buffer.buf[1]) == 0 &&  // ka
                       isConsonantKey(m_buffer.buf[2]) == 34)   // ssa
            {
              composingText->appendText(string("\xE0\xBE\xB9"), true);
            } else {
              composingText->appendText(
                  m_tibetanConsonants[isConsonantKey(m_buffer.buf[1])], true);
              if (m_buffer.len == 4) {
                composingText->appendText(
                    m_tibetanConsonants[isConsonantKey(m_buffer.buf[2])], true);
              }
            }
            composingText->appendText(string("\xE0\xBE\xB9"), true);
            composingText->updateDisplay();
          }
          m_buffer.add(keycode);
          return true;
        }
      }

      if (i == htransform && m_buffer.len > 1) {  // H transform
        if ((k = isHtransform(isConsonantKey(m_buffer.buf[m_buffer.len - 1]))) >
            -1) {
          composingText->clear();
          composingText->updateDisplay();
          if (m_buffer.len < 3) {
            composingText->appendText(m_tibetanHTransform[k], true);
            composingText->updateDisplay();
          } else {  ///
            int l = -1;
            if (((l = isHtransform(isConsonantKey(m_buffer.buf[1]))) > -1) &&
                isConsonantKey(m_buffer.buf[2]) == htransform) {
              composingText->appendText(m_tibetanHTransform[l], true);
            } else if (isConsonantKey(m_buffer.buf[1]) == 0 &&  // ka
                       isConsonantKey(m_buffer.buf[2]) == 34)   // ssa
            {
              composingText->appendText(string("\xE0\xBE\xB9"), true);
            } else {
              composingText->appendText(
                  m_tibetanConsonants[isConsonantKey(m_buffer.buf[1])], true);
              if (m_buffer.len == 4) {
                composingText->appendText(
                    m_tibetanConsonants[isConsonantKey(m_buffer.buf[2])], true);
              }
            }
            composingText->appendText(m_tibetanLowerConsonants[k], true);
            composingText->updateDisplay();
          }
          m_buffer.add(keycode);
          return true;
        }
      }
      if (m_buffer.len > 1) {
        if (i == 22) {
          tmpString = string("\xE0\xBD\xB1");
        } else {
          tmpString = m_tibetanLowerConsonants[i];
        }
      }
      composingText->appendText(tmpString, true);
      composingText->updateDisplay();
      m_buffer.add(keycode);
      return true;
    }
    composingText->appendText(tmpString, true);
    composingText->commit();
    m_buffer.clear();

    return true;  // key processed
  }

  if (keycode == 'M' || keycode == '>') {
    composingText->setText(
        string("\xE0\xBD\xA7\xE0\xBD\xB1\xE0\xBD\xB4\xE0\xBE\x83"));
    composingText->commit();
    m_buffer.clear();
  }
  if (keycode == '+' || keycode == '!') {
    composingText->setText(string("\xE0\xBC\x84\xE0\xBC\x85"));
    composingText->commit();
    m_buffer.clear();
  }
  if (keycode == '=') {
    composingText->setText(string("\xE0\xBD\xA8\xE0\xBD\xB1\xE0\xBD\xBF"));
    composingText->commit();
    m_buffer.clear();
  }
  if (keycode == '<') {
    composingText->setText(string("\xE0\xBD\xA8\xE0\xBD\xBC\xE0\xBD\xBE"));
    composingText->commit();
    m_buffer.clear();
  }
  if (keycode == '^') {
    composingText->setText(string("\xE0\xBC\x81\xE0\xBE\x83"));
    composingText->commit();
    m_buffer.clear();
  }

  return true;
}

short OVIMTibetanContext::isSymbolKey(int key) {
  // Check if the keycode is mapping to a punctuation or a symbol
  int i;
  for (i = 0; i < SYMBOL_NUM; i++) {
    if (key == SymbolKeys[m_currentKeyboard][i]) return i;
  }
  return -1;
}

short OVIMTibetanContext::isConsonantKey(int key) {
  int i;
  for (i = 0; i < CONSONAT_NUM; i++) {
    if (key == ConsonantKeys[m_currentKeyboard][i]) return i;
  }
  return -1;
}

short OVIMTibetanContext::isVowelKey(int key) {
  int i;
  for (i = 0; i < VOWEL_NUM; i++) {
    if (key == VowelKeys[m_currentKeyboard][i]) return i;
  }
  return -1;
}

short OVIMTibetanContext::isFinalAddKey(int key) {
  int i;
  for (i = 0; i < FINALADD_NUM; i++) {
    if (key == FinalAddKeys[m_currentKeyboard][i]) return i;
  }
  return -1;
}

short OVIMTibetanContext::isHtransform(int key) {
  int i;
  for (i = 0; i < 5; i++) {
    if (key == htransformKey[i]) return i;
  }
  return -1;
}

OVEventHandlingContext* OVIMTibetan::createContext() {
  return new OVIMTibetanContext(m_currentKeyboard);
}

const string OVIMTibetan::identifier() const { return string("OVIMTibetan"); }

const string OVIMTibetan::localizedName(const string& locale) {
#ifndef _MSC_VER
  string bname = "བོད་སྐད་";
  string cname = "藏文";
  string jname = "チベット語";
#else
  string bname =
      "\xE0\xBD\x96\xE0\xBD\xBC\xE0\xBD\x91\xE0\xBC\x8B\xE0\xBD\xA6\xE0\xBE\x90"
      "\xE0\xBD\x91\xE0\xBC\x8B";
  string cname = "\xE8\x97\x8F\xE6\x96\x87";
  string jname = "\xE3\x83\x81\xE3\x83\x99\xE3\x83\x83\xE3\x83\x88\xE8\xAA\x9E";
#endif
  if (locale == "bo")
    return bname;
  else if (locale == "zh_TW" || locale == "zh-hant")
    return cname;
  else if (locale == "zh_CN" || locale == "zh-hans")
    return cname;
  else if (locale == "ja")
    return jname;
  else
    return string("Tibetan");
}

bool OVIMTibetan::initialize(OVPathInfo* pathInfo,
                             OVLoaderService* loaderService) {
  loaderService->logger("OVIMTibetan") << "Initializing" << endl;
  return true;
}

void OVIMTibetan::finalize() {}

void OVIMTibetan::loadConfig(OVKeyValueMap* moduleConfig,
                             OVLoaderService* loaderService) {
  if (moduleConfig->hasKey("KeyboardLayout"))
    m_cfgKeyboardLayout = moduleConfig->stringValueForKey("KeyboardLayout");

  if (m_cfgKeyboardLayout == "Sambhota Keymap One") {
    m_currentKeyboard = 0;
  } else if (m_cfgKeyboardLayout == "Sambhota Keymap Two") {
    m_currentKeyboard = 1;
  } else if (m_cfgKeyboardLayout == "TCC Keymboard #1") {
    m_currentKeyboard = 2;
  } else if (m_cfgKeyboardLayout == "TCC Keymboard #2") {
    m_currentKeyboard = 3;
  } else {
    m_cfgKeyboardLayout = "Sambhota Keymap One";
    m_currentKeyboard = 0;
  }

  loaderService->logger("OVIMTibetan") << "Load config" << endl;
}

void OVIMTibetan::saveConfig(OVKeyValueMap* moduleConfig,
                             OVLoaderService* loaderService) {
  loaderService->logger("OVIMTibetan") << "Save config" << endl;
  moduleConfig->setKeyStringValue("KeyboardLayout", m_cfgKeyboardLayout);
}
};  // namespace OpenVanilla
