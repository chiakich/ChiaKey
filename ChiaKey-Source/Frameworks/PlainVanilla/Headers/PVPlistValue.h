//
// PVPlistValue.h
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

#ifndef PVPlistValue_h
#define PVPlistValue_h

#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace OpenVanilla {
using namespace std;

// ownership policy: manage your own objects, and do NOT manage objects returned
// by PVPlistValue's member functions (arrayElementAtIndex and valueForKey)
class PVPlistValue {
 public:
  enum { String = 1, Array = 2, Dictionary = 3 };

  PVPlistValue(int type)
      : m_type(type), m_stringValue(0), m_dictionaryValue(0), m_arrayValue(0) {
    switch (type) {
      case String:
        m_stringValue = new string;
        break;
      case Array:
        m_arrayValue = new vector<PVPlistValue*>;
        break;
      case Dictionary:
        m_dictionaryValue = new map<string, PVPlistValue*>;
        break;
    }
  }

  PVPlistValue(const string& stringValue)
      : m_type(String),
        m_stringValue(new string(stringValue)),
        m_dictionaryValue(0),
        m_arrayValue(0) {}

  ~PVPlistValue() {
    if (m_stringValue) {
      delete m_stringValue;
    } else if (m_arrayValue) {
      vector<PVPlistValue*>::iterator viter;
      for (viter = m_arrayValue->begin(); viter != m_arrayValue->end();
           viter++) {
        if (*viter) delete *viter;
      }

      delete m_arrayValue;
    } else if (m_dictionaryValue) {
      map<string, PVPlistValue*>::iterator miter;
      for (miter = m_dictionaryValue->begin();
           miter != m_dictionaryValue->end(); miter++) {
        if ((*miter).second) delete (*miter).second;
      }

      delete m_dictionaryValue;
    }
  }

  PVPlistValue* copy() {
    PVPlistValue* newValue = new PVPlistValue(m_type);
    vector<PVPlistValue*>::iterator viter;
    map<string, PVPlistValue*>::iterator miter;

    switch (m_type) {
      case String:
        newValue->setStringValue(*m_stringValue);
        break;

      case Array:
        for (viter = m_arrayValue->begin(); viter != m_arrayValue->end();
             ++viter) {
          if (*viter)
            newValue->m_arrayValue->push_back((*viter)->copy());
          else
            newValue->m_arrayValue->push_back(
                PVPlistValue(PVPlistValue::String).copy());
        }
        break;

      case Dictionary:
        for (miter = m_dictionaryValue->begin();
             miter != m_dictionaryValue->end(); miter++) {
          string key = (*miter).first;
          if ((*miter).second)
            (*(newValue->m_dictionaryValue))[key] = (*miter).second->copy();
          else
            (*(newValue->m_dictionaryValue))[key] =
                PVPlistValue(PVPlistValue::String).copy();
        }
        break;
    }

    return newValue;
  }

  int type() const { return m_type; }

  const string stringValue() {
    if (m_stringValue) return string(*m_stringValue);
    return string();
  }

  void setStringValue(const string& newString) {
    if (!m_stringValue) return;
    *m_stringValue = newString;
  }

  void arrayClear() {
    if (m_arrayValue) {
      for (vector<PVPlistValue*>::iterator mai = m_arrayValue->begin();
           mai != m_arrayValue->end(); ++mai) {
        if (*mai) delete *mai;
      }

      m_arrayValue->clear();
    }
  }

  size_t arraySize() {
    if (m_arrayValue) return m_arrayValue->size();
    return 0;
  }

  PVPlistValue* arrayElementAtIndex(size_t index) {
    if (m_arrayValue)
      return index < m_arrayValue->size() ? (*m_arrayValue)[index] : 0;
    return 0;
  }

  void addArrayElement(PVPlistValue* value) {
    if (!m_arrayValue) return;

    if (value) m_arrayValue->push_back(value->copy());
  }

  void removeAllArrayElements() {
    if (!m_arrayValue) return;

    for (vector<PVPlistValue*>::iterator ai = m_arrayValue->begin();
         ai != m_arrayValue->end(); ++ai) {
      if (*ai) delete *ai;
    }

    m_arrayValue->clear();
  }

  void setArrayElementAtIndex(PVPlistValue* value, size_t index) {
    if (!m_arrayValue) return;

    if (index >= m_arrayValue->size()) {
      size_t inc = m_arrayValue->size();
      for (; inc < index; inc++) {
        m_arrayValue->push_back(PVPlistValue(PVPlistValue::String).copy());
      }
      m_arrayValue->push_back(value);
    } else {
      PVPlistValue* oldValue;
      if (oldValue == (*m_arrayValue)[index]) {
        delete oldValue;
      }

      if (value) (*m_arrayValue)[index] = value->copy();
    }
  }

