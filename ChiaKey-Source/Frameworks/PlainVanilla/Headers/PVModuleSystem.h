//
// PVModuleSystem.h
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

#ifndef PVModuleSystem_h
#define PVModuleSystem_h

#if defined(__APPLE__)
#include <PlainVanilla/PVModulePackageSystem.h>
#include <PlainVanilla/PVPropertyList.h>
#else
#include "OpenVanilla.h"
#include "PVCandidate.h"
#include "PVLoaderService.h"
#include "PVModulePackageSystem.h"
#include "PVPropertyList.h"
#include "PVTextBuffer.h"
#endif

#include <map>
#include <set>

namespace OpenVanilla {
using namespace std;

class PVModuleCompare {
 public:
  int operator()(const OVModule* m1, const OVModule* m2) {
    return m1->suggestedOrder() < m2->suggestedOrder();
  }
};

class PVModuleManager : public OVBase {
 protected:
  struct ModuleMetadata {
    ModuleMetadata(const string& paramPackageName = "",
                   OVModule* paramModule = 0)
        : modulePackageName(paramPackageName),
          module(paramModule),
          propertyList(0) {}

    string modulePackageName;
    OVModule* module;
    PVPropertyList* propertyList;
  };

  map<string, ModuleMetadata> m_moduleStore;
  PVModulePackageManager* m_modulePackageManager;

 public:
  PVPlistValue* allModulePropertyListsCombined() {
    PVPlistValue emptyDict(PVPlistValue::Dictionary);

    PVPlistValue* pv = new PVPlistValue(PVPlistValue::Dictionary);
    for (map<string, ModuleMetadata>::const_iterator miter =
             m_moduleStore.begin();
         miter != m_moduleStore.end(); ++miter) {
      pv->setKeyValue((*miter).first,
                      ((*miter).second.propertyList
                           ? (*miter).second.propertyList->rootDictionary()
                           : &emptyDict));
    }
    return pv;
  }

  PVModuleManager(PVModulePackageManager* packageManager,
                  const vector<string>& excludedModuleIdentifiers,
                  OVLoaderService* loaderService)
      : m_modulePackageManager(packageManager) {
    vector<string> packageNames = m_modulePackageManager->loadedPackages();
    vector<string>::iterator iter;

    for (iter = packageNames.begin(); iter != packageNames.end(); ++iter) {
      OVModulePackage* package = m_modulePackageManager->packageWithName(*iter);
      if (!package) continue;

      size_t index, nom = package->numberOfModules(loaderService);

      for (index = 0; index < nom; ++index) {
        OVModule* module = package->moduleAtIndex(index, loaderService);
        if (!module) continue;

        if (OVWildcard::MultiWildcardMatchAny(module->identifier(),
                                              excludedModuleIdentifiers)) {
          // this name is excluded, we don't love it
        } else {
          m_moduleStore[module->identifier()] = ModuleMetadata(*iter, module);
        }
      }
    }
  }

  ~PVModuleManager() {
    map<string, ModuleMetadata>::iterator mdataIter = m_moduleStore.begin();
    for (; mdataIter != m_moduleStore.end(); ++mdataIter) {
      ModuleMetadata& meta = (*mdataIter).second;

      if (meta.module) {
        if (meta.module->isInitialized()) {
          meta.module->finalize();
          delete meta.module;
        }
      }

      if (meta.propertyList) delete meta.propertyList;
    }
  }

