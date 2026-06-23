/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef Minotaur_h
#define Minotaur_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace Minotaur {
using namespace std;

// remember, when you decide the key length, make it at least 11 bytes (88 bits)
// longer than the key you want to encodes
class Minos {
 public:
  // use free() instead of delete[]
  static pair<char*, size_t> GetBack(const char* encodedBlock, size_t blockSize,
                                     const char* RSAKey, size_t keySize,
                                     bool decryptWithPublicKey = true);
  static pair<char*, size_t> GetBack(const pair<char*, size_t>& block,
                                     const pair<char*, size_t>& key,
                                     bool decryptWithPublicKey = true);

  static pair<char*, size_t> Encrypt(const char* dataBlock, size_t blockSize,
                                     const char* RSAKey, size_t keySize,
                                     bool encryptWithPrivateKey = true);
  static pair<char*, size_t> Encrypt(const pair<char*, size_t>& block,
                                     const pair<char*, size_t>& key,
                                     bool encryptWithPrivateKey = true);

  static size_t DigestSize();
  static char* Digest(const char* block, size_t blockSize);

  // use this at your own peril
  static bool LazyMatch(const char* b1, const char* b2, size_t size);
  static bool ValidateFile(const string& filename,
                           const pair<char*, size_t>& block,
                           const pair<char*, size_t>& key);
  static bool ValidateFile(const string& filename, const char* encodedBlock,
                           size_t blockSize, const char* RSAKey,
                           size_t keySize);

  static pair<char*, size_t> BinaryFromHexString(const string& str);
};
};  // namespace Minotaur

#endif
