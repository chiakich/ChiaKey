// [AUTO_HEADER]

#ifndef OVLoaderUserPersistence_h
#define OVLoaderUserPersistence_h

#ifndef OV_USE_SQLITE
#define OV_USE_SQLITE
#endif

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace OpenVanilla {

class OVLoaderUserPersistence : public OVBase {
 public:
  OVLoaderUserPersistence(const string& userDatabasePath);
  ~OVLoaderUserPersistence();

  void setDefaultDatabaseConnection(OVSQLiteConnection* connection,
                                    const string& tableName);

  const string fetchLatestValueByKeyAndPopulateUserDB(const string& key);
  void populateIfValueDifferentUserDB(const string& key, const string& value);

 protected:
  OVSQLiteConnection* m_defaultDatabase;
  string m_defaultDatabaseTableName;

  OVSQLiteConnection* m_userDatabase;

  pair<string, time_t> fetchValueWithTimestampByKey(
      OVSQLiteConnection* connection, const string& tableName,
      const string& keyName);
  void writeValueTimestampByKey(OVSQLiteConnection* connection,
                                const string& tableName, const string& key,
                                const string& value, time_t timestamp);
};

};  // namespace OpenVanilla

#endif
