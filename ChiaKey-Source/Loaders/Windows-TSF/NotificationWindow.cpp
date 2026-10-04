#include "FrontendBehavior.h"
#include "NotificationWindow.h"
#include "GdiText.h"
#include "ModuleState.h"
#include <algorithm>
#include <mutex>

namespace ChiaKey::WindowsTsf {
namespace { constexpr wchar_t kClass[] = L"ChiaKey.TSF.NotificationWindow"; }

void NotificationWindow::show(const std::wstring& message) {
    if (message.empty()) return;
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
    const int height = MulDiv(56, dpi_, 96);
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTONEAREST), &monitor);
    SetWindowPos(window_, HWND_TOPMOST, monitor.rcWork.right - width - MulDiv(10, dpi_, 96),
        monitor.rcWork.bottom - height - MulDiv(60, dpi_, 96), width, height, SWP_NOACTIVATE);
    SetLayeredWindowAttributes(window_, 0, 255, LWA_ALPHA);
    shownAt_ = GetTickCount64();
    SetTimer(window_, 1, 50, nullptr);
    ShowWindow(window_, SW_SHOWNOACTIVATE);
    InvalidateRect(window_, nullptr, FALSE);
}

void NotificationWindow::hide() {
    if (window_) { KillTimer(window_, 1); ShowWindow(window_, SW_HIDE); }
}
void NotificationWindow::destroy() {
    hide(); if (window_) DestroyWindow(window_); window_ = nullptr;
    if (font_) DeleteObject(font_); font_ = nullptr;
}
void NotificationWindow::paint() {
    PAINTSTRUCT paint{}; HDC dc = BeginPaint(window_, &paint);
    RECT rect{}; GetClientRect(window_, &rect);
    HBRUSH background = CreateSolidBrush(RGB(40, 40, 40));
    FillRect(dc, &rect, background); DeleteObject(background);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(148, 0, 211));
    auto oldPen = SelectObject(dc, pen); auto oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, 0, 0, rect.right, rect.bottom, MulDiv(6, dpi_, 96), MulDiv(6, dpi_, 96));
    SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(pen);
    const auto old = SelectObject(dc, font_); SetBkMode(dc, TRANSPARENT);
    RECT title = rect; title.left += MulDiv(10, dpi_, 96); title.top += MulDiv(3, dpi_, 96);
    title.bottom = MulDiv(20, dpi_, 96);
    SetTextColor(dc, RGB(148, 0, 211));
    DrawTextW(dc, UiText(L"千秋輸入法").c_str(), -1, &title, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    RECT body = rect; body.left += MulDiv(10, dpi_, 96); body.right -= MulDiv(10, dpi_, 96);
    body.top = title.bottom; body.bottom -= MulDiv(4, dpi_, 96);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, message_.c_str(), -1, &body, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
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
        const BYTE opacity = NotificationOpacity(GetTickCount64() - self->shownAt_);
        if (!opacity) self->hide(); else SetLayeredWindowAttributes(window, 0, opacity, LWA_ALPHA);
        return 0;
    }
    if (message == WM_DPICHANGED) {
        self->dpi_ = HIWORD(wparam);
        if (self->font_) DeleteObject(self->font_);
        self->font_ = CreateFontW(-MulDiv(12, self->dpi_, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Arial");
        const RECT target = *reinterpret_cast<RECT*>(lparam);
        SetWindowPos(window, nullptr, target.left, target.top, target.right - target.left,
            target.bottom - target.top, SWP_NOACTIVATE | SWP_NOZORDER);
        return 0;
    }
    if (message == WM_NCDESTROY) { self->window_ = nullptr; SetWindowLongPtrW(window, GWLP_USERDATA, 0); }
    return DefWindowProcW(window, message, wparam, lparam);
}
}
