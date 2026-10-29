/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef MJSRUserPhrasePrior_h
#define MJSRUserPhrasePrior_h

// What a hand-added phrase is worth, shared by the engine and both phrase
// editors so a phrase scores the same however it was added. Header-only and on
// plain sqlite3 for the same reason as MJSRLearningCacheTables.h.
//
// The value is the lexicon's 99th-percentile probability over real words.
// Measured on the 18,425-sentence LINE gold set (2026-10-29): at the maximum
// (+0.001) 100 realistic adds won 98.7% of their intended spans but tripled
// the wrong answers at other positions with the same reading (33 -> 107); at
// p99 (-0.89) they still won 96.7%, collateral fell to 73, and total exact
// matches were the highest of any weight tried. See Docs/UserPhrasePrior.md.

#include <sqlite3.h>

#include <string>

namespace Manjusri {

// What a lexicon with no real words (test fixtures) falls back to; also the
// value the editors used to write.
static const double kFallbackUserPhrasePrior = -1.0;

// `unigramTable` may be schema-qualified ("supplement.unigrams"), so it is
// never quoted: SQLite would read the quoted form as one table name.
inline double UserPhrasePriorProbability(sqlite3* db,
                                         const std::string& unigramTable) {
  double prior = 0.0;
  if (!db) return kFallbackUserPhrasePrior;

  // Over real words only: punctuation and marker rows sit at 0 or above.
  long long count = 0;
  sqlite3_stmt* statement = NULL;
  std::string sql =
      "SELECT COUNT(*) FROM " + unigramTable + " WHERE probability < 0";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, NULL) == SQLITE_OK &&
      sqlite3_step(statement) == SQLITE_ROW) {
    count = sqlite3_column_int64(statement, 0);
  }
  sqlite3_finalize(statement);

  if (count > 0) {
    statement = NULL;
    sql = "SELECT probability FROM " + unigramTable +
          " WHERE probability < 0 ORDER BY probability DESC LIMIT 1 OFFSET ?";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, NULL) ==
        SQLITE_OK) {
      sqlite3_bind_int64(statement, 1, count / 100);
      if (sqlite3_step(statement) == SQLITE_ROW)
        prior = sqlite3_column_double(statement, 0);
    }
    sqlite3_finalize(statement);
  }

  return prior < 0.0 ? prior : kFallbackUserPhrasePrior;
}

// Moves phrases earlier builds wrote onto `prior`. Those weights were never a
// user's choice: the engine wrote the lexicon maximum (0.001, or -1e-7 where
// the maximum was 0) and the editors and legacy imports wrote -1.0. No real
// word sits above -1e-6, and an estimated weight landing on exactly -1.0 is not
// worth telling apart. Some importers bound the value as text, hence the CAST.
inline bool MigrateUserPhrasePriors(sqlite3* db, const std::string& userTable,
                                    double prior) {
  if (!db || prior >= 0.0) return false;
  sqlite3_stmt* statement = NULL;
  const std::string sql = "UPDATE " + userTable +
                          " SET probability = ? WHERE "
                          "CAST(probability AS REAL) > -0.0000005 OR "
                          "CAST(probability AS REAL) = -1.0";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, NULL) != SQLITE_OK) {
    sqlite3_finalize(statement);
    return false;
  }
  sqlite3_bind_double(statement, 1, prior);
  const bool ok = sqlite3_step(statement) == SQLITE_DONE;
  sqlite3_finalize(statement);
  return ok;
}

}  // namespace Manjusri

#endif
