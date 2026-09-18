//
// ChiaKeyCoreSmoke.cpp
//

#include <ChiaKeyCore/ChiaKeyCore.h>
#include <ChiaKeyCore/ChiaKeyCoreC.h>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int Fail(const std::string& message) {
  std::cerr << "ChiaKeyCoreSmoke: " << message << std::endl;
  return 1;
}

int RunCppSmoke(const std::string& repoRoot, const std::string& writableDir,
                const std::string& lexiconDatabasePath) {
  ChiaKey::EnginePaths paths;
  paths.loadedPath = repoRoot + "/ChiaKey-Source";
  paths.resourcePath = repoRoot + "/ChiaKey-Source";
  paths.writablePath = writableDir;
  paths.lexiconDatabasePath = lexiconDatabasePath;

  ChiaKey::EngineConfig config;
  std::string errorMessage;
  std::unique_ptr<ChiaKey::Engine> engine =
      ChiaKey::Engine::Create(paths, config, &errorMessage);
  if (!engine) {
    return Fail("failed to create C++ engine: " + errorMessage);
  }

  const char keys[] = {'s', 'u', '3', 'c', 'l', '3'};
  for (char key : keys) {
    if (!engine->handleAsciiKey(key)) {
      return Fail(std::string("C++ engine did not handle key: ") + key);
    }
  }

  ChiaKey::EngineState state = engine->snapshot();
  if (state.composingText != "你好") {
    return Fail("expected C++ composing text 你好, got: " + state.composingText);
  }

  ChiaKey::KeyEvent quickAddKey;
  quickAddKey.keyCode = '2';
  quickAddKey.modifiers.ctrl = true;
  if (!engine->handleKey(quickAddKey)) {
    return Fail("C++ engine did not handle ctrl+2 quick user phrase key");
  }

  state = engine->snapshot();
  if (state.composingText != "你好") {
    return Fail("expected C++ composing text to remain 你好 after ctrl+2, got: " +
                state.composingText);
  }
  if (state.candidateState.visible) {
    return Fail("ctrl+2 quick user phrase key unexpectedly opened candidates");
  }

  ChiaKey::KeyEvent returnKey;
  returnKey.keyCode = 13;
  if (!engine->handleKey(returnKey)) {
    return Fail("C++ engine did not handle return key");
  }

  state = engine->snapshot();
  if (state.committedText != "你好") {
    return Fail("expected C++ committed text 你好, got: " + state.committedText);
  }

  engine->acknowledgeCommit();
  state = engine->snapshot();
  if (!state.committedText.empty()) {
    return Fail("C++ committed text was not cleared by acknowledgeCommit");
  }

  engine->reset();
  for (char key : keys) {
    if (!engine->handleAsciiKey(key)) {
      return Fail(std::string("C++ engine did not handle pre-tab key: ") +
                  key);
    }
  }

  ChiaKey::KeyEvent leftKey;
  leftKey.keyCode = 28;
  if (!engine->handleKey(leftKey)) {
    return Fail("C++ engine did not handle left key before tab break");
  }

  ChiaKey::KeyEvent tabKey;
  tabKey.keyCode = 9;
  if (!engine->handleKey(tabKey)) {
    return Fail("C++ engine did not handle tab break key");
  }

  state = engine->snapshot();
  if (!state.committedText.empty()) {
    return Fail("expected C++ tab break not to commit text, got: " +
                state.committedText);
  }
  if (state.composingText != "你好") {
    return Fail("expected C++ tab break to keep composing 你好, got: " +
                state.composingText);
  }
  if (state.wordSegments.size() != 2 || state.wordSegments[0].length != 1 ||
      state.wordSegments[1].location != 1 ||
      state.wordSegments[1].length != 1) {
    return Fail("expected C++ tab break to force word segments at cursor");
  }

  engine->reset();
  if (!engine->handleAsciiKey('1')) {
    return Fail("C++ engine did not handle standalone ㄅ key");
  }
  if (!engine->handleAsciiKey(' ')) {
    return Fail("C++ engine did not handle space after standalone ㄅ");
  }

  state = engine->snapshot();
  if (!state.committedText.empty()) {
    return Fail("expected C++ standalone ㄅ space to stay composing, got commit: " +
                state.committedText);
  }
  if (state.composingText != "ㄅ") {
    return Fail("expected C++ standalone ㄅ space to compose ㄅ, got: " +
                state.composingText);
  }
  if (!state.readingText.empty()) {
    return Fail("expected C++ standalone ㄅ space to clear reading buffer");
  }
  if (state.beeped) {
    return Fail("C++ standalone ㄅ space unexpectedly beeped");
  }

  if (!engine->handleKey(returnKey)) {
    return Fail("C++ engine did not handle return after standalone ㄅ");
  }
  state = engine->snapshot();
  if (state.committedText != "ㄅ") {
    return Fail("expected C++ standalone ㄅ return to commit ㄅ, got: " +
                state.committedText);
  }
  if (state.committedTextSegments.size() != 1 ||
      state.committedTextSegments[0] != "ㄅ") {
    return Fail("expected C++ standalone ㄅ to commit as a text segment");
  }

  engine->acknowledgeCommit();
  engine->reset();
  if (!engine->handleAsciiKey('5')) {
    return Fail("C++ engine did not handle standalone ㄓ key");
  }
  if (!engine->handleAsciiKey(' ')) {
    return Fail("C++ engine did not handle space after standalone ㄓ");
  }

  state = engine->snapshot();
  if (!state.committedText.empty()) {
    return Fail("C++ standalone ㄓ space unexpectedly committed: " +
                state.committedText);
  }
  if (state.composingText.empty() || state.composingText == "ㄓ") {
    return Fail("expected C++ standalone ㄓ space to compose a non-raw candidate");
  }

  // 冒 + ㄏㄢˋ: the lexicon's bigram promotes 汗, which sits far down the
  // reading's own unigram order, so a flag here proves the whole path --
  // per-node previous resolution, the score gate, and list alignment.
  engine->reset();
  const char contextKeys[] = {'a', 'l', '4', 'c', '0', '4'};
  for (char key : contextKeys) {
    if (!engine->handleAsciiKey(key)) {
      return Fail(std::string("C++ engine did not handle context key: ") + key);
    }
  }

  state = engine->snapshot();
  if (state.composingText != "冒汗") {
    return Fail("expected C++ context reading to compose 冒汗, got: " +
                state.composingText);
  }

  if (!engine->handleAsciiKey(' ')) {
    return Fail("C++ engine did not handle space to open candidates");
  }

  state = engine->snapshot();
  if (!state.candidateState.visible) {
    return Fail("expected space to open the candidate list");
  }
  if (state.candidateState.contextPicks.size() !=
      state.candidateState.candidates.size()) {
    return Fail("expected contextPicks to align with the candidate list");
  }

  std::size_t flagged = 0;
  std::string flaggedText;
  for (std::size_t index = 0; index < state.candidateState.contextPicks.size();
       ++index) {
    if (state.candidateState.contextPicks[index]) {
      ++flagged;
      flaggedText = state.candidateState.candidates[index];
    }
  }
  if (flagged != 1 || flaggedText != "汗") {
    std::ostringstream stream;
    stream << "expected exactly 汗 flagged as a context pick, got " << flagged
           << " flagged (" << flaggedText << ")";
    return Fail(stream.str());
  }

  engine->reset();
  state = engine->snapshot();
  if (!state.candidateState.contextPicks.empty()) {
    return Fail("expected reset to drop the context picks");
  }

  return 0;
}

