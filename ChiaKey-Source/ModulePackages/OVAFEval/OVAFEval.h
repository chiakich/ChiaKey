//
// OVAFEval.h
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

#ifndef OVAFEval_h
#define OVAFEval_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#ifndef OVAFEVAL_IDENTIFIER
#define OVAFEVAL_IDENTIFIER "OVAFEval"
#endif

#include "Evalgelion.h"

namespace OpenVanilla {
using namespace std;

class OVAFEval;

class OVAFEvalContext : public OVEventHandlingContext {
 public:
  OVAFEvalContext(OVAFEval* module)
      : m_module(module),
        m_enabled(false),
        m_interactive(false),
        m_cursor(m_expression.begin()) {}

  virtual bool handleKey(OVKey* key, OVTextBuffer* readingText,
                         OVTextBuffer* composingText,
                         OVCandidateService* candidateService,
                         OVLoaderService* loaderService);

 protected:
  OVAFEval* m_module;
  string m_expression;
  string::iterator m_cursor;
  bool m_enabled;
  bool m_interactive;
};

class OVAFEval : public OVAroundFilter {
 public:
  virtual OVEventHandlingContext* createContext() {
    return new OVAFEvalContext(this);
  }

  virtual bool initialize(OVPathInfo* pathInfo,
                          OVLoaderService* loaderService) {
    return true;
  }

  const string localizedName(const string& locale);
  const string identifier() const;

 protected:
  friend class OVAFEvalContext;
  Evalgelion::Evaluator m_eval;
};
};  // namespace OpenVanilla

#endif