//
// UserPhraseStore.cpp
//

#include "ChiaKeyCore/UserPhraseStore.h"

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "MJSRExportCipher.h"
#include "MJSRLearningCacheTables.h"
#include "Mandarin.h"

#include <sqlite3.h>

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <sstream>

namespace ChiaKey {
namespace {

using Formosa::Mandarin::BPMF;
using OpenVanilla::OVFileHelper;
using OpenVanilla::OVPathHelper;

const char kUserDatabaseName[] = "SmartMandarinUserData.db";
const char kEditingLockName[] = "SmartMandarinUserData.editing";
const char kDirtyFlagName[] = "SmartMandarinUserData.dirty";
// keep in sync with kEditingLockTimeout in LanguageModel.h
const long long kEditingLockTimeoutSeconds = 30 * 60;
// as the mac editor's limits: a phrase line is 45-90 bytes
const unsigned long long kMaxImportFileSize = 256ULL * 1024 * 1024;
const std::size_t kMaxLearningBlobSize = 96UL * 1024 * 1024;

// qstring (2 bytes per syllable, absolute order) -> "ㄋㄧˇ,ㄏㄠˇ"
std::string ComposedFromQstring(const std::string& qstring) {
  std::string result;
  if (qstring.size() % 2) return result;
  for (std::size_t i = 0; i < qstring.size(); i += 2) {
    if (i) result += ",";
    result += BPMF::FromAbsoluteOrderString(qstring.substr(i, 2)).composedString();
  }
  return result;
}

// "ㄋㄧˇ,ㄏㄠˇ" -> qstring; empty syllables are skipped as the engine does
std::string QstringFromComposed(const std::string& composed) {
  std::string result;
  std::size_t start = 0;
  while (start <= composed.size()) {
    const std::size_t comma = composed.find(',', start);
    const BPMF syllable = BPMF::FromComposedString(composed.substr(
        start, comma == std::string::npos ? std::string::npos : comma - start));
    if (!syllable.isEmpty()) result += syllable.absoluteOrderString();
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return result;
}

std::vector<std::string> CodePoints(const std::string& text) {
  return OpenVanilla::OVUTF8Helper::SplitStringByCodePoint(text);
}

std::string EscapeForLike(const std::string& text) {
  std::string result;
  for (char character : text) {
    if (character == '%' || character == '_' || character == '\\') result += '\\';
    result += character;
  }
  return result;
}

std::string ColumnText(sqlite3_stmt* statement, int column) {
  const unsigned char* text = sqlite3_column_text(statement, column);
  return text ? reinterpret_cast<const char*>(text) : std::string();
}

bool ReadFile(const std::string& path, std::string* contents, unsigned long long limit) {
  std::FILE* stream = OVFileHelper::OpenStream(path, "rb");
  if (!stream) return false;
  contents->clear();
  char buffer[65536];
  std::size_t read = 0;
  bool ok = true;
  while ((read = std::fread(buffer, 1, sizeof(buffer), stream)) > 0) {
    if (contents->size() + read > limit) {
      ok = false;
      break;
    }
    contents->append(buffer, read);
  }
  std::fclose(stream);
  return ok;
}

// another process reading the file must never see half of it
bool WriteFileAtomically(const std::string& path, const std::string& contents) {
  const std::string temporary = path + ".tmp";
  std::FILE* stream = OVFileHelper::OpenStream(temporary, "wb");
  if (!stream) return false;
  const bool written =
      std::fwrite(contents.data(), 1, contents.size(), stream) == contents.size();
  const bool closed = std::fclose(stream) == 0;
  if (!written || !closed) {
    OVPathHelper::RemoveEverythingAtPath(temporary);
    return false;
  }
#if defined(_WIN32)
  return MoveFileExW(OpenVanilla::OVUTF16::FromUTF8(temporary).c_str(),
                     OpenVanilla::OVUTF16::FromUTF8(path).c_str(),
                     MOVEFILE_REPLACE_EXISTING) != 0;
#else
  return std::rename(temporary.c_str(), path.c_str()) == 0;
#endif
}

void RemoveFile(const std::string& path) {
#if defined(_WIN32)
  DeleteFileW(OpenVanilla::OVUTF16::FromUTF8(path).c_str());
#else
  unlink(path.c_str());
#endif
}

// seconds since the last write, or -1 when the file is not there
long long SecondsSinceModified(const std::string& path) {
#if defined(_WIN32)
  WIN32_FILE_ATTRIBUTE_DATA data{};
  if (!GetFileAttributesExW(OpenVanilla::OVUTF16::FromUTF8(path).c_str(),
                            GetFileExInfoStandard, &data)) {
    return -1;
  }
  FILETIME now{};
  GetSystemTimeAsFileTime(&now);
  ULARGE_INTEGER modified{}, current{};
  modified.LowPart = data.ftLastWriteTime.dwLowDateTime;
  modified.HighPart = data.ftLastWriteTime.dwHighDateTime;
  current.LowPart = now.dwLowDateTime;
  current.HighPart = now.dwHighDateTime;
  if (current.QuadPart < modified.QuadPart) return 0;
  return static_cast<long long>((current.QuadPart - modified.QuadPart) / 10000000ULL);
#else
  struct stat info;
  if (stat(path.c_str(), &info) != 0) return -1;
  const long long age = static_cast<long long>(time(nullptr) - info.st_mtime);
  return age < 0 ? 0 : age;
#endif
}

long CurrentProcessId() {
#if defined(_WIN32)
  return static_cast<long>(_getpid());
#else
  return static_cast<long>(getpid());
#endif
}

bool ProcessIsAlive(long pid) {
#if defined(_WIN32)
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                               static_cast<DWORD>(pid));
  // a process we may not open is still running
  if (!process) return GetLastError() == ERROR_ACCESS_DENIED;
  DWORD code = 0;
  const bool alive = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
  CloseHandle(process);
  return alive;
#else
  return kill(static_cast<pid_t>(pid), 0) == 0 || errno != ESRCH;
#endif
}

// Serializes rewrites of the lock's owner list across processes, as
// ChiaKeyLockEditingOwnerList() does on the mac; the system drops it if the
// holder dies.
class OwnerListGuard {
 public:
  explicit OwnerListGuard(const std::string& lockPath) {
    const std::string path = lockPath + ".lock";
#if defined(_WIN32)
    file_ = CreateFileW(OpenVanilla::OVUTF16::FromUTF8(path).c_str(),
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_ != INVALID_HANDLE_VALUE) {
      OVERLAPPED overlapped{};
      if (!LockFileEx(file_, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped)) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
      }
    }
#else
    fd_ = open(path.c_str(), O_RDONLY | O_CREAT | O_NOFOLLOW, 0600);
    if (fd_ >= 0 && flock(fd_, LOCK_EX) != 0) {
      close(fd_);
      fd_ = -1;
    }
#endif
  }

