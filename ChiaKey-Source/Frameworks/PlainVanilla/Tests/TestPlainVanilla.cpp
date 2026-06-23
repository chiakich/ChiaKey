//
// TestPlainVanilla.cpp
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

#include "TestPlainVanilla.h"

using namespace OpenVanilla;

TEST(TestBasicKeyValueMapImpl) {
  PVBasicKeyValueMapImpl kvm;

  CHECK_EQUAL(false, kvm.hasKey("foo"));

  kvm.setKeyStringValue("foo", "bar");
  CHECK(kvm.hasKey("foo"));
  CHECK_EQUAL("bar", kvm["foo"]);
}

TEST(PropertyListAccess) {
  string tmp = OVDirectoryHelper::TempDirectory();
  string myplist = OVPathHelper::PathCat(tmp, "test.plist");

  PVPropertyList plist(myplist);

  PVPlistValue stringValue("foobar");
  PVPlistValue arrayValue(PVPlistValue::Array);
  PVPlistValue dictValue1(PVPlistValue::Dictionary);
  PVPlistValue& dictValue2 = *(plist.rootDictionary());

  arrayValue.addArrayElement(&stringValue);
  stringValue.setStringValue("blah");
  arrayValue.addArrayElement(&stringValue);
  dictValue1.setKeyValue("hello", &arrayValue);
  dictValue1.setKeyValue("world", &stringValue);
  dictValue2.setKeyValue("hello", &arrayValue);
  dictValue2.setKeyValue("world", &stringValue);
  dictValue2.setKeyValue("foooo", &dictValue1);

  plist.write();

  PVPropertyList newplist(myplist);
  CHECK(*(plist.rootDictionary()) == *(newplist.rootDictionary()));

#ifndef WIN32
  sleep(1);
#else
  Sleep(1000);
#endif

  plist.write();
  CHECK(!newplist.shouldReadSync());
  newplist.readSync();
  CHECK(!newplist.shouldReadSync());
}

TEST(ConfigAndKeyValueMap) {
  PVPlistValue* strValue = new PVPlistValue(PVPlistValue::String);
  strValue->setStringValue("blah");

  PVPlistValue* arrayValue = new PVPlistValue(PVPlistValue::Array);
  arrayValue->addArrayElement(strValue);
  arrayValue->addArrayElement(0);
  strValue->setStringValue("foobar");
  arrayValue->addArrayElement(strValue);

  PVPlistValue* dictValue = new PVPlistValue(PVPlistValue::Dictionary);
  dictValue->setKeyValue("foo", arrayValue);
  dictValue->setKeyValue("bar", strValue);
  dictValue->setKeyValue("foobar", 0);

  OVKeyValueMap dict = dictValue->keyValueMap();
  CHECK_EQUAL(dict.stringValueForKey("bar"), "foobar");
  CHECK(!dict.hasKey("foobar"));
  CHECK_EQUAL(dict.stringValueForKey("foo"), "");

  delete dictValue;
  delete arrayValue;
  delete strValue;
}

TEST(ConfigSupport) {
  PVPlistValue* strValue = new PVPlistValue(PVPlistValue::String);
  strValue->setStringValue("blah");

  PVPlistValue* arrayValue = new PVPlistValue(PVPlistValue::Array);
  arrayValue->addArrayElement(strValue);
  arrayValue->addArrayElement(0);
  strValue->setStringValue("foobar");
  arrayValue->addArrayElement(strValue);

  PVPlistValue* dictValue = new PVPlistValue(PVPlistValue::Dictionary);
  dictValue->setKeyValue("foo", arrayValue);
  dictValue->setKeyValue("bar", strValue);
  dictValue->setKeyValue("foobar", 0);

  PVPlistValue* anotherStr = strValue->copy();
  PVPlistValue* anotherArray = arrayValue->copy();
  PVPlistValue* anotherDict = dictValue->copy();

  CHECK(*strValue == *anotherStr);
  CHECK(*arrayValue == *anotherArray);
  CHECK(*dictValue == *anotherDict);

  delete anotherStr;
  delete anotherArray;
  delete anotherDict;
  delete dictValue;
  delete arrayValue;
  delete strValue;
}