int RunCSmoke(const std::string& repoRoot, const std::string& writableDir,
              const std::string& lexiconDatabasePath) {
  const std::string sourceDir = repoRoot + "/ChiaKey-Source";

  CKC_EnginePaths paths = {};
  paths.loaded_path = sourceDir.c_str();
  paths.resource_path = sourceDir.c_str();
  paths.writable_path = writableDir.c_str();
  paths.lexicon_database_path = lexiconDatabasePath.c_str();

  char* errorMessage = nullptr;
  CKC_EngineConfig config = CKC_EngineConfigDefault();
  CKC_Engine* engine = CKC_EngineCreate(&paths, &config, &errorMessage);
  if (!engine) {
    std::string message =
        errorMessage ? errorMessage : "unknown C bridge creation error";
    CKC_StringDestroy(errorMessage);
    return Fail("failed to create C bridge engine: " + message);
  }

  const char keys[] = {'s', 'u', '3', 'c', 'l', '3'};
  CKC_KeyModifiers modifiers = CKC_KeyModifiersNone();
  for (char key : keys) {
    if (!CKC_EngineHandleAsciiKey(engine, key, modifiers)) {
      CKC_EngineDestroy(engine);
      return Fail(std::string("C bridge engine did not handle key: ") + key);
    }
  }

  CKC_EngineSnapshot snapshot = CKC_EngineCopySnapshot(engine);
  std::string composingText = snapshot.composing_text ? snapshot.composing_text : "";
  CKC_EngineSnapshotDestroy(&snapshot);
  if (composingText != "你好") {
    CKC_EngineDestroy(engine);
    return Fail("expected C bridge composing text 你好, got: " + composingText);
  }

  CKC_KeyEvent quickAddKey = {};
  quickAddKey.key_code = '2';
  quickAddKey.modifiers.ctrl = 1;
  if (!CKC_EngineHandleKey(engine, &quickAddKey)) {
    CKC_EngineDestroy(engine);
    return Fail("C bridge engine did not handle ctrl+2 quick user phrase key");
  }

  snapshot = CKC_EngineCopySnapshot(engine);
  composingText = snapshot.composing_text ? snapshot.composing_text : "";
  int candidateVisible = snapshot.candidate_state.visible;
  CKC_EngineSnapshotDestroy(&snapshot);
  if (composingText != "你好") {
    CKC_EngineDestroy(engine);
    return Fail(
        "expected C bridge composing text to remain 你好 after ctrl+2, got: " +
        composingText);
  }
  if (candidateVisible) {
    CKC_EngineDestroy(engine);
    return Fail(
        "ctrl+2 quick user phrase key unexpectedly opened C bridge candidates");
  }

  CKC_KeyEvent returnKey = {};
  returnKey.key_code = 13;
  if (!CKC_EngineHandleKey(engine, &returnKey)) {
    CKC_EngineDestroy(engine);
    return Fail("C bridge engine did not handle return key");
  }

  snapshot = CKC_EngineCopySnapshot(engine);
  std::string committedText = snapshot.committed_text ? snapshot.committed_text : "";
  CKC_EngineSnapshotDestroy(&snapshot);
  if (committedText != "你好") {
    CKC_EngineDestroy(engine);
    return Fail("expected C bridge committed text 你好, got: " + committedText);
  }

  CKC_EngineAcknowledgeCommit(engine);
  snapshot = CKC_EngineCopySnapshot(engine);
  committedText = snapshot.committed_text ? snapshot.committed_text : "";
  CKC_EngineSnapshotDestroy(&snapshot);
  if (!committedText.empty()) {
    CKC_EngineDestroy(engine);
    return Fail("C bridge committed text was not cleared by acknowledgeCommit");
  }

  CKC_EngineReset(engine);
  for (char key : keys) {
    if (!CKC_EngineHandleAsciiKey(engine, key, modifiers)) {
      CKC_EngineDestroy(engine);
      return Fail(std::string("C bridge engine did not handle pre-tab key: ") +
                  key);
    }
  }

  CKC_KeyEvent leftKey = {};
  leftKey.key_code = 28;
  if (!CKC_EngineHandleKey(engine, &leftKey)) {
    CKC_EngineDestroy(engine);
    return Fail("C bridge engine did not handle left key before tab break");
  }

  CKC_KeyEvent tabKey = {};
  tabKey.key_code = 9;
  if (!CKC_EngineHandleKey(engine, &tabKey)) {
    CKC_EngineDestroy(engine);
    return Fail("C bridge engine did not handle tab break key");
  }

  snapshot = CKC_EngineCopySnapshot(engine);
  committedText = snapshot.committed_text ? snapshot.committed_text : "";
  composingText = snapshot.composing_text ? snapshot.composing_text : "";
  const bool tabForcedWordSegments =
      snapshot.word_segment_count == 2 &&
      snapshot.word_segments[0].length == 1 &&
      snapshot.word_segments[1].location == 1 &&
      snapshot.word_segments[1].length == 1;
  CKC_EngineSnapshotDestroy(&snapshot);
  if (!committedText.empty()) {
    CKC_EngineDestroy(engine);
    return Fail("expected C bridge tab break not to commit text, got: " +
                committedText);
  }
  if (composingText != "你好") {
    CKC_EngineDestroy(engine);
    return Fail("expected C bridge tab break to keep composing 你好, got: " +
                composingText);
  }
  if (!tabForcedWordSegments) {
    CKC_EngineDestroy(engine);
    return Fail("expected C bridge tab break to force word segments at cursor");
  }

  CKC_EngineReset(engine);
  if (!CKC_EngineHandleAsciiKey(engine, '1', modifiers)) {
    CKC_EngineDestroy(engine);
    return Fail("C bridge engine did not handle standalone ㄅ key");
  }
  if (!CKC_EngineHandleAsciiKey(engine, ' ', modifiers)) {
    CKC_EngineDestroy(engine);
    return Fail("C bridge engine did not handle space after standalone ㄅ");
  }

  snapshot = CKC_EngineCopySnapshot(engine);
  committedText = snapshot.committed_text ? snapshot.committed_text : "";
  composingText = snapshot.composing_text ? snapshot.composing_text : "";
  std::string readingText = snapshot.reading_text ? snapshot.reading_text : "";
  int beeped = snapshot.beeped;
  CKC_EngineSnapshotDestroy(&snapshot);
  if (!committedText.empty()) {
    CKC_EngineDestroy(engine);
    return Fail("expected C bridge standalone ㄅ space to stay composing, got commit: " +
                committedText);
  }
  if (composingText != "ㄅ") {
    CKC_EngineDestroy(engine);
    return Fail("expected C bridge standalone ㄅ space to compose ㄅ, got: " +
                composingText);
  }
  if (!readingText.empty()) {
    CKC_EngineDestroy(engine);
    return Fail("expected C bridge standalone ㄅ space to clear reading buffer");
  }
  if (beeped) {
    CKC_EngineDestroy(engine);
    return Fail("C bridge standalone ㄅ space unexpectedly beeped");
  }

  if (!CKC_EngineHandleKey(engine, &returnKey)) {
    CKC_EngineDestroy(engine);
    return Fail("C bridge engine did not handle return after standalone ㄅ");
  }
  snapshot = CKC_EngineCopySnapshot(engine);
  committedText = snapshot.committed_text ? snapshot.committed_text : "";
  const bool bopomofoCommittedAsTextSegment =
      snapshot.committed_text_segment_count == 1 &&
      snapshot.committed_text_segments[0] &&
      std::string(snapshot.committed_text_segments[0]) == "ㄅ";
  CKC_EngineSnapshotDestroy(&snapshot);
  if (committedText != "ㄅ") {
    CKC_EngineDestroy(engine);
    return Fail("expected C bridge standalone ㄅ return to commit ㄅ, got: " +
                committedText);
  }
  if (!bopomofoCommittedAsTextSegment) {
    CKC_EngineDestroy(engine);
    return Fail("expected C bridge standalone ㄅ to commit as a text segment");
  }

  CKC_EngineAcknowledgeCommit(engine);
  CKC_EngineReset(engine);
  if (!CKC_EngineHandleAsciiKey(engine, '5', modifiers)) {
    CKC_EngineDestroy(engine);
    return Fail("C bridge engine did not handle standalone ㄓ key");
  }
  if (!CKC_EngineHandleAsciiKey(engine, ' ', modifiers)) {
    CKC_EngineDestroy(engine);
    return Fail("C bridge engine did not handle space after standalone ㄓ");
  }

  snapshot = CKC_EngineCopySnapshot(engine);
  committedText = snapshot.committed_text ? snapshot.committed_text : "";
  composingText = snapshot.composing_text ? snapshot.composing_text : "";
  readingText = snapshot.reading_text ? snapshot.reading_text : "";
  CKC_EngineSnapshotDestroy(&snapshot);

  // mirrors the C++ 冒汗 case, so the C ABI's context_picks copy is covered
  CKC_EngineReset(engine);
  const char contextKeys[] = {'a', 'l', '4', 'c', '0', '4', ' '};
  for (char key : contextKeys) {
    if (!CKC_EngineHandleAsciiKey(engine, key, modifiers)) {
      CKC_EngineDestroy(engine);
      return Fail(std::string("C bridge did not handle context key: ") + key);
    }
  }

  snapshot = CKC_EngineCopySnapshot(engine);
  std::size_t bridgeFlagged = 0;
  std::string bridgeFlaggedText;
  if (snapshot.candidate_state.context_picks) {
    for (std::size_t index = 0;
         index < snapshot.candidate_state.candidate_count; ++index) {
      if (snapshot.candidate_state.context_picks[index]) {
        ++bridgeFlagged;
        bridgeFlaggedText = snapshot.candidate_state.candidates[index];
      }
    }
  }
  CKC_EngineSnapshotDestroy(&snapshot);
  if (bridgeFlagged != 1 || bridgeFlaggedText != "汗") {
    CKC_EngineDestroy(engine);
    return Fail("expected the C bridge to flag 汗 as the only context pick");
  }

  CKC_EngineDestroy(engine);
  if (!committedText.empty()) {
    return Fail("C bridge standalone ㄓ space unexpectedly committed: " +
                committedText);
  }
  if (composingText.empty() || composingText == "ㄓ") {
    return Fail(
        "expected C bridge standalone ㄓ space to compose a non-raw candidate");
  }

  return 0;
}


