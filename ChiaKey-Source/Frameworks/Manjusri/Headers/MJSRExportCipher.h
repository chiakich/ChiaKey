/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef MJSRExportCipher_h
#define MJSRExportCipher_h

// Reading the <database> block of a user phrase export. Header-only and free
// of OpenVanilla so both importers -- the C++ one in BPMFUserPhraseHelper and
// the Objective-C one in the phrase editor's PEUserPhraseStore -- can share it.

#include <cstring>
#include <string>

// The passphrase the exporter passes to SQLite SEE; also what ChiaKey's own
// Export still hands to ATTACH ... KEY, where it is a no-op.
#define MANJUSRI_EXPORT_KEY "mjsrexport"

namespace Manjusri {

static const char kSQLiteHeader[] = "SQLite format 3";
static const size_t kSQLiteHeaderSize = 16;  // includes the trailing NUL

static const unsigned char kAESSBox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b,
    0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
    0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
    0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
    0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
    0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
    0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14,
    0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
    0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f,
    0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
    0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
    0xb0, 0x54, 0xbb, 0x16};

inline unsigned char AESXTime(unsigned char x) {
  return (unsigned char)((x << 1) ^ ((x & 0x80) ? 0x1b : 0x00));
}

// Block encryption only: CTR never needs the inverse cipher.
class AES128Encryptor {
 public:
  explicit AES128Encryptor(const unsigned char key[16]) {
    memcpy(m_roundKeys, key, 16);

    unsigned char rcon = 1;
    for (size_t i = 16; i < sizeof(m_roundKeys); i += 4) {
      unsigned char t[4];
      memcpy(t, m_roundKeys + i - 4, 4);

      if (!(i % 16)) {
        unsigned char rotated = t[0];
        t[0] = (unsigned char)(kAESSBox[t[1]] ^ rcon);
        t[1] = kAESSBox[t[2]];
        t[2] = kAESSBox[t[3]];
        t[3] = kAESSBox[rotated];
        rcon = AESXTime(rcon);
      }

      for (size_t j = 0; j < 4; ++j) {
        m_roundKeys[i + j] = (unsigned char)(m_roundKeys[i - 16 + j] ^ t[j]);
      }
    }
  }

  void encryptBlock(const unsigned char in[16], unsigned char out[16]) const {
    unsigned char s[16];
    for (size_t i = 0; i < 16; ++i)
      s[i] = (unsigned char)(in[i] ^ m_roundKeys[i]);

    for (size_t round = 1; round <= 10; ++round) {
      for (size_t i = 0; i < 16; ++i) s[i] = kAESSBox[s[i]];

      // ShiftRows; the state is laid out column by column, so row r lives at
      // s[r], s[r + 4], s[r + 8], s[r + 12].
      unsigned char t = s[1];
      s[1] = s[5];
      s[5] = s[9];
      s[9] = s[13];
      s[13] = t;
      t = s[2];
      s[2] = s[10];
      s[10] = t;
      t = s[6];
      s[6] = s[14];
      s[14] = t;
      t = s[15];
      s[15] = s[11];
      s[11] = s[7];
      s[7] = s[3];
      s[3] = t;

      if (round != 10) {
        for (size_t c = 0; c < 16; c += 4) {
          unsigned char a0 = s[c], a1 = s[c + 1], a2 = s[c + 2], a3 = s[c + 3];
          s[c] = (unsigned char)(AESXTime(a0) ^ AESXTime(a1) ^ a1 ^ a2 ^ a3);
          s[c + 1] =
              (unsigned char)(a0 ^ AESXTime(a1) ^ AESXTime(a2) ^ a2 ^ a3);
          s[c + 2] =
              (unsigned char)(a0 ^ a1 ^ AESXTime(a2) ^ AESXTime(a3) ^ a3);
          s[c + 3] =
              (unsigned char)(AESXTime(a0) ^ a0 ^ a1 ^ a2 ^ AESXTime(a3));
        }
      }

      for (size_t i = 0; i < 16; ++i) s[i] ^= m_roundKeys[round * 16 + i];
    }

    memcpy(out, s, 16);
  }

 private:
  unsigned char m_roundKeys[176];
};

// Yahoo! KeyKey encrypted the exported cache database with SQLite SEE under the
// fixed key MANJUSRI_EXPORT_KEY. We ship no SEE codec (the KEY clause on ATTACH
// is a no-op here), so the block is decrypted in software instead. The scheme,
// recovered from real KeyKey export files:
//
//   key      the passphrase repeated cyclically until it fills 16 bytes
//   cipher   AES-128-CTR, keystream block = AES(key, counter block)
//   IV       the last 16 bytes of each page's reserved area, random per export
//   counter  IV bytes 4..7, little endian, incremented once per 16-byte block
//   clear    every page's 32 reserved bytes, plus bytes 16..23 of page 1 (the
//            page-size and format fields SQLite has to read before decrypting)
//
// ChiaKey's own Export writes the block in the clear, so a plaintext database
// is passed through untouched. Returns false when the data is neither, leaving
// it to the caller to skip the cache rather than fail the whole import.
inline bool DecryptExportDatabase(std::string& data) {
  if (data.size() >= kSQLiteHeaderSize &&
      !memcmp(data.data(), kSQLiteHeader, kSQLiteHeaderSize)) {
    return true;
  }

  // Byte 20 is the per-page reserved size; SEE stored the nonce there, and 32
  // is the only width the KeyKey builds ever wrote.
  if (data.size() < 24) return false;
  const unsigned char* header = (const unsigned char*)data.data();
  if (header[20] != 32) return false;

  size_t pageSize = ((size_t)header[16] << 8) | header[17];
  if (pageSize == 1) pageSize = 65536;  // SQLite's encoding for 64 KiB pages
  if (pageSize < 512 || pageSize > 65536 || (pageSize & (pageSize - 1))) {
    return false;
  }
  if (data.size() % pageSize) return false;

  unsigned char key[16];
  const char* phrase = MANJUSRI_EXPORT_KEY;
  size_t phraseSize = strlen(phrase);
  if (!phraseSize) return false;
  for (size_t i = 0; i < sizeof(key); ++i) {
    key[i] = (unsigned char)phrase[i % phraseSize];
  }
  AES128Encryptor cipher(key);

  const size_t reserved = 32;
  std::string result(data);
  for (size_t offset = 0; offset < data.size(); offset += pageSize) {
    unsigned char* page = (unsigned char*)&result[offset];
    unsigned char counter[16];
    memcpy(counter, page + pageSize - 16, sizeof(counter));

    unsigned int base =
        (unsigned int)counter[4] | ((unsigned int)counter[5] << 8) |
        ((unsigned int)counter[6] << 16) | ((unsigned int)counter[7] << 24);

    unsigned char keystream[16];
    for (size_t i = 0; i < pageSize - reserved; ++i) {
      if (!(i % 16)) {
        unsigned int value = (unsigned int)(base + i / 16);
        counter[4] = (unsigned char)(value);
        counter[5] = (unsigned char)(value >> 8);
        counter[6] = (unsigned char)(value >> 16);
        counter[7] = (unsigned char)(value >> 24);
        cipher.encryptBlock(counter, keystream);
      }
      page[i] ^= keystream[i % 16];
    }

    // Page 1 keeps its page-size and format fields in the clear.
    if (!offset) memcpy(page + 16, data.data() + 16, 8);
  }

  if (memcmp(result.data(), kSQLiteHeader, kSQLiteHeaderSize)) return false;

  data.swap(result);
  return true;
}

}  // namespace Manjusri

#endif
