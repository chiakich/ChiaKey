#pragma once

#include "ChiaKeyEngine.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace ChiaKey::WindowsTsf {
std::wstring InputMethodName(const std::string& inputMethod);


enum class FrontendShortcut { None, NextInputMethod, ToggleSimplified, RepeatCommit, PunctuationKeyboard };

inline bool MatchesLetterShortcut(const KeyEvent& event, const std::string& letter) {
    return event.control && event.alt && !event.shift && letter.size() == 1 &&
           letter[0] >= 'a' && letter[0] <= 'z' &&
           event.virtualKey == static_cast<UINT>(letter[0] - 'a' + 'A');
}

inline FrontendShortcut ShortcutFor(const KeyEvent& event, const FrontendSettings& settings) {
    if (event.virtualKey == VK_OEM_5 && event.control && !event.alt && !event.shift &&
        settings.toggleWithControlBackslash) return FrontendShortcut::NextInputMethod;
    if (event.virtualKey == VK_OEM_COMMA && event.control && event.alt && !event.shift)
        return FrontendShortcut::PunctuationKeyboard;
    // Same precedence as the legacy RPCService. Empty values disable the chords.
    if (MatchesLetterShortcut(event, settings.chineseConverterToggleKey))
        return FrontendShortcut::ToggleSimplified;
    if (MatchesLetterShortcut(event, settings.repeatLastCommitTextKey))
        return FrontendShortcut::RepeatCommit;
    return FrontendShortcut::None;
}

inline std::string NextInputMethod(const std::string& current,
                                  const std::vector<std::pair<std::string, std::wstring>>& methods,
                                  const FrontendSettings& settings) {
    size_t start = methods.empty() ? 0 : methods.size() - 1;
    for (size_t index = 0; index < methods.size(); ++index)
        if (methods[index].first == current) { start = index; break; }
    for (size_t offset = 1; offset <= methods.size(); ++offset) {
        const std::string& candidate = methods[(start + offset) % methods.size()].first;
        bool suppressed = false;
        for (const auto& hidden : settings.suppressedInputMethods)
            if (hidden == candidate) { suppressed = true; break; }
        if (!suppressed) return candidate;
    }
    return current; // Never switch to an unavailable or entirely hidden method.
}

inline bool CapsLockAlphanumeric(const KeyEvent& event, const FrontendSettings& settings) {
    return settings.capsLockTogglesEnglish && event.capsLock;
}

inline wchar_t AlphanumericCharacter(const KeyEvent& event, const FrontendSettings& settings) {
    if (event.control || event.alt || event.text.size() != 1 ||
        event.text[0] < L' ' || event.text[0] > L'~') return 0;
    wchar_t character = event.text[0];
    // Caps Lock is an English-mode latch, not an uppercase latch in legacy KeyKey.
    if (CapsLockAlphanumeric(event, settings)) {
        if (character >= L'A' && character <= L'Z') character += L'a' - L'A';
        else if (character >= L'a' && character <= L'z') character -= L'a' - L'A';
    }
    return character;
}

inline COLORREF CustomColor(const std::string& value, COLORREF fallback) {
    if (value.compare(0, 6, "Color ") != 0 || value.size() == 6) return fallback;
    const char* number = value.c_str() + 6;
    if (*number == '-') ++number;
    if (*number < '0' || *number > '9') return fallback;
    for (const char* digit = number; *digit; ++digit)
        if (*digit < '0' || *digit > '9') return fallback;
    errno = 0;
    char* end = nullptr;
    const long long argb = std::strtoll(value.c_str() + 6, &end, 10);
    if (errno || *end || argb < std::numeric_limits<int32_t>::min() ||
        argb > std::numeric_limits<int32_t>::max()) return fallback;
    const uint32_t bits = static_cast<uint32_t>(argb);
    return RGB((bits >> 16) & 255, (bits >> 8) & 255, bits & 255);
}

class CommitHistory {
public:
    void record(const std::wstring& text) { if (!text.empty()) text_ = text; }
    std::wstring replay(bool composing) const { return composing ? std::wstring() : text_; }
private:
    std::wstring text_;
};

void PlayTypingErrorSound(const FrontendSettings& settings);
std::wstring UiText(const std::wstring& text, const std::string& language);
inline std::wstring UiText(const std::wstring& text) {
    return UiText(text, CurrentFrontendSettings().uiLanguage);
}

} // namespace ChiaKey::WindowsTsf
