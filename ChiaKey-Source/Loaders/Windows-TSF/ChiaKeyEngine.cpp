#include "ChiaKeyEngine.h"

#include <AclAPI.h>
#include <ShlObj.h>
#include <sddl.h>

#include <algorithm>
#include <mutex>

#include "Diagnostics.h"
#include "OpenVanilla.h"

namespace ChiaKey::WindowsTsf {
namespace {

using OpenVanilla::OVKeyCode;

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), length);
    return result;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0,
                                           nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), length, nullptr, nullptr);
    return result;
}

// The core counts code points; TSF ranges count UTF-16 units.
std::vector<std::wstring> SplitCodePoints(const std::string& utf8) {
    const std::wstring wide = Utf8ToWide(utf8);
    std::vector<std::wstring> result;
    for (size_t index = 0; index < wide.size();) {
        const size_t units = IS_HIGH_SURROGATE(wide[index]) && index + 1 < wide.size() &&
                                     IS_LOW_SURROGATE(wide[index + 1])
                                 ? 2
                                 : 1;
        result.emplace_back(wide, index, units);
        index += units;
    }
    return result;
}

std::wstring ModuleDirectory() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&ModuleDirectory), &module)) {
        return {};
    }
    std::wstring path(32768, L'\0');
    const DWORD length =
        GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return {};
    path.resize(length);
    const size_t separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? std::wstring() : path.substr(0, separator);
}

std::wstring KnownFolder(REFKNOWNFOLDERID folder) {
    PWSTR path = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(folder, KF_FLAG_DEFAULT, nullptr, &path))) {
        result = path;
    }
    CoTaskMemFree(path);
    return result;
}

bool IsAppContainer() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    DWORD isAppContainer = 0;
    DWORD returned = 0;
    const BOOL queried = GetTokenInformation(token, TokenIsAppContainer, &isAppContainer,
                                             sizeof(isAppContainer), &returned);
    CloseHandle(token);
    return queried && isAppContainer;
}

// Store apps and Start/Search run in an AppContainer
void GrantAppContainerAccess(const std::wstring& directory) {
    PSID packages = nullptr;
    if (!ConvertStringSidToSidW(L"S-1-15-2-1", &packages)) return;
    PACL current = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (GetNamedSecurityInfoW(directory.c_str(), SE_FILE_OBJECT,
                              DACL_SECURITY_INFORMATION, nullptr, nullptr, &current,
                              nullptr, &descriptor) == ERROR_SUCCESS) {
        EXPLICIT_ACCESSW access{};
        access.grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_WRITE |
                                      FILE_GENERIC_EXECUTE | DELETE;
        access.grfAccessMode = GRANT_ACCESS;
        access.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        access.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        access.Trustee.ptstrName = static_cast<LPWSTR>(packages);
        PACL updated = nullptr;
        if (SetEntriesInAclW(1, &access, current, &updated) == ERROR_SUCCESS) {
            SetNamedSecurityInfoW(const_cast<LPWSTR>(directory.c_str()), SE_FILE_OBJECT,
                                  DACL_SECURITY_INFORMATION, nullptr, nullptr, updated,
                                  nullptr);
            LocalFree(updated);
        }
        LocalFree(descriptor);
    }
    LocalFree(packages);
}

ChiaKey::RuntimePaths DefaultPaths(const std::wstring& writableRoot, bool shareWithAppContainers) {
    const std::wstring moduleDirectory = ModuleDirectory();
    ChiaKey::RuntimePaths paths;
    paths.loadedPath = WideToUtf8(moduleDirectory);
    paths.resourcePath = paths.loadedPath;
    paths.lexiconDatabasePath = WideToUtf8(moduleDirectory + L"\\ChiaKeySource.db");
    if (!writableRoot.empty()) {
        const std::wstring writable = writableRoot + L"\\ChiaKey";
        if (CreateDirectoryW(writable.c_str(), nullptr) && shareWithAppContainers) {
            GrantAppContainerAccess(writable);
        }
        paths.writablePath = WideToUtf8(writable);
    }
    return paths;
}

struct RuntimeHolder {
    std::mutex mutex;
    std::shared_ptr<ChiaKey::Runtime> runtime;
};

// leaked on purpose: no teardown under the loader lock; each Engine saves its own learning
RuntimeHolder& Holder() {
    static RuntimeHolder* holder = new RuntimeHolder();
    return *holder;
}