  ~OwnerListGuard() {
#if defined(_WIN32)
    if (file_ != INVALID_HANDLE_VALUE) {
      OVERLAPPED overlapped{};
      UnlockFileEx(file_, 0, 1, 0, &overlapped);
      CloseHandle(file_);
    }
#else
    if (fd_ >= 0) {
      flock(fd_, LOCK_UN);
      close(fd_);
    }
#endif
  }

  OwnerListGuard(const OwnerListGuard&) = delete;
  OwnerListGuard& operator=(const OwnerListGuard&) = delete;

 private:
#if defined(_WIN32)
  HANDLE file_ = INVALID_HANDLE_VALUE;
#else
  int fd_ = -1;
#endif
};

// The lock lists one PID per line, so overlapping sessions keep each other's
// claim; a dead PID is dropped whenever the list is rewritten.
std::vector<long> LiveLockOwners(const std::string& lockPath) {
  std::vector<long> owners;
  std::string contents;
  if (!ReadFile(lockPath, &contents, 1 << 20)) return owners;
  std::istringstream lines(contents);
  std::string line;
  while (std::getline(lines, line)) {
    const long pid = std::atol(line.c_str());
    if (pid <= 0 || !ProcessIsAlive(pid)) continue;
    if (std::find(owners.begin(), owners.end(), pid) == owners.end()) owners.push_back(pid);
  }
  return owners;
}

void WriteLockOwners(const std::string& lockPath, const std::vector<long>& owners) {
  std::ostringstream contents;
  for (std::size_t index = 0; index < owners.size(); ++index) {
    if (index) contents << "\n";
    contents << owners[index];
  }
  WriteFileAtomically(lockPath, contents.str());
}

std::string HexEncode(const std::string& data) {
  static const char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(data.size() * 2 + data.size() / 15);
  for (std::size_t index = 0; index < data.size(); ++index) {
    // 30 bytes per line, as the engine's own export
    if (!(index % 30)) result += '\n';
    const unsigned char byte = static_cast<unsigned char>(data[index]);
    result += digits[byte >> 4];
    result += digits[byte & 0x0f];
  }
  return result;
}

std::string HexDecode(const std::string& hex) {
  std::string result;
  result.reserve(hex.size() / 2);
  unsigned int byte = 0;
  int nibbles = 0;
  for (char character : hex) {
    unsigned int value;
    if (character >= '0' && character <= '9') {
      value = static_cast<unsigned int>(character - '0');
    } else if (character >= 'a' && character <= 'f') {
      value = static_cast<unsigned int>(character - 'a' + 10);
    } else if (character >= 'A' && character <= 'F') {
      value = static_cast<unsigned int>(character - 'A' + 10);
    } else {
      continue;
    }
    byte = (byte << 4) | value;
    if (++nibbles == 2) {
      result += static_cast<char>(byte);
      byte = 0;
      nibbles = 0;
    }
  }
  return result;
}

std::string FilterClause(const std::string& filter, std::vector<std::string>* parameters) {
  if (filter.empty()) return std::string();
  std::string clause = "WHERE (current LIKE ? ESCAPE '\\')";
  parameters->push_back("%" + EscapeForLike(filter) + "%");
  // a filter that reads as Bopomofo also matches readings by whole-syllable prefix
  const std::string qstring = QstringFromComposed(filter);
  if (!qstring.empty()) {
    clause += " OR (qstring LIKE ? ESCAPE '\\')";
    parameters->push_back(EscapeForLike(qstring) + "%");
  }
  return clause;
}

std::string OrderClause(UserPhraseOrder order, bool ascending) {
  const std::string direction = ascending ? "ASC" : "DESC";
  switch (order) {
    case UserPhraseOrder::Phrase:
      return "ORDER BY current " + direction + ", rowid " + direction;
    case UserPhraseOrder::Reading:
      return "ORDER BY qstring " + direction + ", rowid " + direction;
    case UserPhraseOrder::Insertion:
    default:
      return "ORDER BY rowid " + direction;
  }
}

class Statement {
 public:
  Statement(sqlite3* database, const std::string& sql) {
    if (database) sqlite3_prepare_v2(database, sql.c_str(), -1, &statement_, nullptr);
  }
  ~Statement() { sqlite3_finalize(statement_); }
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  explicit operator bool() const { return statement_ != nullptr; }
  sqlite3_stmt* get() const { return statement_; }
  void bind(int index, const std::string& text) {
    sqlite3_bind_text(statement_, index, text.c_str(), -1, SQLITE_TRANSIENT);
  }
  void bind(int index, long long value) { sqlite3_bind_int64(statement_, index, value); }
  int step() { return sqlite3_step(statement_); }
  void reset() { sqlite3_reset(statement_); }

