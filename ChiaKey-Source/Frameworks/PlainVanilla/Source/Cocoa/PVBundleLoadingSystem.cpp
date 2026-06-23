//
// PVBundleLoadingSystem.cpp
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

#include "PVBundleLoadingSystem.h"

#include <CoreFoundation/CoreFoundation.h>

using namespace OpenVanilla;

template <class T>
class LFCFAutoObject {
 public:
  LFCFAutoObject(T object) : m_object(object) {}

  ~LFCFAutoObject() {
    if (m_object) CFRelease(m_object);
  }

  operator T() { return m_object; }

 protected:
  T m_object;
};

PVBundleLoadingSystem::PVBundleLoadingSystem(PVLoaderPolicy* policy)
    : PVCommonPackageLoadingSystem(policy) {}

PVBundleLoadingSystem::~PVBundleLoadingSystem() { unloadAllUnloadables(); }

void* PVBundleLoadingSystem::loadLibrary(const string& path) {
  LFCFAutoObject<CFStringRef> pathString =
      CFStringCreateWithCString(NULL, path.c_str(), kCFStringEncodingUTF8);
  LFCFAutoObject<CFURLRef> url = CFURLCreateWithFileSystemPath(
      NULL, pathString, kCFURLPOSIXPathStyle, false);
  CFBundleRef bundle = CFBundleCreate(NULL, url);

  if (bundle) {
    if (CFBundleLoadExecutable(bundle)) {
      return (void*)bundle;
    } else {
      CFRelease(bundle);
      return 0;
    }
  }
}

bool PVBundleLoadingSystem::unloadLibrary(void* library) {
  if (!library) return false;

  CFBundleRef bundle = (CFBundleRef)library;
  CFBundleUnloadExecutable(bundle);
  CFRelease(bundle);
  return true;
}

void* PVBundleLoadingSystem::getFunctionNamed(void* library,
                                              const string& name) {
  if (!library) return 0;

  LFCFAutoObject<CFStringRef> funcString =
      CFStringCreateWithCString(NULL, name.c_str(), kCFStringEncodingUTF8);
  CFBundleRef bundle = (CFBundleRef)library;

  return CFBundleGetFunctionPointerForName(bundle, funcString);
}
