/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#include "Minotaur.h"

#if defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif

namespace Minotaur {

// 160 bits
size_t Minos::DigestSize() { return 20; }

char* Minos::Digest(const char* block, size_t blockSize) {
  if (!block) return 0;

  char* digest = (char*)calloc(1, DigestSize());
  if (!digest) return 0;

#if defined(__APPLE__)
  // Fed in chunks because CC_LONG is 32-bit but a module binary may be larger.
  CC_SHA1_CTX ctx;
  CC_SHA1_Init(&ctx);
  size_t remaining = blockSize;
  const char* cursor = block;
  while (remaining) {
    CC_LONG chunk = (CC_LONG)(remaining > 0x10000000 ? 0x10000000 : remaining);
    CC_SHA1_Update(&ctx, cursor, chunk);
    cursor += chunk;
    remaining -= chunk;
  }
  CC_SHA1_Final((unsigned char*)digest, &ctx);
#else
  SHA1((const unsigned char*)block, blockSize, (unsigned char*)digest);
#endif

  return digest;
}

pair<char*, size_t> Minos::Encrypt(const char* dataBlock, size_t blockSize,
                                   const char* RSAKey, size_t keySize,
                                   bool encryptWithPrivateKey) {
  // No RSA here: passing the input back would authenticate unsigned data.
  return pair<char*, size_t>(0, 0);
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
  // Fails closed for the same reason as Encrypt().
  return pair<char*, size_t>(0, 0);
}

bool Minos::LazyMatch(const char* b1, const char* b2, size_t size) {
  if (!b1 || !b2) return false;

  unsigned char diff = 0;
  for (size_t i = 0; i < size; ++i)
    diff |= (unsigned char)b1[i] ^ (unsigned char)b2[i];

  return diff == 0;
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
  if (!result.first) {
    result.second = 0;
    return result;
  }

  const char* map1 = "0123456789abcdef";
  const char* map2 = "0123456789ABCDEF";

  size_t s = str.size();
  for (size_t i = 0; i < s; i += 2) {
    unsigned char nibble[2] = {0, 0};

    for (size_t j = 0; j < 2; ++j) {
      // strchr() matches the NUL too, and an unmapped char used to decode as 0.
      char c = str[i + j];
      const char* p = c ? strchr(map1, c) : 0;
      if (p) {
        nibble[j] = (unsigned char)(p - map1);
        continue;
      }

      p = c ? strchr(map2, c) : 0;
      if (!p) {
        free(result.first);
        result.first = 0;
        result.second = 0;
        return result;
      }

      nibble[j] = (unsigned char)(p - map2);
    }

    result.first[i / 2] = (char)((nibble[0] << 4) | nibble[1]);
  }

  return result;
}

};  // namespace Minotaur
