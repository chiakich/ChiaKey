#include "FrontendBehavior.h"
#include "PunctuationKeyboard.h"

#include <algorithm>
#include <iterator>
#include <mutex>
#include <windowsx.h>
#include "ModuleState.h"

namespace ChiaKey::WindowsTsf {
namespace {
constexpr wchar_t kClass[] = L"ChiaKey.TSF.PunctuationKeyboard";
constexpr DWORD kStyle = WS_POPUP | WS_BORDER;
constexpr DWORD kExStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE;
}

void PunctuationKeyboard::open(HWND owner, const RECT& caret, bool followCursor) {
    static std::once_flag once;
    std::call_once(once, [] {
        WNDCLASSEXW cls{sizeof(cls)};
        cls.hInstance = g_module;
        cls.lpfnWndProc = WindowProc;
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.lpszClassName = kClass;
        RegisterClassExW(&cls);
    });
    const auto previous = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!window_) CreateWindowExW(kExStyle, kClass, UiText(L"標點螢幕鍵盤").c_str(), kStyle,
                                  0, 0, 1, 1, owner, nullptr, g_module, this);
    if (previous) SetThreadDpiAwarenessContext(previous);
    if (!window_) return;
    dpi_ = GetDpiForWindow(window_);
    if (!dpi_) dpi_ = 96;
    if (font_) DeleteObject(font_);
    if (keyFont_) DeleteObject(keyFont_);
    keyFont_ = CreateFontW(-scale(8), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Arial");
    font_ = CreateFontW(-scale(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"PMingLiU");
    position(caret, followCursor);
    ShowWindow(window_, SW_SHOWNOACTIVATE);
    InvalidateRect(window_, nullptr, FALSE);
}

void PunctuationKeyboard::position(const RECT& caret, bool followCursor) {
    RECT size{0, 0, scale(420), scale(127)};
    AdjustWindowRectExForDpi(&size, kStyle, FALSE, kExStyle, dpi_);
    const int width = size.right - size.left, height = size.bottom - size.top;
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromRect(&caret, MONITOR_DEFAULTTONEAREST), &monitor);
    const RECT work = monitor.rcWork;
    RECT current{};
    GetWindowRect(window_, &current);
    int x = current.left, y = current.top;
    if (followCursor) {
        x = caret.left; y = caret.bottom + scale(4);
        if (y + height > work.bottom) y = caret.top - height - scale(4);
    } else if (current.right - current.left <= 1) {
        x = work.right - width - scale(10); y = work.bottom - height - scale(70);
    }
    x = std::clamp<LONG>(x, work.left, std::max(work.left, work.right - width));
    y = std::clamp<LONG>(y, work.top, std::max(work.top, work.bottom - height));
    SetWindowPos(window_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
}

RECT PunctuationKeyboard::cell(size_t index) const {
    const auto& rect = kPunctuationCells[index];
    return {scale(rect.left), scale(rect.top), scale(rect.right), scale(rect.bottom)};
}

int PunctuationKeyboard::hit(POINT point) const {
    for (size_t index = 0; index < std::size(kPunctuationKeys); ++index) {
        const RECT rect = cell(index);
        if (PtInRect(&rect, point)) return static_cast<int>(index);
    }
    return -1;
}

void PunctuationKeyboard::paint() {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window_, &paint);
    RECT client{};
    GetClientRect(window_, &client);
    FillRect(dc, &client, GetSysColorBrush(COLOR_BTNFACE));
    const auto old = SelectObject(dc, font_);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    for (size_t index = 0; index < std::size(kPunctuationKeys); ++index) {
        RECT rect = cell(index);
        DrawFrameControl(dc, &rect, DFC_BUTTON, DFCS_BUTTONPUSH |
                          (pressed_ == static_cast<int>(index) ? DFCS_PUSHED : 0));
        wchar_t text[]{kPunctuationKeys[index].symbol, 0};
        RECT symbolRect = rect;
        symbolRect.left += scale(6); symbolRect.top += scale(4);
        SetTextColor(dc, RGB(0, 0, 0));
        SelectObject(dc, font_);
        DrawTextW(dc, text, 1, &symbolRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        RECT label = rect;
        label.left += scale(3); label.top += scale(3);
        SetTextColor(dc, RGB(0, 128, 0));
        SelectObject(dc, keyFont_);
        DrawTextW(dc, kPunctuationKeys[index].label, -1, &label, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    }
    SelectObject(dc, keyFont_);
    SetTextColor(dc, GetSysColor(COLOR_GRAYTEXT));
    for (const auto& decoration : kKeyboardDecorations) {
        RECT rect{scale(decoration.rect.left), scale(decoration.rect.top),
                  scale(decoration.rect.right), scale(decoration.rect.bottom)};
        DrawFrameControl(dc, &rect, DFC_BUTTON, DFCS_BUTTONPUSH | DFCS_INACTIVE);
        DrawTextW(dc, decoration.label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    SelectObject(dc, old);
    EndPaint(window_, &paint);
}

void PunctuationKeyboard::close() {
    pressed_ = -1;
    if (window_) { if (GetCapture() == window_) ReleaseCapture(); ShowWindow(window_, SW_HIDE); }
}

void PunctuationKeyboard::destroy() {
    if (window_) DestroyWindow(window_);
    window_ = nullptr;
    if (font_) DeleteObject(font_);
    font_ = nullptr;
    if (keyFont_) DeleteObject(keyFont_);
    keyFont_ = nullptr;
}

LRESULT CALLBACK PunctuationKeyboard::WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<PunctuationKeyboard*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<PunctuationKeyboard*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: self->paint(); return 0;
    case WM_CLOSE: self->close(); return 0;
    case WM_NCDESTROY:
        self->window_ = nullptr;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        break;
    case WM_LBUTTONDOWN:
        self->pressed_ = self->hit({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        if (self->pressed_ >= 0) SetCapture(window);
        else { ReleaseCapture(); SendMessageW(window, WM_NCLBUTTONDOWN, HTCAPTION, 0); }
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP: {
        const int index = self->hit({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        const bool send = index >= 0 && index == self->pressed_;
        if (GetCapture() == window) ReleaseCapture();
        self->pressed_ = -1;
        if (send) {
            self->close();
            self->send_(std::wstring(1, kPunctuationKeys[index].symbol));
        }
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    case WM_CAPTURECHANGED: self->pressed_ = -1; InvalidateRect(window, nullptr, FALSE); return 0;
    case WM_DPICHANGED: {
        const RECT target = *reinterpret_cast<RECT*>(lparam);
        self->dpi_ = HIWORD(wparam);
        if (self->font_) DeleteObject(self->font_);
        if (self->keyFont_) DeleteObject(self->keyFont_);
        self->keyFont_ = CreateFontW(-self->scale(8), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Arial");
        self->font_ = CreateFontW(-self->scale(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH, L"PMingLiU");
        SetWindowPos(window, nullptr, target.left, target.top, target.right - target.left,
                     target.bottom - target.top, SWP_NOACTIVATE | SWP_NOZORDER);
        return 0;
    }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace ChiaKey::WindowsTsf