 private:
  sqlite3_stmt* statement_ = nullptr;
};

}  // namespace

class UserPhraseStore::Impl {
 public:
  ~Impl() {
    endEditingSession();
    if (lexicon) sqlite3_close(lexicon);
    if (user) sqlite3_close(user);
  }

  bool open(const std::string& writable, const std::string& lexiconPath,
            std::string* errorMessage) {
    writablePath = writable;
    const std::string userPath = OVPathHelper::PathCat(writablePath, kUserDatabaseName);
    if (sqlite3_open(userPath.c_str(), &user) != SQLITE_OK) {
      if (errorMessage) {
        *errorMessage = "failed to open " + userPath + ": " +
                        (user ? sqlite3_errmsg(user) : "out of memory");
      }
      return false;
    }
    // an editor, not the key path: waiting a little is better than failing an edit
    sqlite3_busy_timeout(user, 3000);
    // lets the engine keep reading while the editor writes; the mode persists
    sqlite3_exec(user, "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr);
    createSchema();

    if (!lexiconPath.empty() &&
        sqlite3_open_v2(lexiconPath.c_str(), &lexicon, SQLITE_OPEN_READONLY, nullptr) !=
            SQLITE_OK) {
      sqlite3_close(lexicon);
      lexicon = nullptr;
    }
    return true;
  }

  // exactly as the engine creates it: it inserts positionally, so no new columns
  void createSchema() {
    sqlite3_exec(user,
                 "CREATE TABLE IF NOT EXISTS user_unigrams "
                 "(qstring, current, probability, backoff);"
                 "CREATE INDEX IF NOT EXISTS user_unigrams_index ON user_unigrams (qstring);"
                 "CREATE INDEX IF NOT EXISTS user_unigrams_current_index "
                 "ON user_unigrams (current);"
                 "CREATE TABLE IF NOT EXISTS user_bigram_cache "
                 "(qstring, previous, current, probability);"
                 "CREATE INDEX IF NOT EXISTS user_bigram_cache_index "
                 "ON user_bigram_cache (qstring);"
                 "CREATE TABLE IF NOT EXISTS user_candidate_override_cache (qstring, current);"
                 "CREATE INDEX IF NOT EXISTS user_candidate_override_cache_index "
                 "ON user_candidate_override_cache (qstring);"
                 "CREATE TABLE IF NOT EXISTS user_context_override_cache (qstring, current);"
                 "CREATE UNIQUE INDEX IF NOT EXISTS user_context_override_cache_qstring_unique "
                 "ON user_context_override_cache (qstring);"
                 "CREATE TABLE IF NOT EXISTS user_learning_stats "
                 "(store, qstring, selection_count, last_used);"
                 "CREATE UNIQUE INDEX IF NOT EXISTS user_learning_stats_key "
                 "ON user_learning_stats (store, qstring);",
                 nullptr, nullptr, nullptr);
    // the unique keys the engine's incremental saves need, once older
    // duplicates are gone; mirrors LanguageModel::MigrateUserLearningTables()
    static const char* const kDeduplicate[] = {
        "DELETE FROM user_bigram_cache WHERE rowid NOT IN "
        "(SELECT MAX(rowid) FROM user_bigram_cache GROUP BY qstring)",
        "DELETE FROM user_candidate_override_cache WHERE rowid NOT IN "
        "(SELECT MAX(rowid) FROM user_candidate_override_cache GROUP BY qstring)",
        "CREATE UNIQUE INDEX IF NOT EXISTS user_bigram_cache_qstring_unique "
        "ON user_bigram_cache (qstring)",
        "CREATE UNIQUE INDEX IF NOT EXISTS user_candidate_override_cache_qstring_unique "
        "ON user_candidate_override_cache (qstring)",
    };
    for (const char* sql : kDeduplicate) sqlite3_exec(user, sql, nullptr, nullptr, nullptr);
  }

  std::string lockPath() const { return OVPathHelper::PathCat(writablePath, kEditingLockName); }

  void beginEditingSession() {
    if (sessionActive) return;
    sessionActive = true;
    OwnerListGuard guard(lockPath());
    std::vector<long> owners;
    const long long age = SecondsSinceModified(lockPath());
    if (age >= 0 && age < kEditingLockTimeoutSeconds) owners = LiveLockOwners(lockPath());
    const long me = CurrentProcessId();
    if (std::find(owners.begin(), owners.end(), me) == owners.end()) owners.push_back(me);
    WriteLockOwners(lockPath(), owners);
  }

  void refreshEditingSession() {
    if (!sessionActive) return;
    OwnerListGuard guard(lockPath());
    std::vector<long> owners = LiveLockOwners(lockPath());
    const long me = CurrentProcessId();
    if (std::find(owners.begin(), owners.end(), me) == owners.end()) owners.push_back(me);
    // rewriting it is what advances the time the engine looks at
    WriteLockOwners(lockPath(), owners);
  }

  void endEditingSession() {
    if (!sessionActive) return;
    sessionActive = false;
    OwnerListGuard guard(lockPath());
    std::vector<long> owners = LiveLockOwners(lockPath());
    owners.erase(std::remove(owners.begin(), owners.end(), CurrentProcessId()), owners.end());
    // another live editor keeps the engine suspended
    if (owners.empty()) {
      RemoveFile(lockPath());
    } else {
      WriteLockOwners(lockPath(), owners);
    }
  }

  void markDirty() {
    std::ostringstream stamp;
    stamp << CurrentProcessId() << " " << ++dirtyGeneration;
    WriteFileAtomically(OVPathHelper::PathCat(writablePath, kDirtyFlagName), stamp.str());
  }

  bool insert(const std::string& phrase, const std::string& qstring) {
    Statement statement(user,
                        "INSERT INTO user_unigrams (qstring, current, probability, backoff) "
                        "VALUES (?, ?, -1.0, 0.0)");
    if (!statement) return false;
    statement.bind(1, qstring);
    statement.bind(2, phrase);
    return statement.step() == SQLITE_DONE;
  }

  std::vector<std::string> readingsForCharacter(const std::string& character) const {
    std::vector<std::string> result;
    std::set<std::string> seen;
    const auto take = [&](const std::string& qstring) {
      const std::string composed = ComposedFromQstring(qstring);
      if (!composed.empty() && seen.insert(composed).second) result.push_back(composed);
    };
    if (lexicon) {
      Statement unigrams(lexicon,
                         "SELECT qstring FROM unigrams WHERE current = ? "
                         "ORDER BY probability DESC");
      if (unigrams) {
        unigrams.bind(1, character);
        while (unigrams.step() == SQLITE_ROW) {
          const std::string qstring = ColumnText(unigrams.get(), 0);
          // special entries such as "xxx#" markers
          if (!qstring.empty() && qstring.back() == '#') continue;
          take(qstring);
        }
      }
      // rare characters missing from unigrams are in the Bopomofo table
      if (result.empty()) {
        Statement table(lexicon, "SELECT key FROM 'Mandarin-bpmf-cin' WHERE value = ?");
        if (table) {
          table.bind(1, character);
          while (table.step() == SQLITE_ROW) take(ColumnText(table.get(), 0));
        }
      }
    }
    if (result.empty()) result.push_back("ㄅ");
    return result;
  }

  std::string defaultReading(const std::string& phrase) const {
    std::string reading;
    for (const std::string& character : CodePoints(phrase)) {
      if (!reading.empty()) reading += ",";
      reading += readingsForCharacter(character).front();
    }
    return reading;
  }

  bool exists(const std::string& qstring, const std::string& phrase) const {
    Statement statement(user,
                        "SELECT 1 FROM user_unigrams WHERE qstring = ? AND current = ? LIMIT 1");
    if (!statement) return false;
    statement.bind(1, qstring);
    statement.bind(2, phrase);
    return statement.step() == SQLITE_ROW;
  }

  bool tableExists(const char* schema, const char* table) const {
    char* sql = sqlite3_mprintf(
        "SELECT COUNT(*) FROM %s.sqlite_master WHERE type = 'table' AND name = %Q", schema,
        table);
    Statement statement(user, sql);
    sqlite3_free(sql);
    return statement && statement.step() == SQLITE_ROW &&
           sqlite3_column_int(statement.get(), 0) > 0;
  }

  bool execute(const char* sql) const {
    return sqlite3_exec(user, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
  }

  // a backup of our own replaces the learning tables wholesale, all or nothing
  bool restoreLearningData(const std::string& path) {
    char* attach = sqlite3_mprintf("ATTACH DATABASE %Q AS export", path.c_str());
    const bool attached = execute(attach);
    sqlite3_free(attach);
    if (!attached) return false;

    bool ok = execute("BEGIN");
    for (std::size_t index = 0; ok && index < Manjusri::kLearningCacheTableCount; ++index) {
      const Manjusri::LearningCacheTable& table = Manjusri::kLearningCacheTables[index];
      // an older file has nothing for the newer stores; keep what is here
      if (!tableExists("export", table.name)) continue;
      if (!tableExists("main", table.name)) {
        char* create = sqlite3_mprintf("CREATE TABLE %s (%s)", table.name, table.columns);
        ok = execute(create);
        sqlite3_free(create);
        if (!ok) break;
        execute(table.uniqueIndex);
      }
      char* remove = sqlite3_mprintf("DELETE FROM %s", table.name);
      ok = execute(remove);
      sqlite3_free(remove);
      if (!ok) break;
      // OR REPLACE: a hand-made file may hold duplicates of the unique key
      char* copy = sqlite3_mprintf("INSERT OR REPLACE INTO %s (%s) SELECT %s FROM export.%s",
                                   table.name, table.columns, table.columns, table.name);
      ok = execute(copy);
      sqlite3_free(copy);
    }
    if (ok) ok = execute("COMMIT");
    if (!ok) execute("ROLLBACK");
    execute("DETACH DATABASE export");
    return ok;
  }

  std::string writablePath;
  sqlite3* user = nullptr;
  sqlite3* lexicon = nullptr;
  bool sessionActive = false;
  unsigned long long dirtyGeneration = 0;
};

std::unique_ptr<UserPhraseStore> UserPhraseStore::Open(const std::string& writablePath,
                                                       const std::string& lexiconDatabasePath,
                                                       std::string* errorMessage) {
  std::unique_ptr<Impl> impl(new Impl);
  if (!impl->open(writablePath, lexiconDatabasePath, errorMessage)) return nullptr;
  return std::unique_ptr<UserPhraseStore>(new UserPhraseStore(std::move(impl)));
}

UserPhraseStore::UserPhraseStore(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

UserPhraseStore::~UserPhraseStore() {}

void UserPhraseStore::beginEditingSession() { impl_->beginEditingSession(); }
void UserPhraseStore::refreshEditingSession() { impl_->refreshEditingSession(); }
void UserPhraseStore::endEditingSession() { impl_->endEditingSession(); }

std::size_t UserPhraseStore::count(const std::string& filter) const {
  std::vector<std::string> parameters;
  Statement statement(impl_->user,
                      "SELECT COUNT(*) FROM user_unigrams " + FilterClause(filter, &parameters));
  if (!statement) return 0;
  for (std::size_t index = 0; index < parameters.size(); ++index) {
    statement.bind(static_cast<int>(index) + 1, parameters[index]);
  }
  return statement.step() == SQLITE_ROW
             ? static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 0))
             : 0;
}

std::vector<UserPhrase> UserPhraseStore::phrases(const std::string& filter,
                                                 UserPhraseOrder order, bool ascending,
                                                 std::size_t offset,
                                                 std::size_t limit) const {
  std::vector<UserPhrase> result;
  std::vector<std::string> parameters;
  Statement statement(impl_->user, "SELECT rowid, qstring, current FROM user_unigrams " +
                                       FilterClause(filter, &parameters) + " " +
                                       OrderClause(order, ascending) + " LIMIT ? OFFSET ?");
  if (!statement) return result;
  int index = 1;
  for (const std::string& parameter : parameters) statement.bind(index++, parameter);
  statement.bind(index++, static_cast<long long>(limit));
  statement.bind(index++, static_cast<long long>(offset));
  while (statement.step() == SQLITE_ROW) {
    UserPhrase phrase;
    phrase.rowid = sqlite3_column_int64(statement.get(), 0);
    phrase.reading = ComposedFromQstring(ColumnText(statement.get(), 1));
    phrase.phrase = ColumnText(statement.get(), 2);
    result.push_back(phrase);
  }
  return result;
}

bool UserPhraseStore::contains(const std::string& phrase) const {
  Statement statement(impl_->user, "SELECT 1 FROM user_unigrams WHERE current = ? LIMIT 1");
  if (!statement) return false;
  statement.bind(1, phrase);
  return statement.step() == SQLITE_ROW;
}

bool UserPhraseStore::add(const std::string& phrase, const std::string& reading,
                          UserPhrase* added) {
  if (phrase.empty()) return false;
  const std::string composed = reading.empty() ? impl_->defaultReading(phrase) : reading;
  const std::string qstring = QstringFromComposed(composed);
  if (qstring.empty() || qstring.size() / 2 != CodePoints(phrase).size()) return false;
  if (!impl_->insert(phrase, qstring)) return false;
  if (added) {
    added->rowid = sqlite3_last_insert_rowid(impl_->user);
    added->phrase = phrase;
    added->reading = ComposedFromQstring(qstring);
  }
  impl_->markDirty();
  return true;
}

bool UserPhraseStore::setPhrase(long long rowid, const std::string& phrase) {
  if (phrase.empty()) return false;
  Statement statement(impl_->user,
                      "UPDATE user_unigrams SET qstring = ?, current = ? WHERE rowid = ?");
  if (!statement) return false;
  statement.bind(1, QstringFromComposed(impl_->defaultReading(phrase)));
  statement.bind(2, phrase);
  statement.bind(3, rowid);
  const bool ok = statement.step() == SQLITE_DONE;
  impl_->markDirty();
  return ok;
}

bool UserPhraseStore::setReading(long long rowid, const std::string& reading) {
  const std::string qstring = QstringFromComposed(reading);
  if (qstring.empty()) return false;
  Statement statement(impl_->user, "UPDATE user_unigrams SET qstring = ? WHERE rowid = ?");
  if (!statement) return false;
  statement.bind(1, qstring);
  statement.bind(2, rowid);
  const bool ok = statement.step() == SQLITE_DONE;
  impl_->markDirty();
  return ok;
}

bool UserPhraseStore::remove(const std::vector<long long>& rowids) {
  if (rowids.empty()) return true;
  Statement statement(impl_->user, "DELETE FROM user_unigrams WHERE rowid = ?");
  if (!statement || !impl_->execute("BEGIN")) return false;
  for (long long rowid : rowids) {
    statement.reset();
    statement.bind(1, rowid);
    statement.step();
  }
  const bool ok = impl_->execute("COMMIT");
  if (!ok) impl_->execute("ROLLBACK");
  impl_->markDirty();
  return ok;
}

std::vector<std::string> UserPhraseStore::readingsForCharacter(
    const std::string& character) const {
  return impl_->readingsForCharacter(character);
}

std::string UserPhraseStore::defaultReading(const std::string& phrase) const {
  return impl_->defaultReading(phrase);
}

bool UserPhraseStore::exportTo(const std::string& path) const {
  std::string out = "MJSR version 1.0.0\n";
  Statement statement(impl_->user,
                      "SELECT qstring, current, probability, backoff FROM user_unigrams");
  if (!statement) return false;
  while (statement.step() == SQLITE_ROW) {
    const std::string qstring = ColumnText(statement.get(), 0);
    const std::string current = ColumnText(statement.get(), 1);
    if (current.empty()) continue;
    // the same exclusions as BPMFUserPhraseHelper::Export
    if (qstring.find("punctuation") != std::string::npos ||
        qstring.find("passthru") != std::string::npos) {
      continue;
    }
    std::string probability = ColumnText(statement.get(), 2);
    std::string backoff = ColumnText(statement.get(), 3);
    out += current + "\t" + ComposedFromQstring(qstring) + "\t" +
           (probability.empty() ? "-1.0" : probability) + "\t" +
           (backoff.empty() ? "0.0" : backoff) + "\n";
  }

  // The learning tables travel as a hex-encoded side database; no KEY, which
  // a codec-enabled SQLite would turn into real encryption.
  const std::string temporary = OpenVanilla::OVDirectoryHelper::GenerateTempFilename();
  char* sql = sqlite3_mprintf("ATTACH DATABASE %Q AS export", temporary.c_str());
  const bool attached = impl_->execute(sql);
  sqlite3_free(sql);
  if (attached) {
    for (std::size_t index = 0; index < Manjusri::kLearningCacheTableCount; ++index) {
      const Manjusri::LearningCacheTable& table = Manjusri::kLearningCacheTables[index];
      char* create =
          sqlite3_mprintf("CREATE TABLE export.%s (%s)", table.name, table.columns);
      impl_->execute(create);
      sqlite3_free(create);
      if (!impl_->tableExists("main", table.name)) continue;
      char* copy = sqlite3_mprintf("INSERT INTO export.%s (%s) SELECT %s FROM %s", table.name,
                                   table.columns, table.columns, table.name);
      impl_->execute(copy);
      sqlite3_free(copy);
    }
    impl_->execute("DETACH DATABASE export");
    std::string learning;
    if (ReadFile(temporary, &learning, kMaxImportFileSize) && !learning.empty()) {
      out +=
          "\n# What follows is the \"Automatic Learning\" database, do not remove this\n"
          "<database>" +
          HexEncode(learning) + "\n</database>\n";
    }
  }
  OVPathHelper::RemoveEverythingAtPath(temporary);
  return WriteFileAtomically(path, out);
}

bool UserPhraseStore::importFrom(const std::string& path, bool* learningDataRestored) {
  if (learningDataRestored) *learningDataRestored = false;
  std::string contents;
  if (!ReadFile(path, &contents, kMaxImportFileSize)) return false;

  std::istringstream lines(contents);
  std::string line;
  if (!std::getline(lines, line) || line.find("MJSR version 1.0.0") == std::string::npos) {
    return false;
  }
  Statement insert(impl_->user, "INSERT INTO user_unigrams VALUES (?, ?, ?, ?)");
  if (!insert || !impl_->execute("BEGIN")) return false;

  std::string hex;
  bool sawDatabase = false;
  bool blobTooLarge = false;
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (sawDatabase) {
      if (line.find("</database>") != std::string::npos) break;
      if (blobTooLarge) continue;
      if ((hex.size() + line.size()) / 2 > kMaxLearningBlobSize) {
        blobTooLarge = true;
        hex.clear();
        continue;
      }
      hex += line;
      continue;
    }
    if (!line.empty() && line[0] == '#') continue;
    if (line.find("<database>") != std::string::npos) {
      sawDatabase = true;
      continue;
    }
    const std::vector<std::string> fields =
        OpenVanilla::OVStringHelper::SplitBySpacesOrTabs(line);
    if (fields.size() < 2) continue;
    const std::string& phrase = fields[0];
    const std::string qstring = QstringFromComposed(fields[1]);
    // the reading has to cover the phrase character for character
    if (qstring.empty() || qstring.size() / 2 != CodePoints(phrase).size()) continue;
    if (impl_->exists(qstring, phrase)) continue;
    insert.reset();
    insert.bind(1, qstring);
    insert.bind(2, phrase);
    insert.bind(3, fields.size() > 2 ? fields[2] : std::string("-1.0"));
    insert.bind(4, fields.size() > 3 ? fields[3] : std::string("0.0"));
    insert.step();
  }
  if (!impl_->execute("COMMIT")) {
    impl_->execute("ROLLBACK");
    return false;
  }

  // a block too large is reported; one we cannot read costs only the learning
  bool restored = !blobTooLarge;
  if (sawDatabase) {
    std::string blob = HexDecode(hex);
    // Yahoo! KeyKey's blocks are encrypted, ours are not; anything else is dropped
    if (!Manjusri::DecryptExportDatabase(blob)) blob.clear();
    if (!blob.empty()) {
      restored = false;
      const std::string temporary = OpenVanilla::OVDirectoryHelper::GenerateTempFilename();
      if (WriteFileAtomically(temporary, blob)) restored = impl_->restoreLearningData(temporary);
      OVPathHelper::RemoveEverythingAtPath(temporary);
    }
  }
  impl_->markDirty();
  if (learningDataRestored) *learningDataRestored = restored;
  return true;
}

}  // namespace ChiaKey
