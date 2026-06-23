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

int RunCppSmoke(const std::string& repoRoot, const std::string& writableDir) {
  ChiaKey::EnginePaths paths;
  paths.loadedPath = repoRoot + "/ChiaKey-Source";
  paths.resourcePath = repoRoot + "/ChiaKey-Source";
  paths.writablePath = writableDir;
  paths.lexiconDatabasePath =
      repoRoot +
      "/ChiaKey-Source/Distributions/Takao/CookedDatabase/ChiaKeySource.db";

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

  return 0;
}

int RunCSmoke(const std::string& repoRoot, const std::string& writableDir) {
  const std::string sourceDir = repoRoot + "/ChiaKey-Source";
  const std::string lexiconDatabasePath =
      sourceDir + "/Distributions/Takao/CookedDatabase/ChiaKeySource.db";

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
  CKC_EngineDestroy(engine);
  if (!committedText.empty()) {
    return Fail("C bridge committed text was not cleared by acknowledgeCommit");
  }

  return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 3) {
    return Fail("usage: ChiaKeyCoreSmoke <repo-root> <writable-dir>");
  }

  const std::string repoRoot = argv[1];
  const std::string writableDir = argv[2];

  if (int result = RunCppSmoke(repoRoot, writableDir)) return result;
  if (int result = RunCSmoke(repoRoot, writableDir)) return result;

  std::cout << "ChiaKeyCoreSmoke: OK" << std::endl;
  return 0;
}
