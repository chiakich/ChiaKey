//
// ChiaKeyCore.cpp
//

#include "ChiaKeyCore/ChiaKeyCore.h"

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#include <PlainVanilla/PlainVanilla.h>
#else
#include "OpenVanilla.h"
#include "PlainVanilla.h"
#endif

#include "OVIMMandarinPackage.h"
#include "OVIMSmartMandarin.h"

#include <unistd.h>

#include <cstdio>
#include <memory>
#include <mutex>
#include <sstream>
#include <utility>

namespace ChiaKey {
namespace {

using OpenVanilla::OVCandidateList;
using OpenVanilla::OVCandidatePanel;
using OpenVanilla::OVDirectoryHelper;
using OpenVanilla::OVEventHandlingContext;
using OpenVanilla::OVIMMandarinPackage;
using OpenVanilla::OVIMSmartMandarinContext;
using OpenVanilla::OVKey;
using OpenVanilla::OVKeyMask;
using OpenVanilla::OVKeyValueMap;
using OpenVanilla::OVModule;
using OpenVanilla::OVPathHelper;
using OpenVanilla::OVPathInfo;
using OpenVanilla::OVSQLiteDatabaseService;
using OpenVanilla::PVLoader;
using OpenVanilla::PVLoaderContext;
using OpenVanilla::PVLoaderPolicy;
using OpenVanilla::PVLoaderService;
using OpenVanilla::PVModulePackageLoadingSystem;
using OpenVanilla::PVOneDimensionalCandidatePanel;
using OpenVanilla::PVPlistValue;
using OpenVanilla::PVPropertyList;
using OpenVanilla::PVStaticModulePackageLoadingSystem;
using OpenVanilla::PVTextBuffer;

const char kMandarinPackageName[] = "OVIMMandarin";
const char kPreferencesDirectoryName[] = "Preferences";

// CheckDirectory() only proves the path exists and is a directory, so a
// read-only path would get through and every later write -- preferences, the
// learning database -- would fail silently.
bool DirectoryIsWritable(const std::string& path) {
  // Per-pid name: release and Dev builds share this directory, and a fixed
  // name would let one delete the other's probe mid-check.
  std::ostringstream name;
  name << ".chiakey-write-probe." << static_cast<long>(getpid());
  const std::string probe = OVPathHelper::PathCat(path, name.str());
  std::FILE* file = std::fopen(probe.c_str(), "w");
  if (!file) return false;

  std::fclose(file);
  std::remove(probe.c_str());
  return true;
}

bool CheckWritableDirectory(const std::string& path) {
  return OVDirectoryHelper::CheckDirectory(path) && DirectoryIsWritable(path);
}

unsigned int MakeModifierMask(const KeyModifiers& modifiers) {
  unsigned int mask = 0;
  if (modifiers.alt) mask |= OVKeyMask::Alt;
  if (modifiers.opt) mask |= OVKeyMask::Opt;
  if (modifiers.ctrl) mask |= OVKeyMask::Ctrl;
  if (modifiers.shift) mask |= OVKeyMask::Shift;
  if (modifiers.command) mask |= OVKeyMask::Command;
  if (modifiers.numLock) mask |= OVKeyMask::NumLock;
  if (modifiers.capsLock) mask |= OVKeyMask::CapsLock;
  if (modifiers.directText) mask |= OVKeyMask::DirectText;
  return mask;
}

OVKey MakeKey(const KeyEvent& event) {
  const unsigned int mask = MakeModifierMask(event.modifiers);
  if (!event.receivedString.empty()) {
    return OVKey(new OpenVanilla::PVKeyImpl(
        event.receivedString, static_cast<unsigned int>(event.keyCode), mask));
  }

  return OVKey(new OpenVanilla::PVKeyImpl(
      static_cast<unsigned int>(event.keyCode), mask));
}

void ApplyConfig(const EngineConfig& source, OVKeyValueMap* target) {
  target->setKeyStringValue("KeyboardLayout", source.keyboardLayout);
  target->setKeyStringValue("CandidateSelectionKeys",
                            source.candidateSelectionKeys);
  target->setKeyBoolValue("CandidateCursorAtEndOfTargetBlock",
                          source.candidateCursorAtEndOfTargetBlock);
  target->setKeyBoolValue("ShowCandidateListWithSpace",
                          source.showCandidateListWithSpace);
  target->setKeyBoolValue("ClearComposingTextWithEsc",
                          source.clearComposingTextWithEsc);
  target->setKeyBoolValue("ShiftKeyAlwaysCommitUppercaseCharacters",
                          source.shiftKeyAlwaysCommitUppercaseCharacters);
  target->setKeyIntValue("ComposingTextBufferSize",
                         static_cast<int>(source.composingTextBufferSize));
}

std::vector<TextRange> ConvertRanges(
    const std::vector<OpenVanilla::OVTextBuffer::RangePair>& ranges) {
  std::vector<TextRange> result;
  result.reserve(ranges.size());
  for (const auto& pair : ranges) {
    TextRange range;
    range.location = pair.first;
    range.length = pair.second;
    result.push_back(range);
  }
  return result;
}

std::vector<std::string> CandidateListToVector(OVCandidateList* list) {
  std::vector<std::string> result;
  if (!list) return result;

  const std::size_t count = list->size();
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    result.push_back(list->candidateAtIndex(index));
  }
  return result;
}

// Keeps the plists under the host's writable path, so the core never shares
// preferences with the IMK host.
class CorePolicy : public PVLoaderPolicy {
 public:
  explicit CorePolicy(const std::string& writablePath)
      : PVLoaderPolicy(std::vector<std::string>()),
        preferencesPath_(
            OVPathHelper::PathCat(writablePath, kPreferencesDirectoryName)) {}

