//
// Version.h
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

#ifndef Version_h
#define Version_h

#include <iostream>
#include <string>

#if defined(__APPLE__)
#include <PlainVanilla/PlainVanilla.h>
#else
#include "PlainVanilla.h"
#endif

namespace CareService {
using namespace std;
using namespace OpenVanilla;

class VersionNumber {
 public:
  VersionNumber(unsigned int major = 0, unsigned int minor = 0,
                unsigned tiny = 0, unsigned int buildNumber = 0)
      : m_major(0), m_minor(0), m_tiny(0), m_buildNumber(0) {}

  VersionNumber(const string& verStr)
      : m_major(0), m_minor(0), m_tiny(0), m_buildNumber(0) {
    vector<string> vvec = OVStringHelper::Split(verStr, '.');

    if (!vvec.size()) return;

    if (vvec.size() == 1) {
      m_buildNumber = atoi(vvec[0].c_str());
    } else if (vvec.size() == 2) {
      m_major = atoi(vvec[0].c_str());
      m_minor = atoi(vvec[1].c_str());
    } else if (vvec.size() == 3) {
      m_major = atoi(vvec[0].c_str());
      m_minor = atoi(vvec[1].c_str());
      m_buildNumber = atoi(vvec[2].c_str());
    } else {
      m_major = atoi(vvec[0].c_str());
      m_minor = atoi(vvec[1].c_str());
      m_tiny = atoi(vvec[2].c_str());
      m_buildNumber = atoi(vvec[3].c_str());
    }
  }

  bool operator==(const VersionNumber& another) const {
    return m_major == another.m_major && m_minor == another.m_minor &&
           m_tiny == another.m_tiny && m_buildNumber == another.m_buildNumber;
  }

  bool operator<(const VersionNumber& another) const {
    if (m_major != another.m_major) {
      return m_major < another.m_major;
    } else {
      if (m_minor != another.m_minor) {
        return m_minor < another.m_minor;
      } else {
        if (m_tiny != another.m_tiny) {
          return m_tiny < another.m_tiny;
        } else if (m_buildNumber != another.m_buildNumber) {
          return m_buildNumber < another.m_buildNumber;
        }
      }
    }

    return false;
  }

  bool operator<=(const VersionNumber& another) const {
    return (*this < another) || (*this == another);
  }

  bool operator>=(const VersionNumber& another) const {
    return !(*this < another);
  }

  bool operator>(const VersionNumber& another) const {
    return !((*this < another) || (*this == another));
  }

  friend ostream& operator<<(ostream& stream, const VersionNumber& vn) {
    stream << vn.m_major << "." << vn.m_minor << "." << vn.m_tiny << "."
           << vn.m_buildNumber;
    return stream;
  }

  unsigned int majorVersion() { return m_major; }

  unsigned int minorVersion() { return m_minor; }

  unsigned int tinyVersion() { return m_tiny; }

  unsigned int buildNumber()

  {
    return m_buildNumber;
  }

 protected:
  unsigned int m_major;
  unsigned int m_minor;
  unsigned int m_tiny;
  unsigned int m_buildNumber;
};

class VersionStringCompare {
 public:
  int operator()(const string& s1, const string& s2) {
    return VersionNumber(s1) < VersionNumber(s2);
  }
};

class VersionChecker {
 public:
  VersionChecker() : m_plist(0) {}

  ~VersionChecker() {
    if (m_plist) delete m_plist;
  }

  void unregisterComponent(const string& component) {
    if (m_registeredComponents.find(component) !=
        m_registeredComponents.end()) {
      m_registeredComponents.erase(component);
    }
  }

  bool registerComponentVersion(const string& component, const string& ver,
                                bool overwriteIfExists = false) {
    if ((m_registeredComponents.find(component) !=
         m_registeredComponents.end()) &&
        !overwriteIfExists) {
      return false;
    }

    m_registeredComponents[component] = ver;
    return true;
  }

