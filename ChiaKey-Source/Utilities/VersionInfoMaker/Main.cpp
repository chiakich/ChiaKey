/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#if defined(__APPLE__)
#include <PlainVanilla/PlainVanilla.h>
#else
#include "PlainVanilla.h"
#endif

#include <iostream>

using namespace std;
using namespace OpenVanilla;

#if defined(__APPLE__)
extern "C" void CocoaInit();
#endif

int main(int argc, char* argv[]) {
#if defined(__APPLE__)
  CocoaInit();
#endif

  if (argc < 3) {
    cerr << "usage: VersionIntoMaker <xml file> <commands> ..." << endl;
    cerr << "commands:" << endl;
    cerr << "    inspect" << endl;
    cerr << "    addcomponent <component-name>" << endl;
    cerr << "    addversion <component-name> <version> <action> <url> [sig]"
         << endl;
    cerr << "suggested actions:" << endl;
    cerr << "    upgrade - indicates an incremental update" << endl;
    cerr << "    reinstall - indiciate that user must visit the provided URL "
            "to download the installer"
         << endl;
    cerr << "    data - suggest this is a data-only update" << endl;
    return 1;
  }

  string command = argv[2];
  PVPropertyList plist(argv[1]);
  PVPlistValue* rootDict = plist.rootDictionary();

  if (command == "inspect") {
    cout << "inspecting:" << endl;
    cout << *rootDict << endl;
  } else if (command == "addcomponent") {
    if (argc < 4) {
      cerr << "Please specify the component name" << endl;
      return 1;
    }

    string componentName = argv[3];
    if (rootDict->valueForKey(componentName)) {
      cerr << "A component of the name already exists" << endl;
      return 1;
    }

    PVPlistValue componentDict(PVPlistValue::Dictionary);
    rootDict->setKeyValue(componentName, &componentDict);
  } else if (command == "addversion") {
    if (argc < 7) {
      cerr << "Please specify component name, version, action, url and an "
              "optional signature url"
           << endl;
      return 1;
    }

    string componentName = argv[3];
    string version = argv[4];
    string action = argv[5];
    string url = argv[6];
    string signature = (argc > 7) ? argv[7] : "";

    PVPlistValue* componentDict = rootDict->valueForKey(componentName);
    if (!componentDict) {
      cerr << "Cannot find the component, please use command 'addcomponent' "
              "first"
           << endl;
      return 1;
    }

    if (componentDict->valueForKey(version)) {
      cerr << "That version already exists" << endl;
      return 1;
    }

    PVPlistValue versionDict(PVPlistValue::Dictionary);
    versionDict.setKeyValue("action", action);
    versionDict.setKeyValue("URL", url);

    if (signature.size()) versionDict.setKeyValue("signature", signature);

    componentDict->setKeyValue(version, &versionDict);
  }

  plist.write();
  return 0;
}
