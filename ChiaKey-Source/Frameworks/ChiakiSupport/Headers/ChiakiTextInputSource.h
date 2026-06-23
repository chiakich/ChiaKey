// [AUTO_HEADER]

#import <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __TISInputSource *TISInputSourceRef;

extern const CFStringRef kTISPropertyInputSourceType;
extern const CFStringRef kTISPropertyInputSourceID;
extern const CFStringRef kTISPropertyLocalizedName;
extern const CFStringRef kTISPropertyIconImageURL;
extern const CFStringRef kTISTypeKeyboardLayout;

extern void *TISGetInputSourceProperty(TISInputSourceRef inputSource,
                                       CFStringRef propertyKey);
extern CFArrayRef TISCreateInputSourceList(CFDictionaryRef properties,
                                           Boolean includeAllInstalled);

#ifdef __cplusplus
}
#endif
