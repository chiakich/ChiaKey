#pragma once
#include <Windows.h>
#include <unknwn.h>

namespace ChiaKey::WindowsTsf {
// Private, versioned COM ABI. All calls run on the activating TSF apartment.
// S_OK means idle; S_FALSE means defer without interrupting the user.
MIDL_INTERFACE("5977E537-814F-43AE-B2CD-117375691E50")
IChiaKeyReloadControl : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetReloadState(DWORD* state) = 0;
    virtual HRESULT STDMETHODCALLTYPE RestoreReloadState(DWORD state) = 0;
};
constexpr DWORD kReloadChinese = 1;
constexpr DWORD kReloadFullWidth = 2;

class ReloadActivity final {
public:
    explicit ReloadActivity(unsigned& depth) : depth_(depth) { ++depth_; }
    ~ReloadActivity() { --depth_; }
    ReloadActivity(const ReloadActivity&) = delete;
    ReloadActivity& operator=(const ReloadActivity&) = delete;
private:
    unsigned& depth_;
};

// TSF may test a key before delivering it. Keep the backend until the paired
// OnKey callback, even if the application pumps messages between the two.
class ReloadKeyTest final {
public:
    ReloadKeyTest(bool& pending, BOOL* eaten) : pending_(pending), eaten_(eaten) {}
    ~ReloadKeyTest() { pending_ = eaten_ && *eaten_ != FALSE; }
private:
    bool& pending_;
    BOOL* eaten_;
};
} // namespace ChiaKey::WindowsTsf
