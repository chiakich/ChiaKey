#pragma once

#include <Windows.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <ChiaKeyCore/ChiaKeyCore.h>

namespace ChiaKey::WindowsTsf {

struct EngineCandidate {
    std::wstring selectionKey;
    std::wstring text;
};

// UTF-16 offsets into EngineResult::compositionText.
struct CompositionSegment {
    LONG start = 0;
    LONG length = 0;
};

struct EngineResult {
    bool handled = false;
    bool beep = false;
    std::wstring committedText;
    std::wstring compositionText;
    LONG compositionCursor = 0;
    // drawn solid; the rest of the composition is dotted
    CompositionSegment focusedSegment;
    bool candidatesVisible = false;
    size_t highlightedCandidate = 0;
    std::vector<EngineCandidate> candidates;
    std::wstring message;
};

struct KeyEvent {
    UINT virtualKey = 0;
    std::wstring text;
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool capsLock = false;
    bool numLock = false;
};

// only chords in bpmf-punctuations.cin; other shortcuts belong to the host
bool IsInputMethodControlKey(const KeyEvent& event);
ChiaKey::KeyEvent MakeCoreKey(const KeyEvent& event);
EngineResult MakeResult(const ChiaKey::EngineState& state);

// for tests; must run before the first session
bool InitializeRuntime(const ChiaKey::RuntimePaths& paths, std::string* errorMessage);

std::string CurrentInputMethod();
bool SelectInputMethod(const std::string& identifier);
std::vector<std::pair<std::string, std::wstring>> InputMethods();

class EngineSession final {
public:
    static std::unique_ptr<EngineSession> Create();

    EngineSession(const EngineSession&) = delete;
    EngineSession& operator=(const EngineSession&) = delete;

    bool ready() const noexcept { return engine_ != nullptr; }
    bool hasComposition() const;
    bool wantsKey(const KeyEvent& event) const;
    EngineResult handleKey(const KeyEvent& event);
    void reset();

private:
    explicit EngineSession(std::unique_ptr<ChiaKey::Engine> engine)
        : engine_(std::move(engine)) {}
    std::unique_ptr<ChiaKey::Engine> engine_;
};

}  // namespace ChiaKey::WindowsTsf
