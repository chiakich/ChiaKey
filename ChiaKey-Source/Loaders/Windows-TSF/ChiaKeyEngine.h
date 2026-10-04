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
    size_t candidatesPerPage = 0;
    // 1-based, as the page indicator shows it
    size_t candidatePage = 0;
    size_t candidatePageCount = 0;
    std::wstring message;
    std::wstring notification;
};

struct KeyEvent {
    UINT virtualKey = 0;
    std::wstring text;
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool capsLock = false;
    bool numLock = false;
    bool directText = false;
};

// only chords in bpmf-punctuations.cin; other shortcuts belong to the host
bool IsInputMethodControlKey(const KeyEvent& event);
ChiaKey::KeyEvent MakeCoreKey(const KeyEvent& event);
EngineResult MakeResult(const ChiaKey::EngineState& state, bool showNotifications = true);

// Preferences/Windows.plist, with the original KeyKey key names
struct FrontendSettings {
    std::string highlightColor = "Purple";  // Purple, Green, Yellow, Red
    std::string backgroundColor = "Black";  // Black, White
    std::string textColor = "White";        // White, Black
    bool backgroundPattern = false;
    bool playSoundOnTypingError = true;
    bool toggleWithControlBackslash = true;
    bool shiftTogglesEnglish = true;
    bool capsLockTogglesEnglish = false;
    std::string chineseConverterToggleKey = "s";
    std::string repeatLastCommitTextKey = "g";
    std::string soundFilename = "Default";
    bool showNotifications = true;
    bool keyboardFollowsCursor = false;
    bool showStatusBar = true;
    bool transparentStatusBar = false;
    bool statusBarInTray = false;
    bool miniStatusBar = false;
    bool wordCountEnabled = false;
    std::string uiLanguage = "zh-TW";
    std::string reverseLookupMethod;
    bool associatedPhrases = false;
    bool simplifiedOutput = false;
    // input method identifiers left out of the menus
    std::vector<std::string> suppressedInputMethods;
};

FrontendSettings CurrentFrontendSettings();
bool SetSimplifiedOutput(bool enabled);
bool SetFrontendBool(const std::string& key, bool enabled);
struct StatusWindowState { bool hasPosition = false; LONG left = 0; LONG top = 0; };
StatusWindowState ReadStatusWindowState();
void WriteStatusWindowState(const StatusWindowState& state);
// rereads the plists if they changed; applying them can rebuild contexts
void RefreshSettings(bool throttled = false);
std::string CurrentWritablePath();
ChiaKey::EngineConfig ReadEngineConfig(const std::string& preferencesPath,
                                       ChiaKey::EngineConfig config);
FrontendSettings ReadFrontendSettings(const std::string& preferencesPath);
std::wstring SettingsAppPath();

struct SymbolEntry {
    std::wstring text;
    std::wstring label;
    std::wstring tooltip;
};

struct SymbolPage {
    std::wstring name;
    bool buttons = false;
    std::vector<SymbolEntry> entries;
};

std::vector<SymbolPage> SymbolPages();
std::wstring UserCannedMessagesPath();

// Preferences/SymbolWindow.plist, so the window follows the user from app to app
struct SymbolWindowState {
    bool visible = false;
    std::wstring page;
    bool hasPosition = false;
    // the bottom stays put when a page of another height is picked
    LONG left = 0;
    LONG bottom = 0;
};

SymbolWindowState ReadSymbolWindowState();
void WriteSymbolWindowState(const SymbolWindowState& state);

// a desktop app's: the shared %APPDATA%\ChiaKey and the lexicon next to the DLL
ChiaKey::RuntimePaths DesktopRuntimePaths();

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
