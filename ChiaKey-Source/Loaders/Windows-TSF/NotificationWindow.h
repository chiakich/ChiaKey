#pragma once
#include <Windows.h>
#include <string>
#include <memory>
#include <vector>
#include <algorithm>

namespace ChiaKey::WindowsTsf {
inline BYTE NotificationOpacity(ULONGLONG elapsed) {
    if (elapsed < 1000) return 255;
    const auto steps = (elapsed - 1000) / 50 + 1;
    return steps >= 5 ? 0 : static_cast<BYTE>(255 - steps * 51);
}
inline BYTE NotificationAnimationOpacity(ULONGLONG elapsed) {
    if (elapsed < 100) return static_cast<BYTE>(std::min<ULONGLONG>(255, 128 + (elapsed / 10) * 26));
    return NotificationOpacity(elapsed - 100);
}
inline int NotificationSlide(ULONGLONG elapsed) {
    return elapsed < 100 ? -10 + static_cast<int>(elapsed / 10) : 0;
}
inline RECT NotificationRectangle(RECT area, int width, int height, int gap, LONG previousBottom) {
    width = std::min(width, static_cast<int>(area.right - area.left));
    height = std::min(height, static_cast<int>(area.bottom - area.top));
    LONG x = std::max(area.left, area.right - width - gap);
    LONG y = std::max(area.top, area.top + gap);
    if (previousBottom >= area.top) y = previousBottom + gap;
    if (y + height > area.bottom) y = std::max(area.top, area.top + gap);
    y = std::min(y, area.bottom - height);
    return {x, y, x + width, y + height};
}
class NotificationWindow final {
public:
    ~NotificationWindow() { destroy(); }
    void show(const std::wstring& message);
    void hide();
    bool isVisible() const {
        if (window_ && IsWindowVisible(window_)) return true;
        for (const auto& notice : notices_) if (notice->isVisible()) return true;
        return false;
    }
    void destroy();
private:
    void showSingle(const std::wstring& message, LONG previousBottom);
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    void paint();
    HWND window_ = nullptr;
    HFONT font_ = nullptr;
    UINT dpi_ = 96;
    ULONGLONG shownAt_ = 0;
    std::wstring message_;
    RECT target_{};
    std::vector<std::unique_ptr<NotificationWindow>> notices_;
};
}
