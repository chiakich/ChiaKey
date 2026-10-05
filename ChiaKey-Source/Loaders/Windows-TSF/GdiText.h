#pragma once

#include <Windows.h>

#include <string>

namespace ChiaKey::WindowsTsf {

inline int TextWidth(HDC dc, HFONT font, const std::wstring& text) {
    HGDIOBJ old = SelectObject(dc, font);
    SIZE extent{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &extent);
    SelectObject(dc, old);
    return extent.cx;
}

}  // namespace ChiaKey::WindowsTsf
