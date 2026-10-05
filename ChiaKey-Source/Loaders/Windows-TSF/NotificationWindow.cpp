#include "FrontendBehavior.h"
#include "NotificationWindow.h"
#include "GdiText.h"
#include "ModuleState.h"
#include <algorithm>
#include <mutex>
#include <climits>

namespace ChiaKey::WindowsTsf {
namespace { constexpr wchar_t kClass[] = L"ChiaKey.TSF.NotificationWindow"; }

void NotificationWindow::show(const std::wstring& message) {
    if (message.empty()) return;
    notices_.erase(std::remove_if(notices_.begin(), notices_.end(), [](const auto& notice) {
        return !notice->window_ || !IsWindowVisible(notice->window_);
    }), notices_.end());
    // Keep bounded resources when repeated keys generate notices faster than they expire.
    if (notices_.size() >= 8) notices_.erase(notices_.begin());
    LONG previousBottom = LONG_MIN;
    const HMONITOR monitor = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTONEAREST);
    for (const auto& notice : notices_)
        if (MonitorFromWindow(notice->window_, MONITOR_DEFAULTTONEAREST) == monitor)
            previousBottom = std::max(previousBottom, notice->target_.bottom);
    auto notice = std::make_unique<NotificationWindow>();
    notice->showSingle(message, previousBottom);
    if (!notice->window_) return;
    // When the stack wraps on a short monitor, remove notices it would obscure.
    notices_.erase(std::remove_if(notices_.begin(), notices_.end(), [&](const auto& older) {
        RECT overlap{};
        return IntersectRect(&overlap, &older->target_, &notice->target_) != FALSE;
    }), notices_.end());
    notices_.push_back(std::move(notice));
}

void NotificationWindow::showSingle(const std::wstring& message, LONG previousBottom) {
    static std::once_flag once;
    std::call_once(once, [] {
        WNDCLASSEXW cls{sizeof(cls)};
        cls.hInstance = g_module; cls.lpfnWndProc = WindowProc; cls.lpszClassName = kClass;
        RegisterClassExW(&cls);
    });
    const auto previous = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!window_) CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        kClass, L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, g_module, this);
    if (previous) SetThreadDpiAwarenessContext(previous);
    if (!window_) return;
    message_ = message;
    dpi_ = GetDpiForWindow(window_); if (!dpi_) dpi_ = 96;
    if (font_) DeleteObject(font_);
    font_ = CreateFontW(-MulDiv(12, dpi_, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Arial");
    HDC dc = GetDC(window_);
    const int width = std::clamp(TextWidth(dc, font_, message_) + MulDiv(30, dpi_, 96),
                                MulDiv(180, dpi_, 96), MulDiv(420, dpi_, 96));
    ReleaseDC(window_, dc);
    const int height = MulDiv(95, dpi_, 96);
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTONEAREST), &monitor);
    target_ = NotificationRectangle(monitor.rcWork, width, height, MulDiv(10, dpi_, 96), previousBottom);
    SetWindowPos(window_, HWND_TOPMOST, target_.left, target_.top + MulDiv(NotificationSlide(0), dpi_, 96),
        target_.right - target_.left, target_.bottom - target_.top, SWP_NOACTIVATE);
    SetLayeredWindowAttributes(window_, 0, NotificationAnimationOpacity(0), LWA_ALPHA);
    shownAt_ = GetTickCount64();
    SetTimer(window_, 1, 10, nullptr);
    ShowWindow(window_, SW_SHOWNOACTIVATE);
    InvalidateRect(window_, nullptr, FALSE);
}

void NotificationWindow::hide() {
    for (auto& notice : notices_) notice->hide();
    if (window_) { KillTimer(window_, 1); ShowWindow(window_, SW_HIDE); }
}
void NotificationWindow::destroy() {
    notices_.clear();
    hide(); if (window_) DestroyWindow(window_); window_ = nullptr;
    if (font_) DeleteObject(font_); font_ = nullptr;
}
void NotificationWindow::paint() {
    PAINTSTRUCT paint{}; HDC dc = BeginPaint(window_, &paint);
    RECT rect{}; GetClientRect(window_, &rect);
    FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    HPEN pen = CreatePen(PS_SOLID, MulDiv(2, dpi_, 96), RGB(83, 1, 105));
    auto oldPen = SelectObject(dc, pen); auto oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, MulDiv(2, dpi_, 96), MulDiv(2, dpi_, 96), rect.right, rect.bottom);
    SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(pen);
    auto gradient = [&](int top, int bottom, COLORREF first, COLORREF last) {
        TRIVERTEX vertices[]{
            {MulDiv(2, dpi_, 96), MulDiv(top, dpi_, 96), static_cast<COLOR16>(GetRValue(first) << 8),
                static_cast<COLOR16>(GetGValue(first) << 8), static_cast<COLOR16>(GetBValue(first) << 8), 0},
            {rect.right - MulDiv(2, dpi_, 96), MulDiv(bottom, dpi_, 96), static_cast<COLOR16>(GetRValue(last) << 8),
                static_cast<COLOR16>(GetGValue(last) << 8), static_cast<COLOR16>(GetBValue(last) << 8), 0}};
        GRADIENT_RECT mesh{0, 1}; GradientFill(dc, vertices, 2, &mesh, 1, GRADIENT_FILL_RECT_V);
    };
    gradient(2, 8, RGB(206, 31, 230), RGB(116, 3, 126));
    gradient(7, 19, RGB(83, 1, 105), RGB(111, 0, 134));
    const auto old = SelectObject(dc, font_); SetBkMode(dc, TRANSPARENT);
    RECT title = rect; title.left += MulDiv(10, dpi_, 96); title.top += MulDiv(3, dpi_, 96);
    title.bottom = MulDiv(20, dpi_, 96);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, UiText(L"千秋輸入法").c_str(), -1, &title, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    RECT body = rect; body.left += MulDiv(10, dpi_, 96); body.right -= MulDiv(10, dpi_, 96);
    body.top = MulDiv(25, dpi_, 96); body.bottom -= MulDiv(4, dpi_, 96);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, message_.c_str(), -1, &body, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old); EndPaint(window_, &paint);
}
LRESULT CALLBACK NotificationWindow::WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<NotificationWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<NotificationWindow*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        self->window_ = window; SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return DefWindowProcW(window, message, wparam, lparam);
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message == WM_PAINT) { self->paint(); return 0; }
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_TIMER) {
        const auto elapsed = GetTickCount64() - self->shownAt_;
        const BYTE opacity = NotificationAnimationOpacity(elapsed);
        if (!opacity) self->hide();
        else {
            SetLayeredWindowAttributes(window, 0, opacity, LWA_ALPHA);
            if (elapsed < 110)
                SetWindowPos(window, nullptr, self->target_.left,
                    self->target_.top + MulDiv(NotificationSlide(elapsed), self->dpi_, 96),
                    0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
        }
        return 0;
    }
    if (message == WM_DPICHANGED) {
        self->dpi_ = HIWORD(wparam);
        if (self->font_) DeleteObject(self->font_);
        self->font_ = CreateFontW(-MulDiv(12, self->dpi_, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Arial");
        const RECT target = *reinterpret_cast<RECT*>(lparam);
        self->target_ = target;
        SetWindowPos(window, nullptr, target.left, target.top, target.right - target.left,
            target.bottom - target.top, SWP_NOACTIVATE | SWP_NOZORDER);
        return 0;
    }
    if (message == WM_NCDESTROY) { self->window_ = nullptr; SetWindowLongPtrW(window, GWLP_USERDATA, 0); }
    return DefWindowProcW(window, message, wparam, lparam);
}
}
