//
// PVKeyImpl.h
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

#ifndef PVKeyImpl_h
#define PVKeyImpl_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace OpenVanilla {
using namespace std;

class PVKeyImpl : public OVKeyImpl {
 public:
  PVKeyImpl(const string& receivedString, unsigned int modifiers = 0)
      : m_keyCode(0), m_modifiers(modifiers), m_receviedString(receivedString) {
    m_keyCode = 0;

    if (receivedString.length() > 0)
      if ((unsigned char)receivedString[0] < 128) m_keyCode = receivedString[0];
  }

  PVKeyImpl(const string& receivedString, unsigned int keyCode,
            unsigned int modifiers)
      : m_keyCode(keyCode),
        m_modifiers(modifiers),
        m_receviedString(receivedString) {}

  PVKeyImpl(unsigned int keyCode = 0, unsigned int modifiers = 0)
      : m_keyCode(keyCode), m_modifiers(modifiers) {
    if (keyCode >= 32 && keyCode <= 126) m_receviedString = string(1, keyCode);
  }

  virtual bool shouldDelete() const { return true; }

  virtual OVKeyImpl* copy() {
    PVKeyImpl* newInstance = new PVKeyImpl(m_keyCode, m_modifiers);
    newInstance->m_receviedString = m_receviedString;
    return newInstance;
  }

  virtual const string receivedString() const { return m_receviedString; }

  virtual unsigned int keyCode() const { return m_keyCode; }

  virtual bool isAltPressed() const { return m_modifiers & OVKeyMask::Alt; }

  virtual bool isOptPressed() const { return !!(m_modifiers & OVKeyMask::Opt); }
  virtual bool isCtrlPressed() const {
    return !!(m_modifiers & OVKeyMask::Ctrl);
  }

  virtual bool isShiftPressed() const {
    return !!(m_modifiers & OVKeyMask::Shift);
  }

  virtual bool isCommandPressed() const {
    return !!(m_modifiers & OVKeyMask::Command);
  }

  virtual bool isNumLockOn() const {
    return !!(m_modifiers & OVKeyMask::NumLock);
  }

  virtual bool isCapsLockOn() const {
    return !!(m_modifiers & OVKeyMask::CapsLock);
  }

  virtual bool isDirectTextKey() const {
    return !!(m_modifiers & OVKeyMask::DirectText);
  }

 protected:
  unsigned int m_keyCode;
  unsigned int m_modifiers;
  string m_receviedString;
};
};  // namespace OpenVanilla

#endif