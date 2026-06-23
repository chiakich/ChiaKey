//
// LFUtilities.h
//
// Copyright (c) 2007-2010 Lithoglyph Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#define LFRetainAssign(foo, bar) \
  do {                           \
    id tmp = (foo);              \
    foo = [(bar) retain];        \
    [tmp release];               \
  } while (0)
#define LFReleaseClean(foo) \
  do {                      \
    id tmp = (foo);         \
    foo = nil;              \
    [tmp release];          \
  } while (0)
#define LFAutoreleasedCopy(foo) [[(foo) copy] autorelease]
#define LFAutoreleasedRetain(foo) [[(foo) retain] autorelease]

#define LFLSTR(key) NSLocalizedString((key), nil)
#define LFLSTR2(key, comment) NSLocalizedString((key), (comment))

#define LFU8(s) ([s UTF8String])
#define LFU8LSTR(k) ([LFLSTR(k) UTF8String])

#define LFSTRNotNull(s) (s ? s : @"")
#define LFU8STRNotNull(s) (s ? s : "")