  const std::string defaultDatabaseFileName() override {
    return "ChiaKeySource.db";
  }
  const std::string loaderIdentifier() override { return "com.chiakey.core"; }
  const std::string loaderName() override { return "ChiaKey"; }
  // static packages only; also sidesteps the Linux #error in the base class
  const std::vector<std::string> modulePackageFilePatterns() override {
    return std::vector<std::string>();
  }
  const std::string propertyListPathForLoader() override {
    return OVPathHelper::PathCat(preferencesPath_, "Loader.plist");
  }
  const std::string propertyListPathFromIdentifier(
      const std::string& identifier) override {
    return OVPathHelper::PathCat(preferencesPath_, identifier + ".plist");
  }

  const std::string& preferencesPath() const { return preferencesPath_; }

 private:
  std::string preferencesPath_;
};

class CoreContext : public PVLoaderContext {
 public:
  explicit CoreContext(PVLoader* loader) : PVLoaderContext(loader) {}

  OVIMSmartMandarinContext* smartMandarinContext() {
    if (!m_sandwich) return nullptr;
    for (OVEventHandlingContext* context : m_sandwich->inputMethods) {
      if (auto* smart = dynamic_cast<OVIMSmartMandarinContext*>(context)) {
        return smart;
      }
    }
    return nullptr;
  }

  PVOneDimensionalCandidatePanel* activePanel() {
    auto* panel = dynamic_cast<PVOneDimensionalCandidatePanel*>(
        m_candidateService->lastUsedPanel());
    return panel ? panel : m_candidateService->accessVerticalCandidatePanel();
  }

  // Goes through the panel's own key so the filters run as they would for a
  // typed selection.
  bool selectCandidate(std::size_t candidateIndex) {
    PVOneDimensionalCandidatePanel* panel = activePanel();
    OVCandidateList* list = panel->candidateList();
    if (!panel->isVisible() || !panel->isInControl() || !list ||
        candidateIndex >= list->size() || !panel->candidatesPerPage()) {
      return false;
    }

    const std::size_t page = candidateIndex / panel->candidatesPerPage();
    if (page != panel->currentPage()) panel->goToPage(page);

    OVKey key = panel->candidateKeyAtIndex(
        candidateIndex - page * panel->candidatesPerPage());
    return handleKeyEvent(&key);
  }
};

}  // namespace

