#include "FrontendBehavior.h"
#include "StatusWindow.h"

#include <shellapi.h>
#include <windowsx.h>
#include <algorithm>
#include <mutex>
#include "GdiText.h"
#include "IconIds.h"
#include "ModuleState.h"

namespace ChiaKey::WindowsTsf {
namespace {
constexpr wchar_t kClass[] = L"ChiaKey.TSF.StatusWindow";
constexpr UINT kTrayMessage = WM_APP + 37;
constexpr UINT_PTR kRefreshTimer = 1;
constexpr DWORD kExStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED;
constexpr int kHead = 36, kButton = 24, kName = 100, kTail = 6, kHeight = 24;
constexpr int kWidth = kHead + kName + kButton * 5 + kTail;
}

void StatusWindow::update(const StatusModel& model) {
    model_ = model;
    if (!CurrentFrontendSettings().showStatusBar) { hide(); return; }
    if (!window_) {
        static std::once_flag once;
        std::call_once(once, [] {
            WNDCLASSEXW cls{sizeof(cls)};
            cls.style = CS_DBLCLKS;
            cls.hInstance = g_module;
            cls.lpfnWndProc = WindowProc;
            cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            cls.lpszClassName = kClass;
            RegisterClassExW(&cls);
        });
        const auto previous = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        CreateWindowExW(kExStyle, kClass, UiText(L"千秋輸入法").c_str(), WS_POPUP, 0, 0, 1, 1,
                        nullptr, nullptr, g_module, this);
        if (previous) SetThreadDpiAwarenessContext(previous);
        if (!window_) return;
        dpi_ = GetDpiForWindow(window_);
        if (!dpi_) dpi_ = 96;
        font_ = CreateFontW(-scale(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH, L"Microsoft JhengHei UI");
        SetTimer(window_, kRefreshTimer, 500, nullptr);
    }
    synchronize();
    InvalidateRect(window_, nullptr, FALSE);
}

void StatusWindow::tray(bool visible) {
    if (!window_ || inTray_ == visible) return;
    NOTIFYICONDATAW icon{sizeof(icon)};
    icon.hWnd = window_;
    icon.uID = 1;
    icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    icon.uCallbackMessage = kTrayMessage;
    icon.hIcon = LoadIconW(g_module, MAKEINTRESOURCEW(IDI_CHIAKEY));
    wcscpy_s(icon.szTip, UiText(L"千秋輸入法：按一下還原浮動狀態列").c_str());
    if (Shell_NotifyIconW(visible ? NIM_ADD : NIM_DELETE, &icon)) inTray_ = visible;
}

void StatusWindow::synchronize() {
    if (!window_) return;
    if (!CurrentFrontendSettings().showStatusBar) { hide(); return; }
    const HWND foreground = GetForegroundWindow();
    if (!foreground || GetWindowThreadProcessId(foreground, nullptr) != GetCurrentThreadId()) {
        // Keep the tray icon while interacting with Explorer's tray flyout. A newly
        // active TIP takes the session lease, so inactive hosts then remove theirs.
        if (!inTray_ || sharedState_.statusOwner() != window_) hide();
        return;
    }
    sharedState_.setStatusOwner(window_);
    const auto settings = CurrentFrontendSettings();
    mini_ = settings.miniStatusBar;
    tray(mini_ && settings.statusBarInTray);
    if (mini_ && settings.statusBarInTray) { ShowWindow(window_, SW_HIDE); return; }
    SetLayeredWindowAttributes(window_, 0, settings.transparentStatusBar ? 128 : 255, LWA_ALPHA);
    RECT rect{};
    GetWindowRect(window_, &rect);
    MONITORINFO monitor{sizeof(monitor)};
    const auto saved = positioned_ ? StatusWindowState{} : ReadStatusWindowState();
    const HMONITOR targetMonitor = saved.hasPosition
        ? MonitorFromPoint({saved.left, saved.top}, MONITOR_DEFAULTTONEAREST)
        : MonitorFromWindow(positioned_ ? window_ : foreground, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfoW(targetMonitor, &monitor);
    const int width = scale(mini_ ? 64 : kWidth), height = scale(kHeight);
    if (!positioned_) {
        rect.left = saved.hasPosition ? saved.left : monitor.rcWork.right - width - scale(10);
        rect.top = saved.hasPosition ? saved.top : monitor.rcWork.bottom - height - scale(20);
        positioned_ = true;
    }
    const LONG x = std::clamp<LONG>(rect.left, monitor.rcWork.left,
        std::max(monitor.rcWork.left, monitor.rcWork.right - width));
    const LONG y = std::clamp<LONG>(rect.top, monitor.rcWork.top,
        std::max(monitor.rcWork.top, monitor.rcWork.bottom - height));
    SetWindowPos(window_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
    ShowWindow(window_, SW_SHOWNOACTIVATE);
}

void StatusWindow::hide() {
    tray(false);
    if (window_) ShowWindow(window_, SW_HIDE);
}

void StatusWindow::destroy() {
    hide();
    if (sharedState_.statusOwner() == window_) sharedState_.setStatusOwner(nullptr);
    if (window_) DestroyWindow(window_);
    window_ = nullptr;
    if (font_) DeleteObject(font_);
    font_ = nullptr;
}

RECT StatusWindow::cell(int index) const {
    const int left = index ? kHead + kName + (index - 1) * kButton : kHead;
    return {scale(left), scale(1), scale(left + (index ? kButton : kName)), scale(kHeight - 1)};
}

int StatusWindow::hit(POINT point) const {
    if (mini_) return -1;
    for (int index = 0; index < 6; ++index) {
        RECT rect = cell(index);
        if (PtInRect(&rect, point)) return index;
    }
    return -1;
}

void StatusWindow::paint() {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window_, &paint);
    RECT client{};
    GetClientRect(window_, &client);
    FillRect(dc, &client, GetSysColorBrush(COLOR_BTNFACE));
    DrawEdge(dc, &client, EDGE_RAISED, BF_RECT);
    const auto old = SelectObject(dc, font_);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    HICON icon = LoadIconW(g_module, MAKEINTRESOURCEW(IDI_CHIAKEY));
    DrawIconEx(dc, scale(4), scale(4), icon, scale(16), scale(16), 0, nullptr, DI_NORMAL);
    if (mini_) {
        RECT text{scale(25), 0, client.right - scale(2), client.bottom};
        DrawTextW(dc, model_.chinese ? L"中" : L"英", -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        const std::wstring labels[]{model_.inputMethod + L" ▾", model_.chinese ? L"中" : L"英",
            model_.simplified ? L"简" : L"繁", model_.fullWidth ? L"全" : L"半", L"符", L"⚙"};
        for (int index = 0; index < 6; ++index) {
            RECT rect = cell(index);
            if (pressed_ == index) DrawEdge(dc, &rect, EDGE_SUNKEN, BF_RECT);
            DrawTextW(dc, labels[index].c_str(), -1, &rect,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
    }
    SelectObject(dc, old);
    EndPaint(window_, &paint);
}

void StatusWindow::savePosition() {
    RECT rect{};
    if (window_ && GetWindowRect(window_, &rect)) WriteStatusWindowState({true, rect.left, rect.top});
}

void StatusWindow::toggleMini() {
    SetFrontendBool("ShouldUseMiniMode", !mini_);
    synchronize();
    InvalidateRect(window_, nullptr, FALSE);
}

LRESULT CALLBACK StatusWindow::WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<StatusWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<StatusWindow*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: self->paint(); return 0;
    case WM_TIMER: self->synchronize(); return 0;
    case WM_LBUTTONDBLCLK: self->toggleMini(); return 0;
    case WM_LBUTTONDOWN:
        self->pressed_ = self->hit({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        if (self->pressed_ >= 0) SetCapture(window);
        else { ReleaseCapture(); SendMessageW(window, WM_NCLBUTTONDOWN, HTCAPTION, 0); }
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP: {
        const int chosen = self->hit({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        const bool activate = chosen >= 0 && chosen == self->pressed_;
        if (GetCapture() == window) ReleaseCapture();
        self->pressed_ = -1;
        if (activate) {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ClientToScreen(window, &point);
            self->action_(static_cast<StatusAction>(chosen), point);
        }
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    case WM_CAPTURECHANGED: self->pressed_ = -1; return 0;
    case WM_RBUTTONUP: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ClientToScreen(window, &point);
        self->action_(StatusAction::Settings, point); return 0;
    }
    case WM_EXITSIZEMOVE: self->savePosition(); return 0;
    case kTrayMessage:
        if (lparam == WM_LBUTTONUP || lparam == WM_LBUTTONDBLCLK) self->toggleMini();
        else if (lparam == WM_RBUTTONUP) {
            POINT point{}; GetCursorPos(&point); self->action_(StatusAction::Settings, point);
        }
        return 0;
    case WM_DPICHANGED: {
        self->dpi_ = HIWORD(wparam);
        if (self->font_) DeleteObject(self->font_);
        self->font_ = CreateFontW(-self->scale(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH, L"Microsoft JhengHei UI");
        const RECT target = *reinterpret_cast<RECT*>(lparam);
        SetWindowPos(window, nullptr, target.left, target.top, target.right - target.left,
                     target.bottom - target.top, SWP_NOACTIVATE | SWP_NOZORDER);
        return 0;
    }
    case WM_NCDESTROY: self->window_ = nullptr; SetWindowLongPtrW(window, GWLP_USERDATA, 0); break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace ChiaKey::WindowsTsf
