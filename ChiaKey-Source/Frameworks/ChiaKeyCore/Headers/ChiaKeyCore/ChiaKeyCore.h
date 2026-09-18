//
// ChiaKeyCore.h
//
// A small, host-neutral facade for embedding ChiaKey's Mandarin engine in
// platform shells (Windows TSF, Fcitx, keyboard extensions, tests).
// One Runtime per process, one Engine per text field.
//

#ifndef ChiaKeyCore_h
#define ChiaKeyCore_h

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ChiaKey {

struct KeyModifiers {
  bool alt = false;
  bool opt = false;
  bool ctrl = false;
  bool shift = false;
  bool command = false;
  bool capsLock = false;
  bool numLock = false;
  bool directText = false;
};

struct KeyEvent {
  int keyCode = 0;
  std::string receivedString;
  KeyModifiers modifiers;
};

struct RuntimePaths {
  std::string loadedPath;
  std::string resourcePath;
  // user data: learning DB, user phrases, and the core's own preference plists
  std::string writablePath;
  std::string lexiconDatabasePath;
};

using EnginePaths = RuntimePaths;

// Applied runtime-wide, not per Engine.
struct EngineConfig {
  std::string locale = "zh_TW";
  std::string keyboardLayout = "Standard";
  std::string candidateSelectionKeys;
  bool candidateCursorAtEndOfTargetBlock = false;
  bool showCandidateListWithSpace = true;
  bool clearComposingTextWithEsc = false;
  bool shiftKeyAlwaysCommitUppercaseCharacters = false;
  std::size_t composingTextBufferSize = 20;
};

struct TextRange {
  std::size_t location = 0;
  std::size_t length = 0;
};

struct CandidateState {
  bool visible = false;
  std::vector<std::string> candidates;
  // aligned with candidates; true = the preceding text promotes this pick
  std::vector<bool> contextPicks;
  std::size_t currentPage = 0;
  std::size_t pageCount = 0;
  std::size_t candidatesPerPage = 0;
  std::size_t highlightedIndex = 0;
  std::size_t highlightedCandidateIndex = 0;
};

struct EngineState {
  std::string readingText;
  std::string composingText;
  std::string committedText;
  std::vector<std::string> committedTextSegments;
  std::size_t cursorPosition = 0;
  TextRange highlight;
  std::vector<TextRange> wordSegments;
  std::string tooltip;
  CandidateState candidateState;
  bool beeped = false;
  std::vector<std::string> notifications;
};

class Engine;

class Runtime : public std::enable_shared_from_this<Runtime> {
 public:
  static std::shared_ptr<Runtime> Create(const RuntimePaths& paths,
                                         const EngineConfig& config,
                                         std::string* errorMessage = nullptr);

  ~Runtime();

  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

  std::unique_ptr<Engine> createEngine(std::string* errorMessage = nullptr);

  // Persists to the preference plist; locale is fixed at Create and ignored.
  void setConfig(const EngineConfig& config);
  EngineConfig config() const;

  // identifier / localized name pairs, in the loader's suggested order
  std::vector<std::pair<std::string, std::string>> inputMethods() const;
  std::string primaryInputMethod() const;
  // Rebuilds every Engine's context: any composition in progress is dropped.
  bool setPrimaryInputMethod(const std::string& identifier);

  bool associatedPhrasesEnabled() const;
  // Rebuilds every Engine's context: any composition in progress is dropped.
  void setAssociatedPhrasesEnabled(bool enabled);

  static const char* SmartMandarinIdentifier();
  static const char* TraditionalMandarinIdentifier();

 private:
  friend class Engine;
  class Impl;

  explicit Runtime(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

class Engine {
 public:
  // Convenience for single-context hosts: creates a private Runtime.
  static std::unique_ptr<Engine> Create(const RuntimePaths& paths,
                                        const EngineConfig& config,
                                        std::string* errorMessage = nullptr);

  ~Engine();

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  bool handleKey(const KeyEvent& event);
  bool handleAsciiKey(char key, const KeyModifiers& modifiers = KeyModifiers());
  // absolute index into CandidateState::candidates
  bool selectCandidate(std::size_t candidateIndex);
  void reset();

  EngineState snapshot() const;
  void acknowledgeCommit();

  std::shared_ptr<Runtime> runtime() const;

 private:
  friend class Runtime;
  class Impl;

  explicit Engine(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace ChiaKey

#endif