class Runtime::Impl {
 public:
  bool initialize(const RuntimePaths& runtimePaths,
                  const EngineConfig& engineConfig, std::string* errorMessage) {
    paths = runtimePaths;
    config = engineConfig;

    // Expand once, here: CorePolicy joins writablePath into plist paths that
    // PVPropertyList opens verbatim, so a literal "~/..." would create a
    // directory named "~" instead of writing into the home directory.
    if (!paths.writablePath.empty()) {
      paths.writablePath =
          OVPathHelper::NormalizeByExpandingTilde(paths.writablePath);
    }

    if (paths.lexiconDatabasePath.empty()) {
      if (errorMessage) *errorMessage = "lexiconDatabasePath is required";
      return false;
    }
    if (paths.writablePath.empty()) {
      if (errorMessage) *errorMessage = "writablePath is required";
      return false;
    }

    // sqlite3_open() happily creates an empty database, which Smart Mandarin
    // would then fill with placeholder tables instead of failing -- the host
    // never gets to fall back to its bundled lexicon.
    if (!OVPathHelper::PathExists(paths.lexiconDatabasePath)) {
      if (errorMessage) {
        *errorMessage =
            "lexicon database does not exist: " + paths.lexiconDatabasePath;
      }
      return false;
    }

    database.reset(OVSQLiteDatabaseService::Create(paths.lexiconDatabasePath));
    if (!database) {
      if (errorMessage) {
        std::ostringstream stream;
        stream << "failed to open lexicon database: "
               << paths.lexiconDatabasePath;
        *errorMessage = stream.str();
      }
      return false;
    }

    std::string missingTable;
    if (!OpenVanilla::ValidateChiaKeySourceDatabase(database->connection(),
                                                    &missingTable)) {
      database.reset();
      if (errorMessage) {
        *errorMessage = "lexicon database is missing the table '" +
                        missingTable + "': " + paths.lexiconDatabasePath;
      }
      return false;
    }

    policy.reset(new CorePolicy(paths.writablePath));
    // An unwritable path would otherwise surface much later as the loader
    // silently falling back to another input method.
    if (!CheckWritableDirectory(paths.writablePath)) {
      if (errorMessage) {
        *errorMessage = "writablePath is not a writable directory: " +
                        paths.writablePath;
      }
      return false;
    }
    if (!CheckWritableDirectory(policy->preferencesPath())) {
      if (errorMessage) {
        *errorMessage = "preferences directory is not writable: " +
                        policy->preferencesPath();
      }
      return false;
    }

    // Seed the default only when the plist has no choice yet; without it the
    // loader falls back to whichever module sorts first. An existing value is
    // the user's own pick and has to survive a Runtime rebuild.
    {
      PVPropertyList loaderPlist(policy->propertyListPathForLoader());
      PVPlistValue* root = loaderPlist.rootDictionary();
      if (!root->valueForKey("PrimaryInputMethod")) {
        root->setKeyValue("PrimaryInputMethod", OVIMSMARTMANDARIN_IDENTIFIER);
        loaderPlist.write();
      }
    }
    writeModuleConfig();

    service.reset(new PVLoaderService(config.locale, nullptr, database.get()));

    OVPathInfo pathInfo;
    pathInfo.loadedPath = paths.loadedPath;
    pathInfo.resourcePath = paths.resourcePath;
    pathInfo.writablePath = paths.writablePath;
    packages.reset(new PVStaticModulePackageLoadingSystem(pathInfo, true));

    auto* mandarin = new OVIMMandarinPackage;
    if (!mandarin->initialize(&pathInfo, service.get()) ||
        !packages->addInitializedPackage(kMandarinPackageName, mandarin)) {
      mandarin->finalize();
      delete mandarin;
      if (errorMessage) *errorMessage = "OVIMMandarin package failed to load";
      return false;
    }

    std::vector<PVModulePackageLoadingSystem*> systems{packages.get()};
    loader.reset(new PVLoader(policy.get(), service.get(), systems));

    // Ask the module directly: the primary input method is the user's choice
    // and says nothing about whether Smart Mandarin's lexicon loaded.
    if (!loader->moduleWithName(OVIMSMARTMANDARIN_IDENTIFIER)) {
      if (errorMessage) {
        *errorMessage =
            "OVIMSmartMandarin failed to initialize (lexicon database "
            "incompatible or unreadable)";
      }
      return false;
    }
    return true;
  }

  void writeModuleConfig() {
    PVPropertyList plist(
        policy->propertyListPathFromIdentifier(OVIMSMARTMANDARIN_IDENTIFIER));
    OVKeyValueMap map = plist.rootDictionary()->keyValueMap();
    ApplyConfig(config, &map);
    plist.write();
  }

  mutable std::recursive_mutex mutex;
  RuntimePaths paths;
  EngineConfig config;

