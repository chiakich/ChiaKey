#pragma once

#include <Windows.h>
#include <functional>
#include <string>

namespace ChiaKey::WindowsTsf {

// BIKeyboardForm's four symbol rows. Physical keys and mouse cells use the same table.
struct PunctuationKey { UINT key; wchar_t symbol; const wchar_t* label; int row; int column; };
inline constexpr PunctuationKey kPunctuationKeys[] = {
    {'1', L'┌', L"1", 0, 0}, {'2', L'┬', L"2", 0, 1}, {'3', L'┐', L"3", 0, 2},
    {'4', L'〝', L"4", 0, 3}, {'5', L'〞', L"5", 0, 4}, {'6', L'‘', L"6", 0, 5},
    {'7', L'’', L"7", 0, 6}, {'8', L'“', L"8", 0, 7}, {'9', L'”', L"9", 0, 8},
    {'0', L'『', L"0", 0, 9}, {VK_OEM_MINUS, L'』', L"-", 0, 10},
    {VK_OEM_PLUS, L'「', L"=", 0, 11}, {VK_OEM_5, L'」', L"\\", 0, 12},
    {'Q', L'├', L"Q", 1, 0}, {'W', L'┼', L"W", 1, 1}, {'E', L'┤', L"E", 1, 2},
    {'R', L'※', L"R", 1, 3}, {'T', L'〈', L"T", 1, 4}, {'Y', L'〉', L"Y", 1, 5},
    {'U', L'《', L"U", 1, 6}, {'I', L'》', L"I", 1, 7}, {'O', L'【', L"O", 1, 8},
    {'P', L'】', L"P", 1, 9}, {VK_OEM_4, L'﹝', L"[", 1, 10}, {VK_OEM_6, L'﹞', L"]", 1, 11},
    {'A', L'└', L"A", 2, 0}, {'S', L'┴', L"S", 2, 1}, {'D', L'┘', L"D", 2, 2},
    {'F', L'○', L"F", 2, 3}, {'G', L'●', L"G", 2, 4}, {'H', L'↑', L"H", 2, 5},
    {'J', L'↓', L"J", 2, 6}, {'K', L'！', L"K", 2, 7}, {'L', L'：', L"L", 2, 8},
    {VK_OEM_1, L'；', L";", 2, 9}, {VK_OEM_7, L'、', L"'", 2, 10},
    {'Z', L'─', L"Z", 3, 0}, {'X', L'│', L"X", 3, 1}, {'C', L'◎', L"C", 3, 2},
    {'V', L'§', L"V", 3, 3}, {'B', L'←', L"B", 3, 4}, {'N', L'→', L"N", 3, 5},
    {'M', L'。', L"M", 3, 6}, {VK_OEM_COMMA, L'，', L",", 3, 7},
    {VK_OEM_PERIOD, L'‧', L".", 3, 8}, {VK_OEM_2, L'？', L"/", 3, 9},
};

// Exact 96-DPI rectangles from BIKeyboardForm.Designer.cs.
inline constexpr RECT kPunctuationCells[] = {
    {27, 4, 55, 27},
    {55, 4, 83, 27},
    {83, 4, 111, 27},
    {111, 4, 139, 27},
    {139, 4, 167, 27},
    {167, 4, 195, 27},
    {195, 4, 223, 27},
    {223, 4, 251, 27},
    {251, 4, 279, 27},
    {279, 4, 307, 27},
    {307, 4, 335, 27},
    {335, 4, 363, 27},
    {376, 28, 404, 51},
    {40, 28, 68, 51},
    {68, 28, 96, 51},
    {96, 28, 124, 51},
    {124, 28, 152, 51},
    {152, 28, 180, 51},
    {180, 28, 208, 51},
    {208, 28, 236, 51},
    {236, 28, 264, 51},
    {264, 28, 292, 51},
    {292, 28, 320, 51},
    {320, 28, 348, 51},
    {348, 28, 376, 51},
    {57, 52, 85, 75},
    {85, 52, 113, 75},
    {113, 52, 141, 75},
    {141, 52, 169, 75},
    {169, 52, 197, 75},
    {197, 52, 225, 75},
    {225, 52, 253, 75},
    {253, 52, 281, 75},
    {281, 52, 309, 75},
    {309, 52, 337, 75},
    {337, 52, 365, 75},
    {68, 76, 96, 99},
    {96, 76, 124, 99},
    {124, 76, 152, 99},
    {152, 76, 180, 99},
    {180, 76, 208, 99},
    {208, 76, 236, 99},
    {236, 76, 264, 99},
    {264, 76, 292, 99},
    {292, 76, 320, 99},
    {320, 76, 348, 99},
};
struct KeyboardDecoration { RECT rect; const wchar_t* label; };
inline constexpr KeyboardDecoration kKeyboardDecorations[] = {
    {{349, 76, 414, 99}, L"Shift"},
    {{2, 76, 66, 99}, L"Shift"},
    {{2, 52, 55, 75}, L"Caps"},
    {{2, 28, 39, 51}, L"Tab"},
    {{366, 52, 414, 75}, L"Enter"},
    {{364, 4, 414, 27}, L"←"},
    {{111, 100, 309, 123}, L""},
    {{2, 100, 37, 123}, L"Ctrl"},
    {{379, 100, 414, 123}, L"Ctrl"},
    {{75, 100, 110, 123}, L"Alt"},
    {{310, 100, 345, 123}, L"Alt"},
};

inline wchar_t PunctuationSymbol(UINT key) {
    for (const auto& entry : kPunctuationKeys) if (entry.key == key) return entry.symbol;
    return 0;
}

inline bool PunctuationModifier(UINT key) {
    return key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT ||
           key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL ||
           key == VK_MENU || key == VK_LMENU || key == VK_RMENU;
}

class PunctuationKeyboard final {
public:
    explicit PunctuationKeyboard(std::function<void(const std::wstring&)> send)
        : send_(std::move(send)) {}
    ~PunctuationKeyboard() { destroy(); }
    void open(HWND owner, const RECT& caret, bool followCursor);
    void close();
    void destroy();
    bool isVisible() const { return window_ && IsWindowVisible(window_); }
private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    void paint();
    void position(const RECT& caret, bool followCursor);
    RECT cell(size_t index) const;
    int hit(POINT point) const;
    int scale(int value) const { return MulDiv(value, static_cast<int>(dpi_), 96); }
    HWND window_ = nullptr;
    HFONT font_ = nullptr;
    HFONT keyFont_ = nullptr;
    UINT dpi_ = 96;
    int pressed_ = -1;
    std::function<void(const std::wstring&)> send_;
};

} // namespace ChiaKey::WindowsTsf
