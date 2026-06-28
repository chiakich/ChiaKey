//
// ChiaKeyCoreSmoke.cpp
//

#include <ChiaKeyCore/ChiaKeyCore.h>
#include <ChiaKeyCore/ChiaKeyCoreC.h>

#include <cstdlib>
#include <iostream>
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

  std::cout << "ChiaKeyCoreSmoke: OK" << std::endl;
  return 0;
}
