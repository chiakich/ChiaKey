#include "FrontendBehavior.h"
#include "SymbolWindow.h"

#include <CommCtrl.h>
#include <shellapi.h>
#include <vssym32.h>
#include <windowsx.h>

#include <algorithm>
#include <mutex>

#include "Diagnostics.h"
#include "GdiText.h"
#include "ModuleState.h"

namespace ChiaKey::WindowsTsf {
namespace {

constexpr wchar_t kSymbolWindowClass[] = L"ChiaKey.TSF.SymbolWindow";
constexpr wchar_t kSymbolListClass[] = L"ChiaKey.TSF.SymbolList";
constexpr DWORD kWindowStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU;
constexpr DWORD kWindowExStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE;

// BISymbolForm's layout, in 96-DPI pixels
constexpr int kWidth = 250;
constexpr int kBarHeight = 25;
constexpr int kCellSize = 25;
constexpr int kColumns = 10;
constexpr int kMargin = 4;
// BISmileyPanel's list and buttons
constexpr int kListHeight = 214;
constexpr int kButtonWidth = 100;
constexpr int kButtonHeight = 23;
constexpr int kScreenMargin = 10;
constexpr int kWheelLines = 3;

constexpr int kHitNone = -1;
constexpr int kHitBar = -2;
constexpr int kHitEdit = -3;
constexpr int kHitSend = -4;

std::once_flag g_classesOnce;
bool g_classesRegistered = false;
// out-of-context WinEvents arrive on the thread that set the hook, one window per thread
thread_local SymbolWindow* t_hookedWindow = nullptr;

HFONT MakeFont(int points, UINT dpi) {
    return CreateFontW(-MulDiv(points, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL, FALSE,
                       FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft JhengHei UI");
}

const std::wstring& DisplayText(const SymbolEntry& entry) {
    return entry.label.empty() ? entry.text : entry.label;
}

RECT WorkAreaFor(const RECT& rect) {
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST), &info);
    return info.rcWork;
}

}  // namespace

SymbolWindow::~SymbolWindow() { destroy(); }

int SymbolWindow::scale(int value) const {
    return MulDiv(value, static_cast<int>(dpi_), USER_DEFAULT_SCREEN_DPI);
}

const SymbolPage* SymbolWindow::currentPage() const {
    return page_ < pages_.size() ? &pages_[page_] : nullptr;
}

// BISymbolForm offers Edit on the last list only, where the user's own messages go
bool SymbolWindow::showsEditButton() const {
    const SymbolPage* page = currentPage();
    if (!page || page->buttons) return false;
    for (size_t index = page_ + 1; index < pages_.size(); ++index) {
        if (!pages_[index].buttons) return false;
    }
    return true;
}

bool SymbolWindow::isVisible() const { return window_ && IsWindowVisible(window_); }

bool SymbolWindow::ensureWindow() {
    std::call_once(g_classesOnce, [] {
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
        InitCommonControlsEx(&controls);
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.hInstance = g_module;
        windowClass.lpfnWndProc = SymbolWindow::WindowProc;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.lpszClassName = kSymbolWindowClass;
        const bool window = RegisterClassExW(&windowClass) != 0 ||
                            GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        windowClass.style = CS_DBLCLKS;
        windowClass.lpfnWndProc = SymbolWindow::ListProc;
        windowClass.lpszClassName = kSymbolListClass;
        const bool list = RegisterClassExW(&windowClass) != 0 ||
                          GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        g_classesRegistered = window && list;
    });
    if (window_) return true;
    if (!g_classesRegistered) return false;

    // hosts that are not DPI aware would get a blurry window and scaled coordinates,
    // which would not match the position another app saved
    const DPI_AWARENESS_CONTEXT previous =
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CreateWindowExW(kWindowExStyle, kSymbolWindowClass, UiText(L"符號表").c_str(), kWindowStyle, 0, 0, 1, 1,
                    nullptr, nullptr, g_module, this);
    if (window_) {
        CreateWindowExW(WS_EX_CLIENTEDGE, kSymbolListClass, L"", WS_CHILD | WS_VSCROLL, 0, 0, 1,
                        1, window_, nullptr, g_module, this);
        tooltip_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, TOOLTIPS_CLASSW, nullptr,
                                   WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT,
                                   CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, window_, nullptr,
                                   g_module, nullptr);
    }
    if (previous) SetThreadDpiAwarenessContext(previous);
    if (!window_ || !list_) {
        destroy();
        return false;
    }
    updateMetrics(GetDpiForWindow(window_));
    // TSF reports a thread focus loss as soon as this window shows, so the foreground is
    // followed directly
    if (!t_hookedWindow) {
        foregroundHook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                          nullptr, ForegroundChanged, 0, 0,
                                          WINEVENT_OUTOFCONTEXT);
        if (foregroundHook_) t_hookedWindow = this;
    }
    return true;
}

