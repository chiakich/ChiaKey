//
// PVLoaderSystem.h
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

#ifndef PVLoaderSystem
#define PVLoaderSystem

#if defined(__APPLE__)
#include <PlainVanilla/PVCandidate.h>
#include <PlainVanilla/PVModuleSystem.h>
#include <PlainVanilla/PVTextBuffer.h>
#else
#include "PVCandidate.h"
#include "PVModuleSystem.h"
#include "PVTextBuffer.h"
#endif

namespace OpenVanilla {
using namespace std;

class PVLoader;

class PVLoaderCallback : public OVBase {
 public:
  virtual void loaderConfigChanged(PVLoader* loader) = 0;
  virtual void loaderConfigSaved(PVLoader* loader) = 0;
};

class PVLoaderContext : public OVBase {
 public:
  PVLoaderContext(PVLoader* loader);
  ~PVLoaderContext();

  // called by the platform-dependent IME code
  virtual void activate();
  virtual void deactivate();
  virtual const string residueComposingTextBeforeDeactivation();
  virtual void clear();
  virtual bool handleKeyEvent(OVKey* key);

  // called by the platform-dependent IME code
  virtual bool shouldUpdateCandidate();
  virtual bool shouldUpdateBufferedTexts();

  // called by PVLoader
  virtual void invalidate();

  // getters
  virtual PVTextBuffer* readingText();
  virtual PVTextBuffer* composingText();
  virtual PVCandidateService* candidateService();

 protected:
  virtual bool shouldEnter(OVLoaderService* loaderService,
                           bool activateModuleIfReloaded = false,
                           bool syncLoaderConfig = false);

  friend class PVLoader;
  PVLoader* m_loader;
  int m_generation;

  PVCandidateService* m_candidateService;
  PVTextBuffer* m_readingText;
  PVTextBuffer* m_composingText;

  // the sandwich
  PVContextSandwich* m_sandwich;
  OVEventHandlingContext* m_focusedContext;

  // candidate panel control system
  OVCandidatePanel* m_lastUsedPanel;
  virtual void checkLastUsedPanel();
};

// PVLoader create order:
// 1. instantiate loader policy
// 2. instantiate loader service
// 3. instantiate module package loading systems
// 5. instantiate loader
//
// PVLoader reload order:
// 1. call prepareReload(), loader will lock itself to prevent context sessions
// from entering
// 2. (prepareReload() kills its current module package manager)
// 3. refresh your own module package loading system
// 4. call reload
// 5. (reload unlocks the loader)

class PVLoader : public OVBase {
 public:
  PVLoader(PVLoaderPolicy* loaderPolicy, PVLoaderService* loaderService,
           const vector<PVModulePackageLoadingSystem*>& loadingSystems)
      : m_loaderPolicy(loaderPolicy),
        m_loaderService(loaderService),
        m_modulePackageLoadingSystems(loadingSystems),
        m_modulePackageManager(0),
        m_moduleManager(0),
        m_loaderPropertyList(0),
        m_activeContext(0),
        m_generation(0),
        m_loaderLocked(true),
        m_callback(0) {
    reload(true);
  }

  ~PVLoader() {
    m_loaderLocked = true;
    nullifyAllContexts();
    finalizeEverything();
    m_loaderLocked = false;
    // m_loaderService->logger("Loader") << "Loader destructed" << endl;
  }

  // call by the platform-dependent code
  void prepareReload() {
    m_loaderService->logger("Loader")
        << "Prepares to reload, invalidating all contexts" << endl;
    m_loaderLocked = true;
    invalidateAllContexts();
    finalizeEverything();
  }

  // call by the platform-dependent code
  void reload(bool firstTime = false) {
    //            if (firstTime)
    //                m_loaderService->logger("Loader") << "Starts to load" <<
    //                endl;
    //            else
    //                m_loaderService->logger("Loader") << "Starts to reload" <<
    //                endl;

    string plistFilename = m_loaderPolicy->propertyListPathForLoader();
    //            m_loaderService->logger("Loader") << "Uses plist: " <<
    //            plistFilename << endl;

    m_loaderPropertyList = new PVPropertyList(plistFilename);
    loadLoaderConfig();

    m_modulePackageManager = new PVModulePackageManager(
        m_modulePackageLoadingSystems, m_cfgExcludedModulePackages,
        m_loaderPolicy, m_loaderService);
    m_moduleManager = new PVModuleManager(
        m_modulePackageManager, m_cfgExcludedModules, m_loaderService);

    validateLoaderConfig();
    saveLoaderConfig();
    m_loaderLocked = false;
  }

