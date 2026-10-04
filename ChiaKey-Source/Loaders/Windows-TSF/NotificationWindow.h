#pragma once
#include <Windows.h>
#include <string>

namespace ChiaKey::WindowsTsf {
inline BYTE NotificationOpacity(ULONGLONG elapsed) {
    if (elapsed < 1000) return 255;
    const auto steps = (elapsed - 1000) / 50 + 1;
    return steps >= 5 ? 0 : static_cast<BYTE>(255 - steps * 51);
}
class NotificationWindow final {
public:
    ~NotificationWindow() { destroy(); }
    void show(const std::wstring& message);
    void hide();
    void destroy();
private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    void paint();
    HWND window_ = nullptr;
    HFONT font_ = nullptr;
    UINT dpi_ = 96;
    ULONGLONG shownAt_ = 0;
    std::wstring message_;
};
}
