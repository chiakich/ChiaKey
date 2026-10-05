#import "ChiaKeyUpdateService.h"

@interface ChiaKeyUpdateService (LegacyTests)
- (ChiaKeyUpdateRelease *)_releaseFromJSON:(NSDictionary *)json;
- (ChiaKeyUpdateRelease *)_releaseFromManifestEntry:(NSDictionary *)entry;
@end

static void Check(BOOL condition) {
  if (!condition) { fputs("Legacy Mac release selection failed\n", stderr); exit(1); }
}

int main(void) {
  @autoreleasepool {
    ChiaKeyUpdateService *service = [[[ChiaKeyUpdateService alloc] init] autorelease];
    NSString *digest = [@"" stringByPaddingToLength:64 withString:@"a" startingAtIndex:0];
    NSDictionary *(^joint)(NSString *, BOOL) = ^NSDictionary *(NSString *tag, BOOL beta) {
      return @{@"tag_name": tag, @"prerelease": @(beta), @"draft": @NO,
        @"assets": @[
          @{@"name": @"ChiaKey-Windows-Setup.exe", @"browser_download_url": @"https://example.invalid/windows.exe"},
          @{@"name": @"ChiaKey-test.pkg", @"browser_download_url": @"https://example.invalid/mac.pkg", @"digest": [@"sha256:" stringByAppendingString:digest]}
        ]};
    };
    // The same candidate/filter/compare policy as shipped GitHub fallback,
    // using each old implementation's parser and comparator.
    NSArray *releases = @[joint(@"v1.2.8-beta.1", YES), joint(@"v1.2.7", NO),
      @{@"tag_name": @"win-v0.1.0-beta.1", @"prerelease": @YES, @"draft": @NO,
        @"assets": @[@{@"name": @"ChiaKey-Windows-0.1.0-beta.1-Setup.exe", @"browser_download_url": @"https://example.invalid/old.exe"}]}];
    for (NSNumber *acceptBeta in @[@NO, @YES]) {
      ChiaKeyUpdateRelease *best = nil;
      for (NSDictionary *json in releases) {
        if ([json[@"draft"] boolValue] || ([json[@"prerelease"] boolValue] && !acceptBeta.boolValue)) continue;
        ChiaKeyUpdateRelease *candidate = [service _releaseFromJSON:json];
        if (!candidate) continue;
        if (best && [ChiaKeyUpdateService compareVersion:best.tag toVersion:candidate.tag] != NSOrderedAscending) continue;
        best = candidate;
      }
      Check([best.tag isEqual:acceptBeta.boolValue ? @"v1.2.8-beta.1" : @"v1.2.7"]);
      Check([best.packageURL isEqual:@"https://example.invalid/mac.pkg"]);
      Check([best.packageSHA256 isEqual:digest]);
    }
    for (NSString *tag in @[@"v1.2.7", @"v1.2.8-beta.1"]) {
      NSDictionary *entry = @{@"tag": tag, @"package_name": @"ChiaKey-test.pkg",
        @"package_url": @"https://example.invalid/mac.pkg", @"sha256": digest,
        @"prerelease": @([tag containsString:@"beta"]), @"published_at": @"2026-10-02T00:00:00Z",
        @"notes_url": @"https://example.invalid/mac-notes.md", @"version": [tag substringFromIndex:1]};
      ChiaKeyUpdateRelease *release = [service _releaseFromManifestEntry:entry];
      Check([release.tag isEqual:tag] && [release.packageURL isEqual:@"https://example.invalid/mac.pkg"]);
      Check([release.packageSHA256 isEqual:digest]);
    }
    puts("Legacy Mac CDN entries and joint GitHub selection: OK");
  }
  return 0;
}