  // called by the UI context
  void syncLoaderConfig(bool forceWrite = false) {
    if (forceWrite) {
      m_loaderLocked = true;
      validateLoaderConfig();
      saveLoaderConfig();
      if (m_callback) m_callback->loaderConfigChanged(this);
      m_loaderLocked = false;
      return;
    }

    if (m_loaderPropertyList->shouldReadSync()) {
      // m_loaderService->logger("Loader") << "Plist needs reload" << endl;
      m_loaderLocked = true;
      loadLoaderConfig();
      validateLoaderConfig();

      if (m_callback) m_callback->loaderConfigChanged(this);

      saveLoaderConfig();
      m_loaderLocked = false;
    }
  }

  // call by the UI context
  void syncSandwichConfig() {
    m_moduleManager->syncConfigForModule(m_cfgPrimaryInputMethod,
                                         m_loaderService);

    for (vector<string>::iterator iter = m_cfgActivatedAroundFilters.begin();
         iter != m_cfgActivatedAroundFilters.end(); ++iter)
      m_moduleManager->syncConfigForModule(*iter, m_loaderService);
  }

  void saveSandwichConfig() {
    m_moduleManager->writeConfigForModule(m_cfgPrimaryInputMethod,
                                          m_loaderService);

    for (vector<string>::iterator iter = m_cfgActivatedAroundFilters.begin();
         iter != m_cfgActivatedAroundFilters.end(); ++iter)
      m_moduleManager->writeConfigForModule(*iter, m_loaderService);
  }

  // WARNING: For expert user who knows the module-in-question only, this allows
  // module config write/flush even if it's not properly initialized
  void forceWriteModuleConfig(const string& identifier) {
    m_moduleManager->writeConfigForModule(identifier, m_loaderService);
  }

  void forceSyncModuleConfigForNextRound(const string& identifier) {
    m_moduleManager->forceSyncModuleConfigForNextRound(identifier);
  }

  PVLoaderService* loaderService() { return m_loaderService; }

  const vector<string> allInputMethodIdentifiers() {
    vector<pair<string, string> > modulesAndNames =
        m_moduleManager->availableModulesAndNames(m_loaderService, true, false,
                                                  false, false);
    vector<string> result;
    for (vector<pair<string, string> >::iterator iter = modulesAndNames.begin();
         iter != modulesAndNames.end(); ++iter)
      result.push_back((*iter).first);
    return result;
  }

  const vector<pair<string, string> > allModuleIdentifiersAndNames() {
    return m_moduleManager->availableModulesAndNames(m_loaderService, false,
                                                     false, false, false);
  }

  const vector<pair<string, string> > allInputMethodIdentifiersAndNames() {
    return m_moduleManager->availableModulesAndNames(m_loaderService, true,
                                                     false, false, false);
  }

  const vector<pair<string, string> > allOutputFilterIdentifiersAndNames() {
    return m_moduleManager->availableModulesAndNames(m_loaderService, false,
                                                     false, true, false);
  }

  const vector<pair<string, string> > allAroundFilterIdentifiersAndNames() {
    return m_moduleManager->availableModulesAndNames(m_loaderService, false,
                                                     false, false, true);
  }

  bool isOutputFilterActivated(const string& identifer) {
    return IsStringInVector(identifer, m_cfgActivatedOutputFilters);
  }

  bool isAroundFilterActivated(const string& identifer) {
    return IsStringInVector(identifer, m_cfgActivatedAroundFilters);
  }

  void toggleAroundFilter(const string& identifier) {
    // m_loaderService->logger("Loader") << "toggle around filter: " <<
    // identifier << endl;
    vector<string> result;

    for (vector<string>::iterator iter = m_cfgActivatedAroundFilters.begin();
         iter != m_cfgActivatedAroundFilters.end(); ++iter)
      if (*iter != identifier) result.push_back(*iter);

    if (result.size() != m_cfgActivatedAroundFilters.size()) {
      // m_loaderService->logger("Loader") << "item removed" << endl;
      // item removed
      m_cfgActivatedAroundFilters = result;
      validateActivatedAroundFilters();
      saveLoaderConfig();
      return;
    }

    // m_loaderService->logger("Loader") << "adding item" << endl;
    m_cfgActivatedAroundFilters.push_back(identifier);
    validateActivatedAroundFilters();
    saveLoaderConfig();

    // we need to add the item
    // @todo: add the item according to the output filter order, should we put
    // this in validateActivatedOutputFilters
  }

