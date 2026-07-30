//
// WalkerGoldSet.cpp
//
// Top-1 accuracy harness for the Manjusri walker: turns plain sentences into
// (reading sequence -> expected text) pairs and reports how often the walk
// reproduces the sentence. Needed before touching anything that changes
// ranking, because a scoring tweak that helps one sentence usually hurts
// another and nothing else in the tree can tell you the net effect.
//
//   build  reading derivation is the hard part. A Chinese sentence only tells
//          you the output, so the input has to be reconstructed, and every
//          polyphonic character is a chance to reconstruct it wrongly. A wrong
//          reading feeds the walker an input it cannot possibly answer
//          correctly, so label noise shows up as walker error. --dominance
//          controls that trade: 0 keeps only characters with exactly one
//          reading in the lexicon (no noise, few sentences), a positive value
//          also accepts a character whose most probable reading leads the next
//          by that many log10 (more sentences, some noise).
//
//   eval   replays each row through the real Graph and LanguageModel.
//
//   replay drives ManjusriComposer exactly as the IME does, correcting whatever
//          the walk got wrong and letting the real learning path record it, so
//          the cost of a weighting scheme is measured in manual selections over
//          a stream of typing rather than in one-shot accuracy. This is the mode
//          to use for anything that changes how strongly learning scores.
//
// Report absolute accuracy only from a --dominance 0 set. Looser sets are for
// comparing two configurations, where the noise sits on both sides.
//

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "Graph.h"
#include "LanguageModel.h"
// ManjusriComposer is the layer the IME drives, so replay mode uses it directly
// rather than reimplementing candidate selection and its learning side effects.
#include "OVIMSmartMandarin.h"

using namespace std;
using namespace Manjusri;
using namespace OpenVanilla;