  virtual const vector<pair<string, string> > availableModulesAndNames(
      OVLoaderService* loaderService, bool onlyInputMethod = false,
      bool onlyPreprocessor = false, bool onlyOutputFilter = false,
      bool onlyAroundFilter = false) {
    vector<OVModule*> modules;
    vector<pair<string, string> > result;
    map<string, ModuleMetadata>::iterator mdataIter = m_moduleStore.begin();
    for (; mdataIter != m_moduleStore.end(); ++mdataIter) {
      ModuleMetadata& meta = (*mdataIter).second;

      OVModule* module = meta.module;
      if (!module) continue;

      if (onlyAroundFilter && !module->isAroundFilter()) continue;

      if (onlyInputMethod && !module->isInputMethod()) continue;

      if (onlyPreprocessor && !module->isPreprocessor()) continue;

      if (onlyOutputFilter && !module->isOutputFilter()) continue;

      modules.push_back(module);
    }

    sort(modules.begin(), modules.end(), PVModuleCompare());

    for (vector<OVModule*>::iterator miter = modules.begin();
         miter != modules.end(); ++miter)
      result.push_back(pair<string, string>(
          (*miter)->identifier(),
          (*miter)->localizedName(loaderService->locale())));

    return result;
  }

  virtual OVModule* moduleWithName(const string& name,
                                   PVLoaderPolicy* loaderPolicy,
                                   OVLoaderService* loaderService,
                                   bool needInit = true) {
    map<string, ModuleMetadata>::iterator mdataIter = m_moduleStore.find(name);
    if (mdataIter == m_moduleStore.end()) return 0;

    ModuleMetadata& meta = (*mdataIter).second;
    if (!meta.module) return 0;

    if (!needInit) return meta.module;

    if (meta.module->isInitialized()) {
      if (meta.module->isUsable())
        return meta.module;
      else
        return 0;
    }

    OVPathInfo pathInfo = m_modulePackageManager->pathInfoForPackage(
        meta.modulePackageName, loaderPolicy);

    if (!meta.module->moduleInitialize(&pathInfo, loaderService)) {
      return 0;
    }

    // successfully initialized, create property list
    string plistFile =
        loaderPolicy->propertyListPathFromIdentifier(meta.module->identifier());
    meta.propertyList = new PVPropertyList(plistFile, true);

    // tell the module to update config, and we write the config
    OVKeyValueMap kvm =
        meta.propertyList->rootDictionary()->readOnlyKeyValueMap();
    meta.module->loadConfig(&kvm, loaderService);

    meta.propertyList->write();
    return meta.module;
  }

  virtual void forceSyncModuleConfigForNextRound(const string& name) {
    map<string, ModuleMetadata>::iterator mdataIter = m_moduleStore.find(name);
    if (mdataIter == m_moduleStore.end()) return;

    ModuleMetadata& meta = (*mdataIter).second;
    if (!meta.propertyList) return;

    meta.propertyList->forceSyncForNextRound();
  }

  virtual bool syncConfigForModule(const string& name,
                                   OVLoaderService* loaderService) {
    map<string, ModuleMetadata>::iterator mdataIter = m_moduleStore.find(name);
    if (mdataIter == m_moduleStore.end()) return false;

    ModuleMetadata& meta = (*mdataIter).second;
    if (!meta.propertyList) return false;

    if (!meta.propertyList->shouldReadSync()) return false;

    meta.propertyList->readSync();

    OVKeyValueMap kvm =
        meta.propertyList->rootDictionary()->readOnlyKeyValueMap();
    meta.module->loadConfig(&kvm, loaderService);
    return true;
  }

  virtual bool writeConfigForModule(const string& name,
                                    OVLoaderService* loaderService) {
    map<string, ModuleMetadata>::iterator mdataIter = m_moduleStore.find(name);
    if (mdataIter == m_moduleStore.end()) return false;

    ModuleMetadata& meta = (*mdataIter).second;
    if (!meta.propertyList) return false;

    OVKeyValueMap kvm = meta.propertyList->rootDictionary()->keyValueMap();

    meta.module->saveConfig(&kvm, loaderService);
    meta.propertyList->write();
    return true;
  }

  virtual PVPlistValue* configDictionaryForModule(const string& name) {
    map<string, ModuleMetadata>::iterator mdataIter = m_moduleStore.find(name);
    if (mdataIter == m_moduleStore.end()) return nullptr;

    ModuleMetadata& meta = (*mdataIter).second;
    if (!meta.propertyList) return nullptr;

    return meta.propertyList->rootDictionary();
  }
};

};  // namespace OpenVanilla

#endif