  void toggleOutputFilter(const string& identifier) {
    // m_loaderService->logger("Loader") << "toggle filter: " << identifier <<
    // endl;
    vector<string> result;

    for (vector<string>::iterator iter = m_cfgActivatedOutputFilters.begin();
         iter != m_cfgActivatedOutputFilters.end(); ++iter)
      if (*iter != identifier) result.push_back(*iter);

    if (result.size() != m_cfgActivatedOutputFilters.size()) {
      // m_loaderService->logger("Loader") << "item removed" << endl;
      // item removed
      m_cfgActivatedOutputFilters = result;
      validateActivatedOutputFilters();
      saveLoaderConfig();
      return;
    }

    // m_loaderService->logger("Loader") << "adding item" << endl;
    m_cfgActivatedOutputFilters.push_back(identifier);
    validateActivatedOutputFilters();
    saveLoaderConfig();

    // we need to add the item
    // @todo: add the item according to the output filter order, should we put
    // this in validateActivatedOutputFilters
  }

  const string primaryInputMethod() { return m_cfgPrimaryInputMethod; }

  void setPrimaryInputMethod(const string identifier) {
    m_cfgPrimaryInputMethod = identifier;
    validatePrimaryInputMethod();
    saveLoaderConfig();
  }

  const vector<string> excludedModulePackages() const {
    return m_cfgExcludedModulePackages;
  }

  void setExcludedModulePackages(const vector<string>& packageNames) {
    m_cfgExcludedModulePackages = packageNames;
    validateExclusionSettings();
    saveLoaderConfig();
  }

  // by failed we mean initialized but unusable
  bool isFailedModule(const string& identifier) {
    OVModule* module = m_moduleManager->moduleWithName(
        identifier, m_loaderPolicy, m_loaderService, false);
    if (!module) return true;

    if (module->isInitialized() && !module->isUsable()) {
      return true;
    }

    return false;
  }

  PVContextSandwich* createSandwich() {
    PVContextSandwich* sandwich = new PVContextSandwich;
    OVModule* module = moduleWithName(m_cfgPrimaryInputMethod);

    if (module) sandwich->inputMethods.push_back(module->createContext());

    for (vector<string>::iterator iter = m_cfgActivatedOutputFilters.begin();
         iter != m_cfgActivatedOutputFilters.end(); ++iter) {
      module = moduleWithName(*iter);
      if (module) sandwich->outputFilters.push_back(module->createContext());
    }

    for (vector<string>::iterator iter = m_cfgActivatedAroundFilters.begin();
         iter != m_cfgActivatedAroundFilters.end(); ++iter) {
      module = moduleWithName(*iter);

      // if module exists *and* it's not in a skipped set
      if (module && (m_skippedAroundFilterIdentifiers.find(*iter) ==
                     m_skippedAroundFilterIdentifiers.end())) {
        sandwich->aroundFilters.push_back(module->createContext());
      }
    }

    sandwich->prepareShortcutContexts();

    return sandwich;
  }

  OVModule* moduleWithName(const string& name) {
    // load the module
    OVModule* module = m_moduleManager->moduleWithName(name, m_loaderPolicy,
                                                       m_loaderService, true);
    return module;
  }

  vector<OVModule*> modulesWithNames(const vector<string>& names) {
    vector<OVModule*> result;
    for (vector<string>::const_iterator iter = names.begin();
         iter != names.end(); ++iter) {
      OVModule* module = moduleWithName(*iter);
      if (module) result.push_back(module);
    }
    return result;
  }

  int generation() { return m_generation; }

  bool locked() { return m_loaderLocked; }

  void registerContext(PVLoaderContext* context) {
    m_contextStore.insert(context);
  }

  void unregisterContext(PVLoaderContext* context) {
    m_contextStore.erase(context);
  }

  PVLoaderContext* createContext() { return new PVLoaderContext(this); }

  // used by the platform-dependent code
  bool registerCallback(PVLoaderCallback* callback) {
    if (m_callback) return false;

    m_callback = callback;
    return true;
  }

  bool saveChangedRootDictionary() {
    if (m_loaderLocked) return false;

    m_loaderLocked = true;
    validateLoaderConfig();
    saveLoaderConfig();
    m_loaderLocked = false;
    return true;
  }

  PVPlistValue* configRootDictionary() {
    return m_loaderPropertyList->rootDictionary();
  }

  // same as above, just it's an easier interface to use
  OVKeyValueMap configKeyValueMap() {
    return m_loaderPropertyList->rootDictionary()->keyValueMap();
  }