namespace {

struct GoldRow {
  vector<string> readings;
  string expected;
  size_t guessedReadings;
};

void Usage() {
  cerr << "Usage:\n"
       << "  WalkerGoldSet build --lexicon DB --corpus FILE --out TSV\n"
       << "        [--dominance N] [--min-chars N] [--max-chars N] [--limit N]\n"
       << "  WalkerGoldSet eval --lexicon DB --gold TSV [--length-prior X]\n"
       << "        [--user-db PATH] [--mismatches FILE]\n"
       << "  WalkerGoldSet replay --lexicon DB --gold TSV [--passes N]\n"
       << "        [--length-prior X] [--user-db PATH] [--keep-user-db]\n"
       << "        [--learned-score X]\n";
}

const string ArgValue(const vector<string>& args, const string& name,
                      const string& fallback = string()) {
  for (size_t i = 0; i + 1 < args.size(); i++)
    if (args[i] == name) return args[i + 1];

  return fallback;
}

bool HasArg(const vector<string>& args, const string& name) {
  return find(args.begin(), args.end(), name) != args.end();
}

// ---- build -----------------------------------------------------------------

// character -> its readings, most probable first
typedef map<string, vector<pair<double, string> > > ReadingMap;

const ReadingMap LoadSingleSyllableReadings(OVSQLiteConnection* db) {
  ReadingMap results;
  OVSQLiteStatement* statement = db->prepare(
      "SELECT current, qstring, probability FROM unigrams "
      "WHERE length(qstring) = 2");
  if (!statement) return results;

  while (statement->step() == SQLITE_ROW) {
    const char* current = statement->textOfColumn(0);
    const char* qstring = statement->textOfColumn(1);
    if (!current || !qstring || !*current) continue;

    results[string(current)].push_back(
        pair<double, string>(statement->doubleOfColumn(2), string(qstring)));
  }
  delete statement;

  for (ReadingMap::iterator iter = results.begin(); iter != results.end();
       ++iter)
    sort((*iter).second.begin(), (*iter).second.end(),
         greater<pair<double, string> >());

  return results;
}

int Build(const vector<string>& args) {
  const string lexicon = ArgValue(args, "--lexicon");
  const string corpus = ArgValue(args, "--corpus");
  const string out = ArgValue(args, "--out");
  if (lexicon.empty() || corpus.empty() || out.empty()) {
    Usage();
    return 1;
  }

  const double dominance = atof(ArgValue(args, "--dominance", "0").c_str());
  const size_t minChars = (size_t)atoi(ArgValue(args, "--min-chars", "4").c_str());
  const size_t maxChars = (size_t)atoi(ArgValue(args, "--max-chars", "30").c_str());
  const size_t limit = (size_t)atoi(ArgValue(args, "--limit", "0").c_str());

  OVSQLiteConnection* db = OVSQLiteConnection::Open(lexicon);
  if (!db) {
    cerr << "cannot open lexicon: " << lexicon << endl;
    return 1;
  }

  ReadingMap readings = LoadSingleSyllableReadings(db);
  cerr << "lexicon: " << readings.size() << " single-syllable characters"
       << endl;

  // characters we are willing to assign a reading to
  map<string, string> accepted;
  size_t polyphonicAccepted = 0;
  for (ReadingMap::const_iterator iter = readings.begin();
       iter != readings.end(); ++iter) {
    const vector<pair<double, string> >& candidates = (*iter).second;
    if (candidates.empty()) continue;

    if (candidates.size() == 1) {
      accepted[(*iter).first] = candidates[0].second;
    } else if (dominance > 0.0 &&
               candidates[0].first - candidates[1].first >= dominance) {
      accepted[(*iter).first] = candidates[0].second;
      polyphonicAccepted++;
    }
  }
  cerr << "accepted " << accepted.size() << " characters ("
       << polyphonicAccepted << " polyphonic, reading inferred)" << endl;

  ifstream in(corpus.c_str());
  if (!in) {
    cerr << "cannot open corpus: " << corpus << endl;
    delete db;
    return 1;
  }

  ofstream tsv(out.c_str());
  if (!tsv) {
    cerr << "cannot write: " << out << endl;
    delete db;
    return 1;
  }

  tsv << "# readings\texpected\tguessed_readings" << endl;

  string line;
  size_t seen = 0, kept = 0, keptChars = 0, keptGuessed = 0;
  while (getline(in, line)) {
    while (line.size() && (line[line.size() - 1] == '\r' ||
                           line[line.size() - 1] == '\n'))
      line.erase(line.size() - 1);
    if (line.empty()) continue;
    seen++;

    vector<string> chars = OVUTF8Helper::SplitStringByCodePoint(line);
    if (chars.size() < minChars || chars.size() > maxChars) continue;

    vector<string> qstrings;
    size_t guessed = 0;
    bool usable = true;
    for (size_t i = 0; i < chars.size(); i++) {
      map<string, string>::const_iterator hit = accepted.find(chars[i]);
      if (hit == accepted.end()) {
        usable = false;
        break;
      }
      qstrings.push_back((*hit).second);
      if (readings[chars[i]].size() > 1) guessed++;
    }
    if (!usable) continue;

    for (size_t i = 0; i < qstrings.size(); i++) {
      if (i) tsv << " ";
      tsv << qstrings[i];
    }
    tsv << "\t" << line << "\t" << guessed << endl;

    kept++;
    keptChars += chars.size();
    keptGuessed += guessed;
    if (limit && kept >= limit) break;
  }

  cerr << "corpus: " << seen << " lines -> " << kept << " sentences, "
       << keptChars << " characters, " << keptGuessed
       << " with an inferred reading";
  if (keptChars)
    cerr << " (" << (100.0 * (double)keptGuessed / (double)keptChars) << "%)";
  cerr << endl;

  delete db;
  return 0;
}

// ---- eval ------------------------------------------------------------------

const vector<GoldRow> LoadGold(const string& path) {
  vector<GoldRow> rows;
  ifstream in(path.c_str());
  if (!in) return rows;

  string line;
  while (getline(in, line)) {
    while (line.size() && (line[line.size() - 1] == '\r' ||
                           line[line.size() - 1] == '\n'))
      line.erase(line.size() - 1);
    if (line.empty() || line[0] == '#') continue;

    size_t firstTab = line.find('\t');
    if (firstTab == string::npos) continue;
    size_t secondTab = line.find('\t', firstTab + 1);

    GoldRow row;
    istringstream readingStream(line.substr(0, firstTab));
    string qstring;
    while (readingStream >> qstring) row.readings.push_back(qstring);

    row.expected = secondTab == string::npos
                       ? line.substr(firstTab + 1)
                       : line.substr(firstTab + 1, secondTab - firstTab - 1);
    row.guessedReadings =
        secondTab == string::npos ? 0 : (size_t)atoi(line.c_str() + secondTab + 1);

    if (row.readings.size() && row.expected.size()) rows.push_back(row);
  }

  return rows;
}

int Eval(const vector<string>& args) {
  const string lexicon = ArgValue(args, "--lexicon");
  const string gold = ArgValue(args, "--gold");
  if (lexicon.empty() || gold.empty()) {
    Usage();
    return 1;
  }

  const string userDBPath = ArgValue(args, "--user-db");
  const string mismatchPath = ArgValue(args, "--mismatches");

  OVSQLiteConnection* db = OVSQLiteConnection::Open(lexicon);
  if (!db) {
    cerr << "cannot open lexicon: " << lexicon << endl;
    return 1;
  }

  bool useUserTable = false;
  if (userDBPath.size()) {
    if (db->execute("ATTACH DATABASE %Q AS userdb", userDBPath.c_str()) ==
        SQLITE_OK) {
      useUserTable = true;
    } else {
      cerr << "warning: could not attach user database " << userDBPath << endl;
    }
  }

  LanguageModel lm(db, 0, useUserTable, false, false, useUserTable,
                   useUserTable);
  if (useUserTable) {
    lm.loadUserBigramCache();
    lm.loadUserCandidateOverrideCache();
  }

  Node::SetUNK(lm.UNKUnigram().probability, lm.UNKUnigram().backoff);
  if (HasArg(args, "--length-prior"))
    Node::SetPhraseLengthBonus(
        (Score)atof(ArgValue(args, "--length-prior").c_str()));

  vector<GoldRow> rows = LoadGold(gold);
  if (rows.empty()) {
    cerr << "no usable rows in " << gold << endl;
    delete db;
    return 1;
  }

  ofstream mismatches;
  if (mismatchPath.size()) {
    mismatches.open(mismatchPath.c_str());
    mismatches << "# expected\tgot\tguessed_readings" << endl;
  }

  size_t exact = 0, lengthMismatch = 0;
  size_t charsTotal = 0, charsCorrect = 0;
  size_t cleanRows = 0, cleanExact = 0;

  for (size_t i = 0; i < rows.size(); i++) {
    const GoldRow& row = rows[i];

    Graph graph(&lm);
    graph.clear();
    for (size_t r = 0; r < row.readings.size(); r++)
      graph.insertQueryBlockAndBuild(row.readings[r], r + 1);

    FastPath path = graph.fastWalk("", Location(0, 0));
    const string got = FastPathAsString(path);

    vector<string> expectedChars =
        OVUTF8Helper::SplitStringByCodePoint(row.expected);
    vector<string> gotChars = OVUTF8Helper::SplitStringByCodePoint(got);

    bool isExact = (got == row.expected);
    if (isExact) exact++;
    if (!row.guessedReadings) {
      cleanRows++;
      if (isExact) cleanExact++;
    }

    charsTotal += expectedChars.size();
    if (expectedChars.size() == gotChars.size()) {
      for (size_t c = 0; c < expectedChars.size(); c++)
        if (expectedChars[c] == gotChars[c]) charsCorrect++;
    } else {
      lengthMismatch++;
    }

    if (!isExact && mismatches.is_open())
      mismatches << row.expected << "\t" << got << "\t" << row.guessedReadings
                 << endl;
  }

  cout << "sentences:              " << rows.size() << endl;
  cout << "exact match:            " << exact << " ("
       << (100.0 * (double)exact / (double)rows.size()) << "%)" << endl;
  cout << "character accuracy:     " << charsCorrect << "/" << charsTotal
       << " (" << (100.0 * (double)charsCorrect / (double)charsTotal) << "%)"
       << endl;
  cout << "length mismatches:      " << lengthMismatch << endl;
  if (cleanRows && cleanRows != rows.size())
    cout << "exact match, no inferred readings: " << cleanExact << "/"
         << cleanRows << " ("
         << (100.0 * (double)cleanExact / (double)cleanRows) << "%)" << endl;

  delete db;
  return 0;
}

// ---- replay ----------------------------------------------------------------

// Index of the candidate a user would pick to fix position `at`: the longest
// one whose text matches the expected characters from there.
int BestCandidateFor(const vector<string>& candidates,
                     const vector<string>& expectedChars, size_t at) {
  int best = -1;
  size_t bestLength = 0;

  for (size_t i = 0; i < candidates.size(); i++) {
    vector<string> candidateChars =
        OVUTF8Helper::SplitStringByCodePoint(candidates[i]);
    if (candidateChars.empty()) continue;
    if (at + candidateChars.size() > expectedChars.size()) continue;

    bool matches = true;
    for (size_t c = 0; c < candidateChars.size(); c++)
      if (candidateChars[c] != expectedChars[at + c]) {
        matches = false;
        break;
      }

    if (matches && candidateChars.size() > bestLength) {
      best = (int)i;
      bestLength = candidateChars.size();
    }
  }

  return best;
}

int Replay(const vector<string>& args) {
  const string lexicon = ArgValue(args, "--lexicon");
  const string gold = ArgValue(args, "--gold");
  if (lexicon.empty() || gold.empty()) {
    Usage();
    return 1;
  }

  const size_t passes = (size_t)atoi(ArgValue(args, "--passes", "1").c_str());

  OVSQLiteConnection* db = OVSQLiteConnection::Open(lexicon);
  if (!db) {
    cerr << "cannot open lexicon: " << lexicon << endl;
    return 1;
  }

  // Learning goes to a scratch database so a measurement never touches the
  // user's own; the caller decides whether to keep it.
  const string userDBPath =
      ArgValue(args, "--user-db", "/tmp/chiakey-replay-user.db");
  if (!HasArg(args, "--keep-user-db")) remove(userDBPath.c_str());

  OVSQLiteConnection* userDB = OVSQLiteConnection::Open(userDBPath);
  if (!userDB) {
    cerr << "cannot open scratch user database: " << userDBPath << endl;
    delete db;
    return 1;
  }
  userDB->execute(
      "CREATE TABLE IF NOT EXISTS user_unigrams "
      "(qstring, current, probability, backoff)");
  userDB->execute(
      "CREATE TABLE IF NOT EXISTS user_bigram_cache "
      "(qstring, previous, current, probability)");
  userDB->execute(
      "CREATE TABLE IF NOT EXISTS user_candidate_override_cache "
      "(qstring, current)");
  LanguageModel::MigrateUserLearningTables(userDB);
  delete userDB;

  if (db->execute("ATTACH DATABASE %Q AS userdb", userDBPath.c_str()) !=
      SQLITE_OK) {
    cerr << "cannot attach scratch user database" << endl;
    delete db;
    return 1;
  }

  LanguageModel lm(db, 0, true, false, false, true, true);
  lm.loadUserBigramCache();
  lm.loadUserCandidateOverrideCache();

  Node::SetUNK(lm.UNKUnigram().probability, lm.UNKUnigram().backoff);
  if (HasArg(args, "--length-prior"))
    Node::SetPhraseLengthBonus(
        (Score)atof(ArgValue(args, "--length-prior").c_str()));
  if (HasArg(args, "--learned-score"))
    LanguageModel::SetLearnedBigramScore(
        atof(ArgValue(args, "--learned-score", "0.0").c_str()));

  vector<GoldRow> rows = LoadGold(gold);
  if (rows.empty()) {
    cerr << "no usable rows in " << gold << endl;
    delete db;
    return 1;
  }

  cout << "sentences per pass: " << rows.size() << endl;

  for (size_t pass = 0; pass < passes; pass++) {
    size_t selections = 0, firstTry = 0, unreachable = 0;

    for (size_t i = 0; i < rows.size(); i++) {
      const GoldRow& row = rows[i];
      vector<string> expectedChars =
          OVUTF8Helper::SplitStringByCodePoint(row.expected);

      ManjusriComposer composer(&lm);
      composer.clear();
      for (size_t r = 0; r < row.readings.size(); r++)
        composer.insertAt(r + 1, row.readings[r]);
      composer.update();

      if (composer.composedString() == row.expected) {
        firstTry++;
        continue;
      }

      // Correct left to right, exactly as a user would, until the sentence
      // matches or the lexicon simply cannot produce it.
      bool stuck = false;
      for (size_t guard = 0; guard < expectedChars.size() + 1; guard++) {
        const string current = composer.composedString();
        if (current == row.expected) break;

        vector<string> currentChars =
            OVUTF8Helper::SplitStringByCodePoint(current);
        size_t at = 0;
        while (at < currentChars.size() && at < expectedChars.size() &&
               currentChars[at] == expectedChars[at])
          at++;
        if (at >= expectedChars.size()) break;

        vector<string> candidates =
            composer.collectCandidates(at + composer.cursorLeftBound(), false);
        int pick = BestCandidateFor(candidates, expectedChars, at);
        if (pick < 0) {
          stuck = true;
          break;
        }

        composer.chooseCandidate((size_t)pick);
        selections++;
      }

      if (stuck) unreachable++;
    }

    lm.saveUserBigramCacheAndCandidateOverrideCache(true, true);

    cout << "pass " << (pass + 1) << ": right first time " << firstTry << " ("
         << (100.0 * (double)firstTry / (double)rows.size())
         << "%), manual selections " << selections << ", unreachable "
         << unreachable << endl;
  }

  delete db;
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  vector<string> args;
  for (int i = 1; i < argc; i++) args.push_back(string(argv[i]));

  if (args.empty()) {
    Usage();
    return 1;
  }

  if (args[0] == "build") return Build(args);
  if (args[0] == "eval") return Eval(args);
  if (args[0] == "replay") return Replay(args);

  Usage();
  return 1;
}
