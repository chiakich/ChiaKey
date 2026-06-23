// [AUTO_HEADER]

#include <iostream>

#include "Mandarin.h"

using namespace std;
using namespace Formosa::Mandarin;

int main(int argc, char *argv[]) {
  while (!cin.eof()) {
    string line;
    getline(cin, line);

    BPMF syllable = BPMF::FromHanyuPinyin(line);
    cout << line << " -> " << syllable.composedString() << endl;
  }

  return 0;
}
