#include "FrontendBehavior.h"
#include "OutputFilter.h"
#include <mmsystem.h>

namespace ChiaKey::WindowsTsf {

std::wstring InputMethodName(const std::string& inputMethod) {
    for (const auto& method : InputMethods()) {
        if (method.first == inputMethod) return method.second;
    }
    return UiText(L"中文");
}

std::wstring UiText(const std::wstring& text, const std::string& language) {
    if (language == "zh-CN") return FilterCommittedText(text, true);
    if (language != "en") return text;
    const std::pair<const wchar_t*, const wchar_t*> labels[]{
        {L"千秋輸入法", L"ChiaKey"}, {L"中文", L"Chinese"}, {L"好打注音", L"Smart Phonetic"},
        {L"傳統注音", L"Traditional Phonetic"}, {L"倉頡", L"Cangjie"}, {L"簡易", L"Simplex"},
        {L"簡體輸出", L"Simplified Chinese output"}, {L"半形", L"Half-width"}, {L"全形", L"Full-width"},
        {L"符號表（Ctrl+Alt+.）", L"Symbols (Ctrl+Alt+.)"}, {L"符號表", L"Symbols"},
        {L"中文模式", L"Switch to Chinese mode."}, {L"英文模式", L"Switch to English mode."},
        {L"全形英數模式", L"Full-width roman letters mode."}, {L"半形英數模式", L"Half-width roman letters mode."},
        {L"繁體中文輸出", L"Traditional Chinese mode."}, {L"簡體中文輸出", L"Simplified Chinese mode."},
        {L"選用", L"Current Input Method: "}, {L"輸入法", L"."},
        {L"字典…", L"Dictionary…"}, {L"詞彙編輯器…", L"Phrase Editor…"}, {L"輸入法設定…", L"Preferences…"}, {L"關於…", L"About…"},
        {L"編輯", L"Edit"}, {L"送出", L"Send"}, {L"標點螢幕鍵盤", L"Punctuation keyboard"},
        {L"切換至英文", L"Switch to English"}, {L"切換至", L"Switch to "},
        {L"（按一下切換英文）", L" (click for English)"}, {L"英文（按一下切換", L"English (click for "},
        {L"）", L")"}, {L"全形（Shift+Space 切換）", L"Full-width (Shift+Space)"},
        {L"半形（Shift+Space 切換）", L"Half-width (Shift+Space)"},
        {L"千秋輸入法：按一下還原浮動狀態列", L"ChiaKey: click to restore the floating status bar"}
    };
    for (const auto& label : labels) if (text == label.first) return label.second;
    return text;
}

void PlayTypingErrorSound(const FrontendSettings& settings) {
    if (!settings.playSoundOnTypingError) return;
    const auto& filename = settings.soundFilename;
    if (!filename.empty() && filename != "Default") {
        int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename.data(),
                                        static_cast<int>(filename.size()), nullptr, 0);
        if (length > 0) {
            std::wstring path(static_cast<size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename.data(),
                               static_cast<int>(filename.size()), path.data(), length);
            if (PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) return;
        }
    }
    MessageBeep(MB_OK);
}

} // namespace ChiaKey::WindowsTsf
