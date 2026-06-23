// [AUTO_HEADER]

#define OV_USE_SQLITE

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "UnitTest++.h"

using namespace OpenVanilla;

TEST(SQLiteDatabaseService) {
  OVSQLiteConnection* connection = OVSQLiteConnection::Open();
  CHECK(connection);

  if (connection) {
    CHECK_EQUAL(SQLITE_OK,
                connection->execute("create table foobar(key, value);"));
    CHECK_EQUAL(SQLITE_OK, connection->execute(
                               "insert into foobar values('hello', 'world');"));

    OVSQLiteStatement* statement =
        connection->prepare("select * from foobar where key = ?");
    CHECK(statement);

    if (statement) {
      CHECK_EQUAL(SQLITE_OK, statement->bindTextToColumn("hello", 1));

      size_t rows = 0;

      while (statement->step() == SQLITE_ROW) {
        CHECK_EQUAL("world", statement->textOfColumn(1));

        rows++;
      }

      CHECK_EQUAL(1, rows);

      delete statement;
    }

    string property = string(OVPropertyStringInternalPrefix) + "name";
    CHECK_EQUAL(SQLITE_OK,
                connection->execute("insert into foobar values(%Q, 'foobar');",
                                    property.c_str()));

    OVSQLiteDatabaseService* service =
        OVSQLiteDatabaseService::ServiceWithExistingConnection(connection);
    CHECK(service);

    if (service) {
      vector<string> tables = service->tables();
      CHECK_EQUAL(1, tables.size());
      CHECK_EQUAL("foobar", tables[0]);

      OVKeyValueDataTableInterface* kvtable =
          service->createKeyValueDataTableInterface("foobar");
      CHECK(kvtable);

      if (kvtable) {
        vector<string> results = kvtable->valuesForKey("hello");
        CHECK_EQUAL(1, results.size());
        CHECK_EQUAL("world", results[0]);

        vector<pair<string, string> > presults;
        presults = kvtable->valuesForKey(OVWildcard(string("he*")));

        CHECK_EQUAL(1, presults.size());
        CHECK_EQUAL("hello", presults[0].first);
        CHECK_EQUAL("world", presults[0].second);

        string value = kvtable->valueForProperty("name");
        CHECK_EQUAL("foobar", value);

        delete kvtable;
      }

      delete service;
    }

    delete connection;
  }
}