  PVPlistValue* configDictionaryForModule(const string moduleName) {
    return m_moduleManager->configDictionaryForModule(moduleName);
  }

  PVPlistValue* loaderAndModulePropertyListsCombined() {
    PVPlistValue* pv = m_moduleManager->allModulePropertyListsCombined();
    pv->setKeyValue("Loader", configRootDictionary());
    return pv;
  }

  void setSkippedAroundFilterIdentifiers(const set<string>& identifierSet) {
    m_skippedAroundFilterIdentifiers.clear();

    for (set<string>::const_iterator si = identifierSet.begin();
         si != identifierSet.end(); ++si) {
      m_skippedAroundFilterIdentifiers.insert(*si);
    }
  }

 protected:
  void validatePrimaryInputMethod() {
    PVPlistValue* dict = m_loaderPropertyList->rootDictionary();
    OVModule* inputMethodModule = moduleWithName(m_cfgPrimaryInputMethod);

    // can't find the module with the name, we fall back to the first of the
    // list
    if (!inputMethodModule) {
      m_loaderService->logger("Loader")
          << "No input method module found." << endl;

      m_cfgPrimaryInputMethod = "";

      vector<string> inputMethods = allInputMethodIdentifiers();
      for (vector<string>::iterator iter = inputMethods.begin();
           iter != inputMethods.end(); ++iter) {
        if (!isFailedModule(*iter) && moduleWithName(*iter)) {
          m_cfgPrimaryInputMethod = *iter;
          m_loaderService->logger("Loader")
              << "Falling back to input method: " << *iter << endl;
          break;
        }
      }
    } else {
      //                m_loaderService->logger("Loader") << "Input method
      //                module found." << endl;
    }

    //            m_loaderService->logger("Loader") << "Primary input method: "
    //            << m_cfgPrimaryInputMethod << endl;
    dict->setKeyValue("PrimaryInputMethod", m_cfgPrimaryInputMethod);
  }

  void validateActivatedOutputFilters() {
    vector<OVModule*> modules;
    vector<string> result;
    PVPlistValue arrayValue(PVPlistValue::Array);

    for (vector<string>::iterator iter = m_cfgActivatedOutputFilters.begin();
         iter != m_cfgActivatedOutputFilters.end(); ++iter) {
      OVModule* outputFilterModule = moduleWithName(*iter);
      if (outputFilterModule) {
        if (outputFilterModule->isOutputFilter()) {
          modules.push_back(outputFilterModule);
        }
      }
    }

    sort(modules.begin(), modules.end(), PVModuleCompare());

    for (vector<OVModule*>::iterator miter = modules.begin();
         miter != modules.end(); ++miter) {
      result.push_back((*miter)->identifier());
      PVPlistValue stringValue((*miter)->identifier());
      arrayValue.addArrayElement(&stringValue);
    }

    PVPlistValue* dict = m_loaderPropertyList->rootDictionary();
    dict->setKeyValue("ActivatedOutputFilters", &arrayValue);
    m_cfgActivatedOutputFilters = result;
  }

  void validateActivatedAroundFilters() {
    vector<OVModule*> modules;
    vector<string> result;
    PVPlistValue arrayValue(PVPlistValue::Array);

    for (vector<string>::iterator iter = m_cfgActivatedAroundFilters.begin();
         iter != m_cfgActivatedAroundFilters.end(); ++iter) {
      OVModule* aroundFilterModule = moduleWithName(*iter);
      if (aroundFilterModule) {
        if (aroundFilterModule->isAroundFilter()) {
          modules.push_back(aroundFilterModule);
        }
      }
    }

    sort(modules.begin(), modules.end(), PVModuleCompare());

    for (vector<OVModule*>::iterator miter = modules.begin();
         miter != modules.end(); ++miter) {
      result.push_back((*miter)->identifier());
      PVPlistValue stringValue((*miter)->identifier());
      arrayValue.addArrayElement(&stringValue);
    }

    PVPlistValue* dict = m_loaderPropertyList->rootDictionary();
    dict->setKeyValue("ActivatedAroundFilters", &arrayValue);
    m_cfgActivatedAroundFilters = result;
  }

