// [AUTO_HEADER]

#include "CVEncodingService.h"

namespace OpenVanilla {

CVEncodingService::CVEncodingService() {
  NSFont* font = [NSFont fontWithName:@"STHeiti" size:[NSFont systemFontSize]];
  if (!font) {
    font = [NSFont fontWithName:@"AppleGothic" size:[NSFont systemFontSize]];
  }

  if (!font) {
    font = [NSFont systemFontOfSize:[NSFont systemFontSize]];
  }

  NSCharacterSet* charset = [font coveredCharacterSet];
  for (UTF32Char c = 0; c < 0x30000; c++) {
    if (!CFCharacterSetIsLongCharacterMember((CFCharacterSetRef)charset, c))
      m_systemFontUnsupportedCharacters.insert(c);
  }

  // NSLog(@"Of %d UTF-32 chars, the number of unsupported chars is %d",
  // 0x2ffff, m_systemFontUnsupportedCharacters.size());
}

CVEncodingService::~CVEncodingService() {}

bool CVEncodingService::codepointSupportedByEncoding(const string& codepoint,
                                                     const string& encoding) {
  if (encoding == "BIG-5") {
    NSString* nstr = [NSString stringWithUTF8String:codepoint.c_str()];
    return [nstr
        canBeConvertedToEncoding:CFStringConvertEncodingToNSStringEncoding(
                                     kCFStringEncodingBig5_HKSCS_1999)];
  } else if (encoding == "UTF-8") {
    return true;
  }

  return codepointSupportedBySystem(codepoint);
}

bool CVEncodingService::codepointSupportedBySystem(const string& codepoint) {
  UTF32Char u32 =
      (UTF32Char)OVUTF8Helper::CodePointFromSingleUTF8String(codepoint);
  if (m_systemFontUnsupportedCharacters.find(u32) !=
      m_systemFontUnsupportedCharacters.end())
    return false;

  // filter out many private-use sections; they are definitely not supported!
  if (u32 >= 0xe000 && u32 < 0xf900) return false;

  if (u32 >= 0xfff0 && u32 < 0x10000) return false;

  if (u32 > 0x2ffff) return false;

  return true;
}

const vector<string> CVEncodingService::supportedEncodings() {
  vector<string> results;
  results.push_back("UTF-8");
  results.push_back("BIG-5");
  return results;
}

bool CVEncodingService::isEncodingSupported(const string& encoding) {
  if (encoding == "BIG-5" || encoding == "UTF-8") return true;

  return false;
}

bool CVEncodingService::isEncodingConversionSupported(
    const string& fromEncoding, const string& toEncoding) {
  if (fromEncoding == "UTF-8" && toEncoding == "BIG-5") return true;

  return false;
}

const pair<bool, string> CVEncodingService::convertEncoding(
    const string& fromEncoding, const string& toEncoding, const string& text) {
  if (fromEncoding == "UTF-8" && toEncoding == "BIG-5") {
    NSString* nstr = [NSString stringWithUTF8String:text.c_str()];
    NSData* data =
        [nstr dataUsingEncoding:CFStringConvertEncodingToNSStringEncoding(
                                    kCFStringEncodingBig5_HKSCS_1999)
            allowLossyConversion:NO];

    if (data) {
      return pair<bool, string>(true,
                                string((char*)[data bytes], 0, [data length]));
    }
  }

  return pair<bool, string>(false, string());
}

};
