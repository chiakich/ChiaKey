#include "TaiwanToSimplified.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

std::string Unescape(const std::string& text) {
  std::string output;
  for (size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '\\' && index + 1 < text.size() && text[index + 1] == 'n') {
      output += '\n';
      ++index;
    } else {
      output += text[index];
    }
  }
  return output;
}

void Expect(const std::string& name, const std::string& actual, const std::string& expected) {
  if (actual == expected) return;
  ++failures;
  std::cerr << "FAIL " << name << ": got [" << actual << "] expected [" << expected << "]\n";
}

void ExpectSegments(const std::vector<std::string>& input,
                    const std::vector<std::string>& expected) {
  const std::vector<std::string> actual = ChiaKey::ConvertTaiwanToSimplified(input);
  std::string name;
  for (const std::string& segment : input) name += segment + "|";
  if (actual.size() != expected.size()) {
    ++failures;
    std::cerr << "FAIL " << name << ": got " << actual.size() << " segments\n";
    return;
  }
  for (size_t index = 0; index < actual.size(); ++index) {
    Expect(name + " #" + std::to_string(index), actual[index], expected[index]);
  }
}

int Upstream(const char* path) {
  std::ifstream file(path);
  if (!file) {
    std::cerr << "cannot read " << path << "\n";
    return 1;
  }
  std::string line;
  int count = 0;
  while (std::getline(file, line)) {
    const size_t tab = line.find('\t');
    if (tab == std::string::npos) continue;
    const std::string input = Unescape(line.substr(0, tab));
    Expect(input, ChiaKey::ConvertTaiwanToSimplified(input), Unescape(line.substr(tab + 1)));
    ++count;
  }
  if (count == 0) {
    std::cerr << "no cases in " << path << "\n";
    return 1;
  }
  std::cout << count << " upstream tw2s cases\n";
  return failures == 0 ? 0 : 1;
}

int ChiaKeyCases() {
  using ChiaKey::ConvertTaiwanToSimplified;
  Expect("empty", ConvertTaiwanToSimplified(""), "");
  Expect("taiwan", ConvertTaiwanToSimplified("臺灣"), "台湾");
  Expect("simplified", ConvertTaiwanToSimplified("这是简体中文"), "这是简体中文");
  Expect("phrase", ConvertTaiwanToSimplified("頭髮理髮"), "头发理发");
  Expect("zhuzuo", ConvertTaiwanToSimplified("著作"), "著作");
  Expect("yuanzhu", ConvertTaiwanToSimplified("這是原著小說"), "这是原著小说");
  Expect("tuzhu", ConvertTaiwanToSimplified("土著"), "土著");
  Expect("hunhe", ConvertTaiwanToSimplified("混合著水"), "混合着水");
  Expect("lunzhu", ConvertTaiwanToSimplified("討論著"), "讨论着");
  Expect("pao", ConvertTaiwanToSimplified("砲兵"), "炮兵");
  Expect("ni", ConvertTaiwanToSimplified("妳好"), "你好");
  Expect("ascii", ConvertTaiwanToSimplified("abc 123\n"), "abc 123\n");
  Expect("supplementary", ConvertTaiwanToSimplified("\xF0\xA0\x80\x80\xE8\xAA\xAA"),
         "\xF0\xA0\x80\x80\xE8\xAF\xB4");
  // Malformed bytes pass through and do not swallow the next character.
  Expect("malformed", ConvertTaiwanToSimplified("\xE8\xAA\xE8\xAA\xAA\xFF"),
         "\xE8\xAA\xE8\xAF\xB4\xFF");
  // Compatibility ideograph U+F9D0 normalizes to 類.
  Expect("compatibility", ConvertTaiwanToSimplified("\xEF\xA7\x90"), "类");

  // A phrase may not straddle a word: 生活|著|名為 is not 著名.
  ExpectSegments({"生活", "著", "名為"}, {"生活", "着", "名为"});
  Expect("flat zhuming", ConvertTaiwanToSimplified("生活著名為"), "生活著名为");
  // A phrase covering whole words still applies across them.
  ExpectSegments({"頭", "髮"}, {"头", "发"});
  ExpectSegments({"著", "作"}, {"著", "作"});
  ExpectSegments({"", "臺灣", ""}, {"", "台湾", ""});
  ExpectSegments({}, {});

  std::cout << "ChiaKey cases\n";
  return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "upstream") return Upstream(argv[2]);
  if (argc == 2 && std::string(argv[1]) == "chiakey") return ChiaKeyCases();
  std::cerr << "usage: " << argv[0] << " upstream CASES.tsv | chiakey\n";
  return 2;
}
