//
// PVLoaderPolicy.h
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

#ifndef PVLoaderPolicy_h
#define PVLoaderPolicy_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace OpenVanilla {
using namespace std;

class PVLoaderPolicy : public OVBase {
 public:
  // these derive information from system default and given path or identifier,
  // but you can override these, too
  PVLoaderPolicy(const vector<string> loadPaths);
  virtual bool addModulePackageLoadPath(const string& path);
  virtual const vector<string> modulePackageLoadPaths() const;
  virtual const string modulePackageIdentifierFromPath(const string& path);
  virtual const OVPathInfo modulePackagePathInfoFromPath(const string& path);
  virtual const string propertyListPathFromIdentifier(const string& identifier);
  virtual const string propertyListPathForLoader();
  virtual const string moduleIdentifierPrefix(const string& identifier);

 public:
  // customizable (overridable) mehtods
  virtual const string defaultDatabaseFileName();
  virtual const string loaderIdentifier();
  virtual const string loaderName();
  virtual const vector<string> modulePackageFilePatterns();

 protected:
  vector<string> m_modulePackageLoadPaths;
};

inline bool PVLoaderPolicy::addModulePackageLoadPath(const string& path) {
  m_modulePackageLoadPaths.push_back(path);
  return true;
}

inline const string PVLoaderPolicy::loaderIdentifier() {
#ifndef PVLOADERPOLICY_LOADER_IDENTIFIER
  return string("org.openvanilla");
#else
  return string(PVLOADERPOLICY_LOADER_IDENTIFIER);
#endif
}

inline const string PVLoaderPolicy::loaderName() {
#ifndef PVLOADERPOLICY_LOADER_NAME
  return string("OpenVanilla");
#else
  return string(PVLOADERPOLICY_LOADER_NAME);
#endif
}

inline const string PVLoaderPolicy::defaultDatabaseFileName() {
  return loaderName() + ".db";
}

inline const vector<string> PVLoaderPolicy::modulePackageFilePatterns() {
  vector<string> result;
#if defined(__APPLE__)
  result.push_back("*.bundle");
#elif defined(WIN32)
  result.push_back("*.dll");
#else
#error Should be so on Linux and FreeBSD--check it out.
  result.push_back("*.so");
#endif

  return result;
}

inline PVLoaderPolicy::PVLoaderPolicy(const vector<string> loadPaths)
    : m_modulePackageLoadPaths(loadPaths) {}

inline const vector<string> PVLoaderPolicy::modulePackageLoadPaths() const {
  return m_modulePackageLoadPaths;
}

inline const string PVLoaderPolicy::modulePackageIdentifierFromPath(
    const string& path) {
  return OVPathHelper::FilenameWithoutExtension(
      OVPathHelper::FilenameWithoutPath(path));
}

inline const OVPathInfo PVLoaderPolicy::modulePackagePathInfoFromPath(
    const string& path) {
  OVPathInfo info;
  string identifier = modulePackageIdentifierFromPath(path);
  info.loadedPath = OVPathHelper::DirectoryFromPath(path);

  if (OVWildcard::Match(path, "*.bundle"))
    info.resourcePath = OVPathHelper::PathCat(
        OVPathHelper::PathCat(path, "/Contents"), "/Resources");
  else
    info.resourcePath = info.loadedPath;

  info.writablePath = OVPathHelper::PathCat(
      OVDirectoryHelper::UserApplicationSupportDataDirectory(loaderName()),
      identifier);

  return info;
}

inline const string PVLoaderPolicy::propertyListPathFromIdentifier(
    const string& identifier) {
  string plist = moduleIdentifierPrefix(identifier) + string(".plist");
  string dir = OVDirectoryHelper::UserPreferencesDirectory(loaderName());

  return OVPathHelper::PathCat(dir, plist);
}

inline const string PVLoaderPolicy::propertyListPathForLoader() {
  string plist = loaderIdentifier() + string(".plist");
  string dir = OVDirectoryHelper::UserPreferencesDirectory(loaderName());

  return OVPathHelper::PathCat(dir, plist);
}

inline const string PVLoaderPolicy::moduleIdentifierPrefix(
    const string& identifier) {
  return loaderIdentifier() + string(".") + identifier;
}
};  // namespace OpenVanilla

#endif