//
// NSBezierPathAdditions.h
//
// Copyright (c) 2007-2010 Lithoglyph Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface NSBezierPath (CVBezelPathAdditions)
+ (NSBezierPath*)bezelPathWithRect:(NSRect)boundingRect radius:(float)radius;
@end
