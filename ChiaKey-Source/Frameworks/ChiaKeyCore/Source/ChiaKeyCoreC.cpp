//
// ChiaKeyCoreC.cpp
//

#include "ChiaKeyCore/ChiaKeyCoreC.h"

#include "ChiaKeyCore/ChiaKeyCore.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

struct CKC_Engine {
  std::unique_ptr<ChiaKey::Engine> engine;
};

namespace {

std::string StringFromCString(const char* string) {
  return string ? std::string(string) : std::string();
}

char* CopyCString(const std::string& string) {
  char* result = static_cast<char*>(std::malloc(string.size() + 1));
  if (!result) return nullptr;

  std::memcpy(result, string.c_str(), string.size() + 1);
  return result;
}

char** CopyStringVector(const std::vector<std::string>& strings) {
  if (strings.empty()) return nullptr;

  char** result = static_cast<char**>(
      std::calloc(strings.size(), sizeof(char*)));
  if (!result) return nullptr;

  for (std::size_t index = 0; index < strings.size(); ++index) {
    result[index] = CopyCString(strings[index]);
    if (!result[index]) {
      for (std::size_t cleanup = 0; cleanup < index; ++cleanup) {
        std::free(result[cleanup]);
      }
      std::free(result);
      return nullptr;
    }
  }

  return result;
}

CKC_TextRange CopyRange(const ChiaKey::TextRange& range) {
  CKC_TextRange result;
  result.location = range.location;
  result.length = range.length;
  return result;
}

CKC_TextRange* CopyRanges(const std::vector<ChiaKey::TextRange>& ranges) {
  if (ranges.empty()) return nullptr;

  CKC_TextRange* result = static_cast<CKC_TextRange*>(
      std::calloc(ranges.size(), sizeof(CKC_TextRange)));
  if (!result) return nullptr;

  for (std::size_t index = 0; index < ranges.size(); ++index) {
    result[index] = CopyRange(ranges[index]);
  }

  return result;
}

ChiaKey::KeyModifiers CopyModifiers(const CKC_KeyModifiers& modifiers) {
  ChiaKey::KeyModifiers result;
  result.alt = modifiers.alt != 0;
  result.opt = modifiers.opt != 0;
  result.ctrl = modifiers.ctrl != 0;
  result.shift = modifiers.shift != 0;
  result.command = modifiers.command != 0;
  result.capsLock = modifiers.caps_lock != 0;
  result.numLock = modifiers.num_lock != 0;
  result.directText = modifiers.direct_text != 0;
  return result;
}

ChiaKey::EnginePaths CopyPaths(const CKC_EnginePaths& paths) {
  ChiaKey::EnginePaths result;
  result.loadedPath = StringFromCString(paths.loaded_path);
  result.resourcePath = StringFromCString(paths.resource_path);
  result.writablePath = StringFromCString(paths.writable_path);
  result.lexiconDatabasePath = StringFromCString(paths.lexicon_database_path);
  return result;
}

ChiaKey::EngineConfig CopyConfig(const CKC_EngineConfig* config) {
  ChiaKey::EngineConfig result;
  if (!config) return result;

  if (config->locale) result.locale = config->locale;
  if (config->keyboard_layout) result.keyboardLayout = config->keyboard_layout;
  if (config->candidate_selection_keys) {
    result.candidateSelectionKeys = config->candidate_selection_keys;
  }
  result.candidateCursorAtEndOfTargetBlock =
      config->candidate_cursor_at_end_of_target_block != 0;
  result.showCandidateListWithSpace =
      config->show_candidate_list_with_space != 0;
  result.clearComposingTextWithEsc =
      config->clear_composing_text_with_esc != 0;
  result.shiftKeyAlwaysCommitUppercaseCharacters =
      config->shift_key_always_commit_uppercase_characters != 0;
  if (config->composing_text_buffer_size > 0) {
    result.composingTextBufferSize = config->composing_text_buffer_size;
  }
  return result;
}

ChiaKey::KeyEvent CopyKeyEvent(const CKC_KeyEvent& event) {
  ChiaKey::KeyEvent result;
  result.keyCode = event.key_code;
  result.receivedString = StringFromCString(event.received_string);
  result.modifiers = CopyModifiers(event.modifiers);
  return result;
}

CKC_CandidateState CopyCandidateState(
    const ChiaKey::CandidateState& candidateState) {
  CKC_CandidateState result;
  result.visible = candidateState.visible ? 1 : 0;
  result.candidates = CopyStringVector(candidateState.candidates);
  result.candidate_count = candidateState.candidates.size();
  result.current_page = candidateState.currentPage;
  result.page_count = candidateState.pageCount;
  result.candidates_per_page = candidateState.candidatesPerPage;
  result.highlighted_index = candidateState.highlightedIndex;
  result.highlighted_candidate_index = candidateState.highlightedCandidateIndex;
  return result;
}

void DestroyStringVector(char** strings, std::size_t count) {
  if (!strings) return;

  for (std::size_t index = 0; index < count; ++index) {
    std::free(strings[index]);
  }
  std::free(strings);
}

}  // namespace

