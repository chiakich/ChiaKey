/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

/*!
        @header TakaoSettings
*/

//#define LFLSTR(x)      [[NSBundle mainBundle] localizedStringForKey:x
// value:nil table:nil]

/*!
        @define PLIST_GLOBAL_FILENAME
        @abstract The file name of the plist file for global settings.
*/
#define PLIST_GLOBAL_FILENAME @"com.chiaki.KeyKey.plist"
/*!
        @define PLIST_PHONETIC_FILENAME
        @abstract The file name of the plist file for settings of the
        Traditional Phonetic Input Method.
*/
#define PLIST_PHONETIC_FILENAME @"com.chiaki.KeyKey.TraditionalMandarin.plist"
/*!
        @define PLIST_SMARTPHONETIC_FILENAME
        @abstract The file name of the plist file for settings of the
        Smart Phonetic Input Method.
*/
#define PLIST_SMARTPHONETIC_FILENAME @"com.chiaki.KeyKey.SmartMandarin.plist"
/*!
        @define PLIST_CANGJIE_FILENAME
        @abstract The file name of the plist file for settings of the
        Cangjie Phonetic Input Method.
*/
#define PLIST_CANGJIE_FILENAME @"com.chiaki.KeyKey.Generic-cj-cin.plist"
/*!
        @define PLIST_SIMPLEX_FILENAME
        @abstract The file name of the plist file for settings of the
        Simplex Phonetic Input Method.
*/
#define PLIST_SIMPLEX_FILENAME @"com.chiaki.KeyKey.Generic-simplex-cin.plist"
#define PLIST_GENERIC_FILENAME(x) \
  [NSString stringWithFormat:@"com.chiaki.KeyKey.%@.plist", x]
#define PLIST_WORDCOUNT_FILENAME @"com.chiaki.KeyKey.YKAFWordCount.plist"

/*!
        @define CACHE_FOLDER
        @abstract The name of the cache folder
*/
#define CACHE_FOLDER @"com.chiaki.KeyKey"
/*!
        @define OPENVANILLA_DO_CONNECTION_NAME
        @abstract The identifer of the DO connection between OpenVanilla
        server and the preference tool.
*/
#ifndef OPENVANILLA_DO_CONNECTION_NAME
#error Must define OPENVANILLA_DO_CONNECTION_NAME
#endif

#import "LFUtilities.h"
#import "OpenVanillaService.h"
