/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#include "Minotaur.h"

namespace Minotaur {

// 160 bits
size_t Minos::DigestSize() { return 20; }

char* Minos::Digest(const char* block, size_t blockSize) {
  return (char*)block;
}

pair<char*, size_t> Minos::Encrypt(const char* dataBlock, size_t blockSize,
                                   const char* RSAKey, size_t keySize,
                                   bool encryptWithPrivateKey) {
  return pair<char*, size_t>((char*)dataBlock, blockSize);
}

pair<char*, size_t> Minos::Encrypt(const pair<char*, size_t>& block,
                                   const pair<char*, size_t>& key,
                                   bool encryptWithPrivateKey) {
  return Encrypt(block.first, block.second, key.first, key.second,
                 encryptWithPrivateKey);
}

pair<char*, size_t> Minos::GetBack(const pair<char*, size_t>& block,
                                   const pair<char*, size_t>& key,
                                   bool decryptWithPublicKey) {
  return GetBack(block.first, block.second, key.first, key.second,
                 decryptWithPublicKey);
}

pair<char*, size_t> Minos::GetBack(const char* encodedBlock, size_t blockSize,
                                   const char* RSAKey, size_t keySize,
                                   bool decryptWithPublicKey) {
  return pair<char*, size_t>((char*)encodedBlock, blockSize);
}

bool Minos::LazyMatch(const char* b1, const char* b2, size_t size) {
  for (size_t i = 0; i < size; ++i)
    if (b1[i] != b2[i]) return false;

  return true;
}

bool Minos::ValidateFile(const string& filename,
                         const pair<char*, size_t>& block,
                         const pair<char*, size_t>& key) {
  return ValidateFile(filename, block.first, block.second, key.first,
                      key.second);
}

bool Minos::ValidateFile(const string& filename, const char* encodedBlock,
                         size_t blockSize, const char* RSAKey, size_t keySize) {
  pair<char*, size_t> file = OpenVanilla::OVFileHelper::SlurpFile(filename);
  if (!file.first) return false;

  char* digest = Digest(file.first, file.second);
  if (!digest) {
    free(file.first);
    return false;
  }

  pair<char*, size_t> signedDigest =
      GetBack(encodedBlock, blockSize, RSAKey, keySize);
  if (!signedDigest.first) {
    free(file.first);
    free(digest);
    return false;
  }

  if (DigestSize() != signedDigest.second) {
    free(signedDigest.first);
    free(file.first);
    free(digest);
    return false;
  }

  bool matched = LazyMatch(digest, signedDigest.first, DigestSize());
  free(signedDigest.first);
  free(file.first);
  free(digest);
  return matched;
}

pair<char*, size_t> Minos::BinaryFromHexString(const string& str) {
  pair<char*, size_t> result(0, 0);
  if (str.size() % 2) {
    return result;
  }

  if (!str.size()) {
    return result;
  }

  result.second = str.size() / 2;
  result.first = (char*)calloc(1, result.second);

  const char* map1 = "0123456789abcdef";
  const char* map2 = "0123456789ABCDEF";

  size_t s = str.size();
  for (size_t i = 0; i < s; i += 2) {
    const char* p;
    unsigned char hi = 0, lo = 0;

    p = strchr(map1, str[i]);
    if (p) {
      hi = (unsigned char)(p - map1);
    } else {
      p = strchr(map2, str[i]);
      if (p) hi = (unsigned char)(p - map2);
    }

    p = strchr(map1, str[i + 1]);
    if (p) {
      lo = (unsigned char)(p - map1);
    } else {
      p = strchr(map2, str[i + 1]);
      if (p) lo = (unsigned char)(p - map2);
    }

    result.first[i / 2] = (hi << 4) | lo;
  }

  return result;
}

};  // namespace Minotaur