bool TypeKeys(ChiaKey::Engine* engine, const char* keys) {
  for (const char* key = keys; *key; ++key) {
    if (!engine->handleAsciiKey(*key)) return false;
  }
  return true;
}

int RunRuntimeSmoke(const std::string& repoRoot, const std::string& writableDir,
                    const std::string& lexiconDatabasePath) {
  ChiaKey::RuntimePaths paths;
  paths.loadedPath = repoRoot + "/ChiaKey-Source";
  paths.resourcePath = repoRoot + "/ChiaKey-Source";
  paths.writablePath = writableDir;
  paths.lexiconDatabasePath = lexiconDatabasePath;

  std::string errorMessage;
  std::shared_ptr<ChiaKey::Runtime> runtime =
      ChiaKey::Runtime::Create(paths, ChiaKey::EngineConfig(), &errorMessage);
  if (!runtime) return Fail("failed to create runtime: " + errorMessage);

  if (runtime->primaryInputMethod() != ChiaKey::Runtime::SmartMandarinIdentifier()) {
    return Fail("expected Smart Mandarin as the primary input method, got: " +
                runtime->primaryInputMethod());
  }

  bool sawSmart = false;
  bool sawTraditional = false;
  for (const auto& entry : runtime->inputMethods()) {
    if (entry.first == ChiaKey::Runtime::SmartMandarinIdentifier()) sawSmart = true;
    if (entry.first == ChiaKey::Runtime::TraditionalMandarinIdentifier()) {
      sawTraditional = true;
    }
    if (entry.second.empty()) return Fail("input method without a name: " + entry.first);
  }
  if (!sawSmart || !sawTraditional) {
    return Fail("runtime did not list both Mandarin input methods");
  }

  // two contexts on one runtime stay independent
  std::unique_ptr<ChiaKey::Engine> first = runtime->createEngine(&errorMessage);
  std::unique_ptr<ChiaKey::Engine> second = runtime->createEngine(&errorMessage);
  if (!first || !second) return Fail("failed to create engines: " + errorMessage);

  if (!TypeKeys(first.get(), "su3cl3")) return Fail("first engine rejected 你好");
  if (!TypeKeys(second.get(), "1")) return Fail("second engine rejected ㄅ");

  if (first->snapshot().composingText != "你好") {
    return Fail("first engine lost its composition to the second engine");
  }
  if (second->snapshot().readingText.empty() ||
      !second->snapshot().composingText.empty()) {
    return Fail("second engine did not keep its own reading state");
  }

  first->reset();
  if (!TypeKeys(first.get(), "al4c04 ")) return Fail("first engine rejected 冒汗 + space");
  ChiaKey::EngineState state = first->snapshot();
  if (!state.candidateState.visible || state.candidateState.candidates.size() < 2) {
    return Fail("expected at least two candidates for 汗");
  }
  // the list leads with whole-phrase candidates such as 冒汗, so pick a
  // single character that actually changes the composition
  std::size_t pick = state.candidateState.candidates.size();
  for (std::size_t index = 0; index < state.candidateState.candidates.size(); ++index) {
    const std::string& candidate = state.candidateState.candidates[index];
    if (candidate.size() == 3 && candidate != "汗") {
      pick = index;
      break;
    }
  }
  if (pick == state.candidateState.candidates.size()) {
    return Fail("no single-character alternative to 汗 in the candidate list");
  }
  // the walker may re-pick the preceding character around the fixed node
  // (冒汗 -> 茂和), so only the selected position is asserted
  const std::string chosen = state.candidateState.candidates[pick];
  if (!first->selectCandidate(pick)) return Fail("selectCandidate refused a valid index");
  state = first->snapshot();
  if (state.candidateState.visible) return Fail("candidate list stayed open after selection");
  if (state.composingText.size() != 6 ||
      state.composingText.compare(3, 3, chosen) != 0) {
    return Fail("expected selectCandidate to end the composition with " + chosen +
                ", got: " + state.composingText);
  }
  if (first->selectCandidate(0)) return Fail("selectCandidate accepted an index with no list open");
  if (!first->snapshot().beeped) return Fail("invalid selectCandidate did not beep");

  // config round-trips through the preference plist and reloads live
  ChiaKey::EngineConfig config = runtime->config();
  config.keyboardLayout = "ETen";
  runtime->setConfig(config);
  if (runtime->config().keyboardLayout != "ETen") return Fail("setConfig did not store the layout");
  first->reset();
  if (!TypeKeys(first.get(), "su3cl3")) return Fail("engine rejected keys under ETen layout");
  if (first->snapshot().composingText == "你好") {
    return Fail("ETen layout was not applied to the live module");
  }
  config.keyboardLayout = "Standard";
  runtime->setConfig(config);
  first->reset();
  if (!TypeKeys(first.get(), "su3cl3") || first->snapshot().composingText != "你好") {
    return Fail("Standard layout was not restored on the live module");
  }

  // the toggle bumps the loader generation, which drops every composition
  runtime->setAssociatedPhrasesEnabled(true);
  if (!runtime->associatedPhrasesEnabled()) return Fail("associated phrases did not enable");
  if (!TypeKeys(first.get(), "su3cl3")) return Fail("engine rejected keys with associated phrases on");
  if (first->snapshot().composingText != "你好") {
    return Fail("expected a fresh 你好 composition after the filter toggle");
  }
  ChiaKey::KeyEvent returnKey;
  returnKey.keyCode = 13;
  if (!first->handleKey(returnKey)) return Fail("return was not handled with associated phrases on");
  if (first->snapshot().committedText != "你好") {
    return Fail("expected 你好 committed with associated phrases on, got: " +
                first->snapshot().committedText);
  }
  first->acknowledgeCommit();
  runtime->setAssociatedPhrasesEnabled(false);
  if (runtime->associatedPhrasesEnabled()) return Fail("associated phrases did not disable");

  // engines keep the runtime alive after the host drops its own reference
  runtime.reset();
  second->reset();
  if (!TypeKeys(second.get(), "su3cl3") || second->snapshot().composingText != "你好") {
    return Fail("engine stopped working after the host released the runtime");
  }
  if (second->runtime()->primaryInputMethod() !=
      ChiaKey::Runtime::SmartMandarinIdentifier()) {
    return Fail("engine->runtime() lost the primary input method");
  }

  return 0;
}