  PVPlistValue* valueForKey(const string& key) {
    if (!m_dictionaryValue) return 0;
    if (m_dictionaryValue->find(key) != m_dictionaryValue->end())
      return (*m_dictionaryValue)[key];

    return 0;
  }

  const string stringValueForKey(const string& key) {
    string result;
    if (!m_dictionaryValue) return result;
    if (m_dictionaryValue->find(key) != m_dictionaryValue->end()) {
      PVPlistValue* v = (*m_dictionaryValue)[key];
      return v->stringValue();
    }

    return result;
  }

  bool isKeyTrue(const string& key) {
    if (!m_dictionaryValue) return false;
    if (m_dictionaryValue->find(key) != m_dictionaryValue->end()) {
      PVPlistValue* value = (*m_dictionaryValue)[key];
      if (!value->m_stringValue) {
        return false;
      }

      if (atoi((*value->m_stringValue).c_str()) > 0) {
        return true;
      }

      return (*value->m_stringValue) == "true";
    }

    return false;
  }

  void setKeyValue(const string& key, const string& value) {
    PVPlistValue stringValue(value);
    setKeyValue(key, &stringValue);
  }

  void setKeyValue(const string& key, PVPlistValue* value) {
    if (!m_dictionaryValue) return;
    if (!key.length()) return;

    PVPlistValue* oldValue = (*m_dictionaryValue)[key];
    if (oldValue) delete oldValue;

    m_dictionaryValue->erase(key);

    if (value) (*m_dictionaryValue)[key] = value->copy();
  }

  void removeAllKeysAndValues() {
    if (!m_dictionaryValue) return;

    vector<PVPlistValue*> values;

    map<string, PVPlistValue*>::iterator miter;
    for (miter = m_dictionaryValue->begin(); miter != m_dictionaryValue->end();
         ++miter) {
      if ((*miter).second) values.push_back((*miter).second);
    }

    m_dictionaryValue->clear();
    for (vector<PVPlistValue*>::iterator viter = values.begin();
         viter != values.end(); ++viter) {
      delete *viter;
    }
  }

  const vector<string> dictionaryKeys() const {
    vector<string> result;
    if (!m_dictionaryValue) {
      return result;
    }

    map<string, PVPlistValue*>::iterator miter;
    for (miter = m_dictionaryValue->begin(); miter != m_dictionaryValue->end();
         ++miter) {
      if ((*miter).second) result.push_back((*miter).first);
    }
    return result;
  }

  bool operator!=(const PVPlistValue& value) { return !(*this == value); }

  bool operator==(const PVPlistValue& value) {
    if (m_type != value.type()) return false;

    size_t vsize;
    vector<string> k1;
    vector<string> k2;
    bool allEqual = true;

    switch (m_type) {
      case String:
        return *m_stringValue == *(value.m_stringValue);
      case Array:
        vsize = m_arrayValue->size();

        if (vsize != value.m_arrayValue->size()) return false;

        for (size_t vindex = 0; vindex < vsize; vindex++) {
          PVPlistValue* v1 = (*m_arrayValue)[vindex];
          PVPlistValue* v2 = (*value.m_arrayValue)[vindex];

          if (v1 && v2) {
            if (allEqual != (v1 == v2)) break;
          } else if (v1 != v2) {
            allEqual = false;
            break;
          }
        }
        return allEqual;

      case Dictionary:
        k1 = dictionaryKeys();
        k2 = value.dictionaryKeys();

        if (k1.size() != k2.size()) return false;

        vector<string>::iterator viter = k1.begin();
        for (; viter != k1.end(); ++viter) {
          PVPlistValue* mv1 = (*m_dictionaryValue)[*viter];
          PVPlistValue* mv2 = (*(value.m_dictionaryValue))[*viter];

          if (mv1 && mv2) {
            if (allEqual != (mv1 == mv2)) break;
          } else if (mv1 != mv2) {
            allEqual = false;
            break;
          }
        }
        return allEqual;
    }

    return false;
  }

  OVKeyValueMap keyValueMap() { return makeKeyValueMap(false); }

  OVKeyValueMap readOnlyKeyValueMap() { return makeKeyValueMap(true); }

  friend ostream& operator<<(ostream&, const PVPlistValue&);
  static const string XMLEntityEscapedString(const string& s) {
    string r;
    for (string::const_iterator i = s.begin(); i != s.end(); ++i) {
      if (*i == '&') {
        r += "&amp;";
      } else if (*i == '<') {
        r += "&lt;";
      } else if (*i == '>') {
        r += "&gt;";
      } else {
        r += *i;
      }
    }

    return r;
  }

