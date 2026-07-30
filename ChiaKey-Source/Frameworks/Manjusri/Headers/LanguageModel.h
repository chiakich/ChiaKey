/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef LanguageModel_h
#define LanguageModel_h

#include <sys/stat.h>
#include <time.h>

#include <algorithm>
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "OVDependency.h"
#include "STLDependency.h"

#define MANJUSRI_USE_CACHE

namespace Manjusri {
using namespace std;
using namespace OpenVanilla;

template <class KeyType, class ValueType>
class DataCache {
 public:
  DataCache(size_t capacity = 200) : m_capacity(capacity) {}

  typename map<KeyType, ValueType>::iterator find(const KeyType& key) {
    return m_map.find(key);
  }

  typename map<KeyType, ValueType>::iterator begin() { return m_map.begin(); }

  typename map<KeyType, ValueType>::iterator end() { return m_map.end(); }

  void forcePush(const KeyType& key, const ValueType& value) {
    // Re-inserting an existing key must not add a second deque entry: a
    // duplicate would later let eviction erase this (still-live) key from
    // m_map while a stale copy of it lingers in m_deque.
    // std:: qualified because the unqualified name resolves to the member
    // find() above, which takes a key rather than a range.
    typename deque<KeyType>::iterator existing =
        std::find(m_deque.begin(), m_deque.end(), key);
    if (existing != m_deque.end()) {
      m_deque.erase(existing);
    } else if (m_deque.size() >= m_capacity) {
      KeyType oldKey = m_deque.front();
      m_deque.pop_front();
      m_map.erase(oldKey);
    }

    m_map[key] = value;
    m_deque.push_back(key);

    // cerr << "push in new entry, deque size: " << m_deque.size() << ", map
    // size: " << m_map.size() << endl;
  }

  void removeKey(const KeyType& key) { m_map.erase(key); }

  void flush() {
    m_map.clear();
    m_deque.clear();
  }

 protected:
  map<KeyType, ValueType> m_map;
  deque<KeyType> m_deque;
  size_t m_capacity;
};

// The user-learning stores hold the user's own corrections, which -- unlike
// everything in DataCache above -- cannot be recomputed from the lexicon.
// Losing an entry costs the user a re-selection, so this store differs from the
// query caches in three ways: a read refreshes recency, eviction drops the
// least-used entry instead of the oldest, and writes are tracked so they can go
// to disk incrementally rather than by rewriting the whole table.
template <class ValueType>
class LearningStore {
 public:
  struct Entry {
    ValueType value;
    size_t selectionCount;
    time_t lastUsed;
    bool dirty;
    list<string>::iterator recency;
  };

  typedef map<string, Entry> EntryMap;

  LearningStore(size_t capacity = 8000) : m_capacity(capacity) {}

  size_t size() const { return m_map.size(); }
  size_t capacity() const { return m_capacity; }
  void setCapacity(size_t capacity) {
    if (capacity) m_capacity = capacity;
  }

  // A hit means the user typed this reading again, so it counts as recency
  // evidence even if they go on to pick something else.
  const ValueType* fetch(const string& key) {
    typename EntryMap::iterator iter = m_map.find(key);
    if (iter == m_map.end()) return 0;

    touch(iter);
    return &(*iter).second.value;
  }

  const Entry* peek(const string& key) const {
    typename EntryMap::const_iterator iter = m_map.find(key);
    if (iter == m_map.end()) return 0;

    return &(*iter).second;
  }

  // The user deliberately picked this text for this reading.
  void learn(const string& key, const ValueType& value) {
    typename EntryMap::iterator iter = m_map.find(key);
    if (iter != m_map.end()) {
      Entry& entry = (*iter).second;
      entry.value = value;
      // Counts how often this reading needed a correction, not how often this
      // particular text won: a reading the user keeps revisiting is worth
      // keeping either way.
      if (entry.selectionCount < c_maxSelectionCount) entry.selectionCount++;
      entry.dirty = true;
      touch(iter);
      return;
    }

    evictIfFull();

    Entry entry;
    entry.value = value;
    entry.selectionCount = 1;
    entry.lastUsed = time(NULL);
    entry.dirty = true;
    m_recency.push_back(key);
    entry.recency = --m_recency.end();
    m_map[key] = entry;
    m_pendingDeletes.erase(key);
  }

  void remove(const string& key) {
    typename EntryMap::iterator iter = m_map.find(key);
    if (iter == m_map.end()) return;

    m_recency.erase((*iter).second.recency);
    m_map.erase(iter);
    m_pendingDeletes.insert(key);
  }

  // Populating from disk must never clobber newer in-memory state: loadConfig()
  // re-reads the tables every time preferences change. Callers feed rows
  // least-valuable first so the recency order survives a round trip.
  bool loadEntry(const string& key, const ValueType& value,
                 size_t selectionCount, time_t lastUsed) {
    if (m_map.find(key) != m_map.end()) return false;
    if (m_map.size() >= m_capacity) return false;

    Entry entry;
    entry.value = value;
    entry.selectionCount = selectionCount ? selectionCount : 1;
    entry.lastUsed = lastUsed;
    entry.dirty = false;
    m_recency.push_back(key);
    entry.recency = --m_recency.end();
    m_map[key] = entry;
    return true;
  }

