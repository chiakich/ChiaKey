//
//  TakaoCINTable.m
//

#import "TakaoCINTable.h"

#import "TakaoSettings.h"

NSString *const TakaoCINTableDisplayNameKey = @"displayName";
NSString *const TakaoCINTableIdentifierKey = @"identifier";
NSString *const TakaoCINTableFileNameKey = @"fileName";
NSString *const TakaoCINTableEntryCountKey = @"entryCount";
NSString *const TakaoCINTableSelectionKeysKey = @"selectionKeys";
NSString *const TakaoCINTableSourceEncodingKey = @"sourceEncoding";
NSString *const TakaoCINTableContentKey = @"content";
NSString *const TakaoCINTablePathKey = @"path";

NSString *const TakaoCINTableErrorDomain = @"org.chiakey.CINTable";

// A table much larger than this is not something anyone typed by hand; the
// limit mostly exists so that picking the wrong file does not stall the UI
// while we parse a few hundred megabytes.
static const unsigned long long kMaximumTableFileSize = 20ULL * 1024 * 1024;

// These two identifiers are how the bundled Cangjie and Simplex tables are
// addressed (they are aliases onto cj-ext.cin / simplex-ext.cin, set up in
// OVIMGenericPackage::initialize). A user table claiming either name would
// shadow a built-in input method, so we refuse those file names.
static NSString *const kReservedIdentifiers[] = {@"Generic-cj-cin",
                                                 @"Generic-simplex-cin"};

@interface TakaoCINTable (Private)
+ (NSString *)userTableDirectoryPath;
+ (NSString *)existingUserTableDirectory;
+ (NSString *)identifierForFileName:(NSString *)fileName;
+ (NSError *)errorWithCode:(TakaoCINTableErrorCode)code
               description:(NSString *)description;
+ (NSString *)decodeData:(NSData *)data
        usedEncodingName:(NSString **)encodingName;
+ (NSString *)firstComponentOfPropertyValue:(NSString *)value;
+ (NSString *)normalizedFileNameForPath:(NSString *)path error:(NSError **)error;
@end

@implementation TakaoCINTable

#pragma mark Locations

+ (NSString *)userTableDirectoryPath {
  return [ChiaKeyServiceUserDataDirectory()
      stringByAppendingPathComponent:@"DataTables/Generic"];
}

// Read-only callers use this: merely opening the Preferences app should not
// leave a folder behind for a feature the user has not used.
+ (NSString *)existingUserTableDirectory {
  NSString *path = [self userTableDirectoryPath];
  BOOL isDirectory = NO;
  if (![[NSFileManager defaultManager] fileExistsAtPath:path
                                            isDirectory:&isDirectory])
    return nil;
  return isDirectory ? path : nil;
}

+ (NSString *)userTableDirectory {
  NSString *path = [self userTableDirectoryPath];

  NSFileManager *manager = [NSFileManager defaultManager];
  BOOL isDirectory = NO;
  if ([manager fileExistsAtPath:path isDirectory:&isDirectory])
    return isDirectory ? path : nil;

  // The parent holds a record of what the user has typed, so keep the same
  // 0700 the IME uses when it creates it.
  ChiaKeyEnsureUserDataDirectoryPrivate();
  if (![manager createDirectoryAtPath:path
          withIntermediateDirectories:YES
                           attributes:nil
                                error:NULL])
    return nil;

  return path;
}

+ (NSString *)identifierForFileName:(NSString *)fileName {
  return [NSString
      stringWithFormat:@"Generic-%@-cin",
                       [fileName stringByDeletingPathExtension]];
}

#pragma mark Errors

+ (NSError *)errorWithCode:(TakaoCINTableErrorCode)code
               description:(NSString *)description {
  return [NSError
      errorWithDomain:TakaoCINTableErrorDomain
                 code:code
             userInfo:[NSDictionary dictionaryWithObject:description
                                                  forKey:NSLocalizedDescriptionKey]];
}

#pragma mark Reading

