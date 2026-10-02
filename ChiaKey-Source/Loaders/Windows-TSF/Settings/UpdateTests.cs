// Offline regression tests. Downloads are fixtures; never registers startup or launches installers.
using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Text;
using System.Web.Script.Serialization;

namespace ChiaKey.Settings
{
    internal static class UpdateTests
    {
        private static int checks;
        private static void Check(bool condition, string message)
        {
            ++checks;
            if (!condition) throw new Exception(message);
        }
        private static void Reject(Action action, string message)
        {
            try { action(); } catch (InvalidDataException) { ++checks; return; }
            throw new Exception(message);
        }
        private static string Manifest(string version, string hash, string date)
        {
            return new JavaScriptSerializer().Serialize(new {
                version = version, database_schema_version = 1, generated_at = date,
                artifacts = new[] {
                    new { kind = "chiakey-source-db", filename = "ChiaKeySource.db", sha256 = hash,
                        url = UpdateService.LexiconCdn + "ChiaKeySource.db" },
                    new { kind = "checksum", filename = "SHA256SUMS", sha256 = "",
                        url = UpdateService.LexiconRepository + version + "/SHA256SUMS" }
                }
            });
        }

        private static int Main(string[] args)
        {
            string root = Path.Combine(args[0], "updates-test-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            try
            {
                Run(root, args[1]);
                Console.WriteLine("Passed " + checks + " updater checks (offline).");
                return 0;
            }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
            finally { Directory.Delete(root, true); }
        }

        private static void Run(string root, string databasePath)
        {
            foreach (string bad in new[] { "../escape", "..", "a\\b", "CON.db", "version.", "a:b", "a\nsecond" })
                Reject(delegate { UpdateService.Component(bad); }, "unsafe component accepted: " + bad);
            Check(UpdateService.Component("2026.10.2-abc") == "2026.10.2-abc", "safe component");
            foreach (string bad in new[] { "http://github.com/chiakich/ChiaKey/releases/download/v1/a", "https://github.com.evil/chiakich/ChiaKey/releases/download/v1/a",
                "https://github.com/chiakich/ChiaKey/releases/download/../evil", "https://github.com/chiakich/ChiaKey/releases/download/%2e%2e/a" })
                Reject(delegate { UpdateService.AllowedUrl(bad, UpdateService.AppRepository); }, "unsafe URL accepted");
            Check(UpdateService.CompareVersions("2026.10.2", "2026.9.30") > 0, "numeric version order");
            Check(UpdateService.CompareVersions("1.2.3", "1.2.3.0") == 0, "padded version equality");
            Reject(delegate { UpdateService.CompareVersions("1.2-beta", "1.2"); }, "unsupported version accepted");
            Check(UpdateService.CompareAppVersions("0.1.0-beta.10", "0.1.0-beta.2") > 0, "numeric beta order");
            Check(UpdateService.CompareAppVersions("0.1.0", "0.1.0-beta.1") > 0, "stable follows beta");
            Check(UpdateService.CompareAppVersions("0.2.0-beta.1", "0.1.0") > 0, "next version beta follows older stable");
            Check(UpdateService.CompareAppVersions("0.1.0-beta.1", "0.1.0-beta.1") == 0, "same beta equality");
            Reject(delegate { UpdateService.CompareAppVersions("0.1.0-beta.0", "0.1.0"); }, "invalid beta accepted");
            string digest = new string('a', 64);
            Check(UpdateService.ListedDigest(digest + "  file.db\r\n", "file.db") == digest, "CRLF checksums");
            Reject(delegate { UpdateService.ListedDigest(digest + "  file.db\n" + digest + "  file.db\n", "file.db"); }, "duplicate checksum accepted");
            Reject(delegate { UpdateService.ListedDigest(digest + "  other.db\n", "file.db"); }, "missing checksum accepted");
            Check(UpdateService.ListedDigest(digest + "  other.db\n", "metadata.json", false) == null, "optional metadata may be absent from checksum list");
            Reject(delegate { UpdateService.ListedDigest(digest + "  metadata.json\n" + digest + "  metadata.json\n", "metadata.json", false); }, "duplicate optional metadata checksum accepted");

            string executable = Path.Combine(root, "ChiaKeySettings.exe");
            byte[] database = File.ReadAllBytes(databasePath);
            string dbHash = UpdateService.Hash(database);
            string version = "9999.1.1";
            string manifest = Manifest(version, dbHash, "2020-01-01T00:00:00Z");
            string checksums = dbHash + "  ChiaKeySource.db\n";
            byte[] payload = database;
            int downloads = 0;
            UpdateService service = new UpdateService(Path.Combine(root, "state"), executable, new Version("0.1.0"));
            service.Fetch = delegate(string url, long limit)
            {
                ++downloads;
                if (url.EndsWith("lexicon-manifest.json")) return Encoding.UTF8.GetBytes(manifest);
                if (url.EndsWith("SHA256SUMS")) return Encoding.UTF8.GetBytes(checksums);
                if (url.EndsWith("ChiaKeySource.db")) return payload;
                throw new Exception("Unexpected URL: " + url);
            };
            var defaults = service.Preferences;
            Check(defaults.GetBool("AutoUpdateApp", true) && defaults.GetBool("AutoUpdateLexicon", true), "automatic updates default on independently");
            defaults.SetBool("AutoUpdateApp", false); defaults.SetBool("AutoUpdateLexicon", false); defaults.Save();
            service.AutomaticPass(); Check(downloads == 0, "disabled updater must stay offline");
            UpdateService.ValidateDatabase(databasePath);
            UpdateOffer offer = service.CheckLexicon();
            Check(offer != null && offer.Version == version, "lexicon offer");
            service.InstallLexicon(offer); // includes actual native-engine load + 你好 smoke test
            string pointer = Path.Combine(service.Root, "Lexicons", "active.txt");
            string firstPointer = File.ReadAllText(pointer);
            Check(service.CurrentLexiconVersion() == version, "installed version is visible");
            int before = downloads; service.InstallLexicon(offer);
            Check(downloads == before, "same version does not redownload");

            manifest = Manifest("9999.1.2", dbHash, "2020-01-01T00:00:00Z");
            offer = service.CheckLexicon();
            checksums = new string('0', 64) + "  ChiaKeySource.db\n";
            Reject(delegate { service.InstallLexicon(offer); }, "cross-origin disagreement accepted");
            Check(File.ReadAllText(pointer) == firstPointer, "checksum-list rejection preserves pointer");
            checksums = dbHash + "  ChiaKeySource.db\n";
            payload = Encoding.UTF8.GetBytes("truncated download");
            Reject(delegate { service.InstallLexicon(offer); }, "truncated database accepted");
            Check(File.ReadAllText(pointer) == firstPointer, "hash rejection preserves pointer");
            manifest = Manifest("9999.1.2", UpdateService.Hash(payload), "2020-01-01T00:00:00Z");
            checksums = UpdateService.Hash(payload) + "  ChiaKeySource.db\n";
            offer = service.CheckLexicon();
            Reject(delegate { service.InstallLexicon(offer); }, "invalid SQLite accepted with valid hash");
            Check(File.ReadAllText(pointer) == firstPointer, "DB rejection preserves pointer");
            payload = database; checksums = dbHash + "  ChiaKeySource.db\n";
            manifest = Manifest("9999.1.2", dbHash, "2020-01-01T00:00:00Z");
            offer = service.CheckLexicon();
            Action<string> native = service.CoreValidator;
            service.CoreValidator = delegate { throw new InvalidDataException("fixture: core refuses database"); };
            Reject(delegate { service.InstallLexicon(offer); }, "core rejection ignored");
            Check(File.ReadAllText(pointer) == firstPointer, "engine rejection preserves pointer");
            service.CoreValidator = native;
            service.InstallLexicon(offer);
            Check(service.CurrentLexiconVersion() == "9999.1.2", "second update activated");
            string[] lines = File.ReadAllLines(pointer);
            Check(lines[1] == firstPointer.Split('\n')[0], "previous version retained");
            File.Delete(Path.Combine(service.Root, "Lexicons", "versions", lines[0], "ChiaKeySource.db"));
            service.RecoverLexicon();
            Check(service.CurrentLexiconVersion() == "9999.1.1", "missing current falls back to previous");
            Check(service.CheckLexicon() != null, "failed release can be retried after rollback");

            service.InstallLexicon(service.CheckLexicon());
            string versionsRoot = Path.Combine(service.Root, "Lexicons", "versions");
            string second = File.ReadAllLines(pointer)[0];
            manifest = Manifest("9999.1.3", dbHash, "2020-01-01T00:00:00Z");
            service.InstallLexicon(service.CheckLexicon());
            lines = File.ReadAllLines(pointer);
            Check(lines[1] == second && Directory.GetDirectories(versionsRoot).Length == 2,
                "third update prunes versions older than current and previous");
            Check(!Directory.Exists(Path.Combine(versionsRoot, firstPointer.Split('\n')[0])),
                "oldest lexicon directory removed");

            string appName = "ChiaKey-Windows-0.2.0-beta.1-Setup.exe";
            byte[] appBytes = Encoding.ASCII.GetBytes("MZoffline installer fixture");
            string appHash = UpdateService.Hash(appBytes);
            var assets = new[] { new { name = appName, browser_download_url = UpdateService.AppRepository + "win-v0.2.0-beta.1/" + appName },
                new { name = "SHA256SUMS.txt", browser_download_url = UpdateService.AppRepository + "win-v0.2.0-beta.1/SHA256SUMS.txt" } };
            string releases = new JavaScriptSerializer().Serialize(new[] {
                new { tag_name = "v99.0.0", draft = false, prerelease = false, published_at = "2020-01-01T00:00:00Z", assets = assets },
                new { tag_name = "win-v0.2.0-beta.1", draft = true, prerelease = true, published_at = "2020-01-01T00:00:00Z", assets = assets },
                new { tag_name = "win-v0.2.0-beta.1", draft = false, prerelease = true, published_at = "2020-01-01T00:00:00Z", assets = assets }
            });
            service.Fetch = delegate(string url, long limit)
            {
                if (url == UpdateService.AppCdn + "appcast.json") throw new WebException("fixture CDN unavailable");
                if (url.StartsWith("https://api.github.com/")) return Encoding.UTF8.GetBytes(releases);
                if (url.EndsWith("SHA256SUMS.txt")) return Encoding.UTF8.GetBytes(appHash + "  " + appName + "\n");
                if (url.EndsWith(".exe")) return appBytes;
                throw new Exception("Unexpected app URL");
            };
            Check(service.CheckApp() == null, "beta releases excluded by default");
            UpdateOffer app = service.CheckApp(true);
            Check(app != null && app.Version == "0.2.0-beta.1", "Windows prerelease selected, Mac and drafts ignored");
            string installer = service.DownloadApp(app);
            string oldInstaller = installer;
            installer = service.DownloadApp(app);
            Check(!Directory.Exists(Path.GetDirectoryName(oldInstaller)) &&
                Directory.GetDirectories(Path.Combine(service.Root, "Downloads")).Length == 1,
                "new installer download prunes old download directories");
            File.AppendAllText(installer, "tamper");
            Reject(delegate { UpdateService.InstallApp(installer, app.Sha256); }, "modified cached installer executed");
            app.Sha256 = new string('0', 64);
            Reject(delegate { service.DownloadApp(app); }, "app checksum ignored");

            string stableName = "ChiaKey-Windows-0.1.1-Setup.exe";
            var stableAssets = new[] {
                new { name = stableName, browser_download_url = UpdateService.AppRepository + "win-v0.1.1/" + stableName },
                new { name = "SHA256SUMS.txt", browser_download_url = UpdateService.AppRepository + "win-v0.1.1/SHA256SUMS.txt" }
            };
            releases = new JavaScriptSerializer().Serialize(new[] {
                new { tag_name = "win-v0.1.1", draft = false, prerelease = false, published_at = "2020-01-01T00:00:00Z", assets = stableAssets },
                new { tag_name = "win-v0.2.0-beta.1", draft = false, prerelease = false, published_at = "2020-01-01T00:00:00Z", assets = assets }
            });
            service.Fetch = delegate(string url, long limit)
            {
                if (url == UpdateService.AppCdn + "appcast.json") throw new WebException("fixture CDN unavailable");
                if (url.StartsWith("https://api.github.com/")) return Encoding.UTF8.GetBytes(releases);
                if (url.EndsWith("SHA256SUMS.txt")) return Encoding.UTF8.GetBytes(appHash + "  " + appName + "\n" + appHash + "  " + stableName + "\n");
                throw new Exception("Unexpected channel URL");
            };
            Check(service.CheckApp(false).Version == "0.1.1", "stable selected when beta channel disabled even if beta tag is not marked prerelease");
            Check(service.CheckApp(true).Version == "0.2.0-beta.1", "beta channel includes newer beta alongside stable");
            var channels = service.Preferences; channels.SetBool("IncludeBetaReleases", true); channels.Save();
            Check(service.CheckApp().Version == "0.2.0-beta.1", "background check honors saved beta option");
            channels.SetBool("IncludeBetaReleases", false); channels.Save();
            Check(service.CheckApp().Version == "0.1.1", "saved beta opt-out restores stable channel");

            // A shared v* release is stable for Mac and still carries the Windows preview.
            string jointName = "ChiaKey-Windows-1.2.7-Setup.exe";
            var jointAssets = new[] {
                new { name = jointName, browser_download_url = UpdateService.AppRepository + "v1.2.7/" + jointName },
                new { name = "ChiaKey-1.2.7.pkg", browser_download_url = UpdateService.AppRepository + "v1.2.7/ChiaKey-1.2.7.pkg" },
                new { name = "SHA256SUMS.txt", browser_download_url = UpdateService.AppRepository + "v1.2.7/SHA256SUMS.txt" }
            };
            releases = new JavaScriptSerializer().Serialize(new[] {
                new { tag_name = "v1.2.7", draft = false, prerelease = false, published_at = "2020-01-01T00:00:00Z", assets = jointAssets },
                new { tag_name = "v99.0.0", draft = false, prerelease = false, published_at = "2020-01-01T00:00:00Z", assets = new[] {
                    new { name = "ChiaKey-99.0.0.pkg", browser_download_url = UpdateService.AppRepository + "v99.0.0/ChiaKey-99.0.0.pkg" }
                } }
            });
            string jointList = appHash + "  " + jointName + "\n";
            int apiRequests = 0;
            string appFeed = null;
            service.Fetch = delegate(string url, long limit)
            {
                if (url == UpdateService.AppCdn + "appcast.json") {
                    if (appFeed == null) throw new WebException("fixture CDN unavailable");
                    return Encoding.UTF8.GetBytes(appFeed);
                }
                if (url.StartsWith("https://api.github.com/")) { ++apiRequests; return Encoding.UTF8.GetBytes(releases); }
                if (url.EndsWith("SHA256SUMS.txt")) return Encoding.UTF8.GetBytes(jointList);
                throw new Exception("Unexpected joint release URL: " + url);
            };
            Check(service.CheckApp(false).Version == "1.2.7", "joint stable selected; Mac-only release ignored");
            var jointEntry = new { tag = "v1.2.7", prerelease = false, package_name = jointName,
                package_url = UpdateService.AppCdn + "releases/v1.2.7/" + jointName,
                sha256 = appHash, published_at = "2020-01-01T00:00:00Z" };
            appFeed = new JavaScriptSerializer().Serialize(new { schema = 1, platform = "windows", stable = jointEntry, beta = jointEntry });
            apiRequests = 0;
            Check(service.CheckApp(false).Url.StartsWith(UpdateService.AppCdn), "platform CDN selected");
            Check(apiRequests == 0, "healthy CDN does not query GitHub release list");
            Check(service.CheckApp(true).Version == "1.2.7", "Beta followers also receive newer stable");
            string validAppFeed = appFeed;
            appFeed = validAppFeed.Replace("\"windows\"", "\"macos\"");
            Check(service.CheckApp(false).Version == "1.2.7" && apiRequests > 0, "wrong platform falls back to filtered GitHub");
            appFeed = validAppFeed.Replace(appHash, new string('0', 64));
            Check(service.CheckApp(false).Url.StartsWith(UpdateService.AppRepository), "CDN checksum disagreement falls back to GitHub verified offer");
            appFeed = validAppFeed;
            var currentService = new UpdateService(Path.Combine(root, "current-state"), executable, new Version("1.2.7"));
            currentService.Fetch = service.Fetch;
            apiRequests = 0;
            Check(currentService.CheckApp(false) == null && apiRequests == 0, "up-to-date CDN avoids GitHub fallback");

            // Automatic age gate and daily throttle; never invokes an installer.
            int autoFetches = 0;
            var options = service.Preferences;
            options.SetBool("AutoUpdateApp", false); options.SetBool("AutoUpdateLexicon", true); options.Save();
            Check(!service.Preferences.GetBool("AutoUpdateApp", true) && service.Preferences.GetBool("AutoUpdateLexicon", true), "app opt-out preserves lexicon automatic updates");
            manifest = Manifest("9999.1.4", dbHash, DateTime.UtcNow.ToString("o"));
            service.Fetch = delegate(string url, long limit)
            {
                ++autoFetches;
                if (url.EndsWith("lexicon-manifest.json")) return Encoding.UTF8.GetBytes(manifest);
                throw new Exception("too-new automatic update attempted to download");
            };
            service.AutomaticPass(); Check(autoFetches == 1, "new release waits three days");
            service.AutomaticPass(); Check(autoFetches == 1, "automatic checks throttled for one day");
            Check(File.ReadAllText(Path.Combine(service.Root, "status.txt")).Contains("三天"), "age gate status persisted");
        }
    }
}