void CALLBACK SymbolWindow::ForegroundChanged(HWINEVENTHOOK, DWORD, HWND window, LONG, LONG,
                                              DWORD, DWORD) {
    if (t_hookedWindow) t_hookedWindow->followForeground(window);
}

void SymbolWindow::followForeground(HWND foreground) {
    if (!window_ || !foreground || foreground == window_) return;
    if (GetWindowThreadProcessId(foreground, nullptr) != GetCurrentThreadId()) {
        hide();
    } else if (!isVisible() && ReadSymbolWindowState().visible) {
        show();
    }
}

void SymbolWindow::releaseMetrics() {
    for (HFONT* font : {&font_, &symbolFont_}) {
        if (*font) DeleteObject(*font);
        *font = nullptr;
    }
    if (buttonTheme_) CloseThemeData(buttonTheme_);
    buttonTheme_ = nullptr;
}

void SymbolWindow::updateMetrics(UINT dpi) {
    releaseMetrics();
    dpi_ = dpi ? dpi : USER_DEFAULT_SCREEN_DPI;
    font_ = MakeFont(9, dpi_);
    symbolFont_ = MakeFont(10, dpi_);
    buttonTheme_ = OpenThemeDataForDpi(window_, L"BUTTON", dpi_);

    HDC dc = GetDC(window_);
    HGDIOBJ old = SelectObject(dc, font_);
    TEXTMETRICW metrics{};
    GetTextMetricsW(dc, &metrics);
    SelectObject(dc, old);
    ReleaseDC(window_, dc);
    rowHeight_ = metrics.tmHeight + scale(4);
    if (tooltip_) SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, scale(300));
}

void SymbolWindow::destroy() {
    if (foregroundHook_) UnhookWinEvent(foregroundHook_);
    foregroundHook_ = nullptr;
    if (t_hookedWindow == this) t_hookedWindow = nullptr;
    if (window_) DestroyWindow(window_);
    window_ = nullptr;
    list_ = nullptr;
    tooltip_ = nullptr;
    toolCount_ = 0;
    releaseMetrics();
}