// The CIN parser in the engine (OVCINDataTable) assumes UTF-8 and has no
// encoding detection whatsoever. Tables published before UTF-8 became the
// default -- which is most of the older Cantonese, Hakka and Taiwanese ones
// -- are Big5, and feeding one to the engine as-is does not fail loudly: it
// silently produces garbled candidates. So transcode at import time instead.
+ (NSString *)decodeData:(NSData *)data
          usedEncodingName:(NSString **)encodingName {
  NSString *text = [[[NSString alloc] initWithData:data
                                          encoding:NSUTF8StringEncoding]
      autorelease];
  if (text) {
    if (encodingName) *encodingName = nil;
    return text;
  }

  // Big5-HKSCS first: it is a superset of Big5 and covers the Hong Kong
  // supplementary characters that Cantonese tables rely on.
  NSStringEncoding candidates[] = {
      CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingBig5_HKSCS_1999),
      CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingBig5),
  };
  NSString *const names[] = {@"Big5-HKSCS", @"Big5"};

  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    text = [[[NSString alloc] initWithData:data
                                  encoding:candidates[i]] autorelease];
    if (text) {
      if (encodingName) *encodingName = names[i];
      return text;
    }
  }

  return nil;
}

// Strips the `name:locale;` decoration some tables use in %ename, e.g.
// "CantonHK:en;港式广东话:zh_CN;港式廣東話:zh;".
+ (NSString *)firstComponentOfPropertyValue:(NSString *)value {
  NSRange colon = [value rangeOfString:@":"];
  if (colon.location == NSNotFound) return value;
  return [value substringToIndex:colon.location];
}

#pragma mark Inspection

