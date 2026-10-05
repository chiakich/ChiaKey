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

#include "OVIMGenericPackage.h"
#if defined(WIN32)
#include "OVAFReverseLookupPackage.h"
#endif
#include "OVIMMandarinPackage.h"
#include "OVIMSmartMandarin.h"

#if defined(WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <utility>

namespace ChiaKey {
namespace {

using OpenVanilla::OVCINDatabaseService;
using OpenVanilla::OVCandidateList;
using OpenVanilla::OVCandidatePanel;
using OpenVanilla::OVDirectoryHelper;
using OpenVanilla::OVEventHandlingContext;
using OpenVanilla::OVIMGenericPackage;
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
const char kGenericPackageName[] = "OVIMGeneric";
const char kPreferencesDirectoryName[] = "Preferences";
const char kTablesDirectoryName[] = "Tables";
const char kUserCannedMessagesPlistName[] = "UserCannedMessages.plist";
const char kUserCannedMessagesTextName[] = "UserCannedMessages.txt";

// CheckDirectory() only proves the path exists and is a directory, so a
// read-only path would get through and every later write -- preferences, the
// learning database -- would fail silently.
bool DirectoryIsWritable(const std::string& path) {
  // Per-pid name: release and Dev builds share this directory, and a fixed
  // name would let one delete the other's probe mid-check.
  std::ostringstream name;
#if defined(WIN32)
  name << ".chiakey-write-probe." << static_cast<long>(_getpid());
#else
  name << ".chiakey-write-probe." << static_cast<long>(getpid());
#endif
  const std::string probe = OVPathHelper::PathCat(path, name.str());
  // narrow CRT calls read UTF-8 as ANSI on Windows, failing CJK user folders
  std::FILE* file = OpenVanilla::OVFileHelper::OpenStream(probe, "w");
  if (!file) return false;

  std::fclose(file);
  OVPathHelper::RemoveEverythingAtPath(probe);
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

#if defined(WIN32)
// what UseCharactersSupportedByEncoding = BIG-5 filters on; the mac's CVEncodingService
// uses Big5-HKSCS, code page 950 is plain Big5 with Microsoft's additions
class CoreEncodingService : public OpenVanilla::PVDefaultEncodingService {
 public:
  bool codepointSupportedByEncoding(const std::string& codepoint,
                                    const std::string& encoding) override {
    if (encoding != "BIG-5") return true;
    const std::wstring wide = OpenVanilla::OVUTF16::FromUTF8(codepoint);
    if (wide.empty()) return true;
    char converted[8];
    BOOL usedDefault = FALSE;
    const int length = WideCharToMultiByte(
        950, WC_NO_BEST_FIT_CHARS, wide.data(), static_cast<int>(wide.size()), converted,
        static_cast<int>(sizeof(converted)), nullptr, &usedDefault);
    return length > 0 && !usedDefault;
  }

  const std::vector<std::string> supportedEncodings() override {
    return std::vector<std::string>{"UTF-8", "BIG-5"};
  }

  bool isEncodingSupported(const std::string& encoding) override {
    return encoding == "UTF-8" || encoding == "BIG-5";
  }
};
#endif

// a name is either a plain string or a {locale: string} dictionary
std::string LocalizedString(PVPlistValue* value, const std::string& locale) {
  if (!value) return std::string();
  if (value->type() == PVPlistValue::String) return value->stringValue();
  std::vector<std::string> keys{locale, "en"};
  for (const std::string& key : value->dictionaryKeys()) keys.push_back(key);
  for (const std::string& key : keys) {
    PVPlistValue* localized = value->valueForKey(key);
    if (localized && localized->type() == PVPlistValue::String) {
      return localized->stringValue();
    }
  }
  return std::string();
}

// messages spell a line break as a literal backslash-n
std::string ExpandLineBreaks(const std::string& text, const char* lineBreak) {
  return OpenVanilla::OVStringHelper::StringByReplacingOccurrencesOfStringWithString(
      text, "\\n", lineBreak);
}

SymbolItem MakeMessageItem(PVPlistValue* message, const std::string& locale) {
  std::string name;
  std::string text;
  if (message->type() == PVPlistValue::String) {
    name = text = message->stringValue();
  } else {
    name = LocalizedString(message->valueForKey("Name"), locale);
    text = LocalizedString(message->valueForKey("Text"), locale);
  }
  SymbolItem item;
  item.text = ExpandLineBreaks(text, "\n");
  item.label = ExpandLineBreaks(name.empty() ? text : name, " ");
  if (item.label == item.text) item.label.clear();
  return item;
}

// the same merge as the mac loader's mergeCannedMessagesData
void AppendSymbolCategories(PVPlistValue* root, const std::string& locale,
                            const std::string& now,
                            std::vector<SymbolCategory>* categories) {
  PVPlistValue* list = root ? root->valueForKey("CannedMessages") : nullptr;
  if (!list) return;
  for (std::size_t index = 0; index < list->arraySize(); ++index) {
    PVPlistValue* entry = list->arrayElementAtIndex(index);
    if (!entry || entry->type() != PVPlistValue::Dictionary) continue;
    const std::string notBefore = entry->stringValueForKey("NotBefore");
    const std::string notAfter = entry->stringValueForKey("NotAfter");
    if ((!notBefore.empty() && now < notBefore) || (!notAfter.empty() && now > notAfter)) {
      continue;
    }

    SymbolCategory category;
    category.name = LocalizedString(entry->valueForKey("Name"), locale);
    category.buttons = entry->isKeyTrue("IsSymbolButtonList");
    if (category.buttons) {
      PVPlistValue* buttons = entry->valueForKey("Buttons");
      PVPlistValue* metadata = entry->valueForKey("SymbolMetadata");
      for (std::size_t at = 0; buttons && at < buttons->arraySize(); ++at) {
        PVPlistValue* button = buttons->arrayElementAtIndex(at);
        if (!button || button->type() != PVPlistValue::String) continue;
        SymbolItem item;
        item.text = button->stringValue();
        if (item.text.empty()) continue;
        // keyed by the raw button string, spaces included
        PVPlistValue* info = metadata ? metadata->valueForKey(item.text) : nullptr;
        if (info && info->type() == PVPlistValue::Dictionary) {
          item.label = info->stringValueForKey("DisplayLabel");
          item.name = info->stringValueForKey("Name");
          item.description = info->stringValueForKey("Description");
        }
        category.items.push_back(item);
      }
    } else {
      PVPlistValue* messages = entry->valueForKey("Messages");
      for (std::size_t at = 0; messages && at < messages->arraySize(); ++at) {
        PVPlistValue* message = messages->arrayElementAtIndex(at);
        if (!message) continue;
        SymbolItem item = MakeMessageItem(message, locale);
        if (!item.text.empty()) category.items.push_back(item);
      }
    }
    if (!category.name.empty() && !category.items.empty()) {
      categories->push_back(category);
    }
  }
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

    const size_t indexOnPage = candidateIndex - page * panel->candidatesPerPage();
    panel->setHighlightIndex(indexOnPage);
    OVKey key = panel->selectionKeyAtIndex(indexOnPage);
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

    // Validated through a read-only connection, so a rejected lexicon is never
    // written to. The connection kept afterwards cannot be read-only itself:
    // Smart Mandarin ATTACHes the learning database to it, and an attached
    // database inherits the read-only flag, which would fail every learning write.
    {
      std::unique_ptr<OVSQLiteDatabaseService> probe(
          OVSQLiteDatabaseService::CreateReadOnly(paths.lexiconDatabasePath));
      if (!probe) {
        if (errorMessage) {
          std::ostringstream stream;
          stream << "failed to open lexicon database: "
                 << paths.lexiconDatabasePath;
          *errorMessage = stream.str();
        }
        return false;
      }

      std::string missingTable;
      if (!OpenVanilla::ValidateChiaKeySourceDatabase(probe->connection(),
                                                      &missingTable)) {
        if (errorMessage) {
          *errorMessage = "lexicon database is missing the table '" +
                          missingTable + "': " + paths.lexiconDatabasePath;
        }
        return false;
      }
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

    // Tables/Generic/x.cin becomes the Generic-x-cin input method
    const std::string tablesPath =
        OVPathHelper::PathCat(paths.writablePath, kTablesDirectoryName);
    if (OVPathHelper::IsDirectory(tablesPath)) {
      cinTables.reset(new OVCINDatabaseService(tablesPath, "*.cin", "", 0));
    }
#if defined(WIN32)
    encoding.reset(new CoreEncodingService);
#endif
    service.reset(new PVLoaderService(config.locale, cinTables.get(), database.get(),
                                      nullptr, encoding.get()));

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

    // Cangjie, Simplex and user tables; none of them is required to type
    auto* generic = new OVIMGenericPackage;
    if (!generic->initialize(&pathInfo, service.get()) ||
        !packages->addInitializedPackage(kGenericPackageName, generic)) {
      generic->finalize();
      delete generic;
    }

#if defined(WIN32)
    auto* reverse = new OpenVanilla::OVAFReverseLookupPackage;
    if (!reverse->initialize(&pathInfo, service.get()) ||
        !packages->addInitializedPackage("OVAFReverseLookup", reverse)) {
      reverse->finalize();
      delete reverse;
    }
#endif
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
  std::unique_ptr<OVCINDatabaseService> cinTables;
  // null off Windows, where the loader service falls back to UTF-8 only
  std::unique_ptr<OpenVanilla::OVEncodingService> encoding;
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

  bool isComposing() const {
    std::lock_guard<std::recursive_mutex> lock(runtime->impl_->mutex);
    return !context->readingText()->composedText().empty() ||
           !context->composingText()->composedText().empty() ||
           context->activePanel()->isVisible();
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
    if (state.candidateState.visible) {
      const std::size_t onPage = panel->currentPageCandidateCount();
      for (std::size_t index = 0; index < onPage; ++index) {
        state.candidateState.selectionKeys.push_back(
            panel->candidateKeyAtIndex(index).receivedString());
      }
    }

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

std::vector<std::pair<std::string, std::string>> Runtime::reverseLookupMethods() const {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  std::vector<std::pair<std::string, std::string>> result;
  for (const auto& method : impl_->loader->allAroundFilterIdentifiersAndNames())
    if (method.first.compare(0, 14, "ReverseLookup-") == 0) result.push_back(method);
  return result;
}

std::string Runtime::reverseLookupMethod() const {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  for (const auto& method : reverseLookupMethods())
    if (impl_->loader->isAroundFilterActivated(method.first)) return method.first;
  return {};
}

bool Runtime::setReverseLookupMethod(const std::string& identifier) {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  const auto methods = reverseLookupMethods();
  bool known = identifier.empty();
  for (const auto& method : methods) if (method.first == identifier) known = true;
  if (!known) return false;
  bool changed = false;
  for (const auto& method : methods) {
    const bool wanted = method.first == identifier;
    if (impl_->loader->isAroundFilterActivated(method.first) != wanted) {
      impl_->loader->toggleAroundFilter(method.first);
      changed = true;
    }
  }
  if (changed) impl_->loader->syncSandwichConfig();
  return true;
}

std::vector<SymbolCategory> Runtime::symbolCategories() const {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  const std::string locale = impl_->service->locale();
  const std::string now = OpenVanilla::OVDateTimeHelper::LocalDateTimeString();
  std::vector<SymbolCategory> categories;

  OpenVanilla::OVSQLiteStatementRef statement = impl_->database->connection()->prepare(
      "SELECT value FROM prepopulated_service_data WHERE key = %Q", "canned_messages");
  if (statement && statement->step() == SQLITE_ROW) {
    const char* value = statement->textOfColumn(0);
    std::unique_ptr<PVPlistValue> lexicon(
        value ? PVPropertyList::ParsePlistFromString(value) : nullptr);
    AppendSymbolCategories(lexicon.get(), locale, now, &categories);
  }

  const std::string plistPath =
      OVPathHelper::PathCat(impl_->paths.writablePath, kUserCannedMessagesPlistName);
  if (OVPathHelper::PathExists(plistPath)) {
    PVPropertyList user(plistPath);
    AppendSymbolCategories(user.rootDictionary(), locale, now, &categories);
  }

  SymbolCategory own;
  own.name = locale.compare(0, 5, "zh_CN") == 0   ? "我自定的罐头讯息"
             : locale.compare(0, 2, "zh") == 0 ? "我自定的罐頭訊息"
                                                : "User Defined";
  std::ifstream text;
  OpenVanilla::OVFileHelper::OpenIFStream(
      text, OVPathHelper::PathCat(impl_->paths.writablePath, kUserCannedMessagesTextName),
      std::ios::in);
  std::string line;
  // the first line is the instructions
  std::getline(text, line);
  while (std::getline(text, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    SymbolItem item;
    item.text = line;
    own.items.push_back(item);
  }
  if (!own.items.empty()) categories.push_back(own);
  return categories;
}

std::string Runtime::userCannedMessagesPath() const {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  const std::string path =
      OVPathHelper::PathCat(impl_->paths.writablePath, kUserCannedMessagesTextName);
  if (OVPathHelper::PathExists(path)) return path;

  const std::string locale = impl_->service->locale();
  const char* header =
      locale.compare(0, 5, "zh_CN") == 0
          ? "=== 请在本行下方添加自定义信息，每行一条，每行不超过 80 个中文或字母数字字符，并请保留本行 ==="
      : locale.compare(0, 2, "zh") == 0
          ? "=== 請從本行以下加入自定訊息，一行一則，每行不超過 80 中文或英數字，並請保留這一行 ==="
          : "=== Add your own pre-defined texts after this line ===";
  const char* example = locale.compare(0, 2, "zh") == 0 ? "你好！" : "Hello!";
  if (std::FILE* stream = OpenVanilla::OVFileHelper::OpenStream(path, "wb")) {
    // a BOM, so Notepad on older Windows does not read the file as ANSI
    std::fputs("\xEF\xBB\xBF", stream);
    std::fputs(header, stream);
    std::fputs("\n", stream);
    std::fputs(example, stream);
    std::fputs("\n", stream);
    std::fclose(stream);
  }
  return path;
}

void Runtime::reloadUserPhrases() {
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  // loadConfig flushes the query cache and reloads the learning caches
  impl_->loader->forceSyncModuleConfigForNextRound(OVIMSMARTMANDARIN_IDENTIFIER);
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

bool Engine::isComposing() const { return impl_->isComposing(); }

void Engine::acknowledgeCommit() { impl_->acknowledgeCommit(); }

std::shared_ptr<Runtime> Engine::runtime() const { return impl_->runtime; }

}  // namespace ChiaKey
