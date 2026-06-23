//
// Developed for Yahoo! Taiwan by Lithoglyph Inc.
// Copyright (c) 2007-2010 Yahoo! Taiwan.
//

#ifndef YKSignedModuleLoadingSystem_h
#define YKSignedModuleLoadingSystem_h

#import <Foundation/Foundation.h>

#include "Minotaur.h"
#include "ModuleDigestSharedSecret.h"
#include "PlainVanilla.h"

extern "C" {
extern char ModulePublicKey[];
extern size_t ModulePublicKeySize;
};

namespace OpenVanilla {
using namespace std;
using namespace Minotaur;

class YKSignedModuleLoadingSystem : public PVCommonPackageLoadingSystem {
 public:
  YKSignedModuleLoadingSystem(PVLoaderPolicy* policy)
      : PVCommonPackageLoadingSystem(policy) {}

  virtual ~YKSignedModuleLoadingSystem() { unloadAllUnloadables(); }

  virtual void reset() {
    m_localizedNameMap.clear();
    m_versionMap.clear();
    PVCommonPackageLoadingSystem::reset();
  }

  virtual const string localizedNameForPackage(const string& name,
                                               OVLoaderService* loaderService) {
    map<string, PackageMetadata>::iterator iter = m_modulePackages.find(name);
    if (iter == m_modulePackages.end()) return name;

    map<string, string>::iterator mi = m_localizedNameMap.find(name);
    if (mi != m_localizedNameMap.end()) return (*mi).second;

    PackageMetadata& pkgdata = (*iter).second;
    if (!pkgdata.library) return name;

    NSDictionary* dict = (NSDictionary*)CFBundleGetLocalInfoDictionary(
        (CFBundleRef)pkgdata.library);
    NSString* displayName = [dict objectForKey:@"CFBundleDisplayName"];

    if (!displayName) return name;

    string ln = [displayName UTF8String];
    m_localizedNameMap[name] = ln;

    return ln;
  }

  const map<string, string> identifiableVersionMap() const {
    return m_versionMap;
  }

 protected:
  map<string, string> m_localizedNameMap;
  map<string, string> m_versionMap;

  virtual const string contentPathFromPackageRoot(const string& path) const {
    return OVPathHelper::PathCat(path, "Contents");
  }

  virtual const string binaryPathFromPackageRoot(const string& path) const {
    return OVPathHelper::PathCat(contentPathFromPackageRoot(path), "MacOS");
  }

  virtual void* loadLibrary(const string& path) {
    string plistPath = OVPathHelper::PathCat(contentPathFromPackageRoot(path),
                                             kMinotaurModulePackagePlistName);

    if (!OVPathHelper::PathExists(plistPath)) return 0;

    PVPropertyList plist(plistPath);
    PVPlistValue* dict = plist.rootDictionary();
    string primaryBinaryPath =
        dict->stringValueForKey(kMinotaurModulePackagePrimaryBinary);

    if (!primaryBinaryPath.length()) {
      NSLog(@"Invalid module: %s", path.c_str());
      return 0;
    }

    string mpId = dict->stringValueForKey(kMinotaurModulePackageIdentifier);
    if (!mpId.length()) return 0;

    primaryBinaryPath = OVPathHelper::PathCat(binaryPathFromPackageRoot(path),
                                              primaryBinaryPath);

    pair<char*, size_t> binData = OVFileHelper::SlurpFile(primaryBinaryPath);
    if (!binData.first || !binData.second) {
      NSLog(@"Cannot load binary: %s", primaryBinaryPath.c_str());
      return 0;
    }

    pair<char*, size_t> keyData(ModulePublicKey, ModulePublicKeySize);

    string secret = mpId + TAKAO_MODULE_DIGEST_SHARED_SECRET;
    pair<char*, size_t> block;
    block.second = secret.size() + binData.second;
    block.first = (char*)calloc(1, block.second);
    memcpy(block.first, secret.data(), secret.size());
    free(binData.first);

    char* digest = Minos::Digest(block.first, block.second);
    free(block.first);

    bool valid = false;

    string sig = dict->stringValueForKey(kMinotaurModulePackageSignature);
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

    free(digest);

    if (!valid) return 0;

    string mpver = dict->stringValueForKey(kMinotaurModulePackageVersion);
    if (mpId.length() && mpver.length()) m_versionMap[mpId] = mpver;

    return internalLoadLibrary(path);
  }

  virtual void* internalLoadLibrary(const string& path) {
    CFStringRef pathString =
        CFStringCreateWithCString(NULL, path.c_str(), kCFStringEncodingUTF8);
    if (!pathString) return 0;

    CFURLRef url = CFURLCreateWithFileSystemPath(NULL, pathString,
                                                 kCFURLPOSIXPathStyle, false);
    CFRelease(pathString);
    if (!url) return 0;

    CFBundleRef bundle = CFBundleCreate(NULL, url);
    CFRelease(url);

    if (!bundle) return 0;

    if (CFBundleLoadExecutable(bundle)) return (void*)bundle;

    CFRelease(bundle);
    return 0;
  }

  virtual bool unloadLibrary(void* library) {
    if (!library) return false;

    CFBundleRef bundle = (CFBundleRef)library;
    CFBundleUnloadExecutable(bundle);
    CFRelease(bundle);
    return true;
  }

  virtual void* getFunctionNamed(void* library, const string& name) {
    if (!library) return 0;

    CFStringRef funcString =
        CFStringCreateWithCString(NULL, name.c_str(), kCFStringEncodingUTF8);
    if (!funcString) return 0;

    CFBundleRef bundle = (CFBundleRef)library;
    void* function = CFBundleGetFunctionPointerForName(bundle, funcString);
    CFRelease(funcString);
    return function;
  }
};
};
#endif
