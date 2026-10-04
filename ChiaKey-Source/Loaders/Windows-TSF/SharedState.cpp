#include "SharedState.h"

#include <sddl.h>
#include <sqlite3.h>
#include <algorithm>
#include <memory>
#include <vector>

namespace ChiaKey::WindowsTsf {
namespace {
constexpr size_t kHistoryUnits = 65536;
struct HistoryMemory { DWORD units; DWORD generation; uint64_t statusOwner; wchar_t text[kHistoryUnits]; };
static_assert(offsetof(HistoryMemory, text) == 16, "x64 and Win32 shared layout");

std::wstring UserSid() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return {};
    DWORD bytes = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
    std::vector<BYTE> buffer(bytes);
    const BOOL ok = GetTokenInformation(token, TokenUser, buffer.data(), bytes, &bytes);
    CloseHandle(token);
    wchar_t* sid = nullptr;
    if (!ok || !ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sid)) return {};
    std::wstring result(sid);
    LocalFree(sid);
    return result;
}

struct Database {
    sqlite3* db = nullptr;
    explicit Database(const std::string& directory) {
        if (directory.empty()) return;
        const std::string path = directory + "/WindowsWordCount.db";
        if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                            SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
            if (db) sqlite3_close(db); db = nullptr; return;
        }
        sqlite3_busy_timeout(db, 50);
        if (sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS days(day INTEGER PRIMARY KEY, count INTEGER NOT NULL CHECK(count>=0));"
                            "CREATE TABLE IF NOT EXISTS total(id INTEGER PRIMARY KEY CHECK(id=1), count INTEGER NOT NULL CHECK(count>=0));"
                            "INSERT OR IGNORE INTO total VALUES(1,0);", nullptr, nullptr, nullptr) != SQLITE_OK) {
            sqlite3_close(db); db = nullptr;
        }
    }
    ~Database() { if (db) sqlite3_close(db); }
};
bool Exec(sqlite3* db, const char* sql) { return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK; }
}

SharedCommitHistory::SharedCommitHistory(const std::wstring& testScope) {
    const auto sid = UserSid();
    if (sid.empty()) return;
    const std::wstring prefix = L"Local\\ChiaKey.CommitHistory." + sid + L".v3" + testScope;
    // Explicit medium integrity supports a normal host after an elevated host wrote.
    const std::wstring sddl = L"D:P(A;;GA;;;" + sid + L")(A;;GA;;;SY)S:(ML;;NW;;;ME)";
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) return;
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};
    mutex_ = CreateMutexW(&attributes, FALSE, (prefix + L".Lock").c_str());
    mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, &attributes, PAGE_READWRITE, 0,
                                  sizeof(HistoryMemory), prefix.c_str());
    LocalFree(descriptor);
    if (mapping_) memory_ = MapViewOfFile(mapping_, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(HistoryMemory));
}

SharedCommitHistory::~SharedCommitHistory() {
    if (memory_) UnmapViewOfFile(memory_);
    if (mapping_) CloseHandle(mapping_);
    if (mutex_) CloseHandle(mutex_);
}

void SharedCommitHistory::record(const std::wstring& text) {
    if (text.empty()) return;
    fallback_ = text;
    if (!memory_ || !mutex_) return;
    const DWORD wait = WaitForSingleObject(mutex_, 50);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) return;
    auto* state = static_cast<HistoryMemory*>(memory_);
    state->units = MAXDWORD;
    fallbackGeneration_ = ++state->generation;
    if (text.size() <= kHistoryUnits) {
        std::copy(text.begin(), text.end(), state->text);
        state->units = static_cast<DWORD>(text.size());
    }
    ReleaseMutex(mutex_);
}

std::wstring SharedCommitHistory::replay(bool composing) const {
    if (composing) return {};
    if (!memory_ || !mutex_) return fallback_;
    const DWORD wait = WaitForSingleObject(mutex_, 50);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) return fallback_;
    const auto* state = static_cast<HistoryMemory*>(memory_);
    const std::wstring result = state->units <= kHistoryUnits ? std::wstring(state->text, state->units)
        : (state->generation == fallbackGeneration_ ? fallback_ : std::wstring());
    ReleaseMutex(mutex_);
    return result;
}

