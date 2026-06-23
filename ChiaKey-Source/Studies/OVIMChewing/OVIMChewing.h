// [AUTO_HEADER]

#ifndef OVIMChewing_h
#define OVIMChewing_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include <string>
#include <vector>

#include "chewing.h"
#include "chewingio.h"

namespace OpenVanilla {

using namespace std;
class OVIMChewing;

class OVIMChewingContext : public OVEventHandlingContext {
 public:
  OVIMChewingContext(ChewingContext *chewingContext) : im(chewingContext) {}
  virtual void startSession(OVLoaderService *loaderService);
  virtual void endSession(OVLoaderService *loaderService);
  virtual bool handleKey(OVKey *key, OVTextBuffer *readingText,
                         OVTextBuffer *composingText,
                         OVCandidateService *candidateService,
                         OVLoaderService *loaderService);

 protected:
  void KeyPress(OVKey *key, OVTextBuffer *composingText,
                OVCandidateService *candidateService,
                OVLoaderService *loaderService);
  void DefaultKey(OVKey *key);
  void Capslock(OVKey *key);
  void Notify(OVLoaderService *loaderService);
  void CandidateWindow(OVCandidateService *candidateService,
                       OVLoaderService *loaderService);
  void Redraw(OVTextBuffer *composingText, OVLoaderService *loaderService);
  // OVIMChewing *parent;
  ChewingContext *im;
};

class OVIMChewing : public OVInputMethod {
 public:
  virtual OVEventHandlingContext *createContext() {
    return new OVIMChewingContext(chewingContext);
  }

  virtual const string identifier() const { return string("OVIMChewing"); }
  virtual const string localizedName(const string &locale);
  virtual bool initialize(OVPathInfo *pathInfo, OVLoaderService *loaderService);
  virtual void finalize();
  virtual void loadConfig(OVKeyValueMap *moduleConfig,
                          OVLoaderService *loaderService);

 protected:
  ChewingContext *chewingContext;
};
};  // namespace OpenVanilla

#endif
