//
// PVBasicKeyValueMapImpl.h
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

#ifndef PVBasicKeyValueImpl_h
#define PVBasicKeyValueImpl_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include <map>

namespace OpenVanilla {
using namespace std;

class PVBasicKeyValueMapImpl : public OVKeyValueMapImpl {
 public:
  PVBasicKeyValueMapImpl() : m_readOnly(false) {}

  virtual bool shouldDelete() { return false; }

  virtual OVKeyValueMapImpl* copy() { return this; }

  virtual bool isReadOnly() { return m_readOnly; }

  virtual bool setKeyStringValue(const string& key, const string& value) {
    if (m_readOnly) return false;

    m_keyValueMap[key] = value;
    return true;
  }
  virtual bool hasKey(const string& key) {
    map<string, string>::iterator it = m_keyValueMap.find(key);
    return it != m_keyValueMap.end();
  }

  virtual const string stringValueForKey(const string& key) {
    map<string, string>::iterator it = m_keyValueMap.find(key);
    if (it == m_keyValueMap.end()) return string();

    return (*it).second;
  }

 public:
  virtual void setReadOnly(bool readOnly) { m_readOnly = readOnly; }

 protected:
  bool m_readOnly;
  map<string, string> m_keyValueMap;
};
};  // namespace OpenVanilla

#endif