//
// OVIMTibetan.h
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

#ifndef OVIMTibetan_h
#define OVIMTibetan_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include <vector>

#define VOWEL 2
#define CONSONAT 1
#define OTHER 0

#define SYMBOL_NUM 11
#define CONSONAT_NUM 35
#define VOWEL_NUM 8
#define FINALADD_NUM 2
#define MAX_COMPOSE 5
#define KEYBOARDS 4

namespace OpenVanilla {
using namespace std;

class TibetanKeyBuffer {
 public:
  TibetanKeyBuffer() {
    len = 0;
    buf[0] = 0;
  }
  void add(char c) {
    if (len == 10) return;
    buf[len++] = c;
  }
  void remove() {
    if (!len) return;
    buf[--len] = 0;
  }
  void clear() {
    len = 0;
    buf[0] = 0;
  }
  void lastIsVowel() { last = VOWEL; }
  void lastIsConsonant() { last = CONSONAT; }
  void lastIsOther() { last = OTHER; }
  char buf[10];
  int len;
  int last;
};

class OVIMTibetanContext : public OVEventHandlingContext {
 public:
  OVIMTibetanContext(int currentKeyboard);
  virtual void startSession(OVLoaderService* loaderService);
  virtual void stopSession(OVLoaderService* loaderService);
  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService);
  virtual short isSymbolKey(int key);
  virtual short isConsonantKey(int key);
  virtual short isVowelKey(int key);
  virtual short isFinalAddKey(int key);
  virtual short isHtransform(int key);
  virtual string lowerString(string from);
  virtual string upperString(string from);

 protected:
  unsigned int m_currentKeyboard;
  TibetanKeyBuffer m_buffer;
  vector<string> m_tibetanNumbers;
  vector<string> m_tibetanFinalAdd;
  vector<string> m_tibetanSymbols;
  vector<string> m_tibetanVowel;
  vector<string> m_tibetanHTransform;
  vector<string> m_tibetanConsonants;
  vector<string> m_tibetanLowerConsonants;
  string m_tmpString;
};

class OVIMTibetan : public OVInputMethod {
 public:
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
  string m_cfgKeyboardLayout;
  unsigned int m_currentKeyboard;
};
};  // namespace OpenVanilla

#endif
