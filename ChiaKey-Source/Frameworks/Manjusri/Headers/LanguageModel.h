/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef LanguageModel_h
#define LanguageModel_h

#include <deque>
#include <iostream>
#include <map>
#include <string>

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
    if (m_deque.size() >= m_capacity) {
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

class LanguageModel {
 public:
  // owns the connection, owns the externalTable
  // if you have userTable (user_unigrams), it must be attached under the db
  // name "userdb"
  LanguageModel(OVSQLiteConnection* connection,
                OVKeyValueDataTableInterface* externalTable = 0,
                bool useUserTable = false,
                bool combineBigramQueryString = false,
                bool ownsDBConnection = true, bool useUserBigramCache = false,
                bool useUserCandidateOverrideCache = false);
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

  virtual void loadUserBigramCache();
  virtual void saveUserBigramCache(bool useTransaction = true);
  virtual void loadUserCandidateOverrideCache();
  virtual void saveUserCandidateOverrideCache(bool useTransaction = true);
  virtual bool saveUserBigramCacheAndCandidateOverrideCache(
      bool useTransaction = true, bool forced = false);

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

  DataCache<string, Bigram> m_userBigramCache;
  DataCache<string, string> m_candidateOverrideCache;
  OVBenchmark m_userCacheTimer;

  string m_unigramTableName;
};

inline void LanguageModel::loadUserBigramCache() {
  if (!m_cfgUseUserBigramCache) return;

  OVSQLiteStatement* statement = m_connection->prepare(
      "SELECT qstring, previous, current, probability FROM user_bigram_cache");
  if (!statement) return;

  size_t loaded = 0;
  while (statement->step() == SQLITE_ROW) {
    m_userBigramCache.forcePush(
        statement->textOfColumn(0),
        Bigram(statement->textOfColumn(0), statement->textOfColumn(1),
               statement->textOfColumn(2), statement->doubleOfColumn(3)));
    loaded++;
  }

  // cerr << "loaded bigram cache entries: " << loaded << endl;
  delete statement;
}

inline void LanguageModel::saveUserBigramCache(bool useTransaction) {
  if (!m_cfgUseUserBigramCache) return;

  if (useTransaction) {
    if (m_connection->execute("BEGIN") != SQLITE_OK) return;
  }

  if (m_connection->execute("DELETE FROM user_bigram_cache") == SQLITE_OK) {
    for (map<string, Bigram>::iterator miter = m_userBigramCache.begin();
         miter != m_userBigramCache.end(); ++miter) {
      const Bigram& bigram = (*miter).second;
      m_connection->execute(
          "INSERT INTO user_bigram_cache VALUES(%Q, %Q, %Q, %f)",
          bigram.queryString.c_str(), bigram.previous.c_str(),
          bigram.current.c_str(), bigram.probability);
    }
  }

  if (useTransaction) {
    m_connection->execute("COMMIT");
  }
}

inline void LanguageModel::loadUserCandidateOverrideCache() {
  if (!m_cfgUseUserCandidateOverrideCache) return;

  OVSQLiteStatement* statement = m_connection->prepare(
      "SELECT qstring, current FROM user_candidate_override_cache");
  if (!statement) return;

  size_t loaded = 0;
  while (statement->step() == SQLITE_ROW) {
    m_candidateOverrideCache.forcePush(statement->textOfColumn(0),
                                       statement->textOfColumn(1));
    loaded++;
  }

  // cerr << "loaded candidate override cache entries: " << loaded << endl;
  delete statement;
}

inline void LanguageModel::saveUserCandidateOverrideCache(bool useTransaction) {
  if (!m_cfgUseUserCandidateOverrideCache) return;

  if (useTransaction) {
    if (m_connection->execute("BEGIN") != SQLITE_OK) return;
  }

  if (m_connection->execute("DELETE FROM user_candidate_override_cache") ==
      SQLITE_OK) {
    for (map<string, string>::iterator miter = m_candidateOverrideCache.begin();
         miter != m_candidateOverrideCache.end(); ++miter) {
      m_connection->execute(
          "INSERT INTO user_candidate_override_cache VALUES(%Q, %Q)",
          (*miter).first.c_str(), (*miter).second.c_str());
    }
  }

  if (useTransaction) {
    m_connection->execute("COMMIT");
  }
}

inline bool LanguageModel::saveUserBigramCacheAndCandidateOverrideCache(
    bool useTransaction, bool forced) {
  if (!m_cfgUseUserCandidateOverrideCache) return false;

  if (m_userCacheTimer.elapsedSeconds() < 3.5 && !forced) return false;

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
    m_userBigramCache.forcePush(combinedQueryString,
                                Bigram(combinedQueryString, previous, current,
                                       cachedMaxUnigramProbability()));
  }
}

inline void LanguageModel::cacheOverrideSelection(const string& qstring,
                                                  const string& current) {
  if (m_cfgUseUserCandidateOverrideCache) {
    m_candidateOverrideCache.forcePush(qstring, current);
  }
}

inline void LanguageModel::removeCachedSelection(const string& qstring) {
  if (m_cfgUseUserCandidateOverrideCache) {
    if (m_candidateOverrideCache.find(qstring) !=
        m_candidateOverrideCache.end())
      m_candidateOverrideCache.removeKey(qstring);
  }
}

inline const string LanguageModel::fetchCachedOverrideSelection(
    const string& qstring) {
  if (m_cfgUseUserCandidateOverrideCache) {
    map<string, string>::iterator miter =
        m_candidateOverrideCache.find(qstring);
    if (miter != m_candidateOverrideCache.end()) return (*miter).second;
  }

  return string();
}

inline LanguageModel::LanguageModel(
    OVSQLiteConnection* connection, OVKeyValueDataTableInterface* externalTable,
    bool useUserTable, bool combineBigramQueryString, bool ownsDBConnection,
    bool useUserBigramCache, bool useUserCandidateOverrideCache)
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
    map<string, Bigram>::iterator sbiter = m_userBigramCache.find(queryString);
    if (sbiter != m_userBigramCache.end()) {
      // cerr << "using cached user bigram result for: " << queryString << ",
      // bigram = " << (*sbiter).second << endl;
      m_cachedQueryCount++;
      results.push_back((*sbiter).second);
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
}
};  // namespace Manjusri

#endif
