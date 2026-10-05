#pragma once

#include <Windows.h>
#include <uxtheme.h>

#include <functional>
#include <string>
#include <vector>

#include "ChiaKeyEngine.h"

namespace ChiaKey::WindowsTsf {

// after Yahoo! KeyKey's BISymbolForm and BISmileyPanel (Loaders/Windows-IMM/BaseIMEUI)
class SymbolWindow final {
public:
    using SendHandler = std::function<void(const std::wstring&)>;

    explicit SymbolWindow(SendHandler send) : send_(std::move(send)) {}
    ~SymbolWindow();

    SymbolWindow(const SymbolWindow&) = delete;
    SymbolWindow& operator=(const SymbolWindow&) = delete;

    // open and close are the user's; show and hide follow the foreground app and keep the
    // saved state
    void open();
    void close();
    void show();
    void hide();
    void destroy();
    bool isVisible() const;
    void setHostAllowed(bool allowed) { hostAllowed_ = allowed; if (!allowed) hide(); }

private:
    bool hostAllowed_ = true;
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK ListProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    static void CALLBACK ForegroundChanged(HWINEVENTHOOK hook, DWORD event, HWND window,
                                           LONG object, LONG child, DWORD thread, DWORD time);
    void followForeground(HWND foreground);
    LRESULT handleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handleListMessage(UINT message, WPARAM wparam, LPARAM lparam);
    bool ensureWindow();
    void updateMetrics(UINT dpi);
    void releaseMetrics();
    void selectPage(size_t index);
    void layout();
    SIZE windowSize() const;
    void resizeKeepingBottom();
    void updateTooltips();
    void updateListScroll();
    void scrollListTo(int top);
    int hitTest(POINT point) const;
    void activate(int hit);
    void showPageMenu();
    void editUserMessages() const;
    void saveState(bool visible) const;
    void paint();
    void paintList();
    void drawButton(HDC dc, const RECT& rect, int state, const std::wstring& text,
                    HFONT font) const;
    int scale(int value) const;
    const SymbolPage* currentPage() const;
    bool showsEditButton() const;

    SendHandler send_;
    HWND window_ = nullptr;
    HWND list_ = nullptr;
    HWND tooltip_ = nullptr;
    HFONT font_ = nullptr;
    HFONT symbolFont_ = nullptr;
    HTHEME buttonTheme_ = nullptr;
    HWINEVENTHOOK foregroundHook_ = nullptr;
    UINT dpi_ = USER_DEFAULT_SCREEN_DPI;
    int rowHeight_ = 0;

    std::vector<SymbolPage> pages_;
    size_t page_ = 0;
    std::vector<RECT> cells_;
    RECT bar_{};
    RECT listRect_{};
    RECT editButton_{};
    RECT sendButton_{};
    int clientHeight_ = 0;
    int hot_ = -1;
    int pressed_ = -1;
    bool trackingMouse_ = false;
    bool dragging_ = false;
    POINT dragCursor_{};
    RECT dragStart_{};
    size_t toolCount_ = 0;
    size_t listSelection_ = 0;
    int listTop_ = 0;
};

}  // namespace ChiaKey::WindowsTsf
