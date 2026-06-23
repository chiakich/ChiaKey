/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#define OV_USE_SQLITE
#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include <iostream>

#include "BPMFUserPhraseHelper.h"
#include "Graph.h"
#include "LanguageModel.h"
#include "Mandarin.h"
#include "StringVectorHelper.h"

using namespace std;
using namespace OpenVanilla;
using namespace Formosa::Mandarin;
using namespace Manjusri;

int main(int argc, char* argv[]) {
  bool cfgCombineQueryString = false;
  string cfgExternalTableName = "Manjusri-bpmf-cin";

  if (argc < 2) {
    cerr << "usage: ManjusriBuilder lm [external-db] [user-phrase-db]" << endl;
    return 1;
  }

  OVSQLiteConnection* lmDB = OVSQLiteConnection::Open(argv[1]);
  if (!lmDB) {
    cerr << "cannot open: " << argv[1] << endl;
    return 1;
  } else {
    cerr << "using lm: " << argv[1] << endl;
  }

  OVKeyValueDataTableInterface* externalBPMFTable = 0;
  OVSQLiteConnection* bpmf = 0;

  if (argc > 2) {
    bpmf = OVSQLiteConnection::Open(argv[2]);

    if (bpmf) {
      OVSQLiteDatabaseService* dbService =
          OVSQLiteDatabaseService::ServiceWithExistingConnection(bpmf);
      if (dbService) {
        if (bpmf->hasTable(cfgExternalTableName)) {
          externalBPMFTable =
              dbService->createKeyValueDataTableInterface(cfgExternalTableName);
          if (externalBPMFTable) {
            cerr << "using external bpmf table: " << argv[2]
                 << ", table name: " << cfgExternalTableName << endl;
          }
        }
      }
    }
  }

  if (!externalBPMFTable) cerr << "no external table is being used" << endl;

  bool useUserTable = false;

  if (argc > 3) {
    if (lmDB->execute("ATTACH DATABASE %Q AS userdb", argv[3]) == SQLITE_OK)
      useUserTable = true;
  }

  if (useUserTable)
    cerr << "using user table: " << argv[3] << endl;
  else
    cerr << "no user table is being used" << endl;

  LanguageModel* lm = new LanguageModel(lmDB, externalBPMFTable, useUserTable,
                                        cfgCombineQueryString);
  Node::SetUNK(lm->UNKUnigram().probability, lm->UNKUnigram().backoff);

  cerr << "interactive mode, openvanilla version = "
       << OVFrameworkInfo::VersionString() << endl;

  Graph graph(lm);

  while (!cin.eof()) {
    cout << "manjusri> ";
    string line;
    getline(cin, line);

    StringVector args = OVStringHelper::SplitBySpacesOrTabs(line);
    if (!args.size()) continue;

    string cmd = args[0];
    args = SVH::SubVector(args.begin() + 1, args.end());

    if (cmd == "end" || cmd == "quit") {
      cout << "ened." << endl;
      break;
    } else if (cmd == "help") {
      cout << "commands:" << endl;
      cout << "  stat" << endl;
      cout
          << "  std2abs : convert standard keyboard layout bpmf to internal rep"
          << endl;
      cout << "  unigram <query> : query unigrams (use internal form)" << endl;
      cout
          << "  bigram <previous> <current> : query bigrams (use internal form)"
          << endl;
      cout << "  clear : clear the graph" << endl;
      cout << "  build <syllable1> <syllable2> ... : build a graph" << endl;
      cout << "  graph : dump the current graph" << endl;
      cout << "  walk [n]: walk the graph and list the top n paths, default is "
              "5, 0 uses aggressive"
           << endl;
      cout << "  dotwalk [n]: generate a dot by walking the graph, cf. walk"
           << endl;
      cout << "  fastwalk: uses the aggressive fastwalk algorithm" << endl;
      cout << "  qblocks : list current query blocks in the graph" << endl;
      cout << "  insert <syllable> <position> : insert a new syllable at "
              "position"
           << endl;
      cout << "  candidate <position> [preceding] : list candidates available "
              "at a qblock position"
           << endl;
      cout << "  override <pos> <length> <qstring> <candidate index> : "
              "override a block's candidate"
           << endl;
      cout << "  delete <position> : delete the qblock at position" << endl;
      cout << "  add <qstring> <current> <probability> <backoff> : add a new "
              "unigram to user table"
           << endl;
      cout << "  pop : pop the first text block" << endl;
      cout << "  export <filename> : export user DB to file" << endl;
      cout << "  import <filename> : import phrases to user DB" << endl;
      cout << "  flush : flush the LM cache" << endl;
    } else if (cmd == "stat") {
      cout << "accumulated lm SQLite query count: " << lm->queryCount()
           << " (use clear command to reset)" << endl;
      cout << "latest graph build tme: " << graph.lastBuildTime() << " secs"
           << endl;
      cout << "latest graph walk time: " << graph.lastWalkTime() << " secs"
           << endl;
    } else if (cmd == "std2abs") {
      const BopomofoKeyboardLayout* layout =
          BopomofoKeyboardLayout::StandardLayout();
      cout << "standard bpmf layout -> absolute string: ";
      for (StringVector::iterator iter = args.begin(); iter != args.end();
           ++iter) {
        cout << layout->syllableFromKeySequence(*iter).absoluteOrderString()
             << " ";
      }
      cout << endl;
    } else if (cmd == "unigram") {
      UnigramVector vec = lm->findUnigrams(args[0]);
      cout << vec;
      cout << endl;
    } else if (cmd == "bigram") {
      if (args.size() >= 2) {
        BigramVector vec =
            lm->findBigrams(lm->combineBigramQueryString(args[0], args[1]));
        cout << vec << endl;
      } else {
        cerr << "must be a bigram separated by space" << endl;
      }
    } else if (cmd == "clear") {
      lm->resetQueryCount();
      graph.clear();
      cout << "graph cleared." << endl;
    } else if (cmd == "graph") {
      cout << graph << endl;
    } else if (cmd == "qblocks") {
      StringVector vec = graph.queryBlocks();
      size_t counter = 0;
      for (StringVector::iterator iter = vec.begin(); iter != vec.end();
           ++iter) {
        cout << counter++ << ": " << *iter << endl;
      }
    } else if (cmd == "walk") {
      size_t max = 5;
      if (args.size()) max = atoi(args[0].c_str());

      bool aggressive = false;
      if (!max) {
        aggressive = true;
        max = 5;
      }

      vector<Path> paths = graph.walk("", Location(0, 0), aggressive);
      sort(paths.begin(), paths.end(), PathCompare());

      if (!paths.size())
        cout << "no path!" << endl;
      else {
        vector<Path> selection;
        selection.insert(selection.begin(), paths.begin(),
                         (paths.begin() + max) > paths.end()
                             ? paths.end()
                             : paths.begin() + max);
        cout << "best " << max << " paths (total " << paths.size()
             << "): " << endl
             << selection;
      }

      cout << endl << "walk time: " << graph.lastWalkTime() << " secs" << endl;
    } else if (cmd == "dotwalk") {
      size_t max = 5;
      if (args.size()) max = atoi(args[0].c_str());

      bool aggressive = false;
      if (!max) {
        aggressive = true;
        max = 5;
      }

      vector<Path> paths = graph.walk("", Location(0, 0), aggressive);
      sort(paths.begin(), paths.end(), PathCompare());

      if (!paths.size()) {
        cout << "no path!" << endl;
      } else {
        vector<Path> selection;
        selection.insert(selection.begin(), paths.begin(),
                         (paths.begin() + max) > paths.end()
                             ? paths.end()
                             : paths.begin() + max);

        char label = 'a';
        for (vector<Path>::iterator si = selection.begin();
             si != selection.end(); ++si) {
          Path& p = *si;

          cout << "bos -> ";

          for (Path::const_iterator iter = p.begin(); iter != p.end() - 1;
               ++iter) {
            const string& s = (*iter).first.first;
            if (s.length()) {
              cout << s << " -> ";
            }
          }

          cout << "eos";
          cout << " [";
          if (label == 'a') {
            cout << "style=bold,";
          }
          cout << "label=" << label << "];";
          cout << endl;

          label++;
        }

        // cout << "best " << max << " paths (total " << paths.size() << "): "
        // << endl << selection;
      }

      // cout << endl << "walk time: " << graph.lastWalkTime() << " secs" <<
      // endl;
    } else if (cmd == "fastwalk") {
      FastPath path = graph.fastWalk("", Location(0, 0));
      cout << "fast walk: " << path << endl;
      cout << "walk time: " << graph.lastWalkTime() << " secs" << endl;
    } else if (cmd == "build") {
      StringVector source;
      const BopomofoKeyboardLayout* layout =
          BopomofoKeyboardLayout::StandardLayout();
      cout << "using source: ";
      for (StringVector::iterator iter = args.begin(); iter != args.end();
           ++iter) {
        BPMF bpmf = layout->syllableFromKeySequence(*iter);
        cout << bpmf.composedString() << " ";
        source.push_back(bpmf.absoluteOrderString());
      }
      cout << endl;

      graph.clear();
      graph.setSource(source);
      graph.build();
      cout << "build time: " << graph.lastBuildTime() << " secs" << endl;
    } else if (cmd == "candidate") {
      if (!args.size())
        cerr << "usage: candidate <index> [preceding]" << endl;
      else {
        cout << "listing candidates at index: " << atoi(args[0].c_str())
             << endl;

        CandidateVector candidates;
        if (args.size() > 1) {
          if (args[1] == "preceding")
            candidates = graph.candidatesAtIndex(atoi(args[0].c_str()), true);
          else
            candidates = graph.candidatesAtIndex(atoi(args[0].c_str()));
        } else
          candidates = graph.candidatesAtIndex(atoi(args[0].c_str()));

        // cout << candidates << endl;
        size_t index, size = candidates.size();
        cout << size << " candidates:" << endl;
        for (index = 0; index < size && index < 20; index++) {
          cout << candidates[index] << endl;
        }

        if (index < size) cout << "... and " << size - index << " more" << endl;
      }
    } else if (cmd == "override") {
      if (args.size() < 3)
        cerr << "usage: override pos length qstring index" << endl;
      else {
        CandidateVector candidates;
        candidates = graph.candidatesAtIndex(atoi(args[0].c_str()));

        if (graph.overrideNodeCandidate(
                Node(Location(atoi(args[0].c_str()), atoi(args[1].c_str())),
                     args[2]),
                candidates[atoi(args[3].c_str())].first.first))
          cout << "overriden" << endl;
        else
          cerr << "cannot find the node at (" << args[0] << ", " << args[1]
               << ")" << endl;
      }
    } else if (cmd == "insert") {
      if (args.size() < 2)
        cerr << "usage: insert syllable position" << endl;
      else {
        const BopomofoKeyboardLayout* layout =
            BopomofoKeyboardLayout::StandardLayout();
        string qblock =
            layout->syllableFromKeySequence(args[0]).absoluteOrderString();
        if (graph.insertQueryBlockAndBuild(qblock, atoi(args[1].c_str()))) {
          cout << "successfully inserted" << endl;
          cout << "build time: " << graph.lastBuildTime() << " secs" << endl;
        } else
          cerr << "insertion failed" << endl;
      }
    } else if (cmd == "delete") {
      if (args.size() < 1)
        cerr << "usage: delete position" << endl;
      else {
        if (graph.removeQueryBlockAndBuild(atoi(args[0].c_str()))) {
          cout << "successfully deleted" << endl;
          cout << "build time: " << graph.lastBuildTime() << " secs" << endl;
        } else
          cerr << "deletion failed" << endl;
      }

    } else if (cmd == "add") {
      if (args.size() < 4)
        cerr << "usage: <qstring> <current> <probability> <backoff>" << endl;
      else {
        if (!useUserTable)
          cerr << "not using user table" << endl;
        else {
          if (lmDB->execute(
                  "INSERT INTO userdb.user_unigrams VALUES(%Q, %Q, %s, %s)",
                  args[0].c_str(), args[1].c_str(), args[2].c_str(),
                  args[3].c_str()) == SQLITE_OK)
            cout << "added" << endl;
          else
            cerr << "failed" << endl;
        }
      }
    } else if (cmd == "pop") {
      double wtime = 0.0;
      vector<Path> paths = graph.walk("", Location(0, 0), true);

      if (!paths.size())
        cerr << "nothing path to pop" << endl;
      else {
        const Path& path = paths[0];

        if (path.size() < 2)
          cerr << "nothing to pop" << endl;
        else {
          const StringScoreNodeSetIteratorPair& ssnsip = path[1];
          const Node& node = *(ssnsip.second);

          const StringScoreNodeSetIteratorPair& ssnsip2 = path[2];
          const Node& node2 = *(ssnsip.second);

          Location loc = node.location();
          pair<bool, string> rsp =
              graph.shiftNodeAndMaintainPathWalk(loc, node2);

          if (!rsp.first)
            cerr << "pop error" << endl;
          else
            cout << "poped text: " << rsp.second << endl;
        }
      }

      cout << "walk time: " << graph.lastWalkTime() << " secs" << endl;
    } else if (cmd == "export") {
      if (args.size() < 1)
        cerr << "usage: export filename" << endl;
      else {
        if (BPMFUserPhraseHelper::Export(lmDB, args[0]))
          cout << "done." << endl;
        else
          cerr << "error." << endl;
      }
    } else if (cmd == "import") {
      if (args.size() < 1)
        cerr << "usage: import filename" << endl;
      else {
        if (BPMFUserPhraseHelper::Import(lmDB, args[0]))
          cout << "done." << endl;
        else
          cerr << "error." << endl;
      }
    } else if (cmd == "flush") {
      lm->flushCache();
      cout << "LM cache flushed." << endl;
    } else {
      cerr << "unknown command" << endl;
    }
  }

  if (lm) delete lm;

  // these two are owned by lm
  // if (externalBPMFTable) delete externalBPMFTable;
  // if (lmDB) delete lmDB;

  if (bpmf) delete bpmf;

  return 0;
}
