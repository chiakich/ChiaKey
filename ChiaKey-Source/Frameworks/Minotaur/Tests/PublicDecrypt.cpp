/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#include <OpenVanilla/OpenVanilla.h>

#include "Minotaur.h"

using namespace OpenVanilla;
using namespace Minotaur;

int main(int argc, char* argv[]) {
  if (argc < 3) {
    cerr << "PublicDecrypt phraseFile publicKey" << endl;
    return 1;
  }

  pair<char*, size_t> phrase = OVFileHelper::SlurpFile(argv[1]);
  pair<char*, size_t> key = OVFileHelper::SlurpFile(argv[2]);

  if (!phrase.first) {
    cerr << "Cannot slurp " << argv[1] << endl;
    return 1;
  }

  if (!key.first) {
    cerr << "Cannot slurp " << argv[2] << endl;
    return 1;
  }

  pair<char*, size_t> decoded = Minos::GetBack(phrase, key);
  if (!decoded.first) {
    cerr << "Decode failed" << endl;
    return 1;
  }

  for (size_t i = 0; i < decoded.second; ++i) {
    printf("byte %04d: %02x ('%c')\n", i, decoded.first[i], decoded.first[i]);
  }

  free(decoded.first);
  free(phrase.first);
  free(key.first);

  return 0;
}
