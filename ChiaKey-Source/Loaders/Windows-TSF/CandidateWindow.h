#pragma once

#include <Windows.h>

#include <cstddef>
#include <vector>

#include "ChiaKeyEngine.h"

namespace ChiaKey::WindowsTsf {

class CandidateWindow final {
public:
    static constexpr size_t kNoHighlight = static_cast<size_t>(-1);

    CandidateWindow() = default;
    ~CandidateWindow();

    CandidateWindow(const CandidateWindow&) = delete;
    CandidateWindow& operator=(const CandidateWindow&) = delete;

    void show(HWND owner, const RECT& textRect, const std::vector<EngineCandidate>& candidates,
              size_t highlightedIndex);
    void hide();

private:
    static bool ensureWindowClass();
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    void paint();
    void ensureWindow(HWND owner);
    void updateFont(UINT dpi);
    SIZE measureContent();
    SIZE windowSizeForContent(const SIZE& content) const;

    HWND window_ = nullptr;
    HFONT font_ = nullptr;
    std::vector<EngineCandidate> candidates_;
    size_t highlightedIndex_ = 0;
    int rowHeight_ = 0;
    UINT dpi_ = USER_DEFAULT_SCREEN_DPI;
};

}  // namespace ChiaKey::WindowsTsf