std::shared_ptr<ChiaKey::Runtime> CreateDefaultRuntime() {
    std::string error;
    const std::wstring roaming = KnownFolder(FOLDERID_RoamingAppData);
    if (!IsAppContainer()) {
        ChiaKey::RuntimePaths paths = DefaultPaths(roaming, true);
        if (auto runtime = ChiaKey::Runtime::Create(paths, ChiaKey::EngineConfig(), &error)) {
            return runtime;
        }
        Trace("Runtime::Create failed: %s", error.c_str());
        return nullptr;
    }

    // until a desktop app has created the shared directory, a private one keeps typing working
    if (!roaming.empty()) {
        ChiaKey::RuntimePaths paths = DefaultPaths(roaming, false);
        if (auto runtime = ChiaKey::Runtime::Create(paths, ChiaKey::EngineConfig(), &error)) {
            return runtime;
        }
        Trace("Runtime::Create (AppContainer, shared) failed: %s", error.c_str());
    }
    wchar_t temp[MAX_PATH + 1]{};
    const DWORD length = GetTempPathW(static_cast<DWORD>(std::size(temp)), temp);
    if (!length || length > MAX_PATH) return nullptr;
    std::wstring privateRoot(temp, length);
    if (!privateRoot.empty() && privateRoot.back() == L'\\') privateRoot.pop_back();
    ChiaKey::RuntimePaths paths = DefaultPaths(privateRoot, false);
    if (auto runtime = ChiaKey::Runtime::Create(paths, ChiaKey::EngineConfig(), &error)) {
        return runtime;
    }
    Trace("Runtime::Create (AppContainer, private) failed: %s", error.c_str());
    return nullptr;
}

std::shared_ptr<ChiaKey::Runtime> SharedRuntime() {
    RuntimeHolder& holder = Holder();
    std::lock_guard<std::mutex> lock(holder.mutex);
    // a failed attempt is retried on the next activation, e.g. after install
    if (!holder.runtime) holder.runtime = CreateDefaultRuntime();
    return holder.runtime;
}

int SpecialKeyCode(UINT virtualKey) {
    switch (virtualKey) {
        case VK_BACK: return OVKeyCode::Backspace;
        case VK_DELETE: return OVKeyCode::Delete;
        case VK_UP: return OVKeyCode::Up;
        case VK_DOWN: return OVKeyCode::Down;
        case VK_LEFT: return OVKeyCode::Left;
        case VK_RIGHT: return OVKeyCode::Right;
        case VK_HOME: return OVKeyCode::Home;
        case VK_END: return OVKeyCode::End;
        case VK_PRIOR: return OVKeyCode::PageUp;
        case VK_NEXT: return OVKeyCode::PageDown;
        case VK_TAB: return OVKeyCode::Tab;
        case VK_ESCAPE: return OVKeyCode::Esc;
        case VK_SPACE: return OVKeyCode::Space;
        case VK_RETURN: return OVKeyCode::Return;
        default: return 0;
    }
}

// Bopomofo is positional; ToUnicodeEx reports localized characters under a Chinese TIP
char PrintableAsciiFromVirtualKey(const KeyEvent& event) {
    if (event.virtualKey >= 'A' && event.virtualKey <= 'Z') {
        const bool upper = event.shift != event.capsLock;
        return static_cast<char>((upper ? 'A' : 'a') + event.virtualKey - 'A');
    }
    if (event.virtualKey >= '0' && event.virtualKey <= '9') {
        static constexpr char shiftedDigits[] = ")!@#$%^&*(";
        return event.shift ? shiftedDigits[event.virtualKey - '0']
                           : static_cast<char>(event.virtualKey);
    }
    if (event.virtualKey >= VK_NUMPAD0 && event.virtualKey <= VK_NUMPAD9 &&
        event.numLock) {
        return static_cast<char>('0' + event.virtualKey - VK_NUMPAD0);
    }
    switch (event.virtualKey) {
        case VK_SPACE: return ' ';
        case VK_OEM_1: return event.shift ? ':' : ';';
        case VK_OEM_PLUS: return event.shift ? '+' : '=';
        case VK_OEM_COMMA: return event.shift ? '<' : ',';
        case VK_OEM_MINUS: return event.shift ? '_' : '-';
        case VK_OEM_PERIOD: return event.shift ? '>' : '.';
        case VK_OEM_2: return event.shift ? '?' : '/';
        case VK_OEM_3: return event.shift ? '~' : '`';
        case VK_OEM_4: return event.shift ? '{' : '[';
        case VK_OEM_5: return event.shift ? '|' : '\\';
        case VK_OEM_6: return event.shift ? '}' : ']';
        case VK_OEM_7: return event.shift ? '"' : '\'';
        default: return 0;
    }
}