  bool loadVersionInfoXMLFile(const string& filename) {
    if (m_plist) {
      delete m_plist;
      m_plist = 0;
    }

    m_latestCheckedComponent = "";
    m_latestNeededVersion = "";
    m_latestAction = "";
    m_latestActionURL = "";
    m_latestSignatureURL = "";

    m_plist = new PVPropertyList(filename);
    return true;
  }

  bool componentNeedsUpdating(const string& component) {
    if (m_registeredComponents.find(component) == m_registeredComponents.end())
      return false;

    VersionNumber currentVersion(m_registeredComponents[component]);

    PVPlistValue* componentDict =
        m_plist->rootDictionary()->valueForKey(component);
    if (!componentDict) return false;

    m_latestCheckedComponent = "";
    m_latestNeededVersion = "";
    m_latestAction = "";
    m_latestActionURL = "";
    m_latestSignatureURL = "";

    vector<string> versions = componentDict->dictionaryKeys();
    sort(versions.begin(), versions.end(), VersionStringCompare());

    PVPlistValue* reinstallAction = 0;
    string reinstallVersion;

    PVPlistValue* upgradeAction = 0;
    string upgradeVersion;

    for (vector<string>::iterator viter = versions.begin();
         viter != versions.end(); ++viter) {
      m_latestNeededVersion = *viter;
      if (currentVersion >= VersionNumber(*viter)) continue;

      PVPlistValue* ver = componentDict->valueForKey(*viter);

      if (!ver) continue;

      PVPlistValue* action = ver->valueForKey("action");
      string actionStr;

      if (action) {
        actionStr = action->stringValue();
      }

      if (actionStr == "upgrade") {
        upgradeAction = ver;
        upgradeVersion = *viter;
      } else if (actionStr == "reinstall") {
        reinstallAction = ver;
        reinstallVersion = *viter;
      } else {
      }
    }

    bool needsUpdate = false;
    if (reinstallAction) {
      OVKeyValueMap kvm = reinstallAction->keyValueMap();
      m_latestCheckedComponent = component;
      m_latestNeededVersion = reinstallVersion;
      m_latestAction = "reinstall";
      m_latestActionURL = kvm.stringValueForKey("URL");
      m_latestSignatureURL = kvm.stringValueForKey("signature");
      m_latestChangeLogBaseURL = kvm.stringValueForKey("changelog");
      m_latestChangeLogLocaleTagURL = kvm.stringValueForKey("localetag");
      needsUpdate = true;

    } else if (upgradeAction) {
      OVKeyValueMap kvm = upgradeAction->keyValueMap();
      m_latestCheckedComponent = component;
      m_latestNeededVersion = upgradeVersion;
      m_latestAction = "upgrade";
      m_latestActionURL = kvm.stringValueForKey("URL");
      m_latestSignatureURL = kvm.stringValueForKey("signature");
      m_latestChangeLogBaseURL = kvm.stringValueForKey("changelog");
      m_latestChangeLogLocaleTagURL = kvm.stringValueForKey("localetag");
      needsUpdate = true;
    }

    return needsUpdate;
  }

  const string latestVersion() { return m_latestNeededVersion; }

  const string updateAction() { return m_latestAction; }

  const string actionURL() { return m_latestActionURL; }

  const string signatureURL() { return m_latestSignatureURL; }

  const string changeLogBaseURL() { return m_latestChangeLogBaseURL; }

  const string changeLogLocaleTagURL() { return m_latestChangeLogLocaleTagURL; }

 protected:
  PVPropertyList* m_plist;
  map<string, string> m_registeredComponents;

  string m_latestCheckedComponent;
  string m_latestNeededVersion;
  string m_latestAction;
  string m_latestActionURL;
  string m_latestSignatureURL;

  // change log may contain the substirng {LOCALE}, and the implemention
  // should replace the substring {LOCALE} accordingly with the current
  // orthodox locale tag
  string m_latestChangeLogBaseURL;
  string m_latestChangeLogLocaleTagURL;
};

};  // namespace CareService

#endif