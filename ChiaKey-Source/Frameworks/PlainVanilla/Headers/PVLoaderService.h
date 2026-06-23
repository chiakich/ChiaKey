//
// PVLoaderService.h
//
// Copyright (c) 2007-2010 Lukhnos D. Liu (http://lukhnos.org)
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

#ifndef PVLoaderService_h
#define PVLoaderService_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#include <PlainVanilla/PVKeyImpl.h>
#else
#include "OpenVanilla.h"
#include "PVKeyImpl.h"
#endif

namespace OpenVanilla {
using namespace std;

class PVDefaultEncodingService : public OVEncodingService {
 public:
  virtual bool codepointSupportedByEncoding(const string&, const string&) {
    return true;
  }

  virtual bool codepointSupportedBySystem(const string&) { return true; }

  virtual const vector<string> supportedEncodings() {
    vector<string> result;
    result.push_back("UTF-8");
    return result;
  }

  virtual bool isEncodingSupported(const string& encoding) {
    return OVWildcard::Match(encoding, "UTF-8");
  }

  virtual bool isEncodingConversionSupported(const string& fromEncoding,
                                             const string& toEncoding) {
    return false;
  }

  virtual const pair<bool, string> convertEncoding(const string& fromEncoding,
                                                   const string& toEncoding,
                                                   const string& text) {
    return pair<bool, string>(false, string());
  }
};

class PVLogEmitter : public OVLogEmitter {
 public:
  virtual const string sectionName() const { return m_sectionName; }

  virtual void setSectionName(const string& sectionName) {
    m_sectionName = sectionName;
  }

  virtual void emitLog(const string& logEntry) {
    cerr << "PlainVanilla (" << m_sectionName << "): " << logEntry;
  }

 protected:
  string m_sectionName;
};

class PVLoaderHookedServiceDelegate : public OVBase {
 public:
  virtual void* loaderSpecificDataObjectForName(const string& name) = 0;
};

class PVLoaderService : public OVLoaderService {
 public:
  PVLoaderService(const string& locale = "en",
                  OVDatabaseService* CINService = 0,
                  OVDatabaseService* SQLiteService = 0,
                  OVLogEmitter* logEmitter = 0,
                  OVEncodingService* encodingService = 0)
      : m_locale(locale),
        m_CINDatabaseService(CINService),
        m_SQLiteDatabaseService(SQLiteService),
        m_logEmitter(logEmitter),
        m_ourLogEmitter(0),
        m_logStringBuffer(m_logEmitter),
        m_logStream(&m_logStringBuffer),
        m_shouldBeep(false),
        m_ourEncodingService(0),
        m_encodingService(encodingService),
        m_hookedServiceDelegate(0) {
    if (!logEmitter) {
      m_ourLogEmitter = new PVLogEmitter;
      m_logEmitter = m_ourLogEmitter;
      m_logStringBuffer.setLogEmitter(m_ourLogEmitter);
    }

    if (!encodingService) {
      m_ourEncodingService = new PVDefaultEncodingService;
      m_encodingService = m_ourEncodingService;
    }
  }

  ~PVLoaderService() {
    if (m_ourLogEmitter) delete m_ourLogEmitter;

    if (m_ourEncodingService) delete m_ourEncodingService;
  }

  virtual void beep() { m_shouldBeep = true; }

  virtual void notify(const string& message) {
    m_notifyMessage.push_back(message);
  }

  virtual void HTMLNotify(const string& content) {
    m_notifyMessage.push_back(content);
  }

  virtual const string locale() const { return m_locale; }

  virtual const OVKey makeOVKey(int characterCode, bool alt = false,
                                bool opt = false, bool ctrl = false,
                                bool shift = false, bool command = false,
                                bool capsLock = false, bool numLock = false) {
    return OVKey(new PVKeyImpl(
        characterCode,
        makeModifiers(alt, opt, ctrl, shift, command, capsLock, numLock)));
  }

  virtual const OVKey makeOVKey(const string& receivedString, bool alt = false,
                                bool opt = false, bool ctrl = false,
                                bool shift = false, bool command = false,
                                bool capsLock = false, bool numLock = false) {
    return OVKey(new PVKeyImpl(
        receivedString,
        makeModifiers(alt, opt, ctrl, shift, command, capsLock, numLock)));
  }

  virtual ostream& logger(const string& sectionName = "") {
    if (m_logEmitter) m_logEmitter->setSectionName(sectionName);

    return m_logStream;
  }

