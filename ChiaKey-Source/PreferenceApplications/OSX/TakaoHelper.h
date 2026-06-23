/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

#import "TakaoSettings.h"

/*
        @header TakaoHelper
*/

/*!
        @class TakaoHelper
        @abstract The class to offer some shared methods.
*/
@interface TakaoHelper : NSObject {
}
/*!
        @method cacheFolder
        @abstract Returns the path of the default cache folder.
        @result Returns the path of the default cache folder.
*/
+ (NSString *)cacheFolder;
/*!
        @method plistFilePath
        @abstract Returns the full path of a specified plist file.
        @param filename The filename of the specified plist file.
        @result Returns the full path of a specified plist file.
*/
+ (NSString *)plistFilePath:(NSString *)filename;
@end
