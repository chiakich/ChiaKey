/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef MJSRLearningCacheTables_h
#define MJSRLearningCacheTables_h

// The learning tables an export block carries. Header-only and free of
// OpenVanilla so both importers -- the C++ one in BPMFUserPhraseHelper and the
// Objective-C one in the phrase editor's PEUserPhraseStore -- restore the same
// set: a store registered in only one of them makes the two paths disagree
// about what a backup replaces.

#include <stddef.h>

namespace Manjusri {

struct LearningCacheTable {
  const char* name;
  const char* columns;
  // Without it INSERT OR REPLACE has nothing to conflict on and quietly
  // appends. Mirrors LanguageModel::MigrateUserLearningTables().
  const char* uniqueIndex;
};

// The two original ones come first so older builds still find the layout they
// expect.
static const LearningCacheTable kLearningCacheTables[] = {
    {"user_bigram_cache", "qstring, previous, current, probability",
     "CREATE UNIQUE INDEX IF NOT EXISTS user_bigram_cache_qstring_unique "
     "ON user_bigram_cache (qstring)"},
    {"user_candidate_override_cache", "qstring, current",
     "CREATE UNIQUE INDEX IF NOT EXISTS "
     "user_candidate_override_cache_qstring_unique "
     "ON user_candidate_override_cache (qstring)"},
    {"user_context_override_cache", "qstring, current",
     "CREATE UNIQUE INDEX IF NOT EXISTS "
     "user_context_override_cache_qstring_unique "
     "ON user_context_override_cache (qstring)"},
    {"user_learning_stats", "store, qstring, selection_count, last_used",
     "CREATE UNIQUE INDEX IF NOT EXISTS user_learning_stats_key "
     "ON user_learning_stats (store, qstring)"},
};

static const size_t kLearningCacheTableCount =
    sizeof(kLearningCacheTables) / sizeof(kLearningCacheTables[0]);

}  // namespace Manjusri

#endif
