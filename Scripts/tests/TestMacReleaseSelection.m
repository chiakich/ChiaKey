#import "ChiaKeyUpdateService.h"

@interface ChiaKeyUpdateService (ReleaseTests)
- (ChiaKeyUpdateRelease *)_releaseFromJSON:(NSDictionary *)json;
- (ChiaKeyUpdateRelease *)_releaseFromManifestEntry:(NSDictionary *)entry;
@end

static void Check(BOOL condition, NSString *message) {
  if (!condition) {
    NSLog(@"FAIL: %@", message);
    exit(1);
  }
}

int main(void) {
  @autoreleasepool {
    ChiaKeyUpdateService *service = [[[ChiaKeyUpdateService alloc] init] autorelease];
    NSDictionary *package = @{@"name": @"ChiaKey-1.2.7.pkg",
      @"browser_download_url": @"https://example.invalid/ChiaKey-1.2.7.pkg"};
    for (NSString *tag in @[@"v1.2.7", @"v1.2.7-beta.1"]) {
      Check([service _releaseFromJSON:@{@"tag_name": tag, @"assets": @[package]}] != nil,
            @"stable and beta macOS packages remain selectable");
      Check([service _releaseFromManifestEntry:@{@"tag": tag,
              @"package_url": package[@"browser_download_url"]}] != nil,
            @"stable and beta macOS manifests remain selectable");
    }
    for (id tag in @[@"win-v99.0.0", @"win-v99.0.0-beta.1", @"windows-v99.0.0",
                     @"v1.2.7-invalid", @"v1.2.7-beta.0", @42, [NSNull null]]) {
      Check([service _releaseFromJSON:@{@"tag_name": tag, @"assets": @[package]}] == nil,
            @"foreign or invalid tags are rejected even with a macOS asset");
      Check([service _releaseFromManifestEntry:@{@"tag": tag,
              @"package_url": package[@"browser_download_url"]}] == nil,
            @"foreign or invalid manifest tags are rejected");
    }
    Check([service _releaseFromJSON:@{@"tag_name": @"v99.0.0", @"assets": @[]} ] == nil,
          @"a release without a macOS package cannot hide an older usable release");
    Check([service _releaseFromJSON:@{@"tag_name": @"v99.0.0", @"assets": @[
            @{@"name": @"ChiaKey-Windows-99.0.0-Setup.exe",
              @"browser_download_url": @"https://example.invalid/windows.exe"}]}] == nil,
          @"a Windows installer is not a macOS package");
    Check([ChiaKeyUpdateService compareVersion:@"win-v0.1.0-beta.1" toVersion:@"v1.2.6"] == NSOrderedAscending,
          @"the current Windows preview sorts below macOS for legacy comparison");
    Check([ChiaKeyUpdateService compareVersion:@"win-v99.0.0-beta.1" toVersion:@"v1.2.6"] == NSOrderedDescending,
          @"a future higher Windows version shows why legacy beta clients need care");
    puts("Mac release selection: OK");
  }
  return 0;
}