  const string asString() const {
    stringstream sst;

    sst << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;

#ifndef WIN32
    // we suppress DTD because this really slows .NET's XML parser down...
    sst << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
           "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
        << endl;
#endif

    sst << "<plist version=\"1.0\">" << endl;
    sst << *this;
    sst << "</plist>" << endl;

    return sst.str();
  }

 protected:
  OVKeyValueMap makeKeyValueMap(bool readOnly) {
    if (m_type != Dictionary) return OVKeyValueMap(new NullKeyValueMapImpl);

    return OVKeyValueMap(
        new DictionaryKeyValueMapImpl(m_dictionaryValue, readOnly));
  }

  ostream& descendDump(ostream& stream, int indent = 0,
                       int perIndent = 1) const {
    vector<PVPlistValue*>::iterator viter;
    map<string, PVPlistValue*>::iterator miter;

    switch (m_type) {
      case String:
        stream << string(indent, '\t') << "<string>"
               << PVPlistValue::XMLEntityEscapedString(*m_stringValue)
               << "</string>" << endl;
        break;

      case Array:
        if (!m_arrayValue->size()) {
          stream << string(indent, '\t') << "<array/>" << endl;
        } else {
          stream << string(indent, '\t') << "<array>" << endl;
          for (viter = m_arrayValue->begin(); viter != m_arrayValue->end();
               viter++) {
            if (*viter)
              (*viter)->descendDump(stream, indent + perIndent, perIndent);
            else
              stream << string(indent + perIndent, '\t') << "<string></string>"
                     << endl;
          }
          stream << string(indent, '\t') << "</array>" << endl;
        }
        break;

      case Dictionary:
        if (!m_dictionaryValue->size()) {
          stream << string(indent, '\t') << "<dict/>" << endl;
        } else {
          stream << string(indent, '\t') << "<dict>" << endl;

          for (miter = m_dictionaryValue->begin();
               miter != m_dictionaryValue->end(); miter++) {
            stream << string(indent + perIndent, '\t') << "<key>"
                   << (*miter).first << "</key>" << endl;
            if ((*miter).second)
              (*miter).second->descendDump(stream, indent + perIndent,
                                           perIndent);
            else
              stream << string(indent + perIndent, '\t') << "<string></string>"
                     << endl;
          }
          stream << string(indent, '\t') << "</dict>" << endl;
        }
        break;
    }
    return stream;
  }

  int m_type;
  string* m_stringValue;
  map<string, PVPlistValue*>* m_dictionaryValue;
  vector<PVPlistValue*>* m_arrayValue;

 protected:
  class NullKeyValueMapImpl : public OVKeyValueMapImpl {
   public:
    virtual bool shouldDelete() { return true; }

    virtual OVKeyValueMapImpl* copy() { return new NullKeyValueMapImpl; }

    virtual bool isReadOnly() { return true; }

    virtual bool setKeyStringValue(const string&, const string&) {
      return false;
    }

    virtual bool hasKey(const string&) { return false; }

    virtual const string stringValueForKey(const string&) { return string(); }
  };

  class DictionaryKeyValueMapImpl : public OVKeyValueMapImpl {
   public:
    DictionaryKeyValueMapImpl(map<string, PVPlistValue*>* dictionaryValue,
                              bool readOnly = false)
        : m_dictionaryValue(dictionaryValue), m_readOnly(readOnly) {}

    virtual bool shouldDelete() { return true; }
    virtual OVKeyValueMapImpl* copy() {
      return new DictionaryKeyValueMapImpl(m_dictionaryValue, m_readOnly);
    }

    virtual bool isReadOnly() { return m_readOnly; }

    virtual bool setKeyStringValue(const string& key, const string& value) {
      if (!m_readOnly) {
        PVPlistValue* oldValue = (*m_dictionaryValue)[key];
        if (oldValue) delete oldValue;

        (*m_dictionaryValue)[key] = PVPlistValue(value).copy();
        return true;
      }

      return false;
    }

    virtual bool hasKey(const string& key) {
      return m_dictionaryValue->find(key) != m_dictionaryValue->end();
    }

    virtual const string stringValueForKey(const string& key) {
      PVPlistValue* value = ((*m_dictionaryValue))[key];
      if (!value) return string();

      return value->stringValue();
    }

   protected:
    bool m_readOnly;
    map<string, PVPlistValue*>* m_dictionaryValue;
  };
};

inline ostream& operator<<(ostream& stream, const PVPlistValue& value) {
  return value.descendDump(stream);
}

};  // namespace OpenVanilla

#endif