  // order matters: the loader tears down its modules before the package
  // system, the service and the database go
  std::unique_ptr<OVSQLiteDatabaseService> database;
  std::unique_ptr<CorePolicy> policy;
  std::unique_ptr<PVLoaderService> service;
  std::unique_ptr<PVStaticModulePackageLoadingSystem> packages;
  std::unique_ptr<PVLoader> loader;
};

class Engine::Impl {
 public:
  Impl(std::shared_ptr<Runtime> owner, std::unique_ptr<CoreContext> loaderContext)
      : runtime(std::move(owner)), context(std::move(loaderContext)) {
    std::lock_guard<std::recursive_mutex> lock(runtime->impl_->mutex);
    context->activate();
  }

  ~Impl() {
    std::lock_guard<std::recursive_mutex> lock(runtime->impl_->mutex);
    context->deactivate();
    context.reset();
  }

  PVLoaderService* service() const { return runtime->impl_->service.get(); }

  bool handleKey(const KeyEvent& event) {
    std::lock_guard<std::recursive_mutex> lock(runtime->impl_->mutex);
    service()->resetState();
    OVKey key = MakeKey(event);
    return context->handleKeyEvent(&key);
  }

  bool selectCandidate(std::size_t candidateIndex) {
    std::lock_guard<std::recursive_mutex> lock(runtime->impl_->mutex);
    service()->resetState();
    if (context->selectCandidate(candidateIndex)) return true;
    service()->beep();
    return false;
  }

  void reset() {
    std::lock_guard<std::recursive_mutex> lock(runtime->impl_->mutex);
    service()->resetState();
    context->clear();
    context->readingText()->finishCommit();
  }

  EngineState snapshot() const {
    std::lock_guard<std::recursive_mutex> lock(runtime->impl_->mutex);
    PVTextBuffer* readingText = context->readingText();
    PVTextBuffer* composingText = context->composingText();

    EngineState state;
    state.readingText = readingText->composedText();
    state.composingText = composingText->composedText();
    state.committedText = readingText->composedCommittedText() +
                          composingText->composedCommittedText();

    const std::vector<std::string> readingSegments =
        readingText->composedCommittedTextSegments();
    state.committedTextSegments.insert(state.committedTextSegments.end(),
                                       readingSegments.begin(),
                                       readingSegments.end());
    const std::vector<std::string> composingSegments =
        composingText->composedCommittedTextSegments();
    state.committedTextSegments.insert(state.committedTextSegments.end(),
                                       composingSegments.begin(),
                                       composingSegments.end());

    state.cursorPosition = composingText->cursorPosition();
    state.highlight.location = composingText->highlightMark().first;
    state.highlight.length = composingText->highlightMark().second;
    state.wordSegments = ConvertRanges(composingText->wordSegments());
    state.tooltip = composingText->toolTipText();
    state.beeped = service()->shouldBeep();
    state.notifications = service()->notifyMessage();

    PVOneDimensionalCandidatePanel* panel = context->activePanel();
    state.candidateState.visible = panel->isVisible();
    state.candidateState.currentPage = panel->currentPage();
    state.candidateState.pageCount = panel->pageCount();
    state.candidateState.candidatesPerPage = panel->candidatesPerPage();
    state.candidateState.highlightedIndex = panel->currentHightlightIndex();
    state.candidateState.highlightedCandidateIndex =
        panel->currentHightlightIndexInCandidateList();
    state.candidateState.candidates =
        CandidateListToVector(panel->candidateList());

    // other fillers (associated phrases) share this panel, so take the flags
    // only when they still describe a list of exactly this length
    OVIMSmartMandarinContext* smartContext = context->smartMandarinContext();
    if (smartContext && state.candidateState.visible &&
        !state.candidateState.candidates.empty()) {
      const std::vector<bool>& contextPicks =
          smartContext->latestCandidateContextPicks();
      if (contextPicks.size() == state.candidateState.candidates.size()) {
        state.candidateState.contextPicks = contextPicks;
      }
    }
    return state;
  }

  void acknowledgeCommit() {
    std::lock_guard<std::recursive_mutex> lock(runtime->impl_->mutex);
    context->readingText()->finishCommit();
    context->composingText()->finishCommit();
  }

  std::shared_ptr<Runtime> runtime;
  std::unique_ptr<CoreContext> context;
};

// Runtime

