// [AUTO_HEADER]

#ifndef CVEncodingService_h
#define CVEncodingService_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>

#include <set>

namespace OpenVanilla {
using namespace std;

class CVEncodingService : public OVEncodingService {
 public:
  CVEncodingService();
  virtual ~CVEncodingService();
  virtual bool codepointSupportedByEncoding(const string&, const string&);
  virtual bool codepointSupportedBySystem(const string&);
  virtual const vector<string> supportedEncodings();
  virtual bool isEncodingSupported(const string& encoding);

  virtual bool isEncodingConversionSupported(const string& fromEncoding,
                                             const string& toEncoding);
  virtual const pair<bool, string> convertEncoding(const string& fromEncoding,
                                                   const string& toEncoding,
                                                   const string& text);

 protected:
  set<UTF32Char> m_systemFontUnsupportedCharacters;
};

};  // namespace OpenVanilla

#endif