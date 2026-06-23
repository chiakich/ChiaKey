/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef OV_USE_SQLITE
#define OV_USE_SQLITE
#endif

#ifndef PHASETOOL_USE_SQLITE_CRYPTO
#define PHASETOOL_USE_SQLITE_CRYPTO
#endif

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "LMDBHelper.h"
#include "Manjusri.h"

//#ifdef PHASETOOL_USE_SQLITE_CRYPTO
// int sqlite3_key(
//   sqlite3 *db,        /* The connection from sqlite3_open() */
//   const void *pKey,   /* The key */
//   int nKey            /* Number of bytes in the key */
//);
//#endif

//#ifdef OVIMSMARTMANDARIN_USE_SQLITE_CRYPTO
// int sqlite3_rekey(sqlite3 *db, const void *pKey, int nKey);
//#endif

using namespace std;
using namespace OpenVanilla;
using namespace Manjusri;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    cerr << "usage: PhraseTool <cooked database> [password]" << endl;
    return 1;
  }

  if (!OVPathHelper::PathExists(argv[1])) {
    cerr << "database doesn't exist: " << argv[1] << "." << endl;
    return 1;
  }

  OVSQLiteConnection* connection = OVSQLiteConnection::Open(argv[1]);
  if (!connection) {
    cerr << "cannot open database: " << argv[1] << endl;
    return 1;
  }

  //    if (argc > 2) {
  //        sqlite3_key(connection->connection(), argv[2], strlen(argv[2]));
  //    }

  LMDBHelper helper(connection, true);

  connection->execute("BEGIN");

  while (!cin.eof()) {
    string line;
    getline(cin, line);

    if (!line.size() || OVWildcard::Match(line, "#*")) continue;

    vector<string> args = OVStringHelper::SplitBySpacesOrTabs(line);

    if (!args.size()) continue;

    string command = args[0];
    args.erase(args.begin());

    if (0) {
    } else if (command == "end") {
      break;
    } else if (command == "version") {
      if (args.size()) {
        // set version
        helper.setVersion(args[0]);
      } else {
        // query version
        cout << "DB version: '" << helper.version() << "'" << endl;
      }
    } else if (command == "hastable") {
      if (args.size()) {
        cout << "hastable \"" << args[0]
             << "\": " << (connection->hasTable(args[0]) ? "true" : "false")
             << endl;
      } else {
        cerr << "usage: hastable <table name>" << endl;
      }
    } else if (command == "occurances") {
      if (args.size()) {
        cout << "phrase: " << args[0] << ", unigram occurances: "
             << helper.countUnigramOccurances(args[0])
             << ", bigram occurances: " << helper.countBigramOccurances(args[0])
             << endl;
      } else {
        cerr << "usage: occurances <phrase>" << endl;
      }
    } else if (command == "occurances+bpmf") {
      if (args.size() >= 2) {
        cout << "phrase: " << args[0] << ", unigram occurances: "
             << helper.countUnigramOccurancesWithBPMF(args[0], args[1])
             << ", bigram occurances: "
             << helper.countBigramOccurancesWithBPMF(args[0], args[1]) << endl;
      } else {
        cerr << "usage: occurances+bpmf <phrase> <bpmf>" << endl;
      }
    } else if (command == "-" || command == "delete") {
      if (args.size()) {
        // if (helper.countUnigramOccurances(args[0]) ||
        // helper.countBigramOccurances(args[0])) {
        helper.removePhrase(args[0]);
        cout << "removed: \"" << args[0] << "\"" << endl;
        // }
        // else {
        //     cerr << "no such phrase to delete: " << args[0] << endl;
        // }
      } else {
        cerr << "usage: delete <phrase>" << endl;
      }
    } else if (command == "-bpmf" || command == "delete+bpmf") {
      if (args.size() >= 2) {
        if (helper.countUnigramOccurances(args[0]) ||
            helper.countBigramOccurances(args[0])) {
          helper.removePhraseWithBPMF(args[0], args[1]);
          cout << "removed: " << args[0] << ", with bpmf: " << args[1] << endl;
        } else {
          cerr << "no such phrase to delete: " << args[0] << endl;
        }
      } else {
        cerr << "usage: delete+bpmf <phrase> <comma-separated-bpmf>" << endl;
      }

    } else if (command == "+bpmf" || command == "add+bpmf") {
      if (args.size() >= 2) {
#warning To do: duplication check
        helper.addPhraseWithBPMF(args[0], args[1]);
        cout << "added phrase: " << args[0] << ", with bpmf: " << args[1]
             << endl;
      } else {
        cerr << "usage: add+bpmf <phrase> <comma-separated-bpmf>" << endl;
      }
    } else if (command == "+qstring" || command == "add+qstring") {
      if (args.size() >= 2) {
#warning To do: duplication check

        string phrase = args[0];
        args.erase(args.begin());
        string qstring = StringVectorHelper::Join(args, " ");
        helper.addPhraseWithQueryString(phrase, qstring);
        cout << "added phrase: " << phrase << ", with qstring: '" << qstring
             << "'" << endl;
      } else {
        cerr << "usage: add+qstring <phrase> <query string>" << endl;
      }
    } else if (command == "demote-lowest") {
      if (args.size() == 1) {
        helper.demotePhraseToLowestProbability(args[0]);
        cout << "demoted phrase: " << args[0] << endl;
      } else {
        cerr << "usage: demote-lowest <phrase>" << endl;
      }
    } else if (command == "promote-highest") {
      if (args.size() == 1) {
        helper.promotePhraseToHighestProbability(args[0]);
        cout << "promoted phrase: " << args[0] << endl;
      } else {
        cerr << "usage: promote-highest <phrase>" << endl;
      }
    } else if (command == "ensure-order") {
      helper.ensurePhraseOrder(args);
    } else if (command == "unigram") {
      helper.showUnigramInfo(args);
    } else if (command == "+qstring+endingspace" ||
               command == "add+qstring+endingspace") {
      if (args.size() >= 2) {
#warning To do: duplication check

        string phrase = args[0];
        args.erase(args.begin());
        string qstring = StringVectorHelper::Join(args, " ") + " ";
        helper.addPhraseWithQueryString(phrase, qstring);
        cout << "added phrase: " << phrase << ", with qstring: '" << qstring
             << "'" << endl;
      } else {
        cerr << "usage: add+qstring+endingspace <phrase> <query string>"
             << endl;
      }
    }

    // else if (command == "+bpmf" || command == "add+bpmf") {
    //     if (args.size() >= 2) {
    //
    //     }
    //     else {
    //         cerr << "usage: add+bpmf <phrase> <comma-separated-bpmf>
    //         [highest/lowest]" << endl;
    //     }
    // }
  }

  connection->execute("COMMIT");

  return 0;
}

// to quiet Manjusri down
pair<char*, size_t> ObtenirUserDonneCle() { return pair<char*, size_t>(0, 0); }
