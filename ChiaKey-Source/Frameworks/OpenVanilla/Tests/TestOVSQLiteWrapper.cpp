// [AUTO_HEADER]

#define OV_USE_SQLITE

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "UnitTest++.h"

using namespace OpenVanilla;

TEST(SQLiteWrapper) {
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
        // cout << statement->textOfColumn(0) << ", " <<
        // statement->textOfColumn(1) << endl;
        CHECK_EQUAL("world", statement->textOfColumn(1));

        rows++;
      }

      CHECK_EQUAL(1, rows);

      delete statement;
    }

    delete connection;
  }
}