  const vector<string> dirtyKeys() const {
    vector<string> results;
    for (typename EntryMap::const_iterator iter = m_map.begin();
         iter != m_map.end(); ++iter)
      if ((*iter).second.dirty) results.push_back((*iter).first);

    return results;
  }

  const set<string>& pendingDeletes() const { return m_pendingDeletes; }

  bool hasPendingWrites() const {
    if (m_pendingDeletes.size()) return true;

    for (typename EntryMap::const_iterator iter = m_map.begin();
         iter != m_map.end(); ++iter)
      if ((*iter).second.dirty) return true;

    return false;
  }

  // Only call once the writes are known to have landed.
  void markClean() {
    m_pendingDeletes.clear();

    for (typename EntryMap::iterator iter = m_map.begin(); iter != m_map.end();
         ++iter)
      (*iter).second.dirty = false;
  }

  void flush() {
    m_map.clear();
    m_recency.clear();
    m_pendingDeletes.clear();
  }

 protected:
  void touch(typename EntryMap::iterator iter) {
    Entry& entry = (*iter).second;
    entry.lastUsed = time(NULL);
    // splice within the same list moves the node and keeps the iterator valid
    m_recency.splice(m_recency.end(), m_recency, entry.recency);
  }

  void evictIfFull() {
    if (m_map.size() < m_capacity) return;

    // Prefer dropping entries the user picked only once, and among those the
    // least recently seen. m_recency runs least- to most-recent, so the first
    // single-selection entry we meet is already the best victim; the scan only
    // runs when the store is full, and only for the one insertion at hand.
    typename EntryMap::iterator victim = m_map.end();
    for (list<string>::iterator riter = m_recency.begin();
         riter != m_recency.end(); ++riter) {
      typename EntryMap::iterator iter = m_map.find(*riter);
      if (iter == m_map.end()) continue;

      if (victim == m_map.end() ||
          (*iter).second.selectionCount < (*victim).second.selectionCount)
        victim = iter;

      if (victim != m_map.end() && (*victim).second.selectionCount <= 1) break;
    }

    if (victim == m_map.end()) return;

    // Memory is the source of truth for these tables, so an eviction has to
    // reach the disk too -- otherwise the table would grow without bound and
    // reload rows we already decided to drop.
    m_pendingDeletes.insert((*victim).first);
    m_recency.erase((*victim).second.recency);
    m_map.erase(victim);
  }

  static const size_t c_maxSelectionCount = 255;

  EntryMap m_map;
  list<string> m_recency;
  set<string> m_pendingDeletes;
  size_t m_capacity;
};

class Bigram {
 public:
  Bigram(const string& pQS = "", const string& pPrev = "",
         const string& pC = "", double pProb = 0.0)
      : queryString(pQS), previous(pPrev), current(pC), probability(pProb) {}

  friend ostream& operator<<(ostream& stream, const Bigram& bigram);

  string queryString;
  string previous;
  string current;
  double probability;
};

inline ostream& operator<<(ostream& stream, const Bigram& bigram) {
  stream << "Bigram '" << bigram.queryString << "', P(" << bigram.current << "|"
         << bigram.previous << ")=" << bigram.probability;
  return stream;
}

class Unigram {
 public:
  Unigram(const string& pQS = "", const string& pC = "", double pProb = 0.0,
          double pB = 0.0)
      : queryString(pQS), current(pC), probability(pProb), backoff(pB) {}

  friend ostream& operator<<(ostream& stream, const Unigram& unigram);

  string queryString;
  string current;
  double probability;
  double backoff;
};

template <class T>
class GramCompare {
 public:
  int operator()(const T& g1, const T& g2) const {
    return g1.probability > g2.probability;
  }
};

inline ostream& operator<<(ostream& stream, const Unigram& unigram) {
  stream << "Unigram '" << unigram.queryString << "', P(" << unigram.current
         << ")=" << unigram.probability << ", BOW('" << unigram.current
         << "')=" << unigram.backoff;
  return stream;
}

typedef vector<Bigram> BigramVector;
typedef vector<Unigram> UnigramVector;

inline ostream& operator<<(ostream& stream, const BigramVector& vec) {
  stream << "bigram (" << vec.size() << " bigrams)";
  for (BigramVector::const_iterator iter = vec.begin(); iter != vec.end();
       ++iter)
    stream << endl << "    " << *iter;
  return stream;
}

inline ostream& operator<<(ostream& stream, const UnigramVector& vec) {
  stream << "unigram (" << vec.size() << " unigrams)";
  for (UnigramVector::const_iterator iter = vec.begin(); iter != vec.end();
       ++iter)
    stream << endl << "    " << *iter;
  return stream;
}

class StringFilter {
 public:
  virtual bool shouldPass(const string& text) = 0;
};

// textOfColumn() hands back SQLite's raw pointer, which is NULL for a NULL
// column -- reachable here because the Phrase Editor's import path fills only
// the columns the old export format carried.
inline const string SafeColumnText(OVSQLiteStatement* statement, int column) {
  const char* text = statement->textOfColumn(column);
  return text ? string(text) : string();
}

// Row shapes for the two-pass load in loadUser*Cache().
struct LoadedBigram {
  string key;
  Bigram value;
  size_t selectionCount;
  time_t lastUsed;
};

struct LoadedOverride {
  string key;
  string value;
  size_t selectionCount;
  time_t lastUsed;
};

class LanguageModel {
 public:
  // owns the connection, owns the externalTable
  // if you have userTable (user_unigrams), it must be attached under the db
  // name "userdb"
  // Learning-store capacities. Measured against a 417k-token LINE corpus: the
  // user needs ~3.7k distinct overrides and ~32k distinct user bigrams, and the
  // old 200-entry stores forced 12k re-selections of things already learned.
  // Overrides get comfortable headroom; bigrams are capped below what perfect
  // recall would need because each entry is much larger in memory.
  static const size_t c_defaultOverrideStoreCapacity = 8000;
  static const size_t c_defaultBigramStoreCapacity = 16000;