CKC_KeyModifiers CKC_KeyModifiersNone(void) {
  CKC_KeyModifiers modifiers = {};
  return modifiers;
}

CKC_EngineConfig CKC_EngineConfigDefault(void) {
  CKC_EngineConfig config = {};
  config.locale = "zh_TW";
  config.keyboard_layout = "Standard";
  config.candidate_selection_keys = "";
  config.candidate_cursor_at_end_of_target_block = 0;
  config.show_candidate_list_with_space = 1;
  config.clear_composing_text_with_esc = 0;
  config.shift_key_always_commit_uppercase_characters = 0;
  config.composing_text_buffer_size = 10;
  return config;
}

CKC_Engine* CKC_EngineCreate(const CKC_EnginePaths* paths,
                             const CKC_EngineConfig* config,
                             char** error_message) {
  if (error_message) *error_message = nullptr;

  if (!paths) {
    if (error_message) *error_message = CopyCString("paths is required");
    return nullptr;
  }

  std::string error;
  std::unique_ptr<ChiaKey::Engine> engine =
      ChiaKey::Engine::Create(CopyPaths(*paths), CopyConfig(config), &error);
  if (!engine) {
    if (error_message) *error_message = CopyCString(error);
    return nullptr;
  }

  CKC_Engine* handle = new (std::nothrow) CKC_Engine;
  if (!handle) {
    if (error_message) *error_message = CopyCString("failed to allocate engine");
    return nullptr;
  }

  handle->engine = std::move(engine);
  return handle;
}

void CKC_EngineDestroy(CKC_Engine* engine) {
  delete engine;
}

int CKC_EngineHandleKey(CKC_Engine* engine, const CKC_KeyEvent* event) {
  if (!engine || !engine->engine || !event) return 0;
  return engine->engine->handleKey(CopyKeyEvent(*event)) ? 1 : 0;
}

int CKC_EngineHandleAsciiKey(CKC_Engine* engine, char key,
                             CKC_KeyModifiers modifiers) {
  if (!engine || !engine->engine) return 0;
  return engine->engine->handleAsciiKey(key, CopyModifiers(modifiers)) ? 1 : 0;
}

int CKC_EngineSelectCandidate(CKC_Engine* engine, size_t candidate_index) {
  if (!engine || !engine->engine) return 0;
  return engine->engine->selectCandidate(candidate_index) ? 1 : 0;
}

void CKC_EngineReset(CKC_Engine* engine) {
  if (!engine || !engine->engine) return;
  engine->engine->reset();
}

CKC_EngineSnapshot CKC_EngineCopySnapshot(CKC_Engine* engine) {
  CKC_EngineSnapshot snapshot = {};
  if (!engine || !engine->engine) return snapshot;

  const ChiaKey::EngineState state = engine->engine->snapshot();
  snapshot.reading_text = CopyCString(state.readingText);
  snapshot.composing_text = CopyCString(state.composingText);
  snapshot.committed_text = CopyCString(state.committedText);
  snapshot.committed_text_segments =
      CopyStringVector(state.committedTextSegments);
  snapshot.committed_text_segment_count = state.committedTextSegments.size();
  snapshot.cursor_position = state.cursorPosition;
  snapshot.highlight = CopyRange(state.highlight);
  snapshot.word_segments = CopyRanges(state.wordSegments);
  snapshot.word_segment_count = state.wordSegments.size();
  snapshot.tooltip = CopyCString(state.tooltip);
  snapshot.candidate_state = CopyCandidateState(state.candidateState);
  snapshot.beeped = state.beeped ? 1 : 0;
  snapshot.notifications = CopyStringVector(state.notifications);
  snapshot.notification_count = state.notifications.size();
  return snapshot;
}

void CKC_EngineAcknowledgeCommit(CKC_Engine* engine) {
  if (!engine || !engine->engine) return;
  engine->engine->acknowledgeCommit();
}

void CKC_EngineSnapshotDestroy(CKC_EngineSnapshot* snapshot) {
  if (!snapshot) return;

  std::free(snapshot->reading_text);
  std::free(snapshot->composing_text);
  std::free(snapshot->committed_text);
  DestroyStringVector(snapshot->committed_text_segments,
                      snapshot->committed_text_segment_count);
  std::free(snapshot->word_segments);
  std::free(snapshot->tooltip);
  DestroyStringVector(snapshot->candidate_state.candidates,
                      snapshot->candidate_state.candidate_count);
  DestroyStringVector(snapshot->notifications, snapshot->notification_count);

  *snapshot = CKC_EngineSnapshot();
}

void CKC_StringDestroy(char* string) {
  std::free(string);
}
