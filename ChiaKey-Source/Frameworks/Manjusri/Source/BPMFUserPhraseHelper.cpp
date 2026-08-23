/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#include "BPMFUserPhraseHelper.h"

#include <cstring>

#include "MJSRExportCipher.h"
#include "Mandarin.h"
#include "Minotaur.h"
#include "StringVectorHelper.h"

//#ifdef OVIMSMARTMANDARIN_USE_SQLITE_CRYPTO
// using namespace std;
// pair<char*, size_t> ObtenirUserDonneCle();
// int sqlite3_rekey(sqlite3 *db, const void *pKey, int nKey);
//#endif

namespace Manjusri {

using namespace Formosa::Mandarin;

namespace {

// Every table the export block carries. The two original ones come first so a
// file written here keeps the layout older builds expect to read.
struct LearningCacheTable {
  const char* name;
  const char* columns;
};

const LearningCacheTable kLearningCacheTables[] = {
    {"user_bigram_cache", "qstring, previous, current, probability"},
    {"user_candidate_override_cache", "qstring, current"},
    {"user_context_override_cache", "qstring, current"},
    {"user_learning_stats", "store, qstring, selection_count, last_used"},
};

const size_t kLearningCacheTableCount =
    sizeof(kLearningCacheTables) / sizeof(kLearningCacheTables[0]);

bool TableExists(OVSQLiteConnection* db, const char* schema,
                 const char* table) {
  OVSQLiteStatementRef statement = db->prepare(
      "SELECT COUNT(*) FROM %s.sqlite_master WHERE type = 'table' AND "
      "name = %Q",
      schema, table);
  if (!statement) return false;

  bool exists = false;
  while (statement->step() == SQLITE_ROW) exists = statement->intOfColumn(0) > 0;
  return exists;
}

// Replaces the learning tables with the ones in an export database. All or
// nothing: a failure part way through used to leave the caller with its caches
// emptied while the import still reported success.
bool RestoreLearningCaches(OVSQLiteConnection* db, const string& exportPath) {
  if (db->execute("ATTACH DATABASE %Q AS export", exportPath.c_str()) !=
      SQLITE_OK)
    return false;

  bool ok = db->execute("BEGIN") == SQLITE_OK;

  for (size_t i = 0; ok && i < kLearningCacheTableCount; ++i) {
    const LearningCacheTable& table = kLearningCacheTables[i];

    // A file written by an older build has nothing for the newer stores, and
    // keeping what is already there beats replacing it with nothing.
    if (!TableExists(db, "export", table.name)) continue;

    // OR REPLACE because these tables carry a unique key on qstring and an
    // older or hand-made export may hold duplicates -- which used to abort the
    // restore after the DELETE had already run.
    if (db->execute("CREATE TABLE IF NOT EXISTS %s (%s)", table.name,
                    table.columns) != SQLITE_OK ||
        db->execute("DELETE FROM %s", table.name) != SQLITE_OK ||
        db->execute("INSERT OR REPLACE INTO %s (%s) SELECT %s FROM export.%s",
                    table.name, table.columns, table.columns,
                    table.name) != SQLITE_OK) {
      ok = false;
    }
  }

  if (ok) ok = db->execute("COMMIT") == SQLITE_OK;
  if (!ok) db->execute("ROLLBACK");

  db->execute("DETACH DATABASE export");
  return ok;
}

}  // namespace

const pair<string, size_t> BPMFUserPhraseHelper::QString(
    const string& bpmfString) {
  size_t size = 0;
  string result;

  vector<string> bpmfs = OVStringHelper::Split(bpmfString, ',');
  for (vector<string>::const_iterator biter = bpmfs.begin();
       biter != bpmfs.end(); ++biter) {
    BPMF b = BPMF::FromComposedString(*biter);
    if (b.isEmpty()) continue;

    result += b.absoluteOrderString();
    ++size;
  }

  return pair<string, size_t>(result, size);
}

const string BPMFUserPhraseHelper::BPMFString(const string& absString) {
  vector<string> result;
  size_t len = absString.size();
  if (len % 2) return string();

  for (size_t i = 0; i < len; i += 2) {
    string as = absString.substr(i, 2);
    BPMF bpmf = BPMF::FromAbsoluteOrderString(as);
    result.push_back(bpmf.composedString());
  }

  return SVH::Join(result, ",");
}

bool BPMFUserPhraseHelper::Import(OVSQLiteConnection* db,
                                  const string& filename) {
  if (!db) return false;

  ifstream ifs;
  OVFileHelper::OpenIFStream(ifs, filename, ios_base::in);
  if (!ifs.is_open()) return false;

  string line;
  getline(ifs, line);

  // currently accepts only one format
  if (!OVWildcard::Match(line, "*MJSR version 1.0.0*")) {
    return false;
  }

  OVSQLiteStatementRef insert =
      db->prepare("INSERT INTO user_unigrams VALUES(?, ?, ?, ?)");
  OVSQLiteStatementRef select = db->prepare(
      "SELECT * FROM user_unigrams where qstring = ? and current = ?");

  if (db->execute("BEGIN") != SQLITE_OK) {
    return false;
  }

  OVWildcard comment("# *");
  OVWildcard dbBegin("*<database>*");
  OVWildcard dbEnd("*</database>*");
  while (!ifs.eof()) {
    getline(ifs, line);

    if (comment.match(line)) {
      continue;
    }

    // cerr << dbBegin.match(line) << ", " << line << endl;
    if (dbBegin.match(line)) {
      break;
    }

    vector<string> entry = OVStringHelper::SplitBySpacesOrTabs(line);
    if (entry.size() < 4) continue;

    pair<string, size_t> qstring = QString(entry[1]);
    vector<string> currentBlocks =
        OVUTF8Helper::SplitStringByCodePoint(entry[0]);

    if (!qstring.first.size() || !qstring.second ||
        qstring.second != currentBlocks.size())
      continue;

    bool found = false;
    select->reset();
    select->bindTextToColumn(qstring.first, 1);
    select->bindTextToColumn(entry[0], 2);

    while (select->step() == SQLITE_ROW) found = true;

    if (found) continue;

    insert->reset();
    insert->bindTextToColumn(qstring.first, 1);
    insert->bindTextToColumn(entry[0], 2);
    insert->bindTextToColumn(entry[2], 3);
    insert->bindTextToColumn(entry[3], 4);

    // silently ignores error
    insert->step();
  }

  if (db->execute("COMMIT") != SQLITE_OK) {
    return false;
  }

  string dbHex;
  while (!ifs.eof()) {
    getline(ifs, line);
    // cerr << "hex: " << line << endl;

    if (dbEnd.match(line)) {
      break;
    }

    dbHex += line;
  }

  bool cacheRestored = true;

  pair<char*, size_t> binData = Minotaur::Minos::BinaryFromHexString(dbHex);
  if (binData.first) {
    string cacheData(binData.first, binData.second);
    free(binData.first);

    // A block we cannot read (truncated, or encrypted with a key that is not
    // ours) costs the caller its learning cache, not the phrases it just
    // imported above.
    if (DecryptExportDatabase(cacheData)) {
      cacheRestored = false;

      string cacheImportTempFile = OVDirectoryHelper::GenerateTempFilename();
      FILE* f = OVFileHelper::OpenStream(cacheImportTempFile, "wb");
      if (f) {
        fwrite(cacheData.data(), 1, cacheData.size(), f);
        fclose(f);

        cacheRestored = RestoreLearningCaches(db, cacheImportTempFile);
        OVPathHelper::RemoveEverythingAtPath(cacheImportTempFile);
      }
    }
  }

  ifs.close();

  // The phrases above are committed either way; the caller still has to hear
  // that the learning data did not come back.
  return cacheRestored;
}

bool BPMFUserPhraseHelper::Export(OVSQLiteConnection* db,
                                  const string& filename) {
  if (!db) return false;

  ofstream ofs;
  OVFileHelper::OpenOFStream(ofs, filename, ios_base::out);
  if (!ofs.is_open()) return false;

  ofs << "MJSR version 1.0.0" << endl;

  OVSQLiteStatementRef select = db->prepare("SELECT * FROM user_unigrams");
  while (select->step() == SQLITE_ROW) {
    string qstring = select->textOfColumn(0);
    string current = select->textOfColumn(1);
    string probability = select->textOfColumn(2);
    string backoff = select->textOfColumn(3);

    if (OVWildcard::Match(qstring, "*punctuation*") ||
        OVWildcard::Match(qstring, "*passthru*"))
      continue;

    ofs << current << "\t" << BPMFString(qstring) << "\t" << probability << "\t"
        << backoff << endl;
  }

  string cacheExportTempFile = OVDirectoryHelper::GenerateTempFilename();

  // No KEY: macOS's libsqlite3 carries a codec, so passing one really did
  // encrypt the file -- with a scheme neither importer can read, because they
  // attach it without a key and DecryptExportDatabase only knows the legacy
  // KeyKey cipher. Every backup lost its learning data on restore.
  db->execute("ATTACH DATABASE %Q AS export", cacheExportTempFile.c_str());
  for (size_t i = 0; i < kLearningCacheTableCount; ++i) {
    const LearningCacheTable& table = kLearningCacheTables[i];

    db->execute("CREATE TABLE export.%s (%s)", table.name, table.columns);

    // A user database that predates one of these tables exports it empty,
    // which is also what it holds.
    if (!TableExists(db, "main", table.name)) continue;

    db->execute("INSERT INTO export.%s (%s) SELECT %s FROM %s", table.name,
                table.columns, table.columns, table.name);
  }
  db->execute("DETACH DATABASE export");

  pair<char*, size_t> data = OVFileHelper::SlurpFile(cacheExportTempFile);

  if (data.first) {
    ofs << endl;
    ofs << "# What follows is the \"Automatic Learning\" database, do not "
           "remove this"
        << endl;
    ofs << "<database>";
    ofs << hex;

    for (size_t i = 0; i < data.second; i++) {
      if (!(i % 30)) {
        ofs << endl;
      }
      ofs.width(2);
      ofs.fill('0');
      unsigned char c = data.first[i];
      ofs << (unsigned int)c;
    }

    ofs << endl << "</database>" << endl;
  }

  ofs.close();
  OVPathHelper::RemoveEverythingAtPath(cacheExportTempFile);

  return true;
}

OVSQLiteConnection* BPMFUserPhraseHelper::OpenUserPhraseDB(
    OVPathInfo* pathInfo, OVLoaderService* loaderService) {
  OVDirectoryHelper::CheckDirectory(pathInfo->writablePath);

  string filename =
      OVPathHelper::PathCat(pathInfo->writablePath, "SmartMandarinUserData.db");
  loaderService->logger("BPMFUserPhraseHelper")
      << "attempting to open " << filename << endl;
  OVSQLiteConnection* userDB = OVSQLiteConnection::Open(filename);
  if (userDB) {
    // WAL is persistent but must be enabled by the IME as well as by the
    // Phrase Editor: the editor may not have opened a newly created DB yet.
    userDB->execute("PRAGMA journal_mode=WAL");
  }

  //	#ifdef OVIMSMARTMANDARIN_USE_SQLITE_CRYPTO
  //    pair<char*, size_t> cle = ObtenirUserDonneCle();
  //    if (cle.first) {
  //        sqlite3_key(userDB->connection(), cle.first, (int)cle.second);
  //        free(cle.first);
  //    }
  //	#endif

  return userDB;
}

};  // namespace Manjusri
