//
// OVOFEscape.h
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

#ifndef OVOFEscape_h
#define OVOFEscape_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include <vector>

namespace OpenVanilla {
using namespace std;

// OVOFEscapeCString

class OVOFEscapeCStringContext : public OVEventHandlingContext {
 public:
  OVOFEscapeCStringContext(string cfgCase);
  virtual const string filterText(const string& inputText,
                                  OVLoaderService* loaderService);

 private:
  string m_cfgCase;
};

class OVOFEscapeCString : public OVOutputFilter {
 public:
  virtual const string localizedName(const string& locale) {
    return string("Escape C String (UTF-8)");
  }

  virtual const string identifier() const { return "OVOFEscapeCString"; }

  virtual OVEventHandlingContext* createContext() {
    return new OVOFEscapeCStringContext(m_cfgCase);
  }
  virtual void loadConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService);
  virtual void saveConfig(OVKeyValueMap* moduleConfig,
                          OVLoaderService* loaderService);

 private:
  string m_cfgCase;
};

// OVOFEscapeURL

class OVOFEscapeURLContext : public OVEventHandlingContext {
 public:
  virtual const string filterText(const string& inputText,
                                  OVLoaderService* loaderService);
};

class OVOFEscapeURL : public OVOutputFilter {
 public:
  virtual const string localizedName(const string& locale) {
    return string("Escape UTF-8 URL (RFC 2396)");
  }

  virtual const string identifier() const { return "OVOFEscapeURL"; }

  virtual OVEventHandlingContext* createContext() {
    return new OVOFEscapeURLContext;
  }
};

// OVOFEscapeC99

class OVOFEscapeC99Context : public OVEventHandlingContext {
 public:
  virtual const string filterText(const string& inputText,
                                  OVLoaderService* loaderService);
};

class OVOFEscapeC99 : public OVOutputFilter {
 public:
  virtual const string localizedName(const string& locale) {
    return string("Escape String in C99/Java Format (UTF-16)");
  }

  virtual const string identifier() const { return "OVOFEscapeC99"; }

  virtual OVEventHandlingContext* createContext() {
    return new OVOFEscapeC99Context;
  }
};

};  // namespace OpenVanilla

#endif