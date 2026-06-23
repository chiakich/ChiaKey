/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoHelper.h"

#import "TakaoSettings.h"

@implementation TakaoHelper

+ (NSString *)cacheFolder {
  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory,
                                                       NSUserDomainMask, YES);
  NSString *basePath =
      ([paths count] > 0) ? [paths objectAtIndex:0] : NSTemporaryDirectory();
  NSString *tempPath = [basePath stringByAppendingPathComponent:CACHE_FOLDER];
  if (![[NSFileManager defaultManager] fileExistsAtPath:tempPath
                                            isDirectory:NULL]) {
    [[NSFileManager defaultManager] createDirectoryAtPath:tempPath
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:nil];
  }
  return tempPath;
}

+ (NSString *)plistFilePath:(NSString *)filename {
  NSString *libPath = [NSSearchPathForDirectoriesInDomains(
      NSLibraryDirectory, NSUserDomainMask, YES) objectAtIndex:0];
  NSString *prefPath = [libPath stringByAppendingPathComponent:@"Preferences"];

  if (![[NSFileManager defaultManager] fileExistsAtPath:prefPath
                                            isDirectory:NULL]) {
    [[NSFileManager defaultManager] createDirectoryAtPath:prefPath
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:nil];
  }
  return [prefPath stringByAppendingPathComponent:filename];
}

@end