void SymbolWindow::layout() {
    const SymbolPage* page = currentPage();
    const int width = scale(kWidth);
    const int cell = scale(kCellSize);
    bar_ = {scale(2), scale(2), width - scale(2), scale(kBarHeight) - scale(2)};
    const int top = scale(kBarHeight) + scale(2);
    cells_.clear();
    if (!page || page->buttons) {
        HDC dc = GetDC(window_);
        int row = 0;
        int column = 0;
        for (const SymbolEntry& entry : page ? page->entries : std::vector<SymbolEntry>()) {
            // a label such as 全形空白 takes as many cells as it needs, as on the mac
            int span = 1;
            if (!entry.label.empty()) {
                const int needed = TextWidth(dc, symbolFont_, entry.label) + scale(6);
                span = std::clamp((needed + cell - 1) / cell, 1, kColumns);
            }
            if (column + span > kColumns) {
                ++row;
                column = 0;
            }
            cells_.push_back({column * cell, top + row * cell, (column + span) * cell,
                              top + (row + 1) * cell});
            column += span;
        }
        ReleaseDC(window_, dc);
        clientHeight_ = top + (row + 1) * cell + scale(kMargin);
        ShowWindow(list_, SW_HIDE);
        return;
    }

    listRect_ = {scale(kMargin), top + scale(kMargin), width - scale(kMargin),
                 top + scale(kMargin) + scale(kListHeight)};
    const int buttonTop = listRect_.bottom + scale(8);
    editButton_ = {scale(kMargin), buttonTop, scale(kMargin + kButtonWidth),
                   buttonTop + scale(kButtonHeight)};
    sendButton_ = {width - scale(kMargin + kButtonWidth), buttonTop, width - scale(kMargin),
                   buttonTop + scale(kButtonHeight)};
    clientHeight_ = buttonTop + scale(kButtonHeight) + scale(8);
    SetWindowPos(list_, nullptr, listRect_.left, listRect_.top, listRect_.right - listRect_.left,
                 listRect_.bottom - listRect_.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    updateListScroll();
}

SIZE SymbolWindow::windowSize() const {
    RECT rect{0, 0, scale(kWidth), clientHeight_};
    AdjustWindowRectExForDpi(&rect, kWindowStyle, FALSE, kWindowExStyle, dpi_);
    return {rect.right - rect.left, rect.bottom - rect.top};
}

// BISymbolForm grows and shrinks upwards, so the window stays by the taskbar
void SymbolWindow::resizeKeepingBottom() {
    RECT current{};
    GetWindowRect(window_, &current);
    const SIZE size = windowSize();
    SetWindowPos(window_, HWND_TOPMOST, current.left, current.bottom - size.cy, size.cx, size.cy,
                 SWP_NOACTIVATE);
    InvalidateRect(window_, nullptr, FALSE);
}

void SymbolWindow::updateTooltips() {
    TTTOOLINFOW tool{};
    tool.cbSize = TTTOOLINFOW_V2_SIZE;
    tool.hwnd = window_;
    for (size_t index = 0; index < toolCount_; ++index) {
        tool.uId = index + 1;
        SendMessageW(tooltip_, TTM_DELTOOL, 0, reinterpret_cast<LPARAM>(&tool));
    }
    toolCount_ = 0;
    const SymbolPage* page = currentPage();
    if (!tooltip_ || !page || !page->buttons) return;
    tool.uFlags = TTF_SUBCLASS;
    for (size_t index = 0; index < cells_.size() && index < page->entries.size(); ++index) {
        tool.uId = index + 1;
        tool.rect = cells_[index];
        tool.lpszText = const_cast<LPWSTR>(page->entries[index].tooltip.c_str());
        SendMessageW(tooltip_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
        ++toolCount_;
    }
}

void SymbolWindow::show() {
    pages_ = SymbolPages();
    if (pages_.empty() || !ensureWindow()) {
        Trace("SymbolWindow: pages=%zu window=%p error=%lu", pages_.size(),
              static_cast<void*>(window_), GetLastError());
        hide();
        return;
    }
    const SymbolWindowState state = ReadSymbolWindowState();
    page_ = 0;
    for (size_t index = 0; index < pages_.size(); ++index) {
        if (pages_[index].name == state.page) page_ = index;
    }
    listSelection_ = 0;
    listTop_ = 0;
    hot_ = pressed_ = kHitNone;

    POINT anchor{state.left, state.bottom};
    if (!state.hasPosition) {
        RECT foreground{};
        if (!GetWindowRect(GetForegroundWindow(), &foreground)) foreground = {};
        const RECT work = WorkAreaFor(foreground);
        anchor = {work.right, work.bottom - scale(kScreenMargin)};
    }
    const RECT anchorRect{anchor.x, anchor.y - 1, anchor.x + 1, anchor.y};
    // the monitor's DPI decides the size, and the size decides where it fits
    SetWindowPos(window_, HWND_TOPMOST, anchor.x, anchor.y - 1, 1, 1, SWP_NOACTIVATE);
    updateMetrics(GetDpiForWindow(window_));
    layout();
    const SIZE size = windowSize();
    const RECT work = WorkAreaFor(anchorRect);
    LONG left = state.hasPosition ? anchor.x : anchor.x - size.cx - scale(kScreenMargin);
    LONG top = anchor.y - size.cy;
    left = std::clamp<LONG>(left, work.left, std::max(work.left, work.right - size.cx));
    top = std::clamp<LONG>(top, work.top, std::max(work.top, work.bottom - size.cy));
    SetWindowPos(window_, HWND_TOPMOST, left, top, size.cx, size.cy,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    updateTooltips();
    InvalidateRect(window_, nullptr, FALSE);
    InvalidateRect(list_, nullptr, FALSE);
}

void SymbolWindow::hide() {
    if (tooltip_) SendMessageW(tooltip_, TTM_POP, 0, 0);
    if (window_ && IsWindowVisible(window_)) ShowWindow(window_, SW_HIDE);
}

void SymbolWindow::open() {
    show();
    if (isVisible()) saveState(true);
}

void SymbolWindow::close() {
    if (isVisible()) {
        saveState(false);
    } else {
        SymbolWindowState state = ReadSymbolWindowState();
        state.visible = false;
        WriteSymbolWindowState(state);
    }
    hide();
}

void SymbolWindow::saveState(bool visible) const {
    SymbolWindowState state;
    state.visible = visible;
    if (const SymbolPage* page = currentPage()) state.page = page->name;
    RECT rect{};
    if (window_ && GetWindowRect(window_, &rect)) {
        state.hasPosition = true;
        state.left = rect.left;
        state.bottom = rect.bottom;
    }
    WriteSymbolWindowState(state);
}

void SymbolWindow::selectPage(size_t index) {
    if (index >= pages_.size()) return;
    page_ = index;
    listSelection_ = 0;
    listTop_ = 0;
    hot_ = pressed_ = kHitNone;
    layout();
    resizeKeepingBottom();
    updateTooltips();
    InvalidateRect(list_, nullptr, FALSE);
    saveState(true);
}

void SymbolWindow::showPageMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    for (size_t index = 0; index < pages_.size(); ++index) {
        AppendMenuW(menu, MF_STRING | (index == page_ ? MF_CHECKED : 0),
                    static_cast<UINT_PTR>(index + 1), pages_[index].name.c_str());
    }
    POINT origin{bar_.left, bar_.bottom};
    ClientToScreen(window_, &origin);
    const UINT chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN |
                                                 TPM_TOPALIGN,
                                       origin.x, origin.y, 0, window_, nullptr);
    DestroyMenu(menu);
    if (chosen) selectPage(chosen - 1);
}

void SymbolWindow::editUserMessages() const {
    const std::wstring path = UserCannedMessagesPath();
    if (path.empty()) return;
    const std::wstring arguments = L"\"" + path + L"\"";
    ShellExecuteW(nullptr, L"open", L"notepad.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
}

int SymbolWindow::hitTest(POINT point) const {
    if (PtInRect(&bar_, point)) return kHitBar;
    const SymbolPage* page = currentPage();
    if (!page) return kHitNone;
    if (page->buttons) {
        for (size_t index = 0; index < cells_.size(); ++index) {
            if (PtInRect(&cells_[index], point)) return static_cast<int>(index);
        }
        return kHitNone;
    }
    if (PtInRect(&sendButton_, point)) return kHitSend;
    if (showsEditButton() && PtInRect(&editButton_, point)) return kHitEdit;
    return kHitNone;
}

void SymbolWindow::activate(int hit) {
    const SymbolPage* page = currentPage();
    if (!page) return;
    if (hit >= 0 && static_cast<size_t>(hit) < page->entries.size()) {
        send_(page->entries[static_cast<size_t>(hit)].text);
    } else if (hit == kHitSend && listSelection_ < page->entries.size()) {
        send_(page->entries[listSelection_].text);
    } else if (hit == kHitEdit) {
        editUserMessages();
    }
}

LRESULT CALLBACK SymbolWindow::WindowProc(HWND window, UINT message, WPARAM wparam,
                                          LPARAM lparam) {
    auto* self = reinterpret_cast<SymbolWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<SymbolWindow*>(
            reinterpret_cast<const CREATESTRUCTW*>(lparam)->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self && self->window_ == window) return self->handleMessage(message, wparam, lparam);
    return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK SymbolWindow::ListProc(HWND window, UINT message, WPARAM wparam,
                                        LPARAM lparam) {
    auto* self = reinterpret_cast<SymbolWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<SymbolWindow*>(
            reinterpret_cast<const CREATESTRUCTW*>(lparam)->lpCreateParams);
        self->list_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self && self->list_ == window) return self->handleListMessage(message, wparam, lparam);
    return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT SymbolWindow::handleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        // the caret has to stay in the app the symbols are typed into
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            paint();
            return 0;
        // the system's move loop makes the window the foreground one, taking the app's caret
        case WM_NCLBUTTONDOWN:
            if (wparam != HTCAPTION) break;
            GetCursorPos(&dragCursor_);
            GetWindowRect(window_, &dragStart_);
            dragging_ = true;
            SetCapture(window_);
            return 0;
        case WM_NCRBUTTONDOWN:
        case WM_NCRBUTTONUP:
            if (wparam == HTCAPTION || wparam == HTSYSMENU) return 0;
            break;
        case WM_MOUSEMOVE: {
            if (dragging_) {
                POINT cursor{};
                GetCursorPos(&cursor);
                SetWindowPos(window_, nullptr, dragStart_.left + cursor.x - dragCursor_.x,
                             dragStart_.top + cursor.y - dragCursor_.y, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }
            if (!trackingMouse_) {
                TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window_, 0};
                trackingMouse_ = TrackMouseEvent(&track) != FALSE;
            }
            const int hit = hitTest({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
            if (hit != hot_) {
                hot_ = hit;
                InvalidateRect(window_, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            trackingMouse_ = false;
            hot_ = kHitNone;
            InvalidateRect(window_, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            const int hit = hitTest({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
            if (hit == kHitBar) {
                showPageMenu();
                return 0;
            }
            if (hit == kHitNone) return 0;
            pressed_ = hit;
            SetCapture(window_);
            InvalidateRect(window_, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            if (dragging_) {
                // WM_CAPTURECHANGED ends the drag
                ReleaseCapture();
                return 0;
            }
            const int pressed = pressed_;
            pressed_ = kHitNone;
            if (GetCapture() == window_) ReleaseCapture();
            InvalidateRect(window_, nullptr, FALSE);
            if (pressed != kHitNone &&
                hitTest({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}) == pressed) {
                activate(pressed);
            }
            return 0;
        }
        case WM_CAPTURECHANGED:
            if (dragging_) {
                dragging_ = false;
                saveState(true);
            }
            if (pressed_ != kHitNone) {
                pressed_ = kHitNone;
                InvalidateRect(window_, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSEWHEEL:
            if (list_ && IsWindowVisible(list_)) {
                return SendMessageW(list_, message, wparam, lparam);
            }
            return 0;
        case WM_CLOSE:
            close();
            return 0;
        case WM_DPICHANGED: {
            const auto* suggested = reinterpret_cast<const RECT*>(lparam);
            updateMetrics(HIWORD(wparam));
            layout();
            const SIZE size = windowSize();
            SetWindowPos(window_, nullptr, suggested->left, suggested->bottom - size.cy, size.cx,
                         size.cy, SWP_NOACTIVATE | SWP_NOZORDER);
            updateTooltips();
            InvalidateRect(window_, nullptr, FALSE);
            InvalidateRect(list_, nullptr, FALSE);
            return 0;
        }
        case WM_THEMECHANGED:
        case WM_SETTINGCHANGE:
            updateMetrics(dpi_);
            InvalidateRect(window_, nullptr, FALSE);
            return 0;
        case WM_NCDESTROY: {
            HWND destroyed = window_;
            window_ = nullptr;
            list_ = nullptr;
            tooltip_ = nullptr;
            toolCount_ = 0;
            return DefWindowProcW(destroyed, message, wparam, lparam);
        }
        default:
            break;
    }
    return DefWindowProcW(window_, message, wparam, lparam);
}

void SymbolWindow::updateListScroll() {
    const SymbolPage* page = currentPage();
    RECT client{};
    GetClientRect(list_, &client);
    const int visible = rowHeight_ ? std::max<int>(1, client.bottom / rowHeight_) : 1;
    const int count = page ? static_cast<int>(page->entries.size()) : 0;
    listTop_ = std::clamp(listTop_, 0, std::max(0, count - visible));
    SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL};
    info.nMax = std::max(0, count - 1);
    info.nPage = static_cast<UINT>(visible);
    info.nPos = listTop_;
    SetScrollInfo(list_, SB_VERT, &info, TRUE);
}

void SymbolWindow::scrollListTo(int top) {
    listTop_ = top;
    updateListScroll();
    InvalidateRect(list_, nullptr, FALSE);
}

LRESULT SymbolWindow::handleListMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    const SymbolPage* page = currentPage();
    const int count = page ? static_cast<int>(page->entries.size()) : 0;
    switch (message) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            paintList();
            return 0;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: {
            const int row = rowHeight_ ? listTop_ + GET_Y_LPARAM(lparam) / rowHeight_ : -1;
            if (row < 0 || row >= count) return 0;
            listSelection_ = static_cast<size_t>(row);
            InvalidateRect(list_, nullptr, FALSE);
            if (message == WM_LBUTTONDBLCLK) activate(kHitSend);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            const int lines = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA * kWheelLines;
            scrollListTo(listTop_ - lines);
            return 0;
        }
        case WM_VSCROLL: {
            SCROLLINFO info{sizeof(info), SIF_ALL};
            GetScrollInfo(list_, SB_VERT, &info);
            int top = listTop_;
            switch (LOWORD(wparam)) {
                case SB_LINEUP: --top; break;
                case SB_LINEDOWN: ++top; break;
                case SB_PAGEUP: top -= static_cast<int>(info.nPage); break;
                case SB_PAGEDOWN: top += static_cast<int>(info.nPage); break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION: top = info.nTrackPos; break;
                case SB_TOP: top = 0; break;
                case SB_BOTTOM: top = count; break;
                default: break;
            }
            scrollListTo(top);
            return 0;
        }
        case WM_SIZE:
            updateListScroll();
            return 0;
        case WM_NCDESTROY: {
            HWND destroyed = list_;
            list_ = nullptr;
            return DefWindowProcW(destroyed, message, wparam, lparam);
        }
        default:
            return DefWindowProcW(list_, message, wparam, lparam);
    }
}

void SymbolWindow::drawButton(HDC dc, const RECT& rect, int state, const std::wstring& text,
                              HFONT font) const {
    if (buttonTheme_) {
        DrawThemeBackground(buttonTheme_, dc, BP_PUSHBUTTON, state, &rect, nullptr);
    } else {
        RECT frame = rect;
        DrawFrameControl(dc, &frame, DFC_BUTTON,
                         DFCS_BUTTONPUSH | (state == PBS_PRESSED ? DFCS_PUSHED : 0));
    }
    RECT textRect = rect;
    if (state == PBS_PRESSED && !buttonTheme_) OffsetRect(&textRect, 1, 1);
    HGDIOBJ old = SelectObject(dc, font);
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &textRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old);
}

void SymbolWindow::paint() {
    PAINTSTRUCT paintStruct{};
    HDC target = BeginPaint(window_, &paintStruct);
    RECT client{};
    GetClientRect(window_, &client);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    SetBkMode(dc, TRANSPARENT);
    FillRect(dc, &client, GetSysColorBrush(COLOR_BTNFACE));

    const auto stateFor = [this](int hit) {
        if (pressed_ == hit && hot_ == hit) return PBS_PRESSED;
        return hot_ == hit && pressed_ == kHitNone ? PBS_HOT : PBS_NORMAL;
    };

    // the category picker, BISymbolForm's drop-down button
    const SymbolPage* page = currentPage();
    if (buttonTheme_ && hot_ == kHitBar) {
        DrawThemeBackground(buttonTheme_, dc, BP_PUSHBUTTON, PBS_HOT, &bar_, nullptr);
    }
    RECT title = bar_;
    title.left += scale(6);
    title.right -= scale(18);
    HGDIOBJ oldFont = SelectObject(dc, font_);
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    if (page) {
        DrawTextW(dc, page->name.c_str(), static_cast<int>(page->name.size()), &title,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    SelectObject(dc, oldFont);
    const int arrow = scale(7);
    const int arrowLeft = bar_.right - scale(14);
    const int arrowTop = (bar_.top + bar_.bottom - arrow / 2) / 2;
    const POINT triangle[] = {{arrowLeft, arrowTop},
                              {arrowLeft + arrow, arrowTop},
                              {arrowLeft + arrow / 2, arrowTop + arrow / 2 + 1}};
    HBRUSH arrowBrush = CreateSolidBrush(GetSysColor(COLOR_BTNTEXT));
    HGDIOBJ oldBrush = SelectObject(dc, arrowBrush);
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Polygon(dc, triangle, 3);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(arrowBrush);

    if (page && page->buttons) {
        for (size_t index = 0; index < cells_.size() && index < page->entries.size(); ++index) {
            drawButton(dc, cells_[index], stateFor(static_cast<int>(index)),
                       DisplayText(page->entries[index]), symbolFont_);
        }
    } else if (page) {
        if (showsEditButton()) drawButton(dc, editButton_, stateFor(kHitEdit), UiText(L"編輯"), font_);
        drawButton(dc, sendButton_, stateFor(kHitSend), UiText(L"送出"), font_);
    }

    BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    EndPaint(window_, &paintStruct);
}

void SymbolWindow::paintList() {
    PAINTSTRUCT paintStruct{};
    HDC target = BeginPaint(list_, &paintStruct);
    RECT client{};
    GetClientRect(list_, &client);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    SetBkMode(dc, TRANSPARENT);
    FillRect(dc, &client, GetSysColorBrush(COLOR_WINDOW));

    const SymbolPage* page = currentPage();
    HGDIOBJ oldFont = SelectObject(dc, font_);
    for (int row = listTop_; page && rowHeight_ && row < static_cast<int>(page->entries.size());
         ++row) {
        RECT line{0, (row - listTop_) * rowHeight_, client.right,
                  (row - listTop_ + 1) * rowHeight_};
        if (line.top >= client.bottom) break;
        const bool selected = static_cast<size_t>(row) == listSelection_;
        if (selected) FillRect(dc, &line, GetSysColorBrush(COLOR_HIGHLIGHT));
        SetTextColor(dc, GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
        line.left += scale(3);
        const std::wstring& text = DisplayText(page->entries[static_cast<size_t>(row)]);
        DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &line,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    SelectObject(dc, oldFont);

    BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    EndPaint(list_, &paintStruct);
}

}  // namespace ChiaKey::WindowsTsf
