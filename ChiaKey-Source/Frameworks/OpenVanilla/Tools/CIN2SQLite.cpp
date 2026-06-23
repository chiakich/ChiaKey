// [AUTO_HEADER]

#define OV_USE_SQLITE

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include <iostream>

using namespace OpenVanilla;

int main(int argc, char* argv[]) {
  if (argc < 3) {
    cerr << "usage: CIN2SQLite cin-table sqlite3-db [table-name-prefix]"
         << endl;
    return 1;
  }

  OVCINDataTableParser parser;

  // note that we preserve case sensitivity here!
  OVCINDataTable* table = parser.CINDataTableFromFileName(argv[1], true);

  if (!table) {
    cerr << "cannot open .cin table: " << argv[1]
         << ", error code = " << parser.lastError() << endl;
    return 1;
  }

  OVSQLiteConnection* connection = OVSQLiteConnection::Open(argv[2]);
  if (!connection) {
    cerr << "cannot open database: " << argv[2] << endl;
    return 1;
  }

  string prefix = argc > 3 ? argv[3] : "";
  string tableName = OVPathHelper::FilenameWithoutPath(argv[1]);

  // replace .cin with -cin
  for (string::iterator iter = tableName.begin(); iter != tableName.end();
       iter++)
    if (*iter == '.') *iter = '-';

  int result = 0;

  if (!OVCINToSQLiteConvertor::Convert(table, connection, prefix + tableName)) {
    cerr << "Failed to convert .cin to database" << endl;
    result = 1;
  }

  delete connection;
  delete table;
  return result;
}
