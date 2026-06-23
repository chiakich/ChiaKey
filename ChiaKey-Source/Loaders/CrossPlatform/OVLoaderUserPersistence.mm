// [AUTO_HEADER]

#include "OVLoaderUserPersistence.h"

using namespace std;
using namespace OpenVanilla;

// #ifdef OVLOADER_USE_SQLITE_CRYPTO
// pair<char*, size_t> ObtenirUserDonneCle();
// int sqlite3_rekey(sqlite3 *db, const void *pKey, int nKey);
// #endif

OVLoaderUserPersistence::OVLoaderUserPersistence(const string& userDatabasePath)
    : m_defaultDatabase(0), m_userDatabase(0) {
  m_userDatabase = OVSQLiteConnection::Open(userDatabasePath);
  if (m_userDatabase) {
    // pair<char*, size_t> cle = ObtenirUserDonneCle();
    // if (cle.first) {
    //     sqlite3_key(m_userDatabase->connection(), cle.first,
    //     (int)cle.second); free(cle.first);
    // }
  } else {
    m_userDatabase = OVSQLiteConnection::Open(":memory:");
  }

  if (!m_userDatabase->hasTable("data")) {
    m_userDatabase->createTable("data", "key, value");
    m_userDatabase->createIndexOnTable("data_index", "data", "key");
  }
}

OVLoaderUserPersistence::~OVLoaderUserPersistence() { delete m_userDatabase; }

void OVLoaderUserPersistence::setDefaultDatabaseConnection(
    OVSQLiteConnection* connection, const string& tableName) {
  m_defaultDatabase = connection;
  m_defaultDatabaseTableName = tableName;
}

const string OVLoaderUserPersistence::fetchLatestValueByKeyAndPopulateUserDB(
    const string& key) {
  pair<string, time_t> fromDefault, fromUser;

  fromDefault = fetchValueWithTimestampByKey(m_defaultDatabase,
                                             m_defaultDatabaseTableName, key);
  fromUser = fetchValueWithTimestampByKey(m_userDatabase, "data", key);

  if (fromUser.second >= fromDefault.second) {
    return fromUser.first;
  }

  writeValueTimestampByKey(m_userDatabase, "data", key, fromDefault.first,
                           OVDateTimeHelper::GetTimeIntervalSince1970());
  return fromDefault.first;
}

void OVLoaderUserPersistence::populateIfValueDifferentUserDB(
    const string& key, const string& value) {
  pair<string, time_t> fromUser;
  fromUser = fetchValueWithTimestampByKey(m_userDatabase, "data", key);

  if (fromUser.first != value) {
    writeValueTimestampByKey(m_userDatabase, "data", key, value,
                             OVDateTimeHelper::GetTimeIntervalSince1970());
  }
}

pair<string, time_t> OVLoaderUserPersistence::fetchValueWithTimestampByKey(
    OVSQLiteConnection* connection, const string& tableName,
    const string& keyName) {
  pair<string, time_t> result;

  if (!connection) {
    return result;
  }

  OVSQLiteStatement* find = connection->prepare(
      "SELECT value FROM %Q WHERE key = ?", tableName.c_str());

  if (find) {
    find->bindTextToColumn(keyName, 1);
    while (find->step() == SQLITE_ROW) {
      result.first = find->textOfColumn(0);
    }

    find->reset();
    find->bindTextToColumn(keyName + "_timestamp", 1);
    while (find->step() == SQLITE_ROW) {
      result.second = OVDateTimeHelper::GetTimeIntervalSince1970FromString(
          find->textOfColumn(0));
    }
  }

  return result;
}

void OVLoaderUserPersistence::writeValueTimestampByKey(
    OVSQLiteConnection* connection, const string& tableName, const string& key,
    const string& value, time_t timestamp) {
  if (!connection) {
    return;
  }

  string tsk = key + "_timestamp";

  connection->execute("DELETE FROM %Q WHERE key = %Q", tableName.c_str(),
                      key.c_str());
  connection->execute("DELETE FROM %Q WHERE key = %Q", tableName.c_str(),
                      tsk.c_str());

  stringstream sst;
  sst << timestamp;

  connection->execute("INSERT INTO %Q (key, value) VALUES (%Q, %Q)",
                      tableName.c_str(), key.c_str(), value.c_str());
  connection->execute("INSERT INTO %Q (key, value) VALUES (%Q, %Q)",
                      tableName.c_str(), tsk.c_str(), sst.str().c_str());
}