std::shared_ptr<Runtime> Runtime::Create(const RuntimePaths& paths,
                                         const EngineConfig& config,
                                         std::string* errorMessage) {
  std::unique_ptr<Impl> impl(new Impl);
  if (!impl->initialize(paths, config, errorMessage)) {
    return std::shared_ptr<Runtime>();
  }
  return std::shared_ptr<Runtime>(new Runtime(std::move(impl)));
}

Runtime::Runtime(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Runtime::~Runtime() {}

std::unique_ptr<Engine> Runtime::createEngine(std::string* errorMessage) {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  if (impl_->loader->locked()) {
    if (errorMessage) *errorMessage = "loader is reloading";
    return std::unique_ptr<Engine>();
  }

  std::unique_ptr<CoreContext> context(new CoreContext(impl_->loader.get()));
  std::unique_ptr<Engine::Impl> engineImpl(
      new Engine::Impl(shared_from_this(), std::move(context)));
  return std::unique_ptr<Engine>(new Engine(std::move(engineImpl)));
}

void Runtime::setConfig(const EngineConfig& config) {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  // locale is fixed at Create: the loader service was built with it
  impl_->config = config;
  impl_->config.locale = impl_->service->locale();
  impl_->writeModuleConfig();
  impl_->loader->forceSyncModuleConfigForNextRound(
      OVIMSMARTMANDARIN_IDENTIFIER);
  impl_->loader->syncSandwichConfig();
}

EngineConfig Runtime::config() const {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  return impl_->config;
}

std::vector<std::pair<std::string, std::string>> Runtime::inputMethods() const {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  std::vector<std::pair<std::string, std::string>> result;
  const std::string locale = impl_->service->locale();
  for (const std::string& identifier : impl_->loader->allInputMethodIdentifiers()) {
    OVModule* module = impl_->loader->moduleWithName(identifier);
    if (!module) continue;
    result.emplace_back(identifier, module->localizedName(locale));
  }
  return result;
}

std::string Runtime::primaryInputMethod() const {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  return impl_->loader->primaryInputMethod();
}

bool Runtime::setPrimaryInputMethod(const std::string& identifier) {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  OVModule* module = impl_->loader->moduleWithName(identifier);
  if (!module || !module->isInputMethod()) return false;

  impl_->loader->setPrimaryInputMethod(identifier);
  impl_->loader->syncSandwichConfig();
  return impl_->loader->primaryInputMethod() == identifier;
}

bool Runtime::associatedPhrasesEnabled() const {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  return impl_->loader->isAroundFilterActivated(OVAFASSOCIATEDPHRASE_IDENTIFIER);
}

void Runtime::setAssociatedPhrasesEnabled(bool enabled) {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  if (associatedPhrasesEnabled() == enabled) return;
  impl_->loader->toggleAroundFilter(OVAFASSOCIATEDPHRASE_IDENTIFIER);
  impl_->loader->syncSandwichConfig();
}

const char* Runtime::SmartMandarinIdentifier() {
  return OVIMSMARTMANDARIN_IDENTIFIER;
}

const char* Runtime::TraditionalMandarinIdentifier() {
  return OVIMTRADITIONALMANDARIN_IDENTIFIER;
}

// Engine

std::unique_ptr<Engine> Engine::Create(const RuntimePaths& paths,
                                       const EngineConfig& config,
                                       std::string* errorMessage) {
  std::shared_ptr<Runtime> runtime = Runtime::Create(paths, config, errorMessage);
  if (!runtime) return std::unique_ptr<Engine>();
  return runtime->createEngine(errorMessage);
}

Engine::Engine(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Engine::~Engine() {}

bool Engine::handleKey(const KeyEvent& event) { return impl_->handleKey(event); }

bool Engine::handleAsciiKey(char key, const KeyModifiers& modifiers) {
  KeyEvent event;
  event.keyCode = key;
  event.receivedString = std::string(1, key);
  event.modifiers = modifiers;
  return handleKey(event);
}

bool Engine::selectCandidate(std::size_t candidateIndex) {
  return impl_->selectCandidate(candidateIndex);
}

void Engine::reset() { impl_->reset(); }

EngineState Engine::snapshot() const { return impl_->snapshot(); }

void Engine::acknowledgeCommit() { impl_->acknowledgeCommit(); }

std::shared_ptr<Runtime> Engine::runtime() const { return impl_->runtime; }

}  // namespace ChiaKey
