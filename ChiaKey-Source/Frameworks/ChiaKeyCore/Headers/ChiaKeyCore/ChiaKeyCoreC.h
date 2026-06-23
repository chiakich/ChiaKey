//
// ChiaKeyCoreC.h
//
// C ABI wrapper for ChiaKeyCore. This is intended as the stable boundary for
// ObjC++/Swift platform hosts that should not include C++ engine types.
//

#ifndef ChiaKeyCoreC_h
#define ChiaKeyCoreC_h

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CKC_Engine CKC_Engine;

typedef struct CKC_KeyModifiers {
  int alt;
  int opt;
  int ctrl;
  int shift;
  int command;
  int caps_lock;
  int num_lock;
  int direct_text;
} CKC_KeyModifiers;

typedef struct CKC_KeyEvent {
  int key_code;
  const char* received_string;
  CKC_KeyModifiers modifiers;
} CKC_KeyEvent;

typedef struct CKC_EnginePaths {
  const char* loaded_path;
  const char* resource_path;
  const char* writable_path;
  const char* lexicon_database_path;
} CKC_EnginePaths;

typedef struct CKC_EngineConfig {
  const char* locale;
  const char* keyboard_layout;
  const char* candidate_selection_keys;
  int candidate_cursor_at_end_of_target_block;
  int show_candidate_list_with_space;
  int clear_composing_text_with_esc;
  int shift_key_always_commit_uppercase_characters;
  size_t composing_text_buffer_size;
} CKC_EngineConfig;

typedef struct CKC_TextRange {
  size_t location;
  size_t length;
} CKC_TextRange;

typedef struct CKC_CandidateState {
  int visible;
  char** candidates;
  size_t candidate_count;
  size_t current_page;
  size_t page_count;
  size_t candidates_per_page;
  size_t highlighted_index;
  size_t highlighted_candidate_index;
} CKC_CandidateState;

typedef struct CKC_EngineSnapshot {
  char* reading_text;
  char* composing_text;
  char* committed_text;
  char** committed_text_segments;
  size_t committed_text_segment_count;
  size_t cursor_position;
  CKC_TextRange highlight;
  CKC_TextRange* word_segments;
  size_t word_segment_count;
  char* tooltip;
  CKC_CandidateState candidate_state;
  int beeped;
  char** notifications;
  size_t notification_count;
} CKC_EngineSnapshot;

CKC_KeyModifiers CKC_KeyModifiersNone(void);
CKC_EngineConfig CKC_EngineConfigDefault(void);

CKC_Engine* CKC_EngineCreate(const CKC_EnginePaths* paths,
                             const CKC_EngineConfig* config,
                             char** error_message);
void CKC_EngineDestroy(CKC_Engine* engine);

int CKC_EngineHandleKey(CKC_Engine* engine, const CKC_KeyEvent* event);
int CKC_EngineHandleAsciiKey(CKC_Engine* engine, char key,
                             CKC_KeyModifiers modifiers);
int CKC_EngineSelectCandidate(CKC_Engine* engine, size_t candidate_index);
void CKC_EngineReset(CKC_Engine* engine);

CKC_EngineSnapshot CKC_EngineCopySnapshot(CKC_Engine* engine);
void CKC_EngineAcknowledgeCommit(CKC_Engine* engine);

void CKC_EngineSnapshotDestroy(CKC_EngineSnapshot* snapshot);
void CKC_StringDestroy(char* string);

#ifdef __cplusplus
}
#endif

#endif
