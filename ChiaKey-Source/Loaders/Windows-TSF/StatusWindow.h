#pragma once

#include "ChiaKeyEngine.h"
#include "SharedState.h"
#include <functional>

namespace ChiaKey::WindowsTsf {

enum class StatusAction { InputMethod, Language, Converter, Width, Symbols, Settings };
struct StatusModel {
    std::wstring inputMethod;
    bool chinese = true;
    bool fullWidth = false;
    bool simplified = false;
};

class StatusWindow final {
public:
    using Handler = std::function<void(StatusAction, POINT)>;
    explicit StatusWindow(Handler action) : action_(std::move(action)) {}
    ~StatusWindow() { destroy(); }
    void update(const StatusModel& model);
    void hide();
    void destroy();
private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    void synchronize();
    void paint();
    void toggleMini();
    RECT cell(int index) const;
    int hit(POINT point) const;
    void tray(bool visible);
    void savePosition();
    int scale(int value) const { return MulDiv(value, static_cast<int>(dpi_), 96); }
    HWND window_ = nullptr;
    HFONT font_ = nullptr;
    UINT dpi_ = 96;
    bool mini_ = false;
    bool inTray_ = false;
    bool positioned_ = false;
    int pressed_ = -1;
    StatusModel model_;
    SharedCommitHistory sharedState_;
    Handler action_;
};

} // namespace ChiaKey::WindowsTsf