bool IsNavigationOrEditingKey(UINT key) {
    return SpecialKeyCode(key) != 0;
}

bool IsQuickUserPhraseKey(const KeyEvent& event) {
    return event.control && !event.alt && !event.shift && event.virtualKey >= '1' &&
           event.virtualKey <= '9';
}

}  // namespace

bool IsInputMethodControlKey(const KeyEvent& event) {
    if (!event.control) return false;

    const char character = PrintableAsciiFromVirtualKey(event);
    if (event.alt) {
        // _ctrl_opt_a through _ctrl_opt_z, plus these five
        if (event.virtualKey >= 'A' && event.virtualKey <= 'Z') return true;
        switch (character) {
            case ';': case '\'': case ',': case '.': case '/':
                return true;
            default:
                return false;
        }
    }

    switch (character) {
        case ',': case '.': case '<': case '>': case '?':
        case ';': case ':': case '\'': case '"': case '[': case ']':
        // Ctrl+0 and Ctrl+1 open _punctuation_list
        case '0': case '1':
            return true;
        default:
            return false;
    }
}

ChiaKey::KeyEvent MakeCoreKey(const KeyEvent& event) {
    ChiaKey::KeyEvent key;
    key.modifiers.alt = event.alt;
    key.modifiers.ctrl = event.control;
    key.modifiers.shift = event.shift;
    key.modifiers.capsLock = event.capsLock;
    // the module rejects main-keyboard Bopomofo keys that carry NumLock
    key.modifiers.numLock = event.numLock && event.virtualKey >= VK_NUMPAD0 &&
                            event.virtualKey <= VK_DIVIDE;
    key.keyCode = SpecialKeyCode(event.virtualKey);

    if (event.control && event.virtualKey >= 'A' && event.virtualKey <= 'Z') {
        key.keyCode = 'a' + static_cast<int>(event.virtualKey - 'A');
        key.receivedString.assign(1, static_cast<char>(key.keyCode));
    } else if (const char character = PrintableAsciiFromVirtualKey(event)) {
        key.keyCode = static_cast<unsigned char>(character);
        key.receivedString.assign(1, character);
    } else if (event.text.size() == 1 && event.text.front() >= 32 &&
               event.text.front() < 127) {
        key.keyCode = static_cast<int>(event.text.front());
        key.receivedString.assign(1, static_cast<char>(key.keyCode));
    } else if (!event.text.empty()) {
        key.receivedString = WideToUtf8(event.text);
    }
    return key;
}