void SharedCommitHistory::setStatusOwner(HWND window) {
    if (!memory_ || !mutex_) return;
    const DWORD wait = WaitForSingleObject(mutex_, 50);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) return;
    static_cast<HistoryMemory*>(memory_)->statusOwner = reinterpret_cast<uintptr_t>(window);
    ReleaseMutex(mutex_);
}
HWND SharedCommitHistory::statusOwner() const {
    if (!memory_ || !mutex_) return nullptr;
    const DWORD wait = WaitForSingleObject(mutex_, 50);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) return nullptr;
    const auto owner = static_cast<HistoryMemory*>(memory_)->statusOwner;
    ReleaseMutex(mutex_);
    return reinterpret_cast<HWND>(static_cast<uintptr_t>(owner));
}

size_t CommittedCodePoints(const std::wstring& text) {
    size_t count = 0;
    for (size_t index = 0; index < text.size(); ++index) {
        if (text[index] >= 0xd800 && text[index] <= 0xdbff && index + 1 < text.size() &&
            text[index + 1] >= 0xdc00 && text[index + 1] <= 0xdfff) ++index;
        ++count;
    }
    return count;
}

int LocalDayNumber() {
    SYSTEMTIME today{};
    GetLocalTime(&today);
    // Calendar date interpreted as UTC, so adjacent dates differ by one even across DST.
    today.wHour = today.wMinute = today.wSecond = today.wMilliseconds = 0;
    FILETIME file{};
    if (!SystemTimeToFileTime(&today, &file)) return 0;
    ULARGE_INTEGER ticks{}; ticks.LowPart = file.dwLowDateTime; ticks.HighPart = file.dwHighDateTime;
    return static_cast<int>(ticks.QuadPart / 864000000000ULL);
}

bool AddWordCount(const std::string& directory, const std::wstring& text, int localDay) {
    if (text.empty()) return true;
    Database database(directory);
    if (!database.db || !Exec(database.db, "BEGIN IMMEDIATE")) return false;
    sqlite3_stmt* statement = nullptr;
    bool ok = sqlite3_prepare_v2(database.db,
        "INSERT OR REPLACE INTO days(day,count) VALUES(?,COALESCE((SELECT count FROM days WHERE day=?),0)+?)", -1,
        &statement, nullptr) == SQLITE_OK;
    const auto count = static_cast<sqlite3_int64>(CommittedCodePoints(text));
    if (ok) {
        sqlite3_bind_int(statement, 1, localDay); sqlite3_bind_int(statement, 2, localDay);
        sqlite3_bind_int64(statement, 3, count);
        ok = sqlite3_step(statement) == SQLITE_DONE;
    }
    sqlite3_finalize(statement); statement = nullptr;
    if (ok) ok = sqlite3_prepare_v2(database.db, "UPDATE total SET count=count+? WHERE id=1", -1,
                                    &statement, nullptr) == SQLITE_OK;
    if (ok) { sqlite3_bind_int64(statement, 1, count); ok = sqlite3_step(statement) == SQLITE_DONE; }
    sqlite3_finalize(statement);
    if (ok && Exec(database.db, "COMMIT")) return true;
    Exec(database.db, "ROLLBACK"); return false;
}

bool ReadWordCounts(const std::string& directory, int localDay, WordCounts* counts) {
    if (!counts) return false;
    Database database(directory);
    if (!database.db) return false;
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database.db,
        "SELECT COALESCE(SUM(CASE WHEN day=? THEN count ELSE 0 END),0),"
        "COALESCE(SUM(CASE WHEN day BETWEEN ? AND ? THEN count ELSE 0 END),0),"
        "(SELECT count FROM total WHERE id=1) FROM days", -1, &statement, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(statement, 1, localDay); sqlite3_bind_int(statement, 2, localDay - 6);
    sqlite3_bind_int(statement, 3, localDay);
    const bool ok = sqlite3_step(statement) == SQLITE_ROW;
    if (ok) *counts = {sqlite3_column_int64(statement, 0), sqlite3_column_int64(statement, 1),
                      sqlite3_column_int64(statement, 2)};
    sqlite3_finalize(statement); return ok;
}

bool ClearWordCounts(const std::string& directory) {
    Database database(directory);
    if (!database.db || !Exec(database.db, "BEGIN IMMEDIATE")) return false;
    if (Exec(database.db, "DELETE FROM days; UPDATE total SET count=0 WHERE id=1;") &&
        Exec(database.db, "COMMIT")) return true;
    Exec(database.db, "ROLLBACK"); return false;
}

} // namespace ChiaKey::WindowsTsf