  LanguageModel(OVSQLiteConnection* connection,
                OVKeyValueDataTableInterface* externalTable = 0,
                bool useUserTable = false,
                bool combineBigramQueryString = false,
                bool ownsDBConnection = true, bool useUserBigramCache = false,
                bool useUserCandidateOverrideCache = false,
                size_t bigramStoreCapacity = c_defaultBigramStoreCapacity,
                size_t overrideStoreCapacity =
                    c_defaultOverrideStoreCapacity);
  virtual ~LanguageModel();

  virtual const BigramVector findBigrams(const string& queryString,
                                         StringFilter* filter = 0);
  virtual const UnigramVector findUnigrams(const string& queryString,
                                           bool withExternalData = true,
                                           StringFilter* filter = 0);
  virtual const string combineBigramQueryString(const string& previous,
                                                const string& current);
  virtual bool isInDictionary(const string& queryString,
                              bool withExternalData = true,
                              StringFilter* filter = 0);

  virtual bool addUserUnigram(const string& qstring, const string& current);

  virtual const Unigram& UNKUnigram();
  virtual const Unigram& BOSUnigram();
  virtual const Unigram& EOSUnigram();
  virtual const string UNKQueryString();
  virtual const string BOSQueryString();
  virtual const string EOSQueryString();

  virtual void resetQueryCount();
  virtual size_t queryCount();
  virtual size_t cachedQueryCount();

  virtual void flushCache();
  virtual void flushUserCache();

  // @in-research: the candidate-selection cache
  virtual void cacheOverrideSelection(const string& qstring,
                                      const string& current);
  virtual void removeCachedSelection(const string& qstring);
  virtual const string fetchCachedOverrideSelection(const string& qstring);

  virtual void cacheUserBigram(const string& combinedQueryString,
                               const string& previous, const string& current);

  // Brings an existing user database up to what the stores below need: the
  // user_learning_stats side table, and a unique key on qstring so saves can be
  // incremental. Lives here rather than in the IME module because it encodes
  // the same schema the load/save paths depend on. Idempotent.
  static void MigrateUserLearningTables(OVSQLiteConnection* userDB);

  virtual void loadUserBigramCache();
  virtual void saveUserBigramCache(bool useTransaction = true);
  virtual void loadUserCandidateOverrideCache();
  virtual void saveUserCandidateOverrideCache(bool useTransaction = true);
  virtual bool saveUserBigramCacheAndCandidateOverrideCache(
      bool useTransaction = true, bool forced = false);

  // While the Phrase Editor holds its editing-lock file, all writes to the
  // user tables are suspended (see ChiaKeyUserPhraseCoordination.h for the
  // protocol). The lock path is set by the module after it resolves the user
  // data directory.
  virtual void setUserPhraseEditingLockPath(const string& path);
  virtual bool userPhraseWritesSuspended();

 protected:
  virtual double cachedMaxUnigramProbability();

  OVSQLiteConnection* m_connection;
  bool m_ownsDBConnection;
  OVKeyValueDataTableInterface* m_externalUnigramDataTable;

  bool m_cfgUseUserTable;
  bool m_cfgCombineBigramQueryString;
  bool m_cfgUseUserBigramCache;
  bool m_cfgUseUserCandidateOverrideCache;

  OVSQLiteStatement* m_selectBigram;
  OVSQLiteStatement* m_selectUnigram;
  OVSQLiteStatement* m_insertUserUnigram;

  double m_maxUnigramProbability;

  Unigram m_UNK;
  Unigram m_BOS;
  Unigram m_EOS;

  string m_UNKText;
  string m_BOSText;
  string m_EOSText;

  size_t m_queryCount;
  size_t m_cachedQueryCount;

  DataCache<string, UnigramVector> m_unigramCache;
  DataCache<string, BigramVector> m_bigramCache;

  LearningStore<Bigram> m_userBigramCache;
  LearningStore<string> m_candidateOverrideCache;
  OVBenchmark m_userCacheTimer;

