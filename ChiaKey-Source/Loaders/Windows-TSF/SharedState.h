#pragma once

#include <Windows.h>
#include <string>
#include <cstdint>

namespace ChiaKey::WindowsTsf {

// Session-local memory, never a plaintext file or the system clipboard.
class SharedCommitHistory final {
public:
    explicit SharedCommitHistory(const std::wstring& testScope = L"");
    ~SharedCommitHistory();
    SharedCommitHistory(const SharedCommitHistory&) = delete;
    SharedCommitHistory& operator=(const SharedCommitHistory&) = delete;
    void record(const std::wstring& text);
    std::wstring replay(bool composing) const;
    void setStatusOwner(HWND window);
    HWND statusOwner() const;
    bool available() const { return memory_ && mutex_; }
private:
    HANDLE mutex_ = nullptr;
    HANDLE mapping_ = nullptr;
    void* memory_ = nullptr;
    std::wstring fallback_;
    DWORD fallbackGeneration_ = 0;
};

// A small session process keeps the mapping alive after all typing hosts close.
// Scope is empty in production; test fixtures use isolated object names.
bool EnsureHistoryHost(const std::wstring& executable, const std::wstring& scope = L"", DWORD waitMs = 50);
int RunHistoryHost(const std::wstring& scope = L"");
bool StopHistoryHost(const std::wstring& scope = L"");

struct WordCounts { int64_t today = 0, week = 0, total = 0; };
size_t CommittedCodePoints(const std::wstring& text);
// Counts only. SQLite transactions coordinate simultaneous x64 and Win32 hosts.
bool AddWordCount(const std::string& directory, const std::wstring& text, int localDay, int busyMs = 1000);
bool ReadWordCounts(const std::string& directory, int localDay, WordCounts* counts);
bool ClearWordCounts(const std::string& directory);
int LocalDayNumber();

} // namespace ChiaKey::WindowsTsf