+ (NSDictionary *)inspectFileAtPath:(NSString *)path error:(NSError **)error {
  NSFileManager *manager = [NSFileManager defaultManager];

  NSDictionary *attributes = [manager attributesOfItemAtPath:path error:NULL];
  if (!attributes) {
    if (error)
      *error = [self errorWithCode:TakaoCINTableErrorUnreadable
                       description:LFLSTR(@"The file could not be read.")];
    return nil;
  }

  if ([attributes fileSize] > kMaximumTableFileSize) {
    if (error)
      *error = [self
          errorWithCode:TakaoCINTableErrorTooLarge
            description:LFLSTR(@"The file is too large to be an input table.")];
    return nil;
  }

  NSData *data = [NSData dataWithContentsOfFile:path];
  if (!data) {
    if (error)
      *error = [self errorWithCode:TakaoCINTableErrorUnreadable
                       description:LFLSTR(@"The file could not be read.")];
    return nil;
  }

  NSString *sourceEncoding = nil;
  NSString *text = [self decodeData:data usedEncodingName:&sourceEncoding];
  if (!text) {
    if (error)
      *error = [self
          errorWithCode:TakaoCINTableErrorUnknownEncoding
            description:LFLSTR(@"The text encoding of the file could not be "
                               @"recognized. ChiaKey can read UTF-8 and Big5 "
                               @"tables.")];
    return nil;
  }

  // Old tables are frequently CRLF- or CR-terminated; the engine splits on
  // newlines only, so normalize before we count anything or write it out.
  NSMutableString *normalized = [[text mutableCopy] autorelease];
  [normalized replaceOccurrencesOfString:@"\r\n"
                              withString:@"\n"
                                 options:NSLiteralSearch
                                   range:NSMakeRange(0, [normalized length])];
  [normalized replaceOccurrencesOfString:@"\r"
                              withString:@"\n"
                                 options:NSLiteralSearch
                                   range:NSMakeRange(0, [normalized length])];

  NSString *ename = nil, *cname = nil, *tcname = nil, *selectionKeys = nil;
  BOOL sawCharDefBegin = NO, sawCharDefEnd = NO, insideCharDef = NO;
  NSUInteger entryCount = 0;

  NSCharacterSet *whitespace = [NSCharacterSet whitespaceCharacterSet];

  for (NSString *rawLine in [normalized componentsSeparatedByString:@"\n"]) {
    NSString *line =
        [rawLine stringByTrimmingCharactersInSet:
                     [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (![line length] || [line hasPrefix:@"#"]) continue;

    if ([line hasPrefix:@"%"]) {
      NSRange separator = [line rangeOfCharacterFromSet:whitespace];
      if (separator.location == NSNotFound) continue;

      NSString *key = [line substringToIndex:separator.location];
      NSString *value = [[line substringFromIndex:separator.location]
          stringByTrimmingCharactersInSet:whitespace];

      if ([key isEqualToString:@"%chardef"]) {
        if ([value isEqualToString:@"begin"]) {
          sawCharDefBegin = YES;
          insideCharDef = YES;
        } else if ([value isEqualToString:@"end"]) {
          sawCharDefEnd = YES;
          insideCharDef = NO;
        }
      } else if ([key isEqualToString:@"%ename"]) {
        ename = value;
      } else if ([key isEqualToString:@"%cname"]) {
        cname = value;
      } else if ([key isEqualToString:@"%tcname"]) {
        tcname = value;
      } else if ([key isEqualToString:@"%selkey"]) {
        selectionKeys = value;
      }
      continue;
    }

    if (!insideCharDef) continue;

    // A usable entry is "<key sequence><whitespace><output>".
    NSRange separator = [line rangeOfCharacterFromSet:whitespace];
    if (separator.location == NSNotFound || separator.location == 0) continue;
    NSString *output = [[line substringFromIndex:separator.location]
        stringByTrimmingCharactersInSet:whitespace];
    if ([output length]) entryCount++;
  }

  if (!sawCharDefBegin || !sawCharDefEnd) {
    if (error)
      *error = [self
          errorWithCode:TakaoCINTableErrorNoCharDef
            description:LFLSTR(@"This does not look like a CIN table: it has "
                               @"no %chardef section.")];
    return nil;
  }

  if (!entryCount) {
    if (error)
      *error = [self errorWithCode:TakaoCINTableErrorNoEntries
                       description:LFLSTR(@"The table contains no entries.")];
    return nil;
  }

  // Mirror how the engine picks a name (OVIMGeneric.cpp): Traditional Chinese
  // falls back tcname -> cname -> ename, and that is what the input menu will
  // show, so preview the same thing.
  NSString *displayName = tcname;
  if (![displayName length]) displayName = cname;
  if (![displayName length])
    displayName = [self firstComponentOfPropertyValue:ename ? ename : @""];

  if (![displayName length]) {
    if (error)
      *error = [self
          errorWithCode:TakaoCINTableErrorNoName
            description:LFLSTR(@"The table has no name (%cname or %ename), so "
                               @"it cannot be shown in the input menu.")];
    return nil;
  }

  NSString *fileName = [self normalizedFileNameForPath:path error:error];
  if (!fileName) return nil;

  NSMutableDictionary *result = [NSMutableDictionary dictionary];
  [result setObject:displayName forKey:TakaoCINTableDisplayNameKey];
  [result setObject:fileName forKey:TakaoCINTableFileNameKey];
  [result setObject:[self identifierForFileName:fileName]
             forKey:TakaoCINTableIdentifierKey];
  [result setObject:[NSNumber numberWithUnsignedInteger:entryCount]
             forKey:TakaoCINTableEntryCountKey];
  if ([selectionKeys length])
    [result setObject:selectionKeys forKey:TakaoCINTableSelectionKeysKey];
  if (sourceEncoding)
    [result setObject:sourceEncoding forKey:TakaoCINTableSourceEncodingKey];
  [result setObject:[normalized dataUsingEncoding:NSUTF8StringEncoding]
             forKey:TakaoCINTableContentKey];

  return result;
}

// The module identifier is derived from the file name, so the file name has
// to survive that transformation intact: `.` becomes `-` and would split the
// identifier in unexpected places, and anything non-ASCII makes for an
// identifier the user cannot match up with what they see.
+ (NSString *)normalizedFileNameForPath:(NSString *)path
                                  error:(NSError **)error {
  NSString *base =
      [[[path lastPathComponent] stringByDeletingPathExtension] lowercaseString];

  NSMutableString *cleaned = [NSMutableString string];
  NSCharacterSet *allowed = [NSCharacterSet
      characterSetWithCharactersInString:
          @"abcdefghijklmnopqrstuvwxyz0123456789-"];

  for (NSUInteger i = 0; i < [base length]; i++) {
    unichar c = [base characterAtIndex:i];
    if ([allowed characterIsMember:c])
      [cleaned appendFormat:@"%C", c];
    else if ([cleaned length] && ![cleaned hasSuffix:@"-"])
      [cleaned appendString:@"-"];
  }

  while ([cleaned hasSuffix:@"-"])
    [cleaned deleteCharactersInRange:NSMakeRange([cleaned length] - 1, 1)];

  if (![cleaned length]) {
    if (error)
      *error = [self
          errorWithCode:TakaoCINTableErrorBadFileName
            description:LFLSTR(@"The file name contains no letters or digits "
                               @"that can be used as an input method name. "
                               @"Please rename the file and try again.")];
    return nil;
  }

  NSString *fileName = [cleaned stringByAppendingPathExtension:@"cin"];
  NSString *identifier = [self identifierForFileName:fileName];

  for (size_t i = 0;
       i < sizeof(kReservedIdentifiers) / sizeof(kReservedIdentifiers[0]);
       i++) {
    if ([identifier isEqualToString:kReservedIdentifiers[i]]) {
      if (error)
        *error = [self
            errorWithCode:TakaoCINTableErrorReservedName
              description:LFLSTR(@"This file name is reserved for a built-in "
                                 @"input method. Please rename the file and "
                                 @"try again.")];
      return nil;
    }
  }

  return fileName;
}

#pragma mark Installed tables

+ (NSArray *)installedTables {
  NSString *directory = [self existingUserTableDirectory];
  if (!directory) return [NSArray array];

  NSArray *contents = [[NSFileManager defaultManager]
      contentsOfDirectoryAtPath:directory
                          error:NULL];

  NSMutableArray *tables = [NSMutableArray array];
  for (NSString *fileName in contents) {
    if (![[fileName pathExtension] isEqualToString:@"cin"]) continue;

    NSString *path = [directory stringByAppendingPathComponent:fileName];
    NSDictionary *info = [self inspectFileAtPath:path error:NULL];

    NSMutableDictionary *entry = [NSMutableDictionary dictionary];
    if (info) {
      [entry addEntriesFromDictionary:info];
      [entry removeObjectForKey:TakaoCINTableContentKey];
    } else {
      // Still list it, so a table that has gone bad can be removed from here
      // rather than only from the Finder.
      [entry setObject:fileName forKey:TakaoCINTableDisplayNameKey];
      [entry setObject:fileName forKey:TakaoCINTableFileNameKey];
      [entry setObject:[self identifierForFileName:fileName]
                forKey:TakaoCINTableIdentifierKey];
    }
    [entry setObject:path forKey:TakaoCINTablePathKey];
    [tables addObject:entry];
  }

  return [tables sortedArrayUsingComparator:^(id a, id b) {
    return [[a objectForKey:TakaoCINTableDisplayNameKey]
        localizedCaseInsensitiveCompare:[b
                                            objectForKey:
                                                TakaoCINTableDisplayNameKey]];
  }];
}

+ (BOOL)isTableInstalledWithFileName:(NSString *)fileName {
  NSString *directory = [self existingUserTableDirectory];
  if (!directory) return NO;
  return [[NSFileManager defaultManager]
      fileExistsAtPath:[directory stringByAppendingPathComponent:fileName]];
}

#pragma mark Writing

+ (BOOL)installTable:(NSDictionary *)table
           overwrite:(BOOL)overwrite
               error:(NSError **)error {
  NSString *directory = [self userTableDirectory];
  if (!directory) {
    if (error)
      *error = [self
          errorWithCode:TakaoCINTableErrorWriteFailed
            description:LFLSTR(@"The folder for imported tables could not be "
                               @"created.")];
    return NO;
  }

  NSString *fileName = [table objectForKey:TakaoCINTableFileNameKey];
  NSString *path = [directory stringByAppendingPathComponent:fileName];

  if (!overwrite &&
      [[NSFileManager defaultManager] fileExistsAtPath:path]) {
    if (error)
      *error = [self errorWithCode:TakaoCINTableErrorWriteFailed
                       description:LFLSTR(@"A table with this name is already "
                                          @"installed.")];
    return NO;
  }

  NSData *content = [table objectForKey:TakaoCINTableContentKey];
  NSError *writeError = nil;
  if (![content writeToFile:path
                    options:NSDataWritingAtomic
                      error:&writeError]) {
    if (error)
      *error = writeError
                   ? writeError
                   : [self errorWithCode:TakaoCINTableErrorWriteFailed
                             description:LFLSTR(@"The table could not be "
                                                @"written.")];
    return NO;
  }

  return YES;
}

+ (BOOL)removeTableWithFileName:(NSString *)fileName error:(NSError **)error {
  NSString *directory = [self existingUserTableDirectory];
  if (!directory) return NO;

  // Guard against a crafted name walking out of the user table directory.
  if ([fileName length] != [[fileName lastPathComponent] length]) {
    if (error)
      *error = [self errorWithCode:TakaoCINTableErrorBadFileName
                       description:LFLSTR(@"Invalid table name.")];
    return NO;
  }

  NSString *path = [directory stringByAppendingPathComponent:fileName];
  return [[NSFileManager defaultManager] removeItemAtPath:path error:error];
}

@end
