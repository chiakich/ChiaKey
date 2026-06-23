/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#include "BPMFUserPhraseHelper.h"

#include "Mandarin.h"
#include "Minotaur.h"
#include "StringVectorHelper.h"

//#ifdef OVIMSMARTMANDARIN_USE_SQLITE_CRYPTO
// using namespace std;
// pair<char*, size_t> ObtenirUserDonneCle();
// int sqlite3_rekey(sqlite3 *db, const void *pKey, int nKey);
//#endif

#define MANJUSRI_EXPORT_KEY "mjsrexport"

namespace Manjusri {

using namespace Formosa::Mandarin;

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

  OVSQLiteStatement* insert =
      db->prepare("INSERT INTO user_unigrams VALUES(?, ?, ?, ?)");
  OVSQLiteStatement* select = db->prepare(
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

  pair<char*, size_t> binData = Minotaur::Minos::BinaryFromHexString(dbHex);
  if (binData.first) {
    string cacheImportTempFile = OVDirectoryHelper::GenerateTempFilename();
    FILE* f = OVFileHelper::OpenStream(cacheImportTempFile, "wb");
    if (f) {
      fwrite(binData.first, 1, binData.second, f);
      fclose(f);

      int result = 0;
      result = db->execute("ATTACH DATABASE %Q AS export KEY %Q",
                           cacheImportTempFile.c_str(), MANJUSRI_EXPORT_KEY);
      result = db->execute("DELETE FROM user_bigram_cache");
      result = db->execute(
          "INSERT INTO user_bigram_cache (qstring, previous, current, "
          "probability) SELECT qstring, previous, current, probability FROM "
          "export.user_bigram_cache");
      result = db->execute("DELETE FROM user_candidate_override_cache");
      result = db->execute(
          "INSERT INTO user_candidate_override_cache (qstring, current) SELECT "
          "qstring, current FROM export.user_candidate_override_cache");
      result = db->execute("DETACH DATABASE export");
      OVPathHelper::RemoveEverythingAtPath(cacheImportTempFile);
    }
  }

  delete insert;
  delete select;

  ifs.close();
  return true;
}

bool BPMFUserPhraseHelper::Export(OVSQLiteConnection* db,
                                  const string& filename) {
  if (!db) return false;

  ofstream ofs;
  OVFileHelper::OpenOFStream(ofs, filename, ios_base::out);
  if (!ofs.is_open()) return false;

  ofs << "MJSR version 1.0.0" << endl;

  OVSQLiteStatement* select = db->prepare("SELECT * FROM user_unigrams");
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
  delete select;

  string cacheExportTempFile = OVDirectoryHelper::GenerateTempFilename();

  db->execute("ATTACH DATABASE %Q AS export KEY %Q",
              cacheExportTempFile.c_str(), MANJUSRI_EXPORT_KEY);
  db->execute(
      "CREATE TABLE export.user_bigram_cache (qstring, previous, current, "
      "probability)");
  db->execute(
      "CREATE TABLE export.user_candidate_override_cache (qstring, current)");
  db->execute(
      "INSERT INTO export.user_bigram_cache (qstring, previous, current, "
      "probability) SELECT qstring, previous, current, probability FROM "
      "user_bigram_cache");
  db->execute(
      "INSERT INTO export.user_candidate_override_cache (qstring, current) "
      "SELECT qstring, current FROM user_candidate_override_cache");
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