  virtual OVDatabaseService* defaultDatabaseService() {
    if (m_SQLiteDatabaseService) return m_SQLiteDatabaseService;

    return m_CINDatabaseService;
  }

  virtual OVDatabaseService* CINDatabaseService() {
    return m_CINDatabaseService;
  }

  virtual OVDatabaseService* SQLiteDatabaseService() {
    return m_SQLiteDatabaseService;
  }

  virtual OVEncodingService* encodingService() { return m_encodingService; }

 public:
  virtual void resetState() {
    m_shouldBeep = false;
    m_notifyMessage.clear();

    // <lithoglyph>
    m_dictionaryKeyword.clear();
    m_URLToOpen.clear();
    m_log.clear();
    m_prompt.clear();
    m_promptDescription.clear();

    m_loaderFeatureKey.clear();
    m_loaderFeatureValue.clear();
    // </lithoglyph>
  }

  virtual vector<string> notifyMessage() const { return m_notifyMessage; }

  virtual bool shouldBeep() const { return m_shouldBeep; }

  virtual void setSQLiteDatabaseService(OVDatabaseService* newService) {
    m_SQLiteDatabaseService = newService;
  }

  virtual void setCINDatabaseService(OVDatabaseService* newService) {
    m_CINDatabaseService = newService;
  }

  // <lithoglyph>
  virtual void setHookedServiceDelegate(
      PVLoaderHookedServiceDelegate* newDelegate) {
    m_hookedServiceDelegate = newDelegate;
  }

  virtual void lookupInDefaultDictionary(const string& keyword) {
    m_dictionaryKeyword = keyword;
  }

  virtual void openURL(const string& URL) { m_URLToOpen = URL; }

  virtual string dicitonaryKeyword() const { return m_dictionaryKeyword; }

  virtual string URLToOpen() const { return m_URLToOpen; }

  virtual void setPrompt(const string& text) { m_prompt = text; }

  virtual const string prompt() const { return m_prompt; }

  virtual void setPromptDescription(const string& text) {
    m_promptDescription = text;
  }

  virtual const string promptDescription() { return m_promptDescription; }

  virtual void setLog(const string& text) { m_log = text; }

  virtual const string log() { return m_log; }

  virtual void callLoaderFeature(const string& key, const string& value) {
    m_loaderFeatureKey = key;
    m_loaderFeatureValue = value;
  }

  virtual const string loaderFeatureKey() { return m_loaderFeatureKey; }

  virtual const string loaderFeatureValue() { return m_loaderFeatureValue; }

  virtual void* loaderSpecificDataObjectForName(const string& name) {
    return m_hookedServiceDelegate
               ? m_hookedServiceDelegate->loaderSpecificDataObjectForName(name)
               : 0;
  }

  // </lithoglyph>

 protected:
  virtual unsigned int makeModifiers(bool alt = false, bool opt = false,
                                     bool ctrl = false, bool shift = false,
                                     bool command = false,
                                     bool capsLock = false,
                                     bool numLock = false) {
    unsigned int modifiers = 0;
    if (alt) modifiers |= OVKeyMask::Alt;
    if (opt) modifiers |= OVKeyMask::Opt;
    if (ctrl) modifiers |= OVKeyMask::Ctrl;
    if (shift) modifiers |= OVKeyMask::Shift;
    if (command) modifiers |= OVKeyMask::Command;
    if (capsLock) modifiers |= OVKeyMask::CapsLock;
    if (numLock) modifiers |= OVKeyMask::NumLock;

    return modifiers;
  }

  OVDatabaseService* m_CINDatabaseService;
  OVDatabaseService* m_SQLiteDatabaseService;
  OVEncodingService* m_encodingService;
  bool m_shouldBeep;
  vector<string> m_notifyMessage;
  string m_locale;
  OVLogEmitter* m_logEmitter;
  OVLogStringBuffer m_logStringBuffer;
  ostream m_logStream;

  PVLogEmitter* m_ourLogEmitter;
  PVDefaultEncodingService* m_ourEncodingService;

  PVLoaderHookedServiceDelegate* m_hookedServiceDelegate;

  // <lithoglyph>
  string m_dictionaryKeyword;
  string m_URLToOpen;
  string m_prompt;
  string m_promptDescription;
  string m_log;
  string m_loaderFeatureKey;
  string m_loaderFeatureValue;
  // </lithoglyph>
};
};  // namespace OpenVanilla

#endif