  void validateExclusionSettings() {
    do {
      PVPlistValue a(PVPlistValue::Array);

      for (vector<string>::const_iterator pi =
               m_cfgExcludedModulePackages.begin();
           pi != m_cfgExcludedModulePackages.end(); ++pi) {
        PVPlistValue s(*pi);
        a.addArrayElement(&s);
      }

      PVPlistValue* dict = m_loaderPropertyList->rootDictionary();
      dict->setKeyValue("ExcludedModulePackages", &a);
    } while (0);

    do {
      PVPlistValue a(PVPlistValue::Array);

      for (vector<string>::const_iterator pi = m_cfgExcludedModules.begin();
           pi != m_cfgExcludedModules.end(); ++pi) {
        PVPlistValue s(*pi);
        a.addArrayElement(&s);
      }

      PVPlistValue* dict = m_loaderPropertyList->rootDictionary();
      dict->setKeyValue("ExcludedModules", &a);
    } while (0);
  }

  void validateLoaderConfig() {
    validatePrimaryInputMethod();
    validateActivatedOutputFilters();
    validateActivatedAroundFilters();
    validateExclusionSettings();
  }

  void loadLoaderConfig() {
    //            m_loaderService->logger("Loader") << "Updates config" << endl;
    m_loaderPropertyList->readSync();
    PVPlistValue* dict = m_loaderPropertyList->rootDictionary();

    // loads config values
    PVPlistValue* primaryInputMethodValue =
        dict->valueForKey("PrimaryInputMethod");
    if (primaryInputMethodValue)
      m_cfgPrimaryInputMethod = primaryInputMethodValue->stringValue();
    else
      m_cfgPrimaryInputMethod = "";

    m_cfgActivatedOutputFilters.clear();
    PVPlistValue* activatedOutputFiltersValue =
        dict->valueForKey("ActivatedOutputFilters");
    if (activatedOutputFiltersValue) {
      if (activatedOutputFiltersValue->type() == PVPlistValue::Array) {
        size_t size = activatedOutputFiltersValue->arraySize();
        size_t index;
        for (index = 0; index < size; index++) {
          PVPlistValue* element =
              activatedOutputFiltersValue->arrayElementAtIndex(index);
          if (element->type() == PVPlistValue::String)
            m_cfgActivatedOutputFilters.push_back(element->stringValue());
        }
      }
    }

    m_cfgActivatedAroundFilters.clear();
    PVPlistValue* activatedAroundFiltersValue =
        dict->valueForKey("ActivatedAroundFilters");
    if (activatedAroundFiltersValue) {
      if (activatedAroundFiltersValue->type() == PVPlistValue::Array) {
        size_t size = activatedAroundFiltersValue->arraySize();
        size_t index;
        for (index = 0; index < size; index++) {
          PVPlistValue* element =
              activatedAroundFiltersValue->arrayElementAtIndex(index);
          if (element->type() == PVPlistValue::String)
            m_cfgActivatedAroundFilters.push_back(element->stringValue());
        }
      }
    }

    m_cfgExcludedModulePackages.clear();
    PVPlistValue* excludedModulePackages =
        dict->valueForKey("ExcludedModulePackages");
    if (excludedModulePackages &&
        excludedModulePackages->type() == PVPlistValue::Array) {
      size_t size = excludedModulePackages->arraySize();
      size_t index;
      for (index = 0; index < size; index++) {
        PVPlistValue* element =
            excludedModulePackages->arrayElementAtIndex(index);
        if (element->type() == PVPlistValue::String) {
          m_cfgExcludedModulePackages.push_back(element->stringValue());
        }
      }
    }

    m_cfgExcludedModules.clear();
    PVPlistValue* excludedModules = dict->valueForKey("ExcludedModules");
    if (excludedModules && excludedModules->type() == PVPlistValue::Array) {
      size_t size = excludedModules->arraySize();
      size_t index;
      for (index = 0; index < size; index++) {
        PVPlistValue* element = excludedModules->arrayElementAtIndex(index);
        if (element->type() == PVPlistValue::String) {
          m_cfgExcludedModules.push_back(element->stringValue());
        }
      }
    }

    //            m_loaderService->logger("Loader") << "Loaded config, primary
    //            input method = " << m_cfgPrimaryInputMethod << endl;
  }

  void saveLoaderConfig() {
    m_loaderPropertyList->write();
    m_generation++;

    if (m_callback) m_callback->loaderConfigSaved(this);
  }

  void finalizeEverything() {
    if (m_loaderPropertyList) {
      delete m_loaderPropertyList;
      m_loaderPropertyList = 0;
    }

    if (m_moduleManager) {
      delete m_moduleManager;
      m_moduleManager = 0;
    }

    if (m_modulePackageManager) {
      delete m_modulePackageManager;
      m_modulePackageManager = 0;
    }
  }

