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
#include <limits>
#include <map>
#include <set>
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
       << "  WalkerGoldSet build --word-level --lexicon DB --corpus FILE\n"
       << "        --out TSV [--pins TSV] [--variants TSV] [--max-variants N]\n"
       << "        [--min-chars N] [--max-chars N] [--max-word-chars N]\n"
       << "        [--limit N]\n"
       << "        (segments first and takes each word's own reading, so the\n"
       << "         common polyphonic characters no longer reject the sentence.\n"
       << "         Words users type more than one way emit one row per\n"
       << "         reading; words whose reading only context can settle are\n"
       << "         rejected outright rather than guessed.)\n"
       << "  WalkerGoldSet eval --lexicon DB --gold TSV [--length-prior X]\n"
       << "        [--unigram-promotion X]\n"
       << "        [--user-db PATH] [--mismatches FILE]\n"
       << "        (eval never writes to --user-db)\n"
       << "  WalkerGoldSet replay --lexicon DB --gold TSV [--passes N]\n"
       << "        [--length-prior X] [--user-db PATH] [--reset-user-db]\n"
       << "        [--keep-user-db] [--learned-score X]\n"
       << "        [--unigram-promotion X]\n"
       << "        (replay WRITES learning. Without --user-db it uses a scratch\n"
       << "         database under TMPDIR and starts it empty unless\n"
       << "         --keep-user-db; a --user-db you name is written to in place\n"
       << "         and only emptied first if you pass --reset-user-db.)\n";
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

// Honour TMPDIR rather than hardcoding /tmp: on macOS that keeps the scratch
// database in the caller's own per-user temp directory.
const string ScratchPath(const string& name) {
  const char* tmpdir = getenv("TMPDIR");
  string base = tmpdir && *tmpdir ? string(tmpdir) : string("/tmp/");
  if (base[base.size() - 1] != '/') base += '/';

  return base + name;
}

// ---- build -----------------------------------------------------------------

// character -> its readings, most probable first
typedef map<string, vector<pair<double, string> > > ReadingMap;

