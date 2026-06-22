// [AUTO_HEADER]

#include "Minotaur.h"
#include "sqlite3.h"

using namespace Minotaur;

extern "C" {
// void sqlite3_activate_see(const char *zPassPhrase);
// void sqlite3_activate_cerod(const char *zKey);

// int sqlite3_key(
//    sqlite3 *db,        /* The connection from sqlite3_open() */
//    const void *pKey,   /* The key */
//    int nKey            /* Number of bytes in the key */
//);
}

extern "C" {
extern char VendorMotcle[];
extern size_t VendorMotcleSize;

extern char VendorDonneCle[];
extern size_t VendorDonneCleSizeMask1;
extern size_t VendorDonneCleSizeMask2;
extern size_t VendorDonneCleOffsetMask1;
extern size_t VendorDonneCleOffsetMask2;

extern char UserDonneCle[];
extern size_t UserDonneCleSizeMask1;
extern size_t UserDonneCleSizeMask2;
extern size_t UserDonneCleOffsetMask1;
extern size_t UserDonneCleOffsetMask2;
};

void InitSQLiteCrypto(sqlite3* db);
char* UserDonnecle();

#define REAL_OFFSET(p, q) (q ^ (p ^ 0xaaddffff))
#define REAL_SIZE(p, q) (q ^ (p ^ 0xffddffaa))

void InitSQLiteCrypto(sqlite3* db) {
  // sqlite3_activate_see("7bb07b8d471d642e");

  //    pair<char*, size_t> phrase = Minos::GetBack(VendorDonneCle +
  //    REAL_OFFSET(VendorDonneCleOffsetMask1, VendorDonneCleOffsetMask2),
  //    REAL_SIZE(VendorDonneCleSizeMask1, VendorDonneCleSizeMask2),
  //    VendorMotcle, VendorMotcleSize); if (phrase.first) {
  ////        cerr << "phrase: " << phrase.second << endl;
  //        sqlite3_key(db, phrase.first, (int)phrase.second);
  //        free(phrase.first);
  //    }
}

string FetchSQLiteCERODKey(const string& filename) {
  // sqlite3_activate_see("7bb07b8d471d642e");
  //	sqlite3_activate_cerod("7bb07b8d471d642e");

  string result = ":cerod:";

  pair<char*, size_t> phrase = Minos::GetBack(
      VendorDonneCle +
          REAL_OFFSET(VendorDonneCleOffsetMask1, VendorDonneCleOffsetMask2),
      REAL_SIZE(VendorDonneCleSizeMask1, VendorDonneCleSizeMask2), VendorMotcle,
      VendorMotcleSize);
  if (phrase.first) {
    string ph(phrase.first, phrase.second);
    result += ph;
  }

  result += ":";
  result += filename;
  return result;
}

pair<char*, size_t> ObtenirUserDonneCle() {
  return Minos::GetBack(UserDonneCle + REAL_OFFSET(UserDonneCleOffsetMask1,
                                                   UserDonneCleOffsetMask2),
                        REAL_SIZE(UserDonneCleSizeMask1, UserDonneCleSizeMask2),
                        VendorMotcle, VendorMotcleSize);
}