  virtual void updateDisplaySubsystemSettings(PVPlistValue* dictionary) {
    // derive your display server update code here
  }

  virtual void updateLoaderServiceSettings(PVPlistValue* dictionary) {
    // derive your platform-dependent loader service update code here
  }

  void nullifyAllContexts() {
    for (set<PVLoaderContext*>::iterator iter = m_contextStore.begin();
         iter != m_contextStore.end(); ++iter) {
      (*iter)->m_loader = 0;
    }
  }

  void invalidateAllContexts() {
    for (set<PVLoaderContext*>::iterator iter = m_contextStore.begin();
         iter != m_contextStore.end(); ++iter) {
      (*iter)->invalidate();
    }
  }

  bool IsStringInVector(const string& str, const vector<string>& vec) {
    for (vector<string>::const_iterator iter = vec.begin(); iter != vec.end();
         ++iter)
      if (str == *iter) return true;

    return false;
  }

  // not managed by loader
  PVLoaderPolicy* m_loaderPolicy;
  PVLoaderService* m_loaderService;
  vector<PVModulePackageLoadingSystem*> m_modulePackageLoadingSystems;

  // managed by loader
  PVModulePackageManager* m_modulePackageManager;
  PVModuleManager* m_moduleManager;
  PVPropertyList* m_loaderPropertyList;
  PVLoaderContext* m_activeContext;
  set<PVLoaderContext*> m_contextStore;
  int m_generation;
  bool m_loaderLocked;

  // the callback system
  PVLoaderCallback* m_callback;

  // configurations
  string m_cfgPrimaryInputMethod;
  vector<string> m_cfgActivatedAroundFilters;
  vector<string> m_cfgActivatedOutputFilters;
  vector<string> m_cfgExcludedModulePackages;
  vector<string> m_cfgExcludedModules;

