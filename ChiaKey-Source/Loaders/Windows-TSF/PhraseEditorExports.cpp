// A C interface to ChiaKey::UserPhraseStore for the settings app's phrase editor,
// which P/Invokes it; strings are UTF-16 both ways.

#include <Windows.h>

#include <memory>
#include <string>
#include <vector>

#include <ChiaKeyCore/UserPhraseStore.h>

#include "ChiaKeyEngine.h"
#include "SharedState.h"
#include "OutputFilter.h"
#include <algorithm>

namespace {

using ChiaKey::UserPhraseStore;

std::string ToUtf8(const wchar_t* text) {
    if (!text || !*text) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) return {};
    std::string result(static_cast<size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring ToWide(const std::string& text) {
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
                        length);
    return result;
}

UserPhraseStore* Store(void* handle) { return static_cast<UserPhraseStore*>(handle); }

}  // namespace

extern "C" {
int __stdcall ChiaKeySimplifyText(const wchar_t* source, wchar_t* output, int capacity) {
    if (!source || !output || capacity <= 0) return 0;
    const auto text = ChiaKey::WindowsTsf::FilterCommittedText(source, true);
    if (text.size() >= static_cast<size_t>(capacity)) return 0;
    std::copy(text.begin(), text.end(), output); output[text.size()] = 0;
    return static_cast<int>(text.size() + 1);
}


BOOL __stdcall ChiaKeyWordCounts(long long* today, long long* week, long long* total) {
    if (!today || !week || !total) return FALSE;
    ChiaKey::WindowsTsf::WordCounts counts;
    if (!ChiaKey::WindowsTsf::ReadWordCounts(ChiaKey::WindowsTsf::DesktopRuntimePaths().writablePath,
                                            ChiaKey::WindowsTsf::LocalDayNumber(), &counts)) return FALSE;
    *today = counts.today; *week = counts.week; *total = counts.total; return TRUE;
}
BOOL __stdcall ChiaKeyClearWordCounts() {
    return ChiaKey::WindowsTsf::ClearWordCounts(ChiaKey::WindowsTsf::DesktopRuntimePaths().writablePath);
}


typedef void(__stdcall* ChiaKeyPhraseCallback)(long long rowid, const wchar_t* phrase,
                                               const wchar_t* reading);
typedef void(__stdcall* ChiaKeyStringCallback)(const wchar_t* text);

void* __stdcall ChiaKeyPhrasesOpen() {
    const ChiaKey::RuntimePaths paths = ChiaKey::WindowsTsf::DesktopRuntimePaths();
    if (paths.writablePath.empty()) return nullptr;
    return UserPhraseStore::Open(paths.writablePath, paths.lexiconDatabasePath).release();
}

void __stdcall ChiaKeyPhrasesClose(void* handle) { delete Store(handle); }

void __stdcall ChiaKeyPhrasesBeginSession(void* handle) {
    if (handle) Store(handle)->beginEditingSession();
}

void __stdcall ChiaKeyPhrasesRefreshSession(void* handle) {
    if (handle) Store(handle)->refreshEditingSession();
}

void __stdcall ChiaKeyPhrasesEndSession(void* handle) {
    if (handle) Store(handle)->endEditingSession();
}

int __stdcall ChiaKeyPhrasesCount(void* handle, const wchar_t* filter) {
    return handle ? static_cast<int>(Store(handle)->count(ToUtf8(filter))) : 0;
}

int __stdcall ChiaKeyPhrasesList(void* handle, const wchar_t* filter, int order, int ascending,
                                 int offset, int limit, ChiaKeyPhraseCallback callback) {
    if (!handle || !callback || offset < 0 || limit < 0) return 0;
    const auto phrases = Store(handle)->phrases(
        ToUtf8(filter), static_cast<ChiaKey::UserPhraseOrder>(order), ascending != 0,
        static_cast<size_t>(offset), static_cast<size_t>(limit));
    for (const ChiaKey::UserPhrase& phrase : phrases) {
        callback(phrase.rowid, ToWide(phrase.phrase).c_str(), ToWide(phrase.reading).c_str());
    }
    return static_cast<int>(phrases.size());
}

int __stdcall ChiaKeyPhrasesContains(void* handle, const wchar_t* phrase) {
    return handle && Store(handle)->contains(ToUtf8(phrase)) ? 1 : 0;
}

// the new row's id, or 0
long long __stdcall ChiaKeyPhrasesAdd(void* handle, const wchar_t* phrase, const wchar_t* reading) {
    ChiaKey::UserPhrase added;
    if (!handle || !Store(handle)->add(ToUtf8(phrase), ToUtf8(reading), &added)) return 0;
    return added.rowid;
}

int __stdcall ChiaKeyPhrasesSetPhrase(void* handle, long long rowid, const wchar_t* phrase) {
    return handle && Store(handle)->setPhrase(rowid, ToUtf8(phrase)) ? 1 : 0;
}

int __stdcall ChiaKeyPhrasesSetPhraseAndReading(void* handle, long long rowid,
                                               const wchar_t* phrase, const wchar_t* reading) {
    return handle && Store(handle)->setPhraseAndReading(rowid, ToUtf8(phrase), ToUtf8(reading)) ? 1 : 0;
}

int __stdcall ChiaKeyPhrasesSetReading(void* handle, long long rowid, const wchar_t* reading) {
    return handle && Store(handle)->setReading(rowid, ToUtf8(reading)) ? 1 : 0;
}

int __stdcall ChiaKeyPhrasesRemove(void* handle, const long long* rowids, int count) {
    if (!handle || (count && !rowids) || count < 0) return 0;
    return Store(handle)->remove(std::vector<long long>(rowids, rowids + count)) ? 1 : 0;
}

int __stdcall ChiaKeyPhrasesReadings(void* handle, const wchar_t* character,
                                     ChiaKeyStringCallback callback) {
    if (!handle || !callback) return 0;
    const auto readings = Store(handle)->readingsForCharacter(ToUtf8(character));
    for (const std::string& reading : readings) callback(ToWide(reading).c_str());
    return static_cast<int>(readings.size());
}

int __stdcall ChiaKeyPhrasesExport(void* handle, const wchar_t* path) {
    return handle && Store(handle)->exportTo(ToUtf8(path)) ? 1 : 0;
}

// 1 with everything, 2 with the phrases but not the learning data, 0 on failure
int __stdcall ChiaKeyPhrasesImport(void* handle, const wchar_t* path) {
    bool learning = false;
    if (!handle || !Store(handle)->importFrom(ToUtf8(path), &learning)) return 0;
    return learning ? 1 : 2;
}

}  // extern "C"
