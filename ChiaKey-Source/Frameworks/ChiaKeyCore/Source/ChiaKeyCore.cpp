//
// ChiaKeyCore.cpp
//

#include "ChiaKeyCore/ChiaKeyCore.h"

#include <OpenVanilla/OpenVanilla.h>
#include <PlainVanilla/PVBasicKeyValueMapImpl.h>
#include <PlainVanilla/PVCandidate.h>
#include <PlainVanilla/PVKeyImpl.h>
#include <PlainVanilla/PVLoaderService.h>
#include <PlainVanilla/PVTextBuffer.h>

#include "OVIMSmartMandarin.h"

#include <sstream>
#include <utility>

namespace ChiaKey {
namespace {

using OpenVanilla::OVCandidateList;
using OpenVanilla::OVEventHandlingContext;
using OpenVanilla::OVIMSmartMandarin;
using OpenVanilla::OVKey;
using OpenVanilla::OVKeyMask;
using OpenVanilla::OVKeyValueMap;
using OpenVanilla::OVPathInfo;
using OpenVanilla::OVSQLiteDatabaseService;
using OpenVanilla::PVBasicKeyValueMapImpl;
using OpenVanilla::PVCandidateService;
using OpenVanilla::PVCandidateState;
using OpenVanilla::PVLoaderService;
using OpenVanilla::PVOneDimensionalCandidatePanel;
using OpenVanilla::PVTextBuffer;

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
    return OVKey(new OpenVanilla::PVKeyImpl(event.receivedString,
                                           static_cast<unsigned int>(
                                               event.keyCode),
                                           mask));
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
  for (std::vector<OpenVanilla::OVTextBuffer::RangePair>::const_iterator it =
           ranges.begin();
       it != ranges.end(); ++it) {
    TextRange range;
    range.location = it->first;
    range.length = it->second;
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

}  // namespace

class Engine::Impl {
 public:
  Impl(OVSQLiteDatabaseService* sqliteService, const EnginePaths& paths,
       const EngineConfig& config)
      : sqliteService_(sqliteService),
        loaderService_(config.locale, nullptr, sqliteService_),
        candidateService_(&loaderService_) {
    pathInfo_.loadedPath = paths.loadedPath;
    pathInfo_.resourcePath = paths.resourcePath;
    pathInfo_.writablePath = paths.writablePath;
  }

  ~Impl() {
    if (context_) {
      context_->stopSession(&loaderService_);
      delete context_;
      context_ = nullptr;
    }

    module_.finalize();
    delete sqliteService_;
    sqliteService_ = nullptr;
  }

  bool initialize(const EngineConfig& config, std::string* errorMessage) {
    if (!module_.initialize(&pathInfo_, &loaderService_)) {
      if (errorMessage) *errorMessage = "OVIMSmartMandarin initialization failed";
      return false;
    }

    PVBasicKeyValueMapImpl configImpl;
    OVKeyValueMap configMap(&configImpl);
    ApplyConfig(config, &configMap);
    module_.loadConfig(&configMap, &loaderService_);

    context_ = module_.createContext();
    if (!context_) {
      if (errorMessage) *errorMessage = "OVIMSmartMandarin context creation failed";
      return false;
    }

    context_->startSession(&loaderService_);
    return true;
  }

  bool handleKey(const KeyEvent& event) {
    loaderService_.resetState();
    OVKey key = MakeKey(event);

    PVOneDimensionalCandidatePanel* panel =
        candidateService_.accessVerticalCandidatePanel();
    if (panel->isInControl()) {
      const PVCandidateState::State state =
          panel->handleKeyEvent(key, &loaderService_);

      switch (state) {
        case PVCandidateState::CandidateChosen:
          context_->candidateSelected(&candidateService_,
                                      panel->chosenCandidateString(),
                                      panel->chosenCandidateIndex(),
                                      &readingText_, &composingText_,
                                      &loaderService_);
          candidateService_.resetAll();
          return true;

        case PVCandidateState::Canceled:
          context_->candidateCanceled(&candidateService_, &readingText_,
                                      &composingText_, &loaderService_);
          candidateService_.resetAll();
          return true;

        case PVCandidateState::UpdatePage:
        case PVCandidateState::UpdateCandidateHighlight:
          panel->updateDisplay();
          return true;

        case PVCandidateState::InvalidCandidateKey:
        case PVCandidateState::ReachedPageBoundary:
          loaderService_.beep();
          return true;

        case PVCandidateState::Ignored:
          if (context_->candidateNonPanelKeyReceived(
                  &candidateService_, &key, &readingText_, &composingText_,
                  &loaderService_)) {
            return true;
          }
          break;
      }
    }

    candidateService_.resetAll();
    return context_->handleKey(&key, &readingText_, &composingText_,
                               &candidateService_, &loaderService_);
  }