  // around filter skip set--reset every time when the context is activated
  set<string> m_skippedAroundFilterIdentifiers;
};

inline void PVLoaderContext::checkLastUsedPanel() {
  if (m_lastUsedPanel) {
    if (m_lastUsedPanel != m_candidateService->lastUsedPanel()) {
      m_lastUsedPanel->reset();
      m_lastUsedPanel = m_candidateService->lastUsedPanel();
    }
  } else
    m_lastUsedPanel = m_candidateService->lastUsedPanel();
}

inline PVLoaderContext::PVLoaderContext(PVLoader* loader)
    : m_loader(loader), m_generation(0), m_sandwich(0), m_focusedContext(0) {
  m_candidateService = new PVCandidateService(loader->loaderService());
  m_readingText = new PVTextBuffer;
  m_composingText = new PVTextBuffer;

  m_loader->registerContext(this);
}

inline PVLoaderContext::~PVLoaderContext() {
  delete m_composingText;
  delete m_readingText;
  delete m_candidateService;
  if (m_sandwich) delete m_sandwich;

  m_loader->unregisterContext(this);
}

inline void PVLoaderContext::invalidate() {
  m_generation = 0;
  if (m_sandwich) {
    delete m_sandwich;
    m_sandwich = 0;
  }
}

inline void PVLoaderContext::activate() {
  if (!shouldEnter(m_loader->loaderService(), false, true)) return;

  m_loader->syncSandwichConfig();
  m_candidateService->resetAll();
  m_sandwich->startAllContexts(m_loader->loaderService());
  m_focusedContext = 0;
}

inline void PVLoaderContext::clear() {
  if (!shouldEnter(m_loader->loaderService())) return;

  m_sandwich->clearAllContexts(m_loader->loaderService());
  m_candidateService->resetAll();
  m_readingText->clear();
  m_composingText->finishCommit();
  m_composingText->clear();
  m_focusedContext = 0;
}

inline void PVLoaderContext::deactivate() {
  if (!shouldEnter(m_loader->loaderService())) return;

  m_sandwich->stopAllContexts(m_loader->loaderService());
  m_loader->saveSandwichConfig();
  m_candidateService->resetAll();
  m_focusedContext = 0;
}

inline const string PVLoaderContext::residueComposingTextBeforeDeactivation() {
  if (!shouldEnter(m_loader->loaderService())) return string();

  vector<string> committedTextSegments;

  // get out all remaining composing buffer text and pass it thru the AFs and
  // OFs
  if (m_composingText->composedText().size()) {
    m_composingText->commit();
    committedTextSegments = m_composingText->composedCommittedTextSegments();

    // cerr << "new segs: " << OVStringHelper::Join(committedTextSegments) <<
    // endl;

    for (vector<OVEventHandlingContext*>::iterator iter =
             m_sandwich->aroundFilters.begin();
         iter != m_sandwich->aroundFilters.end(); ++iter) {
      PVTextBuffer fakeBuffer;
      PVTextBuffer fakeReading;

      if ((*iter)->handleDirectText(committedTextSegments, &fakeReading,
                                    &fakeBuffer, m_candidateService,
                                    m_loader->loaderService())) {
        if (fakeBuffer.isCommitted()) {
          committedTextSegments = fakeBuffer.composedCommittedTextSegments();
          // cerr << "new segs: " << OVStringHelper::Join(committedTextSegments)
          // << endl;
        }
      }
    }
  }

  string committedText = OVStringHelper::Join(committedTextSegments);
  // cerr << "new text: " << committedText << endl;
  for (vector<OVEventHandlingContext*>::iterator iter =
           m_sandwich->outputFilters.begin();
       iter != m_sandwich->outputFilters.end(); ++iter) {
    if (!committedText.length()) break;

    string newText =
        (*iter)->filterText(committedText, m_loader->loaderService());
    if (newText != committedText) committedText = newText;
  }
  // cerr << "new text: " << committedText << endl;

  return committedText;
}

inline bool PVLoaderContext::handleKeyEvent(OVKey* key) {
  OVLoaderService* loaderService = m_loader->loaderService();
  if (!shouldEnter(loaderService, true)) return false;

  bool handled = false;

  OVCandidatePanel* lastUsedCandidatePanel =
      m_candidateService->lastUsedPanel();
  if (!lastUsedCandidatePanel) {
    lastUsedCandidatePanel = m_candidateService->accessVerticalCandidatePanel();
  }

  PVOneDimensionalCandidatePanel* odpanel =
      m_candidateService->accessVerticalCandidatePanel();
  if (lastUsedCandidatePanel ==
      m_candidateService->accessHorizontalCandidatePanel()) {
    odpanel = m_candidateService->accessHorizontalCandidatePanel();
  }

  // if any of the (at least one dimensional) panels are in control
  if (odpanel->isInControl()) {
    PVCandidateState::State candidateState =
        odpanel->handleKeyEvent(*key, loaderService);

    switch (candidateState) {
      // if candidate is chosen, we get into the filter flow
      case PVCandidateState::CandidateChosen:
        odpanel->stopControl();
        m_focusedContext->candidateSelected(
            m_candidateService, odpanel->chosenCandidateString(),
            odpanel->chosenCandidateIndex(), m_readingText, m_composingText,
            loaderService);
        odpanel->reset();
        m_focusedContext = 0;
        handled = true;
        break;

      case PVCandidateState::Canceled:
        odpanel->stopControl();
        m_focusedContext->candidateCanceled(m_candidateService, m_readingText,
                                            m_composingText, loaderService);
        odpanel->reset();
        m_focusedContext = 0;
        handled = true;
        break;

      case PVCandidateState::UpdatePage:
      case PVCandidateState::UpdateCandidateHighlight:
        odpanel->setShouldUpdate(true);
        return true;

      case PVCandidateState::ReachedPageBoundary:
        odpanel->setShouldUpdate(true);
        loaderService->beep();
        return true;

      case PVCandidateState::InvalidCandidateKey:
        loaderService->beep();
        return true;
        break;

      case PVCandidateState::Ignored:
      default:
        if (!(handled = m_focusedContext->candidateNonPanelKeyReceived(
                  m_candidateService, key, m_readingText, m_composingText,
                  loaderService))) {
          loaderService->beep();
          return true;
        }

        if (!odpanel->isInControl()) odpanel->reset();
    }
  } else {
    vector<OVEventHandlingContext*>::iterator iter;
    vector<OVEventHandlingContext*>::iterator begin =
        m_sandwich->keyHandlingContexts.begin();
    vector<OVEventHandlingContext*>::iterator end =
        m_sandwich->keyHandlingContexts.end();
    vector<OVEventHandlingContext*> alternatives;

    if (m_focusedContext) {
      alternatives.push_back(m_focusedContext);
      for (iter = begin; iter != end; ++iter) {
        if (*iter != m_focusedContext) alternatives.push_back(*iter);
      }
    }

    begin = m_sandwich->keyHandlingContexts.begin();
    end = m_sandwich->keyHandlingContexts.end();

    // key event handling sequence
    m_focusedContext = 0;
    m_lastUsedPanel = 0;

    for (iter = begin; iter != end; ++iter) {
      handled = (*iter)->handleKey(key, m_readingText, m_composingText,
                                   m_candidateService, loaderService);

      // if a panel is in control, but the key is not handled, it's not valid
      // (@todo implement it!)

      if (handled) {
        m_focusedContext = *iter;
        break;
      }
    }
  }

  // start the filter session
  if (!m_composingText->isCommitted()) return handled;

  if (handled) {
    string committedText = m_composingText->composedCommittedText();
    vector<string> committedTextSegments =
        m_composingText->composedCommittedTextSegments();

    vector<OVEventHandlingContext*>::iterator iter;

    if (committedText.size() &&
        /* m_composingText->isEmpty() && */ !lastUsedCandidatePanel
            ->isVisible()) {
      // if we have committed text, and m_composingText is empty, and no
      // candidate panel is in control, then we run around filters first
      bool hasAroundFilterEvent = false;

      m_composingText->finishCommit();

      OVEventHandlingContext* m_oldFocus = m_focusedContext;

      // loaderService->logger("sandwich") << "before entry, commited text: " <<
      // committedText << endl;
      for (iter = m_sandwich->aroundFilters.begin();
           iter != m_sandwich->aroundFilters.end(); ++iter) {
        // cannot feed the focus context's committed text to the around filter,
        // will cause infinite loop
        if (m_oldFocus == *iter) continue;

        // loaderService->logger("sandwich") << "in sandwich, commited text: "
        // << committedText << endl; loaderService->logger("sandwich") <<
        // "Segments: " << OVStringHelper::Join(committedTextSegments, ", ") <<
        // endl;

        if ((*iter)->handleDirectText(committedTextSegments, m_readingText,
                                      m_composingText, m_candidateService,
                                      loaderService)) {
          // loaderService->logger("sandwich") << "handled, commited text: " <<
          // m_composingText->composedCommittedText() << endl;

          hasAroundFilterEvent = true;

          m_focusedContext = *iter;
          checkLastUsedPanel();

          if (!m_composingText->isCommitted()) {
            return handled;
          } else {
            committedText = m_composingText->composedCommittedText();
            committedTextSegments =
                m_composingText->composedCommittedTextSegments();
            m_composingText->finishCommit();
          }
        } else {
          // loaderService->logger("sandwich") << "not handled" << endl;
        }
      }

      m_composingText->setComposedCommittedText(committedText);
    }

    for (iter = m_sandwich->outputFilters.begin();
         iter != m_sandwich->outputFilters.end(); ++iter) {
      if (!committedText.length()) break;

      string newText = (*iter)->filterText(committedText, loaderService);
      if (newText != committedText) committedText = newText;
    }

    m_composingText->setComposedCommittedText(committedText);
  }

  if (m_focusedContext && m_composingText->isEmpty() && !odpanel->isInControl())
    m_focusedContext = 0;

  return handled;
}

inline bool PVLoaderContext::shouldUpdateCandidate() {
  checkLastUsedPanel();

  if (m_lastUsedPanel == m_candidateService->accessPlainTextCandidatePanel())
    return m_candidateService->accessPlainTextCandidatePanel()->shouldUpdate();
  else if (m_lastUsedPanel ==
           m_candidateService->accessVerticalCandidatePanel())
    return m_candidateService->accessVerticalCandidatePanel()->shouldUpdate();

  return false;
}

inline bool PVLoaderContext::shouldUpdateBufferedTexts() {
  return m_composingText->shouldUpdate() || m_readingText->shouldUpdate();
}

inline PVTextBuffer* PVLoaderContext::readingText() { return m_readingText; }

inline PVTextBuffer* PVLoaderContext::composingText() {
  return m_composingText;
}

inline PVCandidateService* PVLoaderContext::candidateService() {
  return m_candidateService;
}

inline bool PVLoaderContext::shouldEnter(OVLoaderService* loaderService,
                                         bool activateSandwichIfReloaded,
                                         bool syncLoaderConfig) {
  if (!m_loader) return false;

  if (m_loader->locked()) return false;

  // pool module config here
  if (syncLoaderConfig) m_loader->syncLoaderConfig();

  if (m_generation != m_loader->generation()) {
    m_focusedContext = 0;

    PVContextSandwich* tmp = m_sandwich;
    m_sandwich = m_loader->createSandwich();

    if (tmp) delete tmp;

    if (activateSandwichIfReloaded) {
      m_sandwich->startAllContexts(loaderService);
    }

    m_candidateService->resetAll();

    m_generation = m_loader->generation();
  }

  return true;
}
};  // namespace OpenVanilla

#endif
