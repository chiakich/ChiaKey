//
// PVModulePackageSystem.h
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

#ifndef PVModulePackageSystem
#define PVModulePackageSystem

#if defined(__APPLE__)
#include <PlainVanilla/PVLoaderPolicy.h>
#include <PlainVanilla/PVLoaderService.h>
#else
#include "PVLoaderPolicy.h"
#include "PVLoaderService.h"
#endif

#include <map>
#include <string>
#include <vector>

namespace OpenVanilla {
using namespace std;

class PVModulePackageLoadingSystem : public OVBase {
 public:
  virtual const vector<string> availablePackages() { return vector<string>(); }

  virtual OVModulePackage* packageWithName(const string& name,
                                           PVLoaderPolicy* loaderPolicy,
                                           OVLoaderService* loaderService) {
    return 0;
  }

  virtual bool unloadPackageWithName(const string& name) { return false; }

  virtual OVPathInfo pathInfoForPackage(const string& name,
                                        PVLoaderPolicy* loaderPolicy) {
    return OVPathInfo::DefaultPathInfo();
  }
};

class PVCommonPackageLoadingSystem : public PVModulePackageLoadingSystem {
 public:
  PVCommonPackageLoadingSystem(PVLoaderPolicy* loaderPolicy) {
    rescan(loaderPolicy);
  }

  // notes of caveat here: because of C++'s design constraint, we won't unload
  // packages here; this must be done by the derived classes; also note the
  // module package data is never overwritten, so scan user's module package
  // directory first if you want to give precedence to it (this is different
  // from OVCINDatabaseService)
  ~PVCommonPackageLoadingSystem() { ; }

  virtual void reset() { m_modulePackages.clear(); }

  virtual void rescan(PVLoaderPolicy* loaderPolicy) {
    const vector<string> paths = loaderPolicy->modulePackageLoadPaths();
    const vector<string> patterns = loaderPolicy->modulePackageFilePatterns();
    vector<string>::const_iterator i, j;

    for (i = paths.begin(); i != paths.end(); ++i) {
      if (!(OVPathHelper::PathExists(*i) && OVPathHelper::IsDirectory(*i))) {
        continue;
      }

      for (j = patterns.begin(); j != patterns.end(); ++j) {
        vector<string> files = OVDirectoryHelper::Glob(*i, *j);
        vector<string>::const_iterator k;
        for (k = files.begin(); k != files.end(); ++k) {
          string name = loaderPolicy->modulePackageIdentifierFromPath(*k);

          map<string, PackageMetadata>::iterator iter =
              m_modulePackages.find(name);
          if (iter == m_modulePackages.end()) {
            m_modulePackages[name] = metadataWithFilename(*k);
          } else {
            ;
          }
        }
      }
    }
  }

  virtual void unloadAllUnloadables() {
    map<string, PackageMetadata>::iterator iter = m_modulePackages.begin();
    for (; iter != m_modulePackages.end(); ++iter)
      if ((*iter).second.library && (*iter).second.unloadable)
        unloadPackageWithName((*iter).first);
  }

  virtual const vector<string> availablePackages() {
    vector<string> result;

    map<string, PackageMetadata>::iterator iter = m_modulePackages.begin();
    for (; iter != m_modulePackages.end(); ++iter)
      result.push_back((*iter).first);

    return result;
  }