const ReadingMap LoadSingleSyllableReadings(OVSQLiteConnection* db) {
  ReadingMap results;
  OVSQLiteStatementRef statement = db->prepare(
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

  for (ReadingMap::iterator iter = results.begin(); iter != results.end();
       ++iter)
    sort((*iter).second.begin(), (*iter).second.end(),
         greater<pair<double, string> >());

  return results;
}

// ---- build --word-level ----------------------------------------------------
//
// The character-level rule above keeps a sentence only when every character has
// a single reading in the lexicon. The common characters are exactly the
// polyphonic ones, so it throws away 99.9% of real text and what survives is
// short and full of rare characters -- unusable as an absolute baseline.
//
// Most polyphony is already settled at the word level: 行 is ambiguous, 銀行 is
// not, because the lexicon entry carries the whole word's qstring. So segment
// first, then decide per word:
//
//   single   one reading in the lexicon -- 93.8% of entries, nothing to decide
//   sandhi   readings differ in exactly one syllable and that character is
//            一/不/們; free variation every user types both ways, detected
//            mechanically because the rule is phonological, not statistical
//   variant  listed in --variants: same word, more than one real pronunciation
//   pin      listed in --pins: several readings, but only one occurs for the
//            standalone token in running text (的 is never ㄉㄧˋ; 目的 is a word)
//   reject   anything else -- context decides the reading (數, 更, 曲, 得) and
//            we have no label, so the sentence goes
//
// sandhi and variant words emit one gold row per reading, all with the same
// expected text: those really are several inputs for one sentence, and a walker
// that only wins from one of them has a bug worth seeing.
//
// Weight similarity deliberately plays no part here. Tone is contrastive in
// Mandarin (曲 ㄑㄩ/ㄑㄩˇ, 得 ㄉㄜˊ/ㄉㄜ˙), so "these two readings score alike"
// says the lexicon never separated them, not that both are typed.

// The three CJK ranges the rest of the project treats as Han (matching the
// bigram pipeline's is_han), decoded from UTF-8.
bool IsHan(const string& character) {
  const unsigned char* bytes = (const unsigned char*)character.c_str();
  unsigned int code = 0;
  if (character.size() == 3)
    code = ((bytes[0] & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) |
           (bytes[2] & 0x3F);
  else if (character.size() == 4)
    code = ((bytes[0] & 0x07) << 18) | ((bytes[1] & 0x3F) << 12) |
           ((bytes[2] & 0x3F) << 6) | (bytes[3] & 0x3F);
  else
    return false;

  return (code >= 0x3400 && code <= 0x4DBF) ||
         (code >= 0x4E00 && code <= 0x9FFF) ||
         (code >= 0xF900 && code <= 0xFAFF);
}

typedef map<string, vector<pair<double, string> > > WordReadingMap;

const WordReadingMap LoadWordReadings(OVSQLiteConnection* db,
                                      map<string, double>* bestWeight) {
  WordReadingMap results;
  OVSQLiteStatementRef statement =
      db->prepare("SELECT current, qstring, probability FROM unigrams");
  if (!statement) return results;

  while (statement->step() == SQLITE_ROW) {
    const char* current = statement->textOfColumn(0);
    const char* qstring = statement->textOfColumn(1);
    if (!current || !qstring || !*current || !*qstring) continue;

    const string word(current);
    const string code(qstring);
    // one syllable is two qstring characters; anything else is not a reading
    // for this word and would corrupt the per-character split below
    if (code.size() !=
        2 * OVUTF8Helper::SplitStringByCodePoint(word).size())
      continue;

    const double probability = statement->doubleOfColumn(2);
    vector<pair<double, string> >& readings = results[word];
    bool seen = false;
    for (size_t i = 0; i < readings.size(); i++)
      if (readings[i].second == code) {
        seen = true;
        if (probability > readings[i].first) readings[i].first = probability;
      }
    if (!seen) readings.push_back(pair<double, string>(probability, code));

    map<string, double>::iterator hit = bestWeight->find(word);
    if (hit == bestWeight->end() || probability > (*hit).second)
      (*bestWeight)[word] = probability;
  }

  for (WordReadingMap::iterator iter = results.begin(); iter != results.end();
       ++iter)
    sort((*iter).second.begin(), (*iter).second.end(),
         greater<pair<double, string> >());

  return results;
}

// word -> readings. Blank lines and #-comments are skipped; a trailing field
// starting with # is a human-readable bopomofo note, not a reading.
const map<string, vector<string> > LoadReadingList(const string& path,
                                                   size_t maxReadings) {
  map<string, vector<string> > results;
  if (path.empty()) return results;

  ifstream in(path.c_str());
  if (!in) {
    cerr << "cannot open reading list: " << path << endl;
    return results;
  }

  string line;
  while (getline(in, line)) {
    while (line.size() && (line[line.size() - 1] == '\r' ||
                           line[line.size() - 1] == '\n'))
      line.erase(line.size() - 1);
    if (line.empty() || line[0] == '#') continue;

    vector<string> fields;
    string field;
    for (size_t i = 0; i < line.size(); i++) {
      if (line[i] == '\t') {
        fields.push_back(field);
        field.clear();
      } else
        field += line[i];
    }
    fields.push_back(field);
    if (fields.size() < 2) continue;

    vector<string> readings;
    for (size_t i = 1; i < fields.size() && readings.size() < maxReadings; i++) {
      if (fields[i].empty() || fields[i][0] == '#') break;
      readings.push_back(fields[i]);
    }
    if (readings.size()) results[fields[0]] = readings;
  }

  return results;
}

// Viterbi max-score segmentation. The length prior matches the walker's
// Node::lengthPrior -- c_phraseLengthBonus per EXTRA syllable, so a longer word
// beats the split that covers the same characters. (A bonus per syllable rather
// than per extra syllable sums to the same constant for every segmentation and
// would express no preference at all.)
const vector<string> SegmentWords(const vector<string>& chars,
                                  const map<string, double>& weights,
                                  size_t maxWordChars) {
  const size_t n = chars.size();
  const double kUnknown = -9.0;
  // c_phraseLengthBonus is protected; a two-syllable node's prior is exactly
  // one bonus, so this reads whatever the walker is currently configured with,
  // including a --length-prior override.
  const double lengthBonus = Node(Location(0, 2), string()).lengthPrior();
  vector<double> best(n + 1, -numeric_limits<double>::infinity());
  vector<size_t> backpointer(n + 1, 0);
  best[0] = 0.0;

  for (size_t i = 0; i < n; i++) {
    if (best[i] == -numeric_limits<double>::infinity()) continue;
    string word;
    const size_t limit = min(maxWordChars, n - i);
    for (size_t length = 1; length <= limit; length++) {
      word += chars[i + length - 1];
      double score;
      map<string, double>::const_iterator hit = weights.find(word);
      if (hit != weights.end())
        score = (*hit).second + lengthBonus * (double)(length - 1);
      else if (length == 1)
        score = kUnknown;
      else
        continue;

      if (best[i] + score > best[i + length]) {
        best[i + length] = best[i] + score;
        backpointer[i + length] = length;
      }
    }
  }

  vector<string> words;
  size_t position = n;
  while (position > 0) {
    const size_t length = backpointer[position];
    if (!length) return vector<string>();
    string word;
    for (size_t i = position - length; i < position; i++) word += chars[i];
    words.push_back(word);
    position -= length;
  }
  reverse(words.begin(), words.end());

  return words;
}

// 一/不/們 carry free tone variation: all readings agree except on that one
// syllable. Everything else that differs by one syllable is a different word.
bool IsSandhiVariation(const string& word,
                       const vector<pair<double, string> >& readings) {
  const vector<string> chars = OVUTF8Helper::SplitStringByCodePoint(word);
  const size_t syllables = chars.size();
  for (size_t i = 0; i < readings.size(); i++)
    if (readings[i].second.size() != 2 * syllables) return false;

  size_t differing = 0, at = 0;
  for (size_t s = 0; s < syllables; s++) {
    const string first = readings[0].second.substr(2 * s, 2);
    for (size_t i = 1; i < readings.size(); i++)
      if (readings[i].second.substr(2 * s, 2) != first) {
        differing++;
        at = s;
        break;
      }
  }
  if (differing != 1) return false;

  return chars[at] == "一" || chars[at] == "不" || chars[at] == "們";
}

int BuildWords(const vector<string>& args) {
  const string lexicon = ArgValue(args, "--lexicon");
  const string corpus = ArgValue(args, "--corpus");
  const string out = ArgValue(args, "--out");
  if (lexicon.empty() || corpus.empty() || out.empty()) {
    Usage();
    return 1;
  }

  const size_t minChars = (size_t)atoi(ArgValue(args, "--min-chars", "4").c_str());
  const size_t maxChars = (size_t)atoi(ArgValue(args, "--max-chars", "24").c_str());
  const size_t limit = (size_t)atoi(ArgValue(args, "--limit", "0").c_str());
  const size_t maxWordChars =
      (size_t)atoi(ArgValue(args, "--max-word-chars", "8").c_str());
  const size_t maxVariants =
      (size_t)atoi(ArgValue(args, "--max-variants", "4").c_str());

  OVSQLiteConnection* db = OVSQLiteConnection::Open(lexicon);
  if (!db) {
    cerr << "cannot open lexicon: " << lexicon << endl;
    return 1;
  }

  map<string, double> weights;
  const WordReadingMap readings = LoadWordReadings(db, &weights);
  delete db;
  cerr << "lexicon: " << readings.size() << " words" << endl;

  const map<string, vector<string> > pins =
      LoadReadingList(ArgValue(args, "--pins"), 1);
  const map<string, vector<string> > variants =
      LoadReadingList(ArgValue(args, "--variants"), maxVariants);
  cerr << "pins: " << pins.size() << "   variants: " << variants.size() << endl;

  ifstream in(corpus.c_str());
  if (!in) {
    cerr << "cannot open corpus: " << corpus << endl;
    return 1;
  }
  ofstream tsv(out.c_str());
  if (!tsv) {
    cerr << "cannot write: " << out << endl;
    return 1;
  }
  tsv << "# readings\texpected\tguessed_readings" << endl;

  string line;
  size_t seen = 0, kept = 0, rows = 0, keptChars = 0;
  size_t rejectedWord = 0, rejectedFanout = 0, skippedNonHan = 0;
  size_t duplicates = 0;
  set<string> seenSentences;
  while (getline(in, line)) {
    while (line.size() && (line[line.size() - 1] == '\r' ||
                           line[line.size() - 1] == '\n'))
      line.erase(line.size() - 1);
    if (line.empty()) continue;

    const vector<string> chars = OVUTF8Helper::SplitStringByCodePoint(line);
    if (chars.size() < minChars || chars.size() > maxChars) continue;

    // Forum and gazette corpora repeat whole sentences (boilerplate replies,
    // procedural formulas). Left in, they weight the score towards whatever
    // happens to be duplicated rather than towards the language.
    if (!seenSentences.insert(line).second) {
      duplicates++;
      continue;
    }

    // Only Han runs can be typed as a bopomofo reading at all, so a line with
    // anything else is out of scope rather than an undecidable reading. Count
    // it separately or the reject figure reads as a lexicon problem.
    bool allHan = true;
    for (size_t i = 0; i < chars.size() && allHan; i++)
      if (!IsHan(chars[i])) allHan = false;
    if (!allHan) {
      skippedNonHan++;
      continue;
    }
    seen++;

    const vector<string> words = SegmentWords(chars, weights, maxWordChars);
    if (words.empty()) continue;

    // per word, the readings it may be typed with
    vector<vector<string> > choices;
    bool usable = true;
    size_t fanout = 1;
    for (size_t i = 0; i < words.size() && usable; i++) {
      const WordReadingMap::const_iterator hit = readings.find(words[i]);
      if (hit == readings.end()) {
        usable = false;
        break;
      }
      const vector<pair<double, string> >& candidates = (*hit).second;

      vector<string> allowed;
      const map<string, vector<string> >::const_iterator variant =
          variants.find(words[i]);
      const map<string, vector<string> >::const_iterator pin =
          pins.find(words[i]);
      if (candidates.size() == 1)
        allowed.push_back(candidates[0].second);
      else if (variant != variants.end())
        allowed = (*variant).second;
      else if (IsSandhiVariation(words[i], candidates)) {
        for (size_t c = 0; c < candidates.size() && c < maxVariants; c++)
          allowed.push_back(candidates[c].second);
      } else if (pin != pins.end())
        allowed = (*pin).second;
      else {
        usable = false;
        break;
      }

      fanout *= allowed.size();
      choices.push_back(allowed);
    }
    if (!usable) {
      rejectedWord++;
      continue;
    }
    if (fanout > maxVariants) {
      rejectedFanout++;
      continue;
    }

    // cartesian product over the per-word readings
    for (size_t variantIndex = 0; variantIndex < fanout; variantIndex++) {
      size_t remainder = variantIndex;
      string qstring;
      for (size_t i = 0; i < choices.size(); i++) {
        const string& code = choices[i][remainder % choices[i].size()];
        remainder /= choices[i].size();
        for (size_t s = 0; s + 1 < code.size(); s += 2) {
          if (qstring.size()) qstring += " ";
          qstring += code.substr(s, 2);
        }
      }
      tsv << qstring << "\t" << line << "\t0" << endl;
      rows++;
    }

    kept++;
    keptChars += chars.size();
    if (limit && kept >= limit) break;
  }

  cerr << "corpus: " << seen << " sentences in range -> " << kept << " kept ("
       << (seen ? 100.0 * (double)kept / (double)seen : 0.0) << "%), " << rows
       << " gold rows (" << (kept ? (double)rows / (double)kept : 0.0)
       << " per sentence), " << keptChars << " characters" << endl;
  cerr << "rejected: " << rejectedWord << " on an undecidable reading, "
       << rejectedFanout << " over --max-variants; " << skippedNonHan
       << " lines skipped as not all-Han" << endl;
  cerr << "deduplicated: " << duplicates << " repeated sentences" << endl;

  return 0;
}

int Build(const vector<string>& args) {
  if (HasArg(args, "--word-level")) return BuildWords(args);

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
  if (HasArg(args, "--unigram-promotion"))
    Node::SetUnigramPromotion(
        (Score)atof(ArgValue(args, "--unigram-promotion", "1.0").c_str()));

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

  // Replay writes learning, so unlike eval it needs a database of its own. Only
  // the default scratch path is ever cleared: --user-db used to be wiped too,
  // which turned the documented `--user-db ~/Library/.../SmartMandarinUserData.db`
  // (an eval invocation) into a way to delete your own learning by passing it to
  // the wrong subcommand. Starting a supplied database from empty now has to be
  // asked for.
  const bool defaultUserDB = !HasArg(args, "--user-db");
  const string userDBPath =
      defaultUserDB ? ScratchPath("chiakey-replay-user.db")
                    : ArgValue(args, "--user-db");

  if (HasArg(args, "--reset-user-db") ||
      (defaultUserDB && !HasArg(args, "--keep-user-db")))
    remove(userDBPath.c_str());

  if (!defaultUserDB)
    cerr << "note: replay writes learning into " << userDBPath << endl;

  OVSQLiteConnection* userDB = OVSQLiteConnection::Open(userDBPath);
  if (!userDB) {
    cerr << "cannot open user database: " << userDBPath << endl;
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
  if (HasArg(args, "--unigram-promotion"))
    Node::SetUnigramPromotion(
        (Score)atof(ArgValue(args, "--unigram-promotion", "1.0").c_str()));
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
