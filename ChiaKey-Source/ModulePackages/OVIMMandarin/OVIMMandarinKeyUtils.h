//
// OVIMMandarinKeyUtils.h
//
// Shared key helpers for Mandarin input methods.
//

#ifndef OVIMMandarinKeyUtils_h
#define OVIMMandarinKeyUtils_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "Mandarin.h"

namespace OpenVanilla {
using namespace std;
using namespace Formosa::Mandarin;

inline void OVIMMandarinPushUniqueString(vector<string>* items,
                                         const string& item) {
  if (!item.size()) return;

  for (vector<string>::const_iterator iter = items->begin();
       iter != items->end(); ++iter) {
    if (*iter == item) return;
  }

  items->push_back(item);
}

inline string OVIMMandarinShiftedASCIIString(unsigned int keyCode) {
  switch (keyCode) {
    case '`':
      return "~";
    case '1':
      return "!";
    case '2':
      return "@";
    case '3':
      return "#";
    case '4':
      return "$";
    case '5':
      return "%";
    case '6':
      return "^";
    case '7':
      return "&";
    case '8':
      return "*";
    case '9':
      return "(";
    case '0':
      return ")";
    case '-':
      return "_";
    case '=':
      return "+";
    case '[':
      return "{";
    case ']':
      return "}";
    case '\\':
      return "|";
    case ';':
      return ":";
    case '\'':
      return "\"";
    case ',':
      return "<";
    case '.':
      return ">";
    case '/':
      return "?";
    default:
      return string();
  }
}

inline vector<string> OVIMMandarinPunctuationKeyStringsForKey(
    const OVKey* key) {
  vector<string> results;
  if (!key) return results;

  unsigned int keyCode = key->keyCode();

  if (key->isShiftPressed()) {
    OVIMMandarinPushUniqueString(
        &results, OVIMMandarinShiftedASCIIString(keyCode));
  }

  OVIMMandarinPushUniqueString(&results, key->receivedString());

  if (keyCode >= 32 && keyCode <= 126) {
    OVIMMandarinPushUniqueString(&results, string(1, (char)keyCode));
  }

  return results;
}

inline vector<string> OVIMMandarinPunctuationQueriesForKey(
    const BopomofoKeyboardLayout* layout, const OVKey* key) {
  vector<string> results;
  vector<string> keyStrings = OVIMMandarinPunctuationKeyStringsForKey(key);
  string prefix = "_punctuation_";

  for (vector<string>::const_iterator iter = keyStrings.begin();
       iter != keyStrings.end(); ++iter) {
    if (layout) {
      OVIMMandarinPushUniqueString(
          &results, prefix + layout->name() + "_" + *iter);
    }

    OVIMMandarinPushUniqueString(&results, prefix + *iter);
  }

  return results;
}
};  // namespace OpenVanilla

#endif
