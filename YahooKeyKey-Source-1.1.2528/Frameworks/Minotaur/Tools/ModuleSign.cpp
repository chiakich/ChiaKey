/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#include <PlainVanilla/PlainVanilla.h>
#else
#include "OpenVanilla.h"
#include "PlainVanilla.h"
#endif

#include <stdio.h>

#include <sstream>

#include "Minotaur.h"
#include "ModuleDigestSharedSecret.h"

using namespace std;
using namespace OpenVanilla;
using namespace Minotaur;

int ModulePackageTool(bool signFunction, const string& inPackageRoot,
                      const string& key) {
  string packageRoot = OVPathHelper::PathCat(inPackageRoot, "Contents");
  string binaryRoot = OVPathHelper::PathCat(packageRoot, "MacOS");

  string plistPath =
      OVPathHelper::PathCat(packageRoot, kMinotaurModulePackagePlistName);

  if (!OVPathHelper::PathExists(plistPath) ||
      OVPathHelper::IsDirectory(plistPath)) {
    cerr << "file doesn't exist: " << plistPath << endl;
    return 1;
  }

  PVPropertyList* plist = new PVPropertyList(plistPath);
  PVPlistValue* root = plist->rootDictionary();

  PVPlistValue* mpIdVal = root->valueForKey(kMinotaurModulePackageIdentifier);
  PVPlistValue* mpBinVal =
      root->valueForKey(kMinotaurModulePackagePrimaryBinary);

  if (!mpIdVal || !mpBinVal) {
    cerr << "Invalid plist file" << endl;
    delete plist;
    return 0;
  }

  string mpId = mpIdVal->stringValue();
  string mpBin = mpBinVal->stringValue();
  string binPath = OVPathHelper::PathCat(binaryRoot, mpBin);

  cout << "module package identifier: " << mpId << endl;
  cout << "primary binary: " << mpBin << " (" << binPath << ")" << endl;

  if (!OVPathHelper::PathExists(binPath) ||
      OVPathHelper::IsDirectory(binPath)) {
    cerr << "Primary binary doesn't exist: " << binPath << endl;
    delete plist;
    return 0;
  }

  pair<char*, size_t> binData = OVFileHelper::SlurpFile(binPath);
  if (!binData.first || !binData.second) {
    cerr << "Invalid binary: " << binPath << endl;
    delete plist;
    return 0;
  }

  pair<char*, size_t> keyData = OVFileHelper::SlurpFile(key);
  if (!keyData.first || !keyData.second) {
    cerr << "Invalid key: " << key << endl;
    free(binData.first);
    delete plist;
    return 0;
  }

  string secret = mpId + TAKAO_MODULE_DIGEST_SHARED_SECRET;
  pair<char*, size_t> block;
  block.second = secret.size() + binData.second;
  block.first = (char*)calloc(1, block.second);
  memcpy(block.first, secret.data(), secret.size());
  free(binData.first);

  char* digest = Minos::Digest(block.first, block.second);
  free(block.first);

  if (signFunction) {
    pair<char*, size_t> encryptedDigest = Minos::Encrypt(
        pair<char*, size_t>(digest, Minos::DigestSize()), keyData);

    ostringstream sstr;
    sstr << hex;

    cout << "encrypted (" << encryptedDigest.second << "bytes): ";
    for (size_t i = 0; i < encryptedDigest.second; i++) {
      sstr.width(2);
      sstr.fill('0');
      unsigned char c = encryptedDigest.first[i];
      sstr << (unsigned int)c;
    }
    cout << sstr.str();
    cout << endl;

    free(encryptedDigest.first);
    root->setKeyValue(kMinotaurModulePackageSignature, sstr.str());
  } else {
    bool valid = false;

    PVPlistValue* sigValue = root->valueForKey(kMinotaurModulePackageSignature);
    if (sigValue) {
      string sig = sigValue->stringValue();
      cout << "Signature: " << sig << endl;

      pair<char*, size_t> binSig = Minos::BinaryFromHexString(sig);
      if (binSig.first && binSig.second) {
        pair<char*, size_t> decryptedDigest = Minos::GetBack(binSig, keyData);

        if (decryptedDigest.first && decryptedDigest.second) {
          if (decryptedDigest.second == Minos::DigestSize()) {
            valid = Minos::LazyMatch(digest, decryptedDigest.first,
                                     Minos::DigestSize());
          }
          free(decryptedDigest.first);
        }

        free(binSig.first);
      }
    }

    if (valid) {
      cout << "valid" << endl;
    } else {
      cout << "invalid" << endl;
    }
  }

  free(keyData.first);
  free(digest);

  if (signFunction) {
    plist->write();
  }

  delete plist;
  return 0;
}

int main(int argc, char* argv[]) {
#if defined(__APPLE__)
  PVPropertyList::CreateNSAutoreleasePoolInMain();
#endif

  if (argc < 2) {
    cerr << "usage: " << endl;
    cerr << "    verifymp <macOS Module Package Root> <Public Key>" << endl;
    cerr << "    signmp <macOS Module Package Root> <Private Key>" << endl;

    return 1;
  }

  string command = (const char*)argv[1];
  if (command == "verifymp" || command == "signmp") {
    if (argc < 4) {
      cerr << "see usage" << endl;
      return 1;
    }

    return ModulePackageTool((command == "signmp" ? true : false),
                             (const char*)argv[2], (const char*)argv[3]);
  }

  /*
  if (argc < 4) {
          cerr << "usage: " << endl;
          return 1;
  }

  pair<char*, size_t> privateKey = OVFileHelper::SlurpFile((const
  char*)argv[1]); pair<char*, size_t> publicKey = OVFileHelper::SlurpFile((const
  char*)argv[2]); pair<char*, size_t> signFile = OVFileHelper::SlurpFile((const
  char*)argv[3]);

  if (!privateKey.first || !publicKey.first || !signFile.first) {
          cerr << "error: " << endl;
          return 1;
  }

  char* shaDigest = Minos::Digest(signFile.first, signFile.second);
  cout << "file digest (" << Minos::DigestSize() << "bytes): ";
  for (size_t i = 0 ; i < Minos::DigestSize() ; i++) {
          printf("%02x", (unsigned char)shaDigest[i]);
  }
  cout << endl;

  pair<char*, size_t> digestBlock(shaDigest, Minos::DigestSize());
  pair<char*, size_t> encrypted = Minos::Encrypt(digestBlock, privateKey);

  cout << "encrypted (" << encrypted.second << "bytes): ";
  for (size_t i = 0 ; i < encrypted.second ; i++) {
          printf("%02x", (unsigned char)encrypted.first[i]);
  }
  cout << endl;


  pair<char*, size_t> decrypted = Minos::GetBack(encrypted, publicKey);

  cout << "decrypted (" << decrypted.second << "bytes): ";
  for (size_t i = 0 ; i < decrypted.second ; i++) {
          printf("%02x", (unsigned char)decrypted.first[i]);
  }
  cout << endl;

  free(decrypted.first);
  free(encrypted.first);
  free(shaDigest);
  free(signFile.first);
  free(publicKey.first);
  free(privateKey.first);
  */

  return 0;
}