  virtual OVModulePackage* packageWithName(const string& name,
                                           PVLoaderPolicy* loaderPolicy,
                                           OVLoaderService* loaderService) {
    map<string, PackageMetadata>::iterator iter = m_modulePackages.find(name);
    if (iter == m_modulePackages.end()) return 0;

    if ((*iter).second.library) {
      if ((*iter).second.package)
        return (*iter).second.package;
      else
        return 0;  // no usable package
    }

    if (!((*iter).second.library = loadLibrary((*iter).second.path))) return 0;

    OVFrameworkVersionFunc_t frameworkVersion =
        (OVFrameworkVersionFunc_t)getFunctionNamed(
            (*iter).second.library, "OVModulePackageFrameworkVersion");
    if (!frameworkVersion) return 0;

    if (frameworkVersion() != OVFrameworkInfo::Version()) return 0;

    OVModulePackageMain_t packageMain = (OVModulePackageMain_t)getFunctionNamed(
        (*iter).second.library, "OVModulePackageMain");
    if (!packageMain) return 0;

    OVModulePackage* package = packageMain();
    if (!package) return 0;

    OVPathInfo pathInfo =
        loaderPolicy->modulePackagePathInfoFromPath((*iter).second.path);
    if (!package->initialize(&pathInfo, loaderService)) {
      delete package;
      return 0;
    }

    (*iter).second.package = package;
    return package;
  }

  virtual bool unloadPackageWithName(const string& name) {
    map<string, PackageMetadata>::iterator iter = m_modulePackages.find(name);
    if (iter == m_modulePackages.end()) return false;

    PackageMetadata& pkgdata = (*iter).second;

    if (pkgdata.library && pkgdata.unloadable) {
      if (pkgdata.package) {
        pkgdata.package->finalize();
        delete pkgdata.package;
        pkgdata.package = 0;
      }

      if (unloadLibrary(pkgdata.library)) {
        pkgdata.library = 0;
        return true;
      } else {
        pkgdata.unloadable = true;
      }
    }

    return false;
  }

  virtual OVPathInfo pathInfoForPackage(const string& name,
                                        PVLoaderPolicy* loaderPolicy) {
    map<string, PackageMetadata>::iterator iter = m_modulePackages.find(name);
    if (iter == m_modulePackages.end()) return OVPathInfo::DefaultPathInfo();

    return loaderPolicy->modulePackagePathInfoFromPath((*iter).second.path);
  }

  virtual bool packageExistsInLoadingSystem(const string& name) {
    map<string, PackageMetadata>::iterator iter = m_modulePackages.find(name);
    return iter != m_modulePackages.end();
  }

 protected:
  struct PackageMetadata {
    PackageMetadata(const string& paramPath = "", bool paramUnloadlable = true)
        : path(paramPath), unloadable(true), package(0), library(0) {}

    string path;
    bool unloadable;
    OVModulePackage* package;
    void* library;

    bool isEmpty() { return path.length() != 0; }
  };

  typedef unsigned int (*OVFrameworkVersionFunc_t)();
  typedef OVModulePackage* (*OVModulePackageMain_t)();

  map<string, PackageMetadata> m_modulePackages;

  virtual void* loadLibrary(const string& path) = 0;
  virtual bool unloadLibrary(void* library) = 0;
  virtual void* getFunctionNamed(void* library, const string& name) = 0;

  virtual const PackageMetadata metadataWithFilename(const string& path) {
    return PackageMetadata(path, true);
  }
};

// ownership policy: decide whether you want PVStaticModulePackageLoadingSystem
// to manage the objects in the beginning
class PVStaticModulePackageLoadingSystem : public PVModulePackageLoadingSystem {
 public:
  PVStaticModulePackageLoadingSystem(const OVPathInfo& sharedPathInfo,
                                     bool ownsPackage = false)
      : m_ownsPackage(ownsPackage), m_sharedPathInfo(sharedPathInfo) {}

  ~PVStaticModulePackageLoadingSystem() {
    if (m_ownsPackage)
      for_each(m_modulePackages.begin(), m_modulePackages.end(), DeleteAll);
  }

  void flushModules() {
    if (m_ownsPackage)
      for_each(m_modulePackages.begin(), m_modulePackages.end(), DeleteAll);

    m_modulePackages.clear();
  }

  virtual const vector<string> availablePackages() {
    vector<string> result;
    map<string, OVModulePackage*>::iterator iter = m_modulePackages.begin();
    for (; iter != m_modulePackages.end(); iter++)
      if ((*iter).second) result.push_back((*iter).first);
    return result;
  }

