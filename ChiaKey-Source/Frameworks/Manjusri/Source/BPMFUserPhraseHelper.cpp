/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#include "BPMFUserPhraseHelper.h"

#include <cstring>

#include "MJSRExportCipher.h"
#include "MJSRLearningCacheTables.h"
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

// As in the phrase editor's importer: the file is read whole and copied
// several times over. Overridable for the tests.
#ifndef MJSR_MAX_IMPORT_FILE_SIZE
#define MJSR_MAX_IMPORT_FILE_SIZE (256ULL * 1024 * 1024)
#endif
#ifndef MJSR_MAX_LEARNING_BLOB_SIZE
#define MJSR_MAX_LEARNING_BLOB_SIZE (96UL * 1024 * 1024)
#endif

const unsigned long long kMaxImportFileSize = MJSR_MAX_IMPORT_FILE_SIZE;
const size_t kMaxLearningBlobSize = MJSR_MAX_LEARNING_BLOB_SIZE;

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

// All or nothing: a failure part way through used to leave the caches emptied
// while the import still reported success.
bool RestoreLearningCaches(OVSQLiteConnection* db, const string& exportPath) {
  if (db->execute("ATTACH DATABASE %Q AS export", exportPath.c_str()) !=
      SQLITE_OK)
    return false;

  bool ok = db->execute("BEGIN") == SQLITE_OK;

  for (size_t i = 0; ok && i < kLearningCacheTableCount; ++i) {
    const LearningCacheTable& table = kLearningCacheTables[i];

    // An older file has nothing for the newer stores; keep what is here.
    if (!TableExists(db, "export", table.name)) continue;

    if (!TableExists(db, "main", table.name)) {
      if (db->execute("CREATE TABLE %s (%s)", table.name, table.columns) !=
          SQLITE_OK) {
        ok = false;
        continue;
      }
      // Just created, so no duplicates for the unique key to trip over.
      db->execute("%s", table.uniqueIndex);
    }

    // OR REPLACE: an older or hand-made export may hold duplicates, which
    // used to abort the restore after the DELETE had already run.
    if (db->execute("DELETE FROM %s", table.name) != SQLITE_OK ||
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

  // Bounded before anything is read.
  ifs.seekg(0, ios_base::end);
  streamoff fileSize = ifs.tellg();
  ifs.seekg(0, ios_base::beg);
  if (fileSize < 0 || (unsigned long long)fileSize > kMaxImportFileSize) {
    cerr << "Refusing to import " << filename << ": " << fileSize
         << " bytes exceeds the " << kMaxImportFileSize << " byte limit"
         << endl;
    return false;
  }

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
  bool blobTooLarge = false;
  while (!ifs.eof()) {
    getline(ifs, line);
    // cerr << "hex: " << line << endl;

    if (dbEnd.match(line)) {
      break;
    }

    if (blobTooLarge) continue;

    // Before appending, so one huge line is never held whole either.
    if ((dbHex.size() + line.size()) / 2 > kMaxLearningBlobSize) {
      cerr << "Learning database in " << filename << " exceeds the "
           << kMaxLearningBlobSize << " byte limit; skipping it" << endl;
      blobTooLarge = true;
      dbHex.clear();
      continue;
    }

    dbHex += line;
  }

  // Refusing a block for size is reported; a block we *cannot* read is not.
  bool cacheRestored = !blobTooLarge;

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

  // The phrases are committed either way; this reports the learning data.
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
  if (!select) return false;
  while (select->step() == SQLITE_ROW) {
    // A legacy import can leave NULL columns behind, and textOfColumn() hands
    // back SQLite's raw NULL pointer for those.
    const char* qstringText = select->textOfColumn(0);
    const char* currentText = select->textOfColumn(1);
    const char* probabilityText = select->textOfColumn(2);
    const char* backoffText = select->textOfColumn(3);
    if (!qstringText || !currentText) continue;

    string qstring = qstringText;
    string current = currentText;
    string probability = probabilityText ? probabilityText : "-1.0";
    string backoff = backoffText ? backoffText : "0.0";

    if (OVWildcard::Match(qstring, "*punctuation*") ||
        OVWildcard::Match(qstring, "*passthru*"))
      continue;

    ofs << current << "\t" << BPMFString(qstring) << "\t" << probability << "\t"
        << backoff << endl;
  }

  string cacheExportTempFile = OVDirectoryHelper::GenerateTempFilename();

  // No KEY: macOS's libsqlite3 has a codec, so passing one really encrypted
  // the file, and neither importer can read that back.
  db->execute("ATTACH DATABASE %Q AS export", cacheExportTempFile.c_str());
  for (size_t i = 0; i < kLearningCacheTableCount; ++i) {
    const LearningCacheTable& table = kLearningCacheTables[i];

    db->execute("CREATE TABLE export.%s (%s)", table.name, table.columns);

    // A database predating the table exports it empty, which is what it holds.
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
