#include "CandidateWindow.h"

#include <algorithm>
#include <mutex>
#include <string>

#include "ModuleState.h"

namespace ChiaKey::WindowsTsf {
namespace {

constexpr wchar_t kCandidateWindowClass[] = L"ChiaKey.TSF.CandidateWindow";

// BICandidateForm's color schema; the rest follows the user's settings
constexpr COLORREF kBorderColor = RGB(149, 149, 149);
constexpr COLORREF kHighlightForegroundColor = RGB(255, 255, 255);
constexpr COLORREF kIndicatorColor = RGB(183, 183, 183);
constexpr COLORREF kKeyBackgroundColor = RGB(154, 154, 154);
constexpr COLORREF kKeyHighlightBackgroundColor = RGB(234, 209, 239);
constexpr COLORREF kKeyForegroundColor = RGB(80, 80, 80);

// BICandidateForm's layout, in 96-DPI pixels
constexpr int kBorderWidth = 2;
constexpr int kHeaderHeight = 20;
constexpr int kKeyLeft = 10;
constexpr int kKeySize = 12;
constexpr int kTextLeft = 30;
constexpr int kMinimumWidth = 50;
constexpr int kWidthPadding = 50;
constexpr int kArrowLeft = 12;
constexpr int kArrowTop = 6;
constexpr int kArrowSize = 8;
constexpr int kIndicatorRightMargin = 5;
constexpr int kMessagePadding = 8;
constexpr int kAnchorGap = 2;

std::once_flag g_windowClassOnce;
bool g_windowClassRegistered = false;

HFONT MakeFont(const wchar_t* face, int points, int weight, UINT dpi) {
    return CreateFontW(-MulDiv(points, static_cast<int>(dpi), 72), 0, 0, 0, weight, FALSE,
                       FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

void FillSolid(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void FillPolygon(HDC dc, const POINT* points, int count, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    Polygon(dc, points, count);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

// BICandidateForm.Show() maps the setting names to these System.Drawing colors
CandidatePalette PaletteFor(const FrontendSettings& settings) {
    CandidatePalette palette;
    if (settings.highlightColor == "Green") {
        palette.highlight = RGB(0, 128, 0);
        palette.highlightEnd = RGB(0, 100, 0);
    } else if (settings.highlightColor == "Yellow") {
        palette.highlight = RGB(255, 255, 0);
        palette.highlightEnd = RGB(255, 140, 0);
    } else if (settings.highlightColor == "Red") {
        palette.highlight = RGB(255, 0, 0);
        palette.highlightEnd = RGB(139, 0, 0);
    }
    if (settings.backgroundColor == "White") {
        palette.background = RGB(255, 255, 255);
        palette.pattern = RGB(211, 211, 211);
    }
    if (settings.textColor == "Black") palette.foreground = RGB(0, 0, 0);
    palette.usePattern = settings.backgroundPattern;
    return palette;
}

void FillGradient(HDC dc, const RECT& rect, COLORREF top, COLORREF bottom) {
    TRIVERTEX vertices[2] = {
        {rect.left, rect.top, static_cast<COLOR16>(GetRValue(top) << 8),
         static_cast<COLOR16>(GetGValue(top) << 8), static_cast<COLOR16>(GetBValue(top) << 8), 0},
        {rect.right, rect.bottom, static_cast<COLOR16>(GetRValue(bottom) << 8),
         static_cast<COLOR16>(GetGValue(bottom) << 8), static_cast<COLOR16>(GetBValue(bottom) << 8),
         0},
    };
    GRADIENT_RECT gradient{0, 1};
    GradientFill(dc, vertices, 2, &gradient, 1, GRADIENT_FILL_RECT_V);
}

int TextWidth(HDC dc, HFONT font, const std::wstring& text) {
    HGDIOBJ old = SelectObject(dc, font);
    SIZE extent{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &extent);
    SelectObject(dc, old);
    return extent.cx;
}

void DrawTextIn(HDC dc, HFONT font, COLORREF color, const std::wstring& text, RECT rect,
                UINT format) {
    HGDIOBJ old = SelectObject(dc, font);
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect,
              format | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER);
    SelectObject(dc, old);
}

}  // namespace

CandidateWindow::~CandidateWindow() {
    if (window_) DestroyWindow(window_);
    releaseFonts();
}

int CandidateWindow::scale(int value) const {
    return MulDiv(value, static_cast<int>(dpi_), USER_DEFAULT_SCREEN_DPI);
}

bool CandidateWindow::ensureWindowClass() {
    std::call_once(g_windowClassOnce, [] {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.hInstance = g_module;
        windowClass.lpfnWndProc = CandidateWindow::WindowProc;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        windowClass.lpszClassName = kCandidateWindowClass;
        g_windowClassRegistered = RegisterClassExW(&windowClass) != 0 ||
                                  GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    });
    return g_windowClassRegistered;
}

void CandidateWindow::ensureWindow(HWND owner) {
    if (window_ || !ensureWindowClass()) return;
    window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
                              kCandidateWindowClass, L"", WS_POPUP, 0, 0, 1, 1, owner,
                              nullptr, g_module, this);
}

void CandidateWindow::releaseFonts() {
    for (HFONT* font : {&candidateFont_, &keyFont_, &indicatorFont_}) {
        if (*font) DeleteObject(*font);
        *font = nullptr;
    }
}

void CandidateWindow::updateFonts(UINT dpi) {
    if (candidateFont_ && dpi_ == dpi) return;
    releaseFonts();
    dpi_ = dpi;
    candidateFont_ = MakeFont(L"Microsoft JhengHei", 12, FW_BOLD, dpi);
    keyFont_ = MakeFont(L"Arial", 8, FW_BOLD, dpi);
    indicatorFont_ = MakeFont(L"Arial", 7, FW_BOLD, dpi);

    HDC dc = GetDC(window_);
    HGDIOBJ old = SelectObject(dc, candidateFont_);
    TEXTMETRICW metrics{};
    GetTextMetricsW(dc, &metrics);
    SelectObject(dc, old);
    ReleaseDC(window_, dc);
    rowHeight_ = metrics.tmHeight + scale(4);
}

SIZE CandidateWindow::measure() {
    HDC dc = GetDC(window_);
    SIZE size{};
    if (!message_.empty()) {
        size.cx = TextWidth(dc, candidateFont_, message_) + scale(kMessagePadding) * 2;
        size.cy = rowHeight_ + scale(kMessagePadding);
    } else {
        int width = scale(kMinimumWidth);
        for (const EngineCandidate& candidate : candidates_) {
            width = std::max(width, TextWidth(dc, candidateFont_, candidate.text) +
                                        scale(kWidthPadding));
        }
        const std::wstring indicator =
            L"(" + std::to_wstring(page_) + L"/" + std::to_wstring(pageCount_) + L")";
        width = std::max(width, TextWidth(dc, indicatorFont_, indicator) + scale(kTextLeft));
        size.cx = width;
        // sized for a full page plus the indicator row, so paging never resizes it
        size.cy = scale(kHeaderHeight) + rowHeight_ * static_cast<int>(candidatesPerPage_ + 1);
    }
    ReleaseDC(window_, dc);
    return size;
}

void CandidateWindow::place(HWND owner, const RECT& textRect) {
    SetWindowLongPtrW(window_, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));
    UINT dpi = owner ? GetDpiForWindow(owner) : 0;
    if (!dpi) dpi = GetDpiForWindow(window_);
    updateFonts(dpi ? dpi : USER_DEFAULT_SCREEN_DPI);
    const SIZE size = measure();

    int x = textRect.left;
    int y = textRect.bottom + scale(kAnchorGap);
    HMONITOR monitor = MonitorFromRect(&textRect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    if (GetMonitorInfoW(monitor, &monitorInfo)) {
        const RECT& work = monitorInfo.rcWork;
        if (x + size.cx > work.right) x = work.right - size.cx;
        if (y + size.cy > work.bottom) y = textRect.top - size.cy - scale(kAnchorGap);
        x = std::max(x, static_cast<int>(work.left));
        y = std::max(y, static_cast<int>(work.top));
    }
    SetWindowPos(window_, HWND_TOPMOST, x, y, size.cx, size.cy,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(window_, nullptr, FALSE);
    NotifyWinEvent(EVENT_OBJECT_IME_SHOW, window_, OBJID_CLIENT, CHILDID_SELF);
}

void CandidateWindow::show(HWND owner, const RECT& textRect, const EngineResult& result) {
    candidates_ = result.candidates;
    highlightedIndex_ = result.highlightedCandidate;
    candidatesPerPage_ = std::max(result.candidatesPerPage, candidates_.size());
    page_ = result.candidatePage;
    pageCount_ = result.candidatePageCount;
    palette_ = PaletteFor(CurrentFrontendSettings());
    message_.clear();
    ensureWindow(owner);
    if (!window_ || candidates_.empty()) {
        hide();
        return;
    }
    place(owner, textRect);
}

void CandidateWindow::showMessage(HWND owner, const RECT& textRect, const std::wstring& message) {
    candidates_.clear();
    message_ = message;
    palette_ = PaletteFor(CurrentFrontendSettings());
    ensureWindow(owner);
    if (!window_ || message_.empty()) {
        hide();
        return;
    }
    place(owner, textRect);
}

void CandidateWindow::hide() {
    if (window_ && IsWindowVisible(window_)) {
        ShowWindow(window_, SW_HIDE);
        NotifyWinEvent(EVENT_OBJECT_IME_HIDE, window_, OBJID_CLIENT, CHILDID_SELF);
    }
    candidates_.clear();
    message_.clear();
}

LRESULT CALLBACK CandidateWindow::WindowProc(HWND window, UINT message, WPARAM wparam,
                                             LPARAM lparam) {
    auto* self = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        self = static_cast<CandidateWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self) return self->handleMessage(message, wparam, lparam);
    return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CandidateWindow::handleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_PAINT:
            paint();
            return 0;
        case WM_DPICHANGED: {
            const auto* suggested = reinterpret_cast<const RECT*>(lparam);
            updateFonts(HIWORD(wparam));
            const SIZE size = measure();
            SetWindowPos(window_, nullptr, suggested->left, suggested->top, size.cx, size.cy,
                         SWP_NOACTIVATE | SWP_NOZORDER);
            InvalidateRect(window_, nullptr, FALSE);
            return 0;
        }
        case WM_NCDESTROY: {
            HWND destroyedWindow = window_;
            window_ = nullptr;
            return DefWindowProcW(destroyedWindow, message, wparam, lparam);
        }
        default:
            return DefWindowProcW(window_, message, wparam, lparam);
    }
}

void CandidateWindow::paint() {
    PAINTSTRUCT paintStruct{};
    HDC target = BeginPaint(window_, &paintStruct);
    RECT client{};
    GetClientRect(window_, &client);

    // drawn off screen first, or every key press flickers the panel
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    SetBkMode(dc, TRANSPARENT);

    FillSolid(dc, client, palette_.background);
    if (palette_.usePattern) {
        // HatchStyle.NarrowHorizontal: a line on every other row
        for (LONG row = 0; row < client.bottom; row += 2) {
            const RECT line{0, row, client.right, row + 1};
            FillSolid(dc, line, palette_.pattern);
        }
    }
    if (message_.empty()) {
        paintCandidates(dc, client);
    } else {
        paintMessage(dc, client);
    }
    HBRUSH border = CreateSolidBrush(kBorderColor);
    FrameRect(dc, &client, border);
    DeleteObject(border);

    BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    EndPaint(window_, &paintStruct);
}

void CandidateWindow::paintMessage(HDC dc, const RECT& client) {
    RECT text = client;
    InflateRect(&text, -scale(kMessagePadding), 0);
    DrawTextIn(dc, candidateFont_, palette_.foreground, message_, text, DT_LEFT);
}

void CandidateWindow::paintCandidates(HDC dc, const RECT& client) {
    const int top = scale(kHeaderHeight);
    const int keySize = scale(kKeySize);
    const int notch = std::max(1, scale(1));

    for (size_t index = 0; index < candidates_.size(); ++index) {
        const int rowTop = top + static_cast<int>(index) * rowHeight_;
        const bool highlighted = index == highlightedIndex_;
        if (highlighted) {
            const RECT bar{scale(kBorderWidth + 3), rowTop,
                           client.right - scale(kBorderWidth + 7), rowTop + rowHeight_};
            FillGradient(dc, bar, palette_.highlight, palette_.highlightEnd);
        }

        // a square with its corners notched, as Utilities.DrawBezelPath draws it
        const int keyLeft = scale(kKeyLeft);
        const int keyTop = rowTop + (rowHeight_ - keySize) / 2;
        const POINT bezel[] = {
            {keyLeft + notch, keyTop},           {keyLeft + keySize - notch, keyTop},
            {keyLeft + keySize, keyTop + notch}, {keyLeft + keySize, keyTop + keySize - notch},
            {keyLeft + keySize - notch, keyTop + keySize},
            {keyLeft + notch, keyTop + keySize}, {keyLeft, keyTop + keySize - notch},
            {keyLeft, keyTop + notch},
        };
        FillPolygon(dc, bezel, static_cast<int>(std::size(bezel)),
                    highlighted ? kKeyHighlightBackgroundColor : kKeyBackgroundColor);
        const RECT keyRect{keyLeft, keyTop, keyLeft + keySize + 1, keyTop + keySize + 1};
        DrawTextIn(dc, keyFont_, kKeyForegroundColor, candidates_[index].selectionKey, keyRect,
                   DT_CENTER);

        const RECT textRect{scale(kTextLeft), rowTop, client.right, rowTop + rowHeight_};
        DrawTextIn(dc, candidateFont_,
                   highlighted ? kHighlightForegroundColor : palette_.foreground,
                   candidates_[index].text, textRect, DT_LEFT);
    }

    const int footerTop = top + static_cast<int>(candidatesPerPage_) * rowHeight_;
    const std::wstring indicator =
        L"(" + std::to_wstring(page_) + L"/" + std::to_wstring(pageCount_) + L")";
    const RECT indicatorRect{0, footerTop, client.right - scale(kIndicatorRightMargin),
                             footerTop + rowHeight_};
    DrawTextIn(dc, indicatorFont_, kIndicatorColor, indicator, indicatorRect, DT_RIGHT);

    if (pageCount_ > 1) {
        const int left = scale(kArrowLeft);
        const int size = scale(kArrowSize);
        const int upTop = scale(kArrowTop);
        const POINT up[] = {{left, upTop + size}, {left + size, upTop + size},
                            {left + size / 2, upTop}};
        FillPolygon(dc, up, 3, kIndicatorColor);
        const int downTop = footerTop + (rowHeight_ - size) / 2;
        const POINT down[] = {{left, downTop}, {left + size, downTop},
                              {left + size / 2, downTop + size}};
        FillPolygon(dc, down, 3, kIndicatorColor);
    }
}

}  // namespace ChiaKey::WindowsTsf