TEST(LoaderPolicy) {
#if defined(__APPLE__)
  string testpkg = "/Library/Foobar/Modules/1.0.0/TestModule.bundle";
#elif defined(WIN32)
  string testpkg = "C:\\Program Files\\Foobar\\Modules\\1.0.0\\TestModule.dll";
#else
#error Fill in test case for Linux/FreeBSD here
  string testpkg = "/usr/local/foobar/modules/1.0.0/TestModule.so";
#endif

  vector<string> loadPaths;
  loadPaths.push_back(g_modulePackageLoadPath);
  PVLoaderPolicy policy(loadPaths);
  CHECK_EQUAL(policy.modulePackageIdentifierFromPath(testpkg), "TestModule");
}

TEST(ModuleLoadingSystem) {
  vector<string>::const_iterator iter;
  vector<string> loadPaths;
  loadPaths.push_back(g_modulePackageLoadPath);

  PVLoaderPolicy policy(loadPaths);

  PVLoaderService loaderService;

#if defined(__APPLE__)
  PVBundleLoadingSystem loadingSystem(&policy);
#elif defined(WIN32)
  PVDLLLoadingSystem loadingSystem(&policy);
#else
#error not yet for other systems
#endif

  vector<string> packages = loadingSystem.availablePackages();
  cout << "available packages from bundle loading system = " << endl;
  for (iter = packages.begin(); iter != packages.end(); ++iter)
    cout << "  " << *iter << endl;

  vector<PVModulePackageLoadingSystem*> systems;
  systems.push_back(&loadingSystem);

  PVModulePackageManager modPkgManager(systems, vector<string>(), &policy,
                                       &loaderService);
  vector<string> loaded = modPkgManager.loadedPackages();

  cout << "loaded packages from module package manager = " << endl;
  for (iter = loaded.begin(); iter != loaded.end(); ++iter)
    cout << "  " << *iter << endl;
}

TEST(PVTextBuffer) {
  PVTextBuffer composingText;
  PVTextBuffer readingText;

  composingText.setText("123");
  readingText.setText("456");

  PVCombinedUTF16TextBuffer buf1(composingText, readingText);
  CHECK_EQUAL(6, buf1.cursorPosition());
  CHECK_EQUAL(6, buf1.wideCursorPosition());
  CHECK_EQUAL("123456", buf1.composedText());

  composingText.setCursorPosition(1);
  PVCombinedUTF16TextBuffer buf2(composingText, readingText);
  CHECK_EQUAL(4, buf2.cursorPosition());
  CHECK_EQUAL(4, buf2.wideCursorPosition());
  CHECK_EQUAL("145623", buf2.composedText());

  string a = "\x61";              // U+0061
  string b = "\xc3\xa1";          // U+00E1
  string c = "\xe3\x81\x82";      // U+3042
  string d = "\xf0\x90\x85\x89";  // U+10149 (D800 DD49)
  string e = "\xf0\xa0\x85\x98";  // U+20158 (D840 DD58)

  string abcde = a + b + c + d + e;
  string abcdabcdee = a + b + c + d + abcde + e;

  composingText.setText(abcde);
  readingText.setText(abcde);

  PVCombinedUTF16TextBuffer buf3(composingText, readingText);
  CHECK_EQUAL(10, buf3.cursorPosition());
  CHECK_EQUAL(14, buf3.wideCursorPosition());
  CHECK_EQUAL(abcde + abcde, buf3.composedText());

  // CHECK_EQUAL(OVUTF16::FromUTF8(abcde + abcde), buf3.wideComposedText());

  composingText.setCursorPosition(4);
  composingText.setHighlightMark(OVTextBuffer::RangePair(1, 4));

  readingText.setCursorPosition(4);
  PVCombinedUTF16TextBuffer buf4(composingText, readingText, true, true);
  CHECK_EQUAL(8, buf4.cursorPosition());
  CHECK_EQUAL(10, buf4.wideCursorPosition());
  CHECK_EQUAL(abcdabcdee, buf4.composedText());
  // CHECK_EQUAL(OVUTF16::FromUTF8(abcdabcdee), buf4.wideComposedText());
  // cout << buf4;

  vector<OVTextBuffer::RangePair> segs;
  segs.push_back(OVTextBuffer::RangePair(0, 1));
  segs.push_back(OVTextBuffer::RangePair(3, 1));
  segs.push_back(OVTextBuffer::RangePair(4, 0));

  composingText.setHighlightMark(OVTextBuffer::RangePair(4, 1));
  composingText.setWordSegments(segs);
  readingText.clear();
  PVCombinedUTF16TextBuffer buf5(composingText, readingText, true, false);
  CHECK_EQUAL(4, buf5.cursorPosition());
  CHECK_EQUAL(5, buf5.wideCursorPosition());
  CHECK_EQUAL(abcde, buf5.composedText());
  cout << buf5;
}