int RunCRuntimeSmoke(const std::string& repoRoot, const std::string& writableDir,
                     const std::string& lexiconDatabasePath) {
  const std::string sourceDir = repoRoot + "/ChiaKey-Source";

  CKC_EnginePaths paths = {};
  paths.loaded_path = sourceDir.c_str();
  paths.resource_path = sourceDir.c_str();
  paths.writable_path = writableDir.c_str();
  paths.lexicon_database_path = lexiconDatabasePath.c_str();

  char* errorMessage = nullptr;
  CKC_EngineConfig config = CKC_EngineConfigDefault();
  CKC_Runtime* runtime = CKC_RuntimeCreate(&paths, &config, &errorMessage);
  if (!runtime) {
    std::string message = errorMessage ? errorMessage : "unknown C runtime error";
    CKC_StringDestroy(errorMessage);
    return Fail("failed to create C bridge runtime: " + message);
  }

  char** identifiers = nullptr;
  char** names = nullptr;
  const size_t count = CKC_RuntimeCopyInputMethods(runtime, &identifiers, &names);
  bool sawSmart = false;
  for (size_t index = 0; index < count; ++index) {
    if (std::string(identifiers[index]) == CKC_SmartMandarinIdentifier()) sawSmart = true;
  }
  CKC_StringArrayDestroy(identifiers, count);
  CKC_StringArrayDestroy(names, count);
  if (count < 2 || !sawSmart) {
    CKC_RuntimeDestroy(runtime);
    return Fail("C bridge runtime did not list the Mandarin input methods");
  }

  char* primary = CKC_RuntimeCopyPrimaryInputMethod(runtime);
  const bool primaryIsSmart =
      primary && std::string(primary) == CKC_SmartMandarinIdentifier();
  CKC_StringDestroy(primary);
  if (!primaryIsSmart) {
    CKC_RuntimeDestroy(runtime);
    return Fail("C bridge runtime primary input method is not Smart Mandarin");
  }

  CKC_Engine* engine = CKC_RuntimeCreateEngine(runtime, &errorMessage);
  if (!engine) {
    std::string message = errorMessage ? errorMessage : "unknown C engine error";
    CKC_StringDestroy(errorMessage);
    CKC_RuntimeDestroy(runtime);
    return Fail("failed to create C bridge engine from runtime: " + message);
  }

  // the runtime handle may go first; the engine keeps the runtime alive
  CKC_RuntimeDestroy(runtime);

  const char keys[] = {'s', 'u', '3', 'c', 'l', '3'};
  CKC_KeyModifiers modifiers = CKC_KeyModifiersNone();
  for (char key : keys) {
    if (!CKC_EngineHandleAsciiKey(engine, key, modifiers)) {
      CKC_EngineDestroy(engine);
      return Fail(std::string("C bridge runtime engine did not handle key: ") + key);
    }
  }

  CKC_EngineSnapshot snapshot = CKC_EngineCopySnapshot(engine);
  const std::string composingText = snapshot.composing_text ? snapshot.composing_text : "";
  CKC_EngineSnapshotDestroy(&snapshot);
  CKC_EngineDestroy(engine);
  if (composingText != "你好") {
    return Fail("expected C bridge runtime engine to compose 你好, got: " + composingText);
  }

  return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 4) {
    return Fail(
        "usage: ChiaKeyCoreSmoke <repo-root> <writable-dir> "
        "<lexicon-database-path>");
  }

  const std::string repoRoot = argv[1];
  const std::string writableDir = argv[2];
  const std::string lexiconDatabasePath = argv[3];

  if (int result = RunCppSmoke(repoRoot, writableDir, lexiconDatabasePath))
    return result;
  if (int result = RunCSmoke(repoRoot, writableDir, lexiconDatabasePath))
    return result;
  if (int result = RunRuntimeSmoke(repoRoot, writableDir, lexiconDatabasePath))
    return result;
  if (int result = RunCRuntimeSmoke(repoRoot, writableDir, lexiconDatabasePath))
    return result;

  std::cout << "ChiaKeyCoreSmoke: OK" << std::endl;
  return 0;
}
