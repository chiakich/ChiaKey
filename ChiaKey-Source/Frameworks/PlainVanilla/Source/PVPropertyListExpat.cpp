//
// PVPropertyListExpat.cpp
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

#include <expat.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <set>
#include <sstream>
#include <string>

#include "OVBase.h"
#include "OVFileHelper.h"
#include "OVKeyValueMap.h"
#include "PVPlistValue.h"
#include "PVPropertyList.h"

namespace OpenVanilla {

class PVExpatPlistParser {
 protected:
  PVPlistValue* last;
  vector<PVPlistValue*> stack;
  vector<string> keyStack;
  PVPlistValue* current;

  PVPlistValue* validLast() {
    if (!stack.size() && !keyStack.size() && last) return last->copy();

    return 0;
  }

 public:
  PVExpatPlistParser() : current(0), last(0) {
    if (!stringBuffer) stringBuffer = new string;
  }

  ~PVExpatPlistParser() {
    set<PVPlistValue*> s;

    s.insert(current);
    s.insert(last);

    for (vector<PVPlistValue*>::iterator iter = stack.begin();
         iter != stack.end(); ++iter)
      s.insert(*iter);

    for (set<PVPlistValue*>::iterator siter = s.begin(); siter != s.end();
         ++siter)
      if (*siter) delete *siter;
  }

  static PVPlistValue* Parse(const char* buf, size_t bufSize = 0) {
    PVExpatPlistParser parsedData;

    size_t readSize = bufSize ? bufSize : strlen(buf);

    XML_Parser parser = XML_ParserCreate("UTF-8");
    XML_SetElementHandler(parser, PVExpatPlistParser::Start,
                          PVExpatPlistParser::End);
    XML_SetCharacterDataHandler(parser, PVExpatPlistParser::CharData);
    XML_SetUserData(parser, &parsedData);

    try {
      XML_Parse(parser, buf, (int)readSize, 1);
    } catch (...) {
      XML_ParserFree(parser);
      return 0;
    }
    XML_ParserFree(parser);

    PVPlistValue* last = parsedData.validLast();
    return last;
  }

 protected:
  static void Start(void* data, const char* el, const char** attr) {
    PVExpatPlistParser* parsedData = (PVExpatPlistParser*)data;

    string element = el;
    *stringBuffer = "";

    if (element == "plist") {
    } else if (element == "dict") {
      if (parsedData->stack.size())
        parsedData->stack.push_back(parsedData->current);

      parsedData->current = new PVPlistValue(PVPlistValue::Dictionary);

      if (!parsedData->stack.size()) {
        parsedData->stack.push_back(parsedData->current);
      }
    } else if (element == "array") {
      parsedData->stack.push_back(parsedData->current);
      parsedData->current = new PVPlistValue(PVPlistValue::Array);
    } else if (element == "string" || element == "real") {
      parsedData->stack.push_back(parsedData->current);
      parsedData->current = new PVPlistValue(PVPlistValue::String);
    } else {
      parsedData->stack.push_back(parsedData->current);
      parsedData->current = 0;
    }
  }

  static void End(void* data, const char* el) {
    PVExpatPlistParser* parsedData = (PVExpatPlistParser*)data;
    if (!parsedData->stack.size()) return;

    string element = el;

    PVPlistValue* previous = parsedData->stack.back();

    parsedData->stack.pop_back();
    if (!parsedData->stack.size()) {
      parsedData->last = previous;
      parsedData->current = 0;
      return;
    }

    PVPlistValue* current = parsedData->current;
    if (current) {
      if (current->type() == PVPlistValue::String)
        current->setStringValue(*stringBuffer);

      if (previous) {
        switch (previous->type()) {
          case PVPlistValue::Array:
            previous->addArrayElement(current);
            break;
          case PVPlistValue::Dictionary:
            if (!parsedData->keyStack.size()) {
              throw 1;
            }

            string key = parsedData->keyStack.back();
            previous->setKeyValue(key, current);
            parsedData->keyStack.pop_back();
            break;
        }
      }

      delete current;
    }

    if (element == "key") parsedData->keyStack.push_back(*stringBuffer);

    parsedData->current = previous;
  }

  static void CharData(void* data, const XML_Char* s, int len) {
    *stringBuffer += string(s, len);
  }

  static string* stringBuffer;
};

string* PVExpatPlistParser::stringBuffer;
};  // namespace OpenVanilla

namespace OpenVanilla {

PVPlistValue* PVPropertyList::ParsePlistFromString(const char* stringData) {
  return PVExpatPlistParser::Parse(stringData, strlen(stringData));
}

PVPlistValue* PVPropertyList::ParsePlist(const string& filename) {
  pair<char*, size_t> data = OVFileHelper::SlurpFile(filename);
  if (!data.first || !data.second) {
    return 0;
  }

  PVPlistValue* dictValue = PVExpatPlistParser::Parse(data.first, data.second);
  free(data.first);
  return dictValue;
}

void PVPropertyList::WritePlist(const string& filename,
                                PVPlistValue* rootDictionary) {
  if (!rootDictionary) return;

  if (rootDictionary->type() != PVPlistValue::Dictionary) return;

  FILE* f = OVFileHelper::OpenStream(filename, "w");
  if (!f) {
    return;
  }

  stringstream sst;
  sst << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;

#ifndef WIN32
  // we suppress DTD because this really slows .NET's XML parser down...
  ofs << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
         "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
      << endl;
#endif

  sst << "<plist version=\"1.0\">" << endl;
  sst << *rootDictionary;
  sst << "</plist>" << endl;

  fputs(sst.str().c_str(), f);
  fclose(f);
}

};  // namespace OpenVanilla
