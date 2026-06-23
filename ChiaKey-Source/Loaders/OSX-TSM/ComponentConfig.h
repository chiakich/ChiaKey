// [AUTO_HEADER]

#ifndef ComponentConfig_h
#define ComponentConfig_h

// shared constants

#ifndef TSMC_SCRIPT
#define TSMC_SCRIPT 2  // smTradCinese
#endif

#ifndef TSMC_LANGUAGE
#define TSMC_LANGUAGE 19  // langtradChinese
#endif

#ifndef TSMC_RESOURCE_ID
#define TSMC_RESOURCE_ID \
  16896  // (15872+script_id*512) here script_id=smTradChinese
#endif

#ifndef TSMC_FLAGS
#define TSMC_FLAGS 0x8000 + TSMC_SCRIPT * 0x100 + TSMC_LANGUAGE
#endif

#ifndef TSMC_VERSION
#define TSMC_VERSION 0x01000000  // OpenVanilla 1.0.0
#endif

#ifndef TSMC_VENDOR
#define TSMC_VENDOR 'ltgf'
#endif

#ifndef TSMC_NAME
#define TSMC_NAME "Lithoglyph"
#endif

#ifndef TSMC_NAME_LENGTH
#define TSMC_NAME_LENGTH $ "0a"
#endif

#ifndef TSMC_BUNDLE_ID
#define TSMC_BUNDLE_ID "com.lithoglyph.tsm"
#endif

#ifndef TSMC_BUNDLE_ID_LENGTH
#define TSMC_BUNDLE_ID_LENGTH $ "12"
#endif

// #define TSMC_SCRIPT             25          // smSimpCinese
// #define TSMC_LANGUAGE           33          // langsimpChinese
// #define TSMC_RESOURCE_ID         28672       // (15872+script_id*512) here
// script_id=smSimpChinese #define TSMC_FLAGS               0x8000 + TSMC_SCRIPT
// * 0x100 + TSMC_LANGUAGE

// #define TSMC_SCRIPT             0x7e        // smUnicodeScript
// #define TSMC_LANGUAGE           0           // langEnglish
// #define TSMC_RESOURCE_ID        0xa600      // Unicode component ID
// #define TSMC_FLAGS              0x0000fe4a  // Unicode component flags

#define TSMC_PENCIL_MENU_ID TSMC_RESOURCE_ID + 1

#define TARGET_REZ_MAC_PPC 1
#define TARGET_REZ_MAC_X86 1

#endif
