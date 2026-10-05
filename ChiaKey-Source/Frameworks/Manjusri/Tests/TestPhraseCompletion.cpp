// Deterministic completion checks against an old-schema lexicon (reading index
// only), independent of release word frequencies and installed user data.
#include <iostream>
#include "OVIMSmartMandarin.h"

using namespace OpenVanilla;
using namespace Manjusri;
using namespace Formosa::Mandarin;

static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
  std::cerr << "FAIL " << __LINE__ << ": " << #condition << std::endl; \
  ++failures; } } while (0)

static string Reading(const string& text) {
  return BPMF::FromComposedString(text).absoluteOrderString();
}

class RejectWisdom : public StringFilter {
 public:
  bool shouldPass(const string& text) { return text != "人工智慧"; }
};

class OnlyAlgorithm : public StringFilter {
 public:
  bool shouldPass(const string& text) { return text == "演算法"; }
};

int main() {
  auto db = OVSQLiteConnection::Open(":memory:");
  CHECK(db);
  if (!db) return 1;
  db->execute("CREATE TABLE unigrams (qstring, current, probability, backoff)");
  db->execute("CREATE INDEX unigrams_index ON unigrams(qstring)");
  db->execute("CREATE TABLE bigrams (qstring, previous, current, probability)");
  db->execute("ATTACH DATABASE ':memory:' AS userdb");
  db->execute("CREATE TABLE userdb.user_unigrams (qstring, current, probability, backoff)");
  db->execute("CREATE INDEX userdb.user_unigrams_index ON user_unigrams(qstring)");
  for (const char* key : {"!", "$", "*"})
    db->execute("INSERT INTO unigrams VALUES(%Q, '', -8, 0)", key);
  vector<string> readings = {Reading("ㄖㄣˊ"), Reading("ㄍㄨㄥ"),
                             Reading("ㄓˋ"), Reading("ㄏㄨㄟˋ")};
  vector<string> chars = {"人", "工", "智", "慧"};
  string full;
  for (size_t i = 0; i < readings.size(); ++i) {
    full += readings[i];
    db->execute("INSERT INTO unigrams VALUES(%Q, %Q, -3, 0)",
                readings[i].c_str(), chars[i].c_str());
  }
  string prefix = readings[0] + readings[1];
  db->execute("INSERT INTO unigrams VALUES(%Q, '人工', -1, 0)", prefix.c_str());
  db->execute("INSERT INTO unigrams VALUES(%Q, '人工智慧', -2, 0)", full.c_str());
  // Higher-scoring homophone must not complete the confirmed text 人工.
  db->execute("INSERT INTO unigrams VALUES(%Q, '人工智惠', -3, 0)", full.c_str());
  db->execute("INSERT INTO unigrams VALUES(%Q, '人工智慧', -2.5, 0)", full.c_str());
  db->execute("INSERT INTO unigrams VALUES(%Q, '人工致慧', -4, 0)", full.c_str());
  db->execute("INSERT INTO unigrams VALUES(%Q, '人工至慧', -5, 0)", full.c_str());
  db->execute("INSERT INTO unigrams VALUES(%Q, '仁工智慧', -0.5, 0)", full.c_str());
  {
    LanguageModel lm(db, 0, true, false, false);
    Node::SetUNK(lm.UNKUnigram().probability, lm.UNKUnigram().backoff);
    ManjusriComposer composer(&lm);
    composer.clear();
    CHECK(composer.insertAt(1, readings[0]));
    composer.update();
    CHECK(composer.phraseCompletions().front().suffix == "工");
    CHECK(composer.insertAt(2, readings[1]));
    composer.update();
    CHECK(composer.composedString() == "人工");
    auto completions = composer.phraseCompletions();
    CHECK(completions.size() == 3);
    CHECK(completions[1].suffix == "智惠");
    CHECK(completions[2].suffix == "致慧");
    auto completion = completions.front();
    CHECK(completion.suffix == "智慧");
    RejectWisdom filter;
    CHECK(composer.phraseCompletions(&filter).front().suffix == "智惠");
    CHECK(composer.acceptPhraseCompletion(completion));
    CHECK(composer.composedString() == "人工智慧");
    CHECK(composer.cursorRightBound() - composer.cursorLeftBound() == 4);
    composer.backspaceAt(composer.cursorRightBound());
    composer.update();
    CHECK(composer.composedString() == "人工智");
    composer.backspaceAt(composer.cursorRightBound());
    composer.update();
    CHECK(composer.composedString() == "人工");
    CHECK(composer.toggleForcedBreakAt(2));
    composer.update();
    CHECK(composer.phraseCompletions().empty());
    CHECK(!composer.acceptPhraseCompletion(completion));
    CHECK(composer.composedString() == "人工");
    CHECK(composer.cursorRightBound() - composer.cursorLeftBound() == 2);
    CHECK(composer.toggleForcedBreakAt(2));
    composer.update();
    // A user phrase is visible immediately without changing release data.
    CHECK(lm.addUserUnigram(full, "人工治慧"));
    CHECK(composer.phraseCompletions().front().suffix == "治慧");
    // A missing suffix reading must fail without inserting the first one.
    db->execute("DELETE FROM unigrams WHERE qstring = %Q", readings[3].c_str());
    lm.flushCache();
    CHECK(!composer.acceptPhraseCompletion(completion));
    CHECK(composer.composedString() == "人工");
    CHECK(composer.cursorRightBound() - composer.cursorLeftBound() == 2);
  }
  // A single confirmed character can complete a multi-character suffix.
  string yan = Reading("ㄧㄢˇ");
  string suan = Reading("ㄙㄨㄢˋ");
  string fa = Reading("ㄈㄚˇ");
  db->execute("INSERT INTO unigrams VALUES(%Q, '演', -1, 0)", yan.c_str());
  db->execute("INSERT INTO unigrams VALUES(%Q, '算', -1, 0)", suan.c_str());
  db->execute("INSERT INTO unigrams VALUES(%Q, '法', -1, 0)", fa.c_str());
  db->execute("INSERT INTO unigrams VALUES(%Q, '演算法', -2, 0)",
              (yan + suan + fa).c_str());
  {
    LanguageModel lm(db, 0, true, false, false);
    ManjusriComposer composer(&lm);
    composer.clear();
    CHECK(composer.phraseCompletions().empty());
    CHECK(composer.insertAt(1, yan));
    composer.update();
    auto completions = composer.phraseCompletions();
    CHECK(composer.composedString() == "演");
    CHECK(completions.size() == 1);
    CHECK(completions.front().suffix == "算法");
    CHECK(completions.front().prefixLength == 1);
    // Nonmatching high-ranked words must not consume the query's 32 slots.
    for (int i = 0; i < 40; ++i)
      db->execute("INSERT INTO unigrams VALUES(%Q, '演出', %f, 0)",
                  (yan + Reading("ㄔㄨ")).c_str(), -0.1 - i * 0.01);
    // Duplicate probabilities and release/user rows share a candidate slot.
    db->execute("INSERT INTO userdb.user_unigrams VALUES(%Q, '演出', 0, 0)",
                (yan + Reading("ㄔㄨ")).c_str());
    auto distinct = lm.findPhraseCompletions(yan, "演");
    CHECK(distinct.size() == 2);
    CHECK(distinct.front().current == "演出");
    CHECK(distinct.front().probability == 0);
    CHECK(distinct.back().current == "演算法");
    auto unfiltered = composer.phraseCompletions();
    CHECK(unfiltered.size() == 2);
    CHECK(unfiltered.front().suffix == "出");
    CHECK(unfiltered.back().suffix == "算法");
    OnlyAlgorithm outputFilter;
    auto filtered = composer.phraseCompletions(&outputFilter);
    CHECK(filtered.size() == 1);
    CHECK(filtered.front().phrase.current == "演算法");
    // Repeated calls use fresh predicate data after statements are finalized.
    CHECK(composer.phraseCompletions(&outputFilter).size() == 1);
    auto partial = composer.phraseCompletions(0, BPMF::FromComposedString("ㄙ"));
    CHECK(partial.size() == 1);
    CHECK(partial.front().phrase.current == "演算法");
    CHECK(composer.phraseCompletions(0, BPMF::FromComposedString("ㄙㄢ")).empty());
    CHECK(composer.phraseCompletions(0, BPMF::FromComposedString("ㄙㄨㄢˊ")).empty());
    CHECK(composer.phraseCompletions(0, BPMF::FromComposedString("ㄙㄨ")).size() == 1);
    CHECK(composer.acceptPhraseCompletion(completions.front()));
    CHECK(composer.composedString() == "演算法");
  }
  string gong = Reading("ㄍㄨㄥ");
  string neng = Reading("ㄋㄥˊ");
  string nong = Reading("ㄋㄨㄥˊ");
  db->execute("INSERT INTO unigrams VALUES(%Q, '功', -4, 0)", gong.c_str());
  db->execute("INSERT INTO unigrams VALUES(%Q, '能', -2, 0)", neng.c_str());
  db->execute("INSERT INTO unigrams VALUES(%Q, '農', -2, 0)", nong.c_str());
  db->execute("INSERT INTO unigrams VALUES(%Q, '功能', -1, 0)", (gong + neng).c_str());
  db->execute("INSERT INTO unigrams VALUES(%Q, '工農', -2, 0)", (gong + nong).c_str());
  {
    LanguageModel lm(db, 0, true, false, false);
    ManjusriComposer composer(&lm);
    composer.clear();
    CHECK(composer.insertAt(1, gong));
    composer.update();
    CHECK(composer.composedString() == "工");
    CHECK(composer.phraseCompletions().front().phrase.current == "工農");
    auto completions = composer.phraseCompletions(0, BPMF::FromComposedString("ㄋ"));
    CHECK(completions.size() == 2);
    CHECK(completions.front().phrase.current == "功能");
    CHECK(composer.composedString() == "工"); // Query never changes the buffer.
    CHECK(composer.acceptPhraseCompletion(completions.front()));
    CHECK(composer.composedString() == "功能");
    composer.clear();
    CHECK(composer.insertAt(1, gong));
    composer.update();
    auto candidates = composer.collectCandidates(composer.cursorRightBound(), true);
    for (size_t i = 0; i < candidates.size(); ++i)
      if (candidates[i] == "工") composer.chooseCandidate(i, true, false);
    completions = composer.phraseCompletions(0, BPMF::FromComposedString("ㄋ"));
    CHECK(completions.size() == 1);
    CHECK(completions.front().phrase.current == "工農"); // Explicit selection stays locked.
  }
  delete db;
  if (failures) return 1;
  std::cout << "TestPhraseCompletion: OK" << std::endl;
  return 0;
}
