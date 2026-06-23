//
// ChiaKeyCore.h
//
// A small, host-neutral facade for embedding ChiaKey's Mandarin engine in
// future shells such as an iOS keyboard extension.
//

#ifndef ChiaKeyCore_h
#define ChiaKeyCore_h

#include <cstddef>
#include <memory>
#include <string>
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

struct EnginePaths {
  std::string loadedPath;
  std::string resourcePath;
  std::string writablePath;
  std::string lexiconDatabasePath;
};

struct EngineConfig {
  std::string locale = "zh_TW";
  std::string keyboardLayout = "Standard";
  std::string candidateSelectionKeys;
  bool candidateCursorAtEndOfTargetBlock = false;
  bool showCandidateListWithSpace = true;
  bool clearComposingTextWithEsc = false;
  bool shiftKeyAlwaysCommitUppercaseCharacters = false;
  std::size_t composingTextBufferSize = 10;
};

struct TextRange {
  std::size_t location = 0;
  std::size_t length = 0;
};

struct CandidateState {
  bool visible = false;
  std::vector<std::string> candidates;
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

class Engine {
 public:
  static std::unique_ptr<Engine> Create(const EnginePaths& paths,
                                        const EngineConfig& config,
                                        std::string* errorMessage = nullptr);

  ~Engine();

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  bool handleKey(const KeyEvent& event);
  bool handleAsciiKey(char key, const KeyModifiers& modifiers = KeyModifiers());
  bool selectCandidate(std::size_t candidateIndex);
  void reset();

  EngineState snapshot() const;
  void acknowledgeCommit();

 private:
  class Impl;

  explicit Engine(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace ChiaKey

#endif