  virtual OVModulePackage* packageWithName(const string& name,
                                           PVLoaderPolicy* loaderPolicy,
                                           OVLoaderService* loaderService) {
    map<string, OVModulePackage*>::iterator iter = m_modulePackages.find(name);
    if (iter != m_modulePackages.end()) return (*iter).second;

    return 0;
  }

  virtual bool unloadPackageWithName(const string&) { return false; }

  virtual bool ownsPackage() { return m_ownsPackage; }

  virtual bool addInitializedPackage(const string& name,
                                     OVModulePackage* package) {
    map<string, OVModulePackage*>::iterator iter = m_modulePackages.find(name);
    if (iter == m_modulePackages.end()) {
      m_modulePackages[name] = package;
      return true;
    }

    return false;
  }

  virtual OVPathInfo pathInfoForPackage(const string& name,
                                        PVLoaderPolicy* loaderPolicy) {
    return m_sharedPathInfo;
  }

 protected:
  static void DeleteAll(pair<string, OVModulePackage*> p) {
    if (p.second) {
      p.second->finalize();
      delete p.second;
    }
  }

  map<string, OVModulePackage*> m_modulePackages;
  bool m_ownsPackage;
  OVPathInfo m_sharedPathInfo;
};

// PVModulePackageManager does not manage loading systems
class PVModulePackageManager : public OVBase {
 public:
  PVModulePackageManager(
      const vector<PVModulePackageLoadingSystem*>& loadingSystems,
      const vector<string>& excludedPackageNames, PVLoaderPolicy* loaderPolicy,
      OVLoaderService* loaderService)
      : m_loadingSystems(loadingSystems) {
    m_loadingSystems = loadingSystems;

    vector<PVModulePackageLoadingSystem*>::iterator systemIter =
        m_loadingSystems.begin();
    for (; systemIter != m_loadingSystems.end(); ++systemIter) {
      PVModulePackageLoadingSystem* system = *systemIter;

      vector<string> packages = system->availablePackages();
      vector<string>::iterator pkgIter = packages.begin();

      for (; pkgIter != packages.end(); ++pkgIter) {
        if (OVWildcard::MultiWildcardMatchAny(*pkgIter, excludedPackageNames)) {
          // this name is excluded, we don't love it
        } else {
          OVModulePackage* pkg =
              system->packageWithName(*pkgIter, loaderPolicy, loaderService);

          if (pkg) m_packageStore[*pkgIter] = PackageData(pkg, system);
        }
      }
    }
  }

  ~PVModulePackageManager() {
    map<string, PackageData>::iterator iter = m_packageStore.begin();

    for (; iter != m_packageStore.end(); ++iter) {
      PackageData data = (*iter).second;
      data.second->unloadPackageWithName((*iter).first);
    }
  }

  virtual const vector<string> loadedPackages() {
    vector<string> result;

    map<string, PackageData>::iterator iter = m_packageStore.begin();

    for (; iter != m_packageStore.end(); ++iter)
      result.push_back((*iter).first);

    return result;
  }

  virtual OVModulePackage* packageWithName(const string& name) {
    map<string, PackageData>::iterator iter = m_packageStore.find(name);

    if (iter != m_packageStore.end())
      return (*iter).second.first;
    else
      return 0;
  }

  virtual OVPathInfo pathInfoForPackage(const string& name,
                                        PVLoaderPolicy* loaderPolicy) {
    map<string, PackageData>::iterator iter = m_packageStore.find(name);

    if (iter != m_packageStore.end())
      return (*iter).second.second->pathInfoForPackage(name, loaderPolicy);
    else
      return OVPathInfo::DefaultPathInfo();
  }

 protected:
  typedef pair<OVModulePackage*, PVModulePackageLoadingSystem*> PackageData;

  map<string, PackageData> m_packageStore;
  vector<PVModulePackageLoadingSystem*> m_loadingSystems;
};

};  // namespace OpenVanilla

#endif