EngineResult MakeResult(const ChiaKey::EngineState& state) {
    EngineResult result;
    result.beep = state.beeped;
    result.committedText = Utf8ToWide(state.committedText);

    const std::vector<std::wstring> composing = SplitCodePoints(state.composingText);
    const std::vector<std::wstring> reading = SplitCodePoints(state.readingText);
    const size_t cursor = std::min(state.cursorPosition, composing.size());

    // The reading is shown at the cursor, as PVCombinedUTF16TextBuffer does.
    std::vector<std::wstring> display(composing.begin(), composing.begin() + cursor);
    display.insert(display.end(), reading.begin(), reading.end());
    display.insert(display.end(), composing.begin() + cursor, composing.end());
    std::vector<LONG> offsets{0};
    for (const std::wstring& codePoint : display) {
        result.compositionText += codePoint;
        offsets.push_back(static_cast<LONG>(result.compositionText.size()));
    }
    result.compositionCursor = offsets[cursor + reading.size()];

    // composing positions at or after the cursor sit behind the reading
    const auto displayIndex = [&](size_t position, bool isEnd) {
        const bool shifted = isEnd ? position > cursor : position >= cursor;
        return std::min(position + (shifted ? reading.size() : 0), display.size());
    };
    size_t from = 0;
    size_t to = 0;
    if (!reading.empty()) {
        from = cursor;
        to = cursor + reading.size();
    } else if (state.highlight.length) {
        from = displayIndex(state.highlight.location, false);
        to = displayIndex(state.highlight.location + state.highlight.length, true);
    } else if (!composing.empty()) {
        const size_t target = cursor ? cursor - 1 : 0;
        for (const ChiaKey::TextRange& segment : state.wordSegments) {
            if (target >= segment.location && target < segment.location + segment.length) {
                from = displayIndex(segment.location, false);
                to = displayIndex(segment.location + segment.length, true);
                break;
            }
        }
    }
    if (to > from) {
        result.focusedSegment.start = offsets[from];
        result.focusedSegment.length = offsets[to] - offsets[from];
    }

    const ChiaKey::CandidateState& panel = state.candidateState;
    if (panel.visible && !panel.selectionKeys.empty()) {
        const size_t first = panel.currentPage * panel.candidatesPerPage;
        for (size_t index = 0; index < panel.selectionKeys.size() &&
                               first + index < panel.candidates.size();
             ++index) {
            result.candidates.push_back({Utf8ToWide(panel.selectionKeys[index]),
                                         Utf8ToWide(panel.candidates[first + index])});
        }
        result.candidatesVisible = !result.candidates.empty();
        result.highlightedCandidate = panel.highlightedIndex;
    }

    result.message = Utf8ToWide(state.tooltip);
    if (result.message.empty() && !state.notifications.empty()) {
        result.message = Utf8ToWide(state.notifications.back());
    }
    return result;
}

bool InitializeRuntime(const ChiaKey::RuntimePaths& paths, std::string* errorMessage) {
    RuntimeHolder& holder = Holder();
    std::lock_guard<std::mutex> lock(holder.mutex);
    if (holder.runtime) return true;
    holder.runtime = ChiaKey::Runtime::Create(paths, ChiaKey::EngineConfig(), errorMessage);
    return holder.runtime != nullptr;
}

std::string CurrentInputMethod() {
    const auto runtime = SharedRuntime();
    return runtime ? runtime->primaryInputMethod() : std::string();
}

bool SelectInputMethod(const std::string& identifier) {
    const auto runtime = SharedRuntime();
    return runtime && runtime->setPrimaryInputMethod(identifier);
}

std::vector<std::pair<std::string, std::wstring>> InputMethods() {
    std::vector<std::pair<std::string, std::wstring>> result;
    if (const auto runtime = SharedRuntime()) {
        for (const auto& entry : runtime->inputMethods()) {
            result.emplace_back(entry.first, Utf8ToWide(entry.second));
        }
    }
    return result;
}

std::unique_ptr<EngineSession> EngineSession::Create() {
    std::unique_ptr<ChiaKey::Engine> engine;
    if (const auto runtime = SharedRuntime()) {
        std::string error;
        engine = runtime->createEngine(&error);
        if (!engine) Trace("createEngine failed: %s", error.c_str());
    }
    return std::unique_ptr<EngineSession>(new EngineSession(std::move(engine)));
}

bool EngineSession::hasComposition() const {
    if (!engine_) return false;
    const ChiaKey::EngineState state = engine_->snapshot();
    return !state.composingText.empty() || !state.readingText.empty() ||
           state.candidateState.visible;
}

bool EngineSession::wantsKey(const KeyEvent& event) const {
    if (!engine_) return false;
    // Ctrl+1..9 marks the last N composed characters as a user phrase
    if (IsQuickUserPhraseKey(event)) return hasComposition();
    // TextService checks the punctuation chords itself
    if (event.control || event.alt) return false;
    if (hasComposition()) {
        return IsNavigationOrEditingKey(event.virtualKey) ||
               PrintableAsciiFromVirtualKey(event) != 0 || !event.text.empty();
    }
    // declining here makes TSF skip OnKeyDown and hand the raw key to the app
    return PrintableAsciiFromVirtualKey(event) != 0 || !event.text.empty();
}

EngineResult EngineSession::handleKey(const KeyEvent& event) {
    if (!engine_) return {};
    const bool handled = engine_->handleKey(MakeCoreKey(event));
    EngineResult result = MakeResult(engine_->snapshot());
    result.handled = handled;
    if (!result.committedText.empty()) engine_->acknowledgeCommit();
    return result;
}

void EngineSession::reset() {
    if (engine_) engine_->reset();
}

}  // namespace ChiaKey::WindowsTsf