  bool selectCandidate(std::size_t candidateIndex) {
    loaderService_.resetState();
    PVOneDimensionalCandidatePanel* panel =
        candidateService_.accessVerticalCandidatePanel();
    OVCandidateList* list = panel->candidateList();
    if (!panel->isVisible() || !list || candidateIndex >= list->size()) {
      loaderService_.beep();
      return false;
    }

    const std::string candidate = list->candidateAtIndex(candidateIndex);
    const bool handled = context_->candidateSelected(
        &candidateService_, candidate, candidateIndex, &readingText_,
        &composingText_, &loaderService_);
    candidateService_.resetAll();
    return handled;
  }

  void reset() {
    loaderService_.resetState();
    candidateService_.resetAll();
    if (context_) context_->clear(&loaderService_);
    readingText_.clear();
    readingText_.finishCommit();
    composingText_.clear();
    composingText_.finishCommit();
  }

  EngineState snapshot() {
    EngineState state;
    state.readingText = readingText_.composedText();
    state.composingText = composingText_.composedText();
    state.committedText = readingText_.composedCommittedText() +
                          composingText_.composedCommittedText();

    const std::vector<std::string> readingSegments =
        readingText_.composedCommittedTextSegments();
    state.committedTextSegments.insert(state.committedTextSegments.end(),
                                       readingSegments.begin(),
                                       readingSegments.end());

    const std::vector<std::string> composingSegments =
        composingText_.composedCommittedTextSegments();
    state.committedTextSegments.insert(state.committedTextSegments.end(),
                                       composingSegments.begin(),
                                       composingSegments.end());

    state.cursorPosition = composingText_.cursorPosition();
    state.highlight.location = composingText_.highlightMark().first;
    state.highlight.length = composingText_.highlightMark().second;
    state.wordSegments = ConvertRanges(composingText_.wordSegments());
    state.tooltip = composingText_.toolTipText();
    state.beeped = loaderService_.shouldBeep();
    state.notifications = loaderService_.notifyMessage();

    PVOneDimensionalCandidatePanel* panel =
        candidateService_.accessVerticalCandidatePanel();
    state.candidateState.visible = panel->isVisible();
    state.candidateState.currentPage = panel->currentPage();
    state.candidateState.pageCount = panel->pageCount();
    state.candidateState.candidatesPerPage = panel->candidatesPerPage();
    state.candidateState.highlightedIndex = panel->currentHightlightIndex();
    state.candidateState.highlightedCandidateIndex =
        panel->currentHightlightIndexInCandidateList();
    state.candidateState.candidates =
        CandidateListToVector(panel->candidateList());
    return state;
  }

  void acknowledgeCommit() {
    readingText_.finishCommit();
    composingText_.finishCommit();
  }

 private:
  OVPathInfo pathInfo_;
  OVSQLiteDatabaseService* sqliteService_;
  PVLoaderService loaderService_;
  OVIMSmartMandarin module_;
  OVEventHandlingContext* context_ = nullptr;
  PVTextBuffer readingText_;
  PVTextBuffer composingText_;
  PVCandidateService candidateService_;
};

std::unique_ptr<Engine> Engine::Create(const EnginePaths& paths,
                                       const EngineConfig& config,
                                       std::string* errorMessage) {
  if (paths.lexiconDatabasePath.empty()) {
    if (errorMessage) *errorMessage = "lexiconDatabasePath is required";
    return std::unique_ptr<Engine>();
  }

  OVSQLiteDatabaseService* sqliteService =
      OVSQLiteDatabaseService::Create(paths.lexiconDatabasePath);
  if (!sqliteService) {
    if (errorMessage) {
      std::ostringstream stream;
      stream << "failed to open lexicon database: " << paths.lexiconDatabasePath;
      *errorMessage = stream.str();
    }
    return std::unique_ptr<Engine>();
  }

  std::unique_ptr<Impl> impl(new Impl(sqliteService, paths, config));
  if (!impl->initialize(config, errorMessage)) {
    return std::unique_ptr<Engine>();
  }

  return std::unique_ptr<Engine>(new Engine(std::move(impl)));
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

}  // namespace ChiaKey