  string m_unigramTableName;
  string m_userPhraseEditingLockPath;
};

inline void LanguageModel::setUserPhraseEditingLockPath(const string& path) {
  m_userPhraseEditingLockPath = path;
}

inline bool LanguageModel::userPhraseWritesSuspended() {
  if (!m_userPhraseEditingLockPath.length()) return false;

  struct stat st;
  if (stat(m_userPhraseEditingLockPath.c_str(), &st)) return false;

  // A stale lock means the editor crashed; never suspend writes forever.
  // Keep in sync with ChiaKeyPhraseEditorSessionTimeout.
  const time_t kEditingLockTimeout = 30 * 60;
  return (time(NULL) - st.st_mtime) < kEditingLockTimeout;
}

inline bool UserTableHasColumn(OVSQLiteConnection* userDB, const char* table,
                               const char* column) {
  OVSQLiteStatement* probe =
      userDB->prepare("SELECT %s FROM %s LIMIT 1", column, table);
  if (!probe) return false;

  delete probe;
  return true;
}

// Rebuilds one learning table in its original column shape, moving the
// selection_count/last_used pair an earlier build added into
// user_learning_stats. Shipped ChiaKey builds INSERT into these tables
// positionally, so an extra column makes their learning writes fail outright --
// and because a dev install shares this database with the release install, that
// breakage is not hypothetical. The stats have to live beside the table, not
// inside it. The bundled SQLite (3.6.11) has no DROP COLUMN, hence the rebuild.
inline void RollBackInlineLearningStats(OVSQLiteConnection* userDB,
                                       const char* table, const char* store,
                                       const char* columns) {
  if (!UserTableHasColumn(userDB, table, "selection_count")) return;

  if (userDB->execute("BEGIN") != SQLITE_OK) return;

  userDB->execute(
      "INSERT OR REPLACE INTO user_learning_stats "
      "(store, qstring, selection_count, last_used) "
      "SELECT %Q, qstring, selection_count, last_used FROM %s",
      store, table);
  userDB->execute("CREATE TABLE %s_rebuild (%s)", table, columns);
  userDB->execute("INSERT INTO %s_rebuild SELECT %s FROM %s", table, columns,
                  table);
  userDB->execute("DROP TABLE %s", table);
  userDB->execute("ALTER TABLE %s_rebuild RENAME TO %s", table, table);
  userDB->execute("COMMIT");
}

inline void LanguageModel::MigrateUserLearningTables(
    OVSQLiteConnection* userDB) {
  if (!userDB->hasTable("user_learning_stats")) {
    userDB->createTable("user_learning_stats",
                        "store, qstring, selection_count, last_used");
  }
  userDB->execute(
      "CREATE UNIQUE INDEX IF NOT EXISTS user_learning_stats_key "
      "ON user_learning_stats (store, qstring)");

  RollBackInlineLearningStats(userDB, "user_bigram_cache", "bigram",
                              "qstring, previous, current, probability");
  RollBackInlineLearningStats(userDB, "user_candidate_override_cache",
                              "override", "qstring, current");

  // The old full-table rewrite wrote one row per qstring, but an imported file
  // could carry duplicates; they have to go before a unique index can exist.
  userDB->execute(
      "DELETE FROM user_bigram_cache WHERE rowid NOT IN "
      "(SELECT MAX(rowid) FROM user_bigram_cache GROUP BY qstring)");
  userDB->execute(
      "DELETE FROM user_candidate_override_cache WHERE rowid NOT IN "
      "(SELECT MAX(rowid) FROM user_candidate_override_cache GROUP BY qstring)");

  userDB->execute(
      "CREATE UNIQUE INDEX IF NOT EXISTS user_bigram_cache_qstring_unique "
      "ON user_bigram_cache (qstring)");
  userDB->execute(
      "CREATE UNIQUE INDEX IF NOT EXISTS "
      "user_candidate_override_cache_qstring_unique "
      "ON user_candidate_override_cache (qstring)");
}

inline void LanguageModel::loadUserBigramCache() {
  if (!m_cfgUseUserBigramCache) return;

  // Best entries first so a table larger than the store (an imported file, or a
  // lowered capacity) keeps what the user actually uses; then replayed in
  // reverse so the store's recency order matches what was saved.
  OVSQLiteStatement* statement = m_connection->prepare(
      "SELECT c.qstring, c.previous, c.current, c.probability, "
      "COALESCE(s.selection_count, 1), COALESCE(s.last_used, 0) "
      "FROM user_bigram_cache c LEFT JOIN user_learning_stats s "
      "ON s.store = 'bigram' AND s.qstring = c.qstring "
      "ORDER BY COALESCE(s.selection_count, 1) DESC, "
      "COALESCE(s.last_used, 0) DESC LIMIT %d",
      (int)m_userBigramCache.capacity());
  if (!statement) return;

  vector<LoadedBigram> rows;
  while (statement->step() == SQLITE_ROW) {
    LoadedBigram row;
    row.key = SafeColumnText(statement, 0);
    row.value = Bigram(row.key, SafeColumnText(statement, 1),
                       SafeColumnText(statement, 2),
                       statement->doubleOfColumn(3));
    row.selectionCount = (size_t)statement->intOfColumn(4);
    row.lastUsed = (time_t)statement->intOfColumn(5);
    rows.push_back(row);
  }
  delete statement;

  for (vector<LoadedBigram>::reverse_iterator iter = rows.rbegin();
       iter != rows.rend(); ++iter)
    m_userBigramCache.loadEntry((*iter).key, (*iter).value,
                                (*iter).selectionCount, (*iter).lastUsed);
}

inline void LanguageModel::saveUserBigramCache(bool useTransaction) {
  if (!m_cfgUseUserBigramCache) return;

  if (useTransaction) {
    if (m_connection->execute("BEGIN") != SQLITE_OK) return;
  }

  const set<string>& deletes = m_userBigramCache.pendingDeletes();
  for (set<string>::const_iterator iter = deletes.begin();
       iter != deletes.end(); ++iter) {
    m_connection->execute("DELETE FROM user_bigram_cache WHERE qstring = %Q",
                          (*iter).c_str());
    m_connection->execute(
        "DELETE FROM user_learning_stats WHERE store = 'bigram' AND "
        "qstring = %Q",
        (*iter).c_str());
  }

  vector<string> dirty = m_userBigramCache.dirtyKeys();
  for (vector<string>::iterator iter = dirty.begin(); iter != dirty.end();
       ++iter) {
    const LearningStore<Bigram>::Entry* entry = m_userBigramCache.peek(*iter);
    if (!entry) continue;

    const Bigram& bigram = entry->value;
    m_connection->execute(
        "INSERT OR REPLACE INTO user_bigram_cache "
        "(qstring, previous, current, probability) VALUES(%Q, %Q, %Q, %f)",
        bigram.queryString.c_str(), bigram.previous.c_str(),
        bigram.current.c_str(), bigram.probability);
    m_connection->execute(
        "INSERT OR REPLACE INTO user_learning_stats "
        "(store, qstring, selection_count, last_used) "
        "VALUES('bigram', %Q, %d, %d)",
        (*iter).c_str(), (int)entry->selectionCount, (int)entry->lastUsed);
  }

  if (useTransaction) {
    if (m_connection->execute("COMMIT") != SQLITE_OK) return;
  }

  m_userBigramCache.markClean();
}

inline void LanguageModel::loadUserCandidateOverrideCache() {
  if (!m_cfgUseUserCandidateOverrideCache) return;

  OVSQLiteStatement* statement = m_connection->prepare(
      "SELECT c.qstring, c.current, COALESCE(s.selection_count, 1), "
      "COALESCE(s.last_used, 0) "
      "FROM user_candidate_override_cache c LEFT JOIN user_learning_stats s "
      "ON s.store = 'override' AND s.qstring = c.qstring "
      "ORDER BY COALESCE(s.selection_count, 1) DESC, "
      "COALESCE(s.last_used, 0) DESC LIMIT %d",
      (int)m_candidateOverrideCache.capacity());
  if (!statement) return;

  vector<LoadedOverride> rows;
  while (statement->step() == SQLITE_ROW) {
    LoadedOverride row;
    row.key = SafeColumnText(statement, 0);
    row.value = SafeColumnText(statement, 1);
    row.selectionCount = (size_t)statement->intOfColumn(2);
    row.lastUsed = (time_t)statement->intOfColumn(3);
    rows.push_back(row);
  }
  delete statement;

  for (vector<LoadedOverride>::reverse_iterator iter = rows.rbegin();
       iter != rows.rend(); ++iter)
    m_candidateOverrideCache.loadEntry((*iter).key, (*iter).value,
                                       (*iter).selectionCount,
                                       (*iter).lastUsed);
}

inline void LanguageModel::saveUserCandidateOverrideCache(bool useTransaction) {
  if (!m_cfgUseUserCandidateOverrideCache) return;

  if (useTransaction) {
    if (m_connection->execute("BEGIN") != SQLITE_OK) return;
  }

  const set<string>& deletes = m_candidateOverrideCache.pendingDeletes();
  for (set<string>::const_iterator iter = deletes.begin();
       iter != deletes.end(); ++iter) {
    m_connection->execute(
        "DELETE FROM user_candidate_override_cache WHERE qstring = %Q",
        (*iter).c_str());
    m_connection->execute(
        "DELETE FROM user_learning_stats WHERE store = 'override' AND "
        "qstring = %Q",
        (*iter).c_str());
  }

  vector<string> dirty = m_candidateOverrideCache.dirtyKeys();
  for (vector<string>::iterator iter = dirty.begin(); iter != dirty.end();
       ++iter) {
    const LearningStore<string>::Entry* entry =
        m_candidateOverrideCache.peek(*iter);
    if (!entry) continue;

    m_connection->execute(
        "INSERT OR REPLACE INTO user_candidate_override_cache "
        "(qstring, current) VALUES(%Q, %Q)",
        (*iter).c_str(), entry->value.c_str());
    m_connection->execute(
        "INSERT OR REPLACE INTO user_learning_stats "
        "(store, qstring, selection_count, last_used) "
        "VALUES('override', %Q, %d, %d)",
        (*iter).c_str(), (int)entry->selectionCount, (int)entry->lastUsed);
  }

  if (useTransaction) {
    if (m_connection->execute("COMMIT") != SQLITE_OK) return;
  }

  m_candidateOverrideCache.markClean();
}

inline bool LanguageModel::saveUserBigramCacheAndCandidateOverrideCache(
    bool useTransaction, bool forced) {
  if (!m_cfgUseUserCandidateOverrideCache) return false;

  // Editor session in progress: keep the caches in memory; they will be
  // written at the first save after the session ends.
  if (userPhraseWritesSuspended()) return false;

  if (m_userCacheTimer.elapsedSeconds() < 3.5 && !forced) return false;

  // Writes are incremental now, so an idle save has nothing to do.
  if (!m_userBigramCache.hasPendingWrites() &&
      !m_candidateOverrideCache.hasPendingWrites())
    return false;

  m_userCacheTimer.start();

  if (useTransaction) {
    if (m_connection->execute("BEGIN") != SQLITE_OK) return false;
  }

  saveUserBigramCache(false);
  saveUserCandidateOverrideCache(false);

  if (useTransaction) {
    m_connection->execute("COMMIT");
  }

  return true;
}

inline void LanguageModel::cacheUserBigram(const string& combinedQueryString,
                                           const string& previous,
                                           const string& current) {
  if (m_cfgUseUserBigramCache) {
    m_userBigramCache.learn(combinedQueryString,
                            Bigram(combinedQueryString, previous, current,
                                   cachedMaxUnigramProbability()));
  }
}

inline void LanguageModel::cacheOverrideSelection(const string& qstring,
                                                  const string& current) {
  if (m_cfgUseUserCandidateOverrideCache) {
    m_candidateOverrideCache.learn(qstring, current);
  }
}

inline void LanguageModel::removeCachedSelection(const string& qstring) {
  if (m_cfgUseUserCandidateOverrideCache) {
    m_candidateOverrideCache.remove(qstring);
  }
}

inline const string LanguageModel::fetchCachedOverrideSelection(
    const string& qstring) {
  if (m_cfgUseUserCandidateOverrideCache) {
    const string* cached = m_candidateOverrideCache.fetch(qstring);
    if (cached) return *cached;
  }

  return string();
}

inline LanguageModel::LanguageModel(
    OVSQLiteConnection* connection, OVKeyValueDataTableInterface* externalTable,
    bool useUserTable, bool combineBigramQueryString, bool ownsDBConnection,
    bool useUserBigramCache, bool useUserCandidateOverrideCache,
    size_t bigramStoreCapacity, size_t overrideStoreCapacity)
    : m_connection(connection),
      m_externalUnigramDataTable(externalTable),
      m_cfgUseUserTable(useUserTable),
      m_cfgCombineBigramQueryString(combineBigramQueryString),
      m_selectBigram(0),
      m_selectUnigram(0),
      m_insertUserUnigram(0),
      m_maxUnigramProbability(0.0),
      m_UNKText("*"),
      m_BOSText("!"),
      m_EOSText("$"),
      m_queryCount(0),
      m_cachedQueryCount(0),
      m_ownsDBConnection(ownsDBConnection),
      m_cfgUseUserBigramCache(useUserBigramCache),
      m_cfgUseUserCandidateOverrideCache(useUserCandidateOverrideCache),
      m_userBigramCache(bigramStoreCapacity),
      m_candidateOverrideCache(overrideStoreCapacity),
      m_unigramTableName("unigrams") {
  // see if table 'supplement.unigrams' exists
  OVSQLiteStatement* supplementFind =
      m_connection->prepare("SELECT * FROM supplement.unigrams LIMIT 1");
  if (supplementFind) {
    cerr << "LM: Supplement find" << endl;

    m_unigramTableName = "supplement.unigrams";
    while (supplementFind->step() == SQLITE_ROW)
      ;
    delete supplementFind;
  }

  if (m_cfgUseUserTable)
    m_insertUserUnigram = m_connection->prepare(
        "INSERT INTO userdb.user_unigrams VALUES(?, ?, ?, ?)");

  m_selectBigram = m_connection->prepare("SELECT * FROM bigrams WHERE qstring = ?" /* " ORDER BY previous, probability DESC" */);

  string selectUnigramCommand;
  if (m_cfgUseUserTable) {
    selectUnigramCommand = "SELECT * FROM ";
    selectUnigramCommand += m_unigramTableName;
    selectUnigramCommand +=
        " WHERE qstring = ? UNION SELECT * from userdb.user_unigrams WHERE "
        "qstring = ?";
    /* " ORDER BY probability DESC, current" */ /*, m_unigramTableName.c_str()
                                                 */
  } else {
    selectUnigramCommand = "SELECT * FROM ";
    selectUnigramCommand += m_unigramTableName;
    selectUnigramCommand +=
        " WHERE qstring = ?"; /* " ORDER BY probability DESC, current" */
  }

  // cerr << selectUnigramCommand << endl;
  m_selectUnigram = m_connection->prepare(selectUnigramCommand.c_str());

  // cerr << "m_selectUnigram: " << m_selectUnigram << endl;

  UnigramVector uvec;
  uvec = findUnigrams(m_UNKText, false);
  if (uvec.size()) m_UNK = uvec[0];

  uvec = findUnigrams(m_BOSText, false);
  if (uvec.size()) m_BOS = uvec[0];

  uvec = findUnigrams(m_EOSText, false);
  if (uvec.size()) m_EOS = uvec[0];

  m_userCacheTimer.start();
}

inline LanguageModel::~LanguageModel() {
  delete m_selectUnigram;
  delete m_selectBigram;

  if (m_insertUserUnigram) delete m_insertUserUnigram;

  if (m_ownsDBConnection) delete m_connection;
}

inline const BigramVector LanguageModel::findBigrams(const string& queryString,
                                                     StringFilter* filter) {
  BigramVector results;
  if (!m_selectBigram) return results;

#ifdef MANJUSRI_USE_CACHE
  if (m_cfgUseUserBigramCache) {
    const Bigram* learned = m_userBigramCache.fetch(queryString);
    if (learned) {
      // cerr << "using cached user bigram result for: " << queryString << ",
      // bigram = " << *learned << endl;
      m_cachedQueryCount++;
      results.push_back(*learned);
      return results;
    }
  }

  map<string, BigramVector>::iterator citer = m_bigramCache.find(queryString);
  if (citer != m_bigramCache.end()) {
    m_cachedQueryCount++;
    // cerr << "using cached result for: " << queryString << endl;
    return (*citer).second;
  }
#endif

  m_queryCount++;
  m_selectBigram->reset();
  m_selectBigram->bindTextToColumn(queryString, 1);

  while (m_selectBigram->step() == SQLITE_ROW) {
    if (!filter)
      results.push_back(Bigram(queryString, m_selectBigram->textOfColumn(1),
                               m_selectBigram->textOfColumn(2),
                               m_selectBigram->doubleOfColumn(3)));
    else {
      if (filter->shouldPass(m_selectBigram->textOfColumn(2)))
        results.push_back(Bigram(queryString, m_selectBigram->textOfColumn(1),
                                 m_selectBigram->textOfColumn(2),
                                 m_selectBigram->doubleOfColumn(3)));
    }
  }

  stable_sort(results.begin(), results.end(), GramCompare<Bigram>());

#ifdef MANJUSRI_USE_CACHE
  m_bigramCache.forcePush(queryString, results);
#endif

  return results;
}

inline const UnigramVector LanguageModel::findUnigrams(
    const string& queryString, bool withExternalData, StringFilter* filter) {
  // cerr << "findUnigrams " << (filter ? "has filter" : "has no filter") <<
  // endl;

  if (!m_selectUnigram) return UnigramVector();

#ifdef MANJUSRI_USE_CACHE
  map<string, UnigramVector>::iterator citer = m_unigramCache.find(queryString);
  if (citer != m_unigramCache.end()) {
    m_cachedQueryCount++;
    return (*citer).second;
  }
#endif

  UnigramVector results;

  m_queryCount++;
  m_selectUnigram->reset();
  m_selectUnigram->bindTextToColumn(queryString, 1);
  if (m_cfgUseUserTable) m_selectUnigram->bindTextToColumn(queryString, 2);

  set<string> strset;

  while (m_selectUnigram->step() == SQLITE_ROW) {
    if (!filter)
      results.push_back(Unigram(queryString, m_selectUnigram->textOfColumn(1),
                                m_selectUnigram->doubleOfColumn(2),
                                m_selectUnigram->doubleOfColumn(3)));
    else {
      if (filter->shouldPass(m_selectUnigram->textOfColumn(1)))
        results.push_back(Unigram(queryString, m_selectUnigram->textOfColumn(1),
                                  m_selectUnigram->doubleOfColumn(2),
                                  m_selectUnigram->doubleOfColumn(3)));
    }

    strset.insert(m_selectUnigram->textOfColumn(1));
  }

  stable_sort(results.begin(), results.end(), GramCompare<Unigram>());

  if (withExternalData && m_externalUnigramDataTable) {
    vector<string> externals =
        m_externalUnigramDataTable->valuesForKey(queryString);
    for (vector<string>::iterator iter = externals.begin();
         iter != externals.end(); ++iter)
      if (strset.find(*iter) == strset.end()) {
        strset.insert(*iter);

        if (!filter) {
          // cerr << "no filter passed: " << *iter << endl;
          results.push_back(
              Unigram(queryString, *iter, m_UNK.probability, m_UNK.backoff));
        } else {
          if (filter->shouldPass(*iter)) {
            // cerr << "passed: " << *iter << endl;
            results.push_back(
                Unigram(queryString, *iter, m_UNK.probability, m_UNK.backoff));
          }
        }
      }
  }

  if (!results.size()) {
    // passthru chars must end in this form "_passthru_[char] ", and an ending
    // space must be the last char this is to prevent this kind of combination
    // "_passthru_a _passthru_b "
    if (queryString.size() > 10) {
      if (queryString.substr(0, 10) == "_passthru_") {
        size_t index = 11;

        for (; index < queryString.size(); ++index) {
          if (queryString[index] == ' ') break;
        }

        if (index == queryString.size() - 1) {
          string back = queryString.substr(10, queryString.size() - 11);

          if (back == "space") back = " ";

          results.push_back(
              Unigram(queryString, back, m_UNK.probability, m_UNK.backoff));
        }
      }
    }
  }

#ifdef MANJUSRI_USE_CACHE
  m_unigramCache.forcePush(queryString, results);
#endif

  return results;
}

inline bool LanguageModel::isInDictionary(const string& queryString,
                                          bool withExternalData,
                                          StringFilter* filter) {
  // cerr << "isInDict " << (filter ? "has filter" : "has no filter") << endl;

  UnigramVector univec = findUnigrams(queryString, withExternalData, filter);
  return univec.size() != 0;

  // m_queryCount++;
  // m_selectUnigram->reset();
  // m_selectUnigram->bindTextToColumn(queryString, 1);
  // if (m_cfgUseUserTable)
  //     m_selectUnigram->bindTextToColumn(queryString, 2);
  //
  // bool result = false;
  //
  // while (m_selectUnigram->step() == SQLITE_ROW) {
  //     result = true;
  // }
  //
  // if (!result && withExternalData && m_externalUnigramDataTable) {
  //     vector<string> externals =
  //     m_externalUnigramDataTable->valuesForKey(queryString); result =
  //     externals.size() > 0;
  // }
  //
  // return result;
}

inline const string LanguageModel::combineBigramQueryString(
    const string& previous, const string& current) {
  if (m_cfgCombineBigramQueryString) return previous + current;

  return previous + " " + current;
}

inline const Unigram& LanguageModel::UNKUnigram() { return m_UNK; }

inline const Unigram& LanguageModel::BOSUnigram() { return m_BOS; }

inline const Unigram& LanguageModel::EOSUnigram() { return m_EOS; }

inline const string LanguageModel::UNKQueryString() { return m_UNKText; }

inline const string LanguageModel::BOSQueryString() { return m_BOSText; }

inline const string LanguageModel::EOSQueryString() { return m_EOSText; }

inline void LanguageModel::resetQueryCount() {
  m_queryCount = 0;
  m_cachedQueryCount = 0;
}

inline size_t LanguageModel::queryCount() { return m_queryCount; }

inline size_t LanguageModel::cachedQueryCount() { return m_cachedQueryCount; }

inline bool LanguageModel::addUserUnigram(const string& qstring,
                                          const string& current) {
  if (!m_cfgUseUserTable) return false;
  if (userPhraseWritesSuspended()) return false;

  // check if it's already in either unigram table
  // this is a time-consuming operation anyway, so we'll prepare a statement
  // here
  string selectCommand = "SELECT * FROM ";
  selectCommand += m_unigramTableName;
  selectCommand +=
      " WHERE qstring = ? AND current = ? UNION SELECT * from "
      "userdb.user_unigrams WHERE qstring = ? AND current = ?";
  OVSQLiteStatement* select = m_connection->prepare(selectCommand.c_str());
  if (!select) return false;

  select->bindTextToColumn(qstring, 1);
  select->bindTextToColumn(current, 2);
  select->bindTextToColumn(qstring, 3);
  select->bindTextToColumn(current, 4);
  if (select->step() == SQLITE_ROW) {
    // we have found something, uh-oh. won't add this in
    while (select->step() == SQLITE_ROW) {
    }
    delete select;
    return false;
  }
  delete select;

  if (!m_insertUserUnigram) return false;

  m_insertUserUnigram->reset();
  m_insertUserUnigram->bindTextToColumn(qstring, 1);
  m_insertUserUnigram->bindTextToColumn(current, 2);
  m_insertUserUnigram->bindDoubleToColumn(cachedMaxUnigramProbability(), 3);
  m_insertUserUnigram->bindDoubleToColumn(m_UNK.backoff, 4);
  if (m_insertUserUnigram->step() == SQLITE_DONE)
    ;
  // cerr << "successfully added." << endl;
  else
    ;
  // cerr << "something wrong in insertion" << endl;

  flushCache();
  return true;
}

inline double LanguageModel::cachedMaxUnigramProbability() {
  if (m_maxUnigramProbability != 0.0) return m_maxUnigramProbability;

  string selectMaxCommand = "SELECT MAX(probability) from '";
  selectMaxCommand += m_unigramTableName;
  selectMaxCommand += "'";
  OVSQLiteStatement* selectMax =
      m_connection->prepare(selectMaxCommand.c_str());
  if (selectMax) {
    while (selectMax->step() == SQLITE_ROW) {
      m_maxUnigramProbability = selectMax->doubleOfColumn(0);
      // cerr << "max probability: " << m_maxUnigramProbability << endl;
    }
    delete selectMax;
  }

  // make it non-zero
  if (m_maxUnigramProbability == 0.0) m_maxUnigramProbability = -0.0000001;

  return m_maxUnigramProbability;
}

inline void LanguageModel::flushCache() {
  m_bigramCache.flush();
  m_unigramCache.flush();
}

inline void LanguageModel::flushUserCache() {
  m_userBigramCache.flush();
  m_candidateOverrideCache.flush();

  // The stores are the source of truth for these tables. Now that saves are
  // incremental, dropping the entries in memory no longer empties the table as
  // a side effect, and the next load would bring the discarded learning back.
  if (userPhraseWritesSuspended()) return;

  m_connection->execute("DELETE FROM user_bigram_cache");
  m_connection->execute("DELETE FROM user_candidate_override_cache");
  m_connection->execute("DELETE FROM user_learning_stats");
}
};  // namespace Manjusri

#endif
