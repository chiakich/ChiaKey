// Updates run in a desktop helper, never in a host application's TSF/key thread.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Net;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using System.Xml;
using Microsoft.Win32;

namespace ChiaKey.Settings
{
    internal sealed class UpdateOffer
    {
        public string Version, Url, Filename, Sha256, Manifest;
        public DateTime Published;
        public bool Lexicon;
    }

    internal sealed class UpdateService
    {
        internal const string AppRepository = "https://github.com/chiakich/ChiaKey/releases/download/";
        internal const string LexiconRepository = "https://github.com/chiakich/ChiaKey-Lexicon/releases/download/";
        internal const string LexiconCdn = "https://cdn.chiaki.ch/chiakey/lexicon/";
        internal readonly string Root, Executable;
        internal readonly Version AppVersion;
        internal readonly string AppReleaseVersion;
        internal Func<string, long, byte[]> Fetch;
        internal Action<string> CoreValidator;
        private static readonly UTF8Encoding Utf8 = new UTF8Encoding(false, true);
        private static readonly JavaScriptSerializer Json = new JavaScriptSerializer { MaxJsonLength = 8 * 1024 * 1024 };

        internal UpdateService(string root, string executable, Version version, string releaseVersion = null)
        {
            Root = root; Executable = executable; AppVersion = version;
            AppReleaseVersion = releaseVersion ?? version.ToString(3);
            Fetch = Download;
            CoreValidator = ValidateCore;
        }

        internal static UpdateService Default()
        {
            // ChiaKey itself is low integrity and writable by Store apps. Executable downloads
            // and the activation pointer must live in a separate, medium-integrity directory.
            Assembly assembly = Assembly.GetExecutingAssembly();
            var release = (AssemblyInformationalVersionAttribute)Attribute.GetCustomAttribute(
                assembly, typeof(AssemblyInformationalVersionAttribute));
            return new UpdateService(Path.Combine(Environment.GetFolderPath(
                Environment.SpecialFolder.ApplicationData), "ChiaKeyUpdates"),
                Application.ExecutablePath, assembly.GetName().Version,
                release != null ? release.InformationalVersion : null);
        }

        internal Plist Preferences { get { return new Plist(Path.Combine(Root, "Updates.plist")); } }

        internal static string Component(string value)
        {
            if (value == null || value.Length > 150 ||
                !Regex.IsMatch(value, @"\A[A-Za-z0-9][A-Za-z0-9._-]*\z") || value.Contains("..") ||
                Regex.IsMatch(value, @"\A(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)", RegexOptions.IgnoreCase) ||
                value.EndsWith("."))
                throw new InvalidDataException("更新資料包含不合法的檔名或版本。");
            return value;
        }

        internal static string AllowedUrl(string value, params string[] prefixes)
        {
            Uri uri;
            if (!Uri.TryCreate(value, UriKind.Absolute, out uri) || uri.Scheme != "https" ||
                !string.IsNullOrEmpty(uri.UserInfo) || !uri.IsDefaultPort ||
                !string.IsNullOrEmpty(uri.Query) || !string.IsNullOrEmpty(uri.Fragment) ||
                value.Contains("..") || value.Contains("%") || value.Contains("\\"))
                throw new InvalidDataException("更新下載網址不合法。");
            foreach (string prefix in prefixes)
                if (value.StartsWith(prefix, StringComparison.Ordinal)) return value;
            throw new InvalidDataException("更新來源不在允許的發布位置。");
        }

        internal static string Digest(string value)
        {
            if (value == null || !Regex.IsMatch(value, @"\A[0-9a-fA-F]{64}\z"))
                throw new InvalidDataException("更新資料缺少有效的 SHA-256。");
            return value.ToLowerInvariant();
        }

        internal static string Hash(byte[] bytes)
        {
            using (SHA256 sha = SHA256.Create())
                return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant();
        }

        internal static string ListedDigest(string checksums, string filename, bool required = true)
        {
            string found = null;
            foreach (string line in checksums.Split('\n'))
            {
                Match match = Regex.Match(line.TrimEnd('\r'), @"\A([0-9a-fA-F]{64})[ \t]+\*?([^\r\n]+)\z");
                if (!match.Success || match.Groups[2].Value != filename) continue;
                if (found != null) throw new InvalidDataException("校驗清單中有重複的檔名。");
                found = Digest(match.Groups[1].Value);
            }
            if (found == null && required) throw new InvalidDataException("校驗清單沒有這個更新檔案。");
            return found;
        }

        private static byte[] Download(string url, long limit)
        {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
            Uri current = new Uri(url);
            for (int redirects = 0; redirects < 6; ++redirects)
            {
                HttpWebRequest request = (HttpWebRequest)WebRequest.Create(current);
                request.UserAgent = "ChiaKey-Windows-Updater";
                request.Accept = "application/vnd.github+json";
                request.AllowAutoRedirect = false;
                request.Timeout = 30000; request.ReadWriteTimeout = 30000;
                using (HttpWebResponse response = (HttpWebResponse)request.GetResponse())
                {
                    if ((int)response.StatusCode >= 300 && (int)response.StatusCode < 400)
                    {
                        Uri next = new Uri(current, response.Headers["Location"]);
                        if (next.Scheme != "https" || !next.IsDefaultPort || !string.IsNullOrEmpty(next.UserInfo) ||
                            !(next.Host == "github.com" || next.Host == "cdn.chiaki.ch" ||
                              next.Host == "release-assets.githubusercontent.com" || next.Host == "objects.githubusercontent.com"))
                            throw new InvalidDataException("更新下載重新導向至不允許的位置。");
                        current = next; continue;
                    }
                    if (response.ContentLength > limit) throw new InvalidDataException("更新檔案超過大小限制。");
                    using (Stream input = response.GetResponseStream())
                    using (MemoryStream output = new MemoryStream())
                    {
                        byte[] buffer = new byte[65536]; int count;
                        while ((count = input.Read(buffer, 0, buffer.Length)) != 0)
                        {
                            if (output.Length + count > limit) throw new InvalidDataException("更新檔案超過大小限制。");
                            output.Write(buffer, 0, count);
                        }
                        return output.ToArray();
                    }
                }
            }
            throw new InvalidDataException("更新下載重新導向次數過多。");
        }

        private static Dictionary<string, object> Object(object value)
        {
            Dictionary<string, object> result = value as Dictionary<string, object>;
            if (result == null) throw new InvalidDataException("更新資料格式不正確。");
            return result;
        }
        private static string Text(Dictionary<string, object> value, string key)
        {
            object result;
            if (!value.TryGetValue(key, out result) || !(result is string))
                throw new InvalidDataException("更新資料缺少 " + key + "。");
            return (string)result;
        }
        private static string OptionalText(Dictionary<string, object> value, string key)
        {
            object result; return value.TryGetValue(key, out result) && result is string ? (string)result : "";
        }
        private static DateTime Date(string text)
        {
            DateTime result;
            if (!DateTime.TryParse(text, CultureInfo.InvariantCulture, DateTimeStyles.AdjustToUniversal |
                DateTimeStyles.AssumeUniversal, out result)) throw new InvalidDataException("更新日期不正確。");
            return result;
        }
        private static IEnumerable<Dictionary<string, object>> Items(object value)
        {
            IEnumerable list = value as IEnumerable;
            if (list == null || value is string || value is Dictionary<string, object>)
                throw new InvalidDataException("更新清單格式不正確。");
            foreach (object item in list) yield return Object(item);
        }
        private static Dictionary<string, object> Artifact(Dictionary<string, object> manifest, string kind, bool required)
        {
            Dictionary<string, object> result = null;
            foreach (var item in Items(manifest["artifacts"]))
                if (Text(item, "kind") == kind)
                {
                    if (result != null) throw new InvalidDataException("更新資料有重複的 artifact。");
                    result = item;
                }
            if (required && result == null) throw new InvalidDataException("更新資料缺少 " + kind + "。");
            return result;
        }

        internal static int CompareVersions(string a, string b)
        {
            if (!Regex.IsMatch(a ?? "", @"\A\d+(\.\d+)*\z") || !Regex.IsMatch(b ?? "", @"\A\d+(\.\d+)*\z"))
                throw new InvalidDataException("更新版本格式不正確。");
            string[] left = a.Split('.'), right = b.Split('.');
            for (int i = 0; i < Math.Max(left.Length, right.Length); ++i)
            {
                long x = i < left.Length ? long.Parse(left[i], CultureInfo.InvariantCulture) : 0;
                long y = i < right.Length ? long.Parse(right[i], CultureInfo.InvariantCulture) : 0;
                int result = x.CompareTo(y); if (result != 0) return result;
            }
            return 0;
        }

        internal static int CompareAppVersions(string a, string b)
        {
            Match left = Regex.Match(a ?? "", @"\A(\d+\.\d+\.\d+)(?:-beta\.([1-9][0-9]*))?\z");
            Match right = Regex.Match(b ?? "", @"\A(\d+\.\d+\.\d+)(?:-beta\.([1-9][0-9]*))?\z");
            if (!left.Success || !right.Success) throw new InvalidDataException("更新版本格式不正確。");
            int result = CompareVersions(left.Groups[1].Value, right.Groups[1].Value);
            if (result != 0) return result;
            bool leftBeta = left.Groups[2].Success, rightBeta = right.Groups[2].Success;
            if (leftBeta != rightBeta) return leftBeta ? -1 : 1;
            return leftBeta ? CompareVersions(left.Groups[2].Value, right.Groups[2].Value) : 0;
        }

        internal string CurrentLexiconVersion()
        {
            string pointer = Path.Combine(Root, "Lexicons", "active.txt");
            if (File.Exists(pointer))
            {
                string[] lines = File.ReadAllLines(pointer, Utf8);
                if (lines.Length > 0 && lines[0] != "")
                {
                    string manifest = Path.Combine(Root, "Lexicons", "versions", Component(lines[0]), "lexicon-manifest.json");
                    if (File.Exists(manifest)) return Text(Object(Json.DeserializeObject(File.ReadAllText(manifest, Utf8))), "version");
                }
            }
            string bundled = Path.Combine(Path.GetDirectoryName(Executable), "ChiaKeySource.db");
            if (!File.Exists(bundled)) return "0";
            using (Sqlite db = new Sqlite(bundled))
            {
                string version = db.Scalar("SELECT value FROM chiaki_db_metadata WHERE key='lexicon_release_version';");
                return string.IsNullOrEmpty(version) ? "0" : version;
            }
        }

        internal UpdateOffer CheckLexicon()
        {
            string manifest;
            try { manifest = Utf8.GetString(Fetch(LexiconCdn + "lexicon-manifest.json", 1024 * 1024)); }
            catch (WebException) { manifest = Utf8.GetString(Fetch("https://github.com/chiakich/ChiaKey-Lexicon/releases/latest/download/lexicon-manifest.json", 1024 * 1024)); }
            var data = Object(Json.DeserializeObject(manifest));
            if (Convert.ToString(data["database_schema_version"], CultureInfo.InvariantCulture) != "1")
                throw new InvalidDataException("詞庫 schema 尚未支援。");
            string version = Component(Text(data, "version"));
            if (version.Length > 80) throw new InvalidDataException("詞庫版本名稱過長。");
            string minimum = OptionalText(data, "minimum_app_version");
            if (minimum != "" && CompareVersions(AppVersion.ToString(), minimum) < 0)
                throw new InvalidDataException("這份詞庫需要先更新千秋輸入法。");
            var db = Artifact(data, "chiakey-source-db", true);
            var checksum = Artifact(data, "checksum", true);
            AllowedUrl(Text(checksum, "url"), LexiconRepository + version + "/");
            Component(Text(checksum, "filename"));
            string filename = Component(Text(db, "filename"));
            if (!filename.EndsWith(".db", StringComparison.Ordinal)) throw new InvalidDataException("詞庫檔名必須是 .db。");
            UpdateOffer offer = new UpdateOffer { Lexicon = true, Version = version, Manifest = manifest,
                Url = AllowedUrl(Text(db, "url"), LexiconCdn, LexiconRepository + version + "/"),
                Filename = filename, Sha256 = Digest(Text(db, "sha256")),
                Published = Date(Text(data, "generated_at")) };
            return CompareVersions(version, CurrentLexiconVersion()) > 0 ? offer : null;
        }

        internal UpdateOffer CheckApp()
        {
            return CheckApp(Preferences.GetBool("IncludeBetaReleases", false));
        }

        internal UpdateOffer CheckApp(bool includeBeta)
        {
            // Windows currently ships as prereleases; /latest would return the Mac release.
            UpdateOffer newest = null;
            for (int page = 1; page <= 5; ++page)
            {
                var releases = Items(Json.DeserializeObject(Utf8.GetString(Fetch(
                    "https://api.github.com/repos/chiakich/ChiaKey/releases?per_page=100&page=" + page, 8 * 1024 * 1024))));
                int count = 0;
                foreach (var release in releases)
                {
                    ++count;
                    if (Convert.ToBoolean(release["draft"], CultureInfo.InvariantCulture)) continue;
                    string tag = Text(release, "tag_name");
                    if (!Regex.IsMatch(tag, @"\Awin-v\d+\.\d+\.\d+(?:-beta\.[1-9][0-9]*)?\z")) continue;
                    string version = tag.Substring("win-v".Length);
                    if (!includeBeta && (version.Contains("-beta.") ||
                        Convert.ToBoolean(release["prerelease"], CultureInfo.InvariantCulture))) continue;
                    if (CompareAppVersions(version, AppReleaseVersion) <= 0 ||
                        (newest != null && CompareAppVersions(version, newest.Version) <= 0)) continue;
                    string name = "ChiaKey-Windows-" + version + "-Setup.exe";
                    string url = null, checksum = null;
                    foreach (var asset in Items(release["assets"]))
                    {
                        if (Text(asset, "name") == name) url = AllowedUrl(Text(asset, "browser_download_url"), AppRepository + tag + "/");
                        if (Text(asset, "name") == "SHA256SUMS.txt") checksum = AllowedUrl(Text(asset, "browser_download_url"), AppRepository + tag + "/");
                    }
                    if (url == null || checksum == null) continue;
                    newest = new UpdateOffer { Version = version, Filename = name, Url = url,
                        Sha256 = ListedDigest(Utf8.GetString(Fetch(checksum, 1024 * 1024)), name),
                        Published = Date(Text(release, "published_at")) };
                }
                if (count < 100) break;
            }
            return newest;
        }

        internal static void AtomicText(string path, string contents)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            string temporary = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                using (FileStream stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                {
                    byte[] bytes = Utf8.GetBytes(contents); stream.Write(bytes, 0, bytes.Length); stream.Flush(true);
                }
                if (File.Exists(path)) File.Replace(temporary, path, null);
                else File.Move(temporary, path);
            }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
        }

        private void PrepareRoot()
        {
            Directory.CreateDirectory(Root);
            // Read-only access for AppContainers; unlike the learning folder this is not low integrity.
            DirectorySecurity acl = Directory.GetAccessControl(Root);
            acl.AddAccessRule(new FileSystemAccessRule(new SecurityIdentifier("S-1-15-2-1"),
                FileSystemRights.ReadAndExecute, InheritanceFlags.ContainerInherit | InheritanceFlags.ObjectInherit,
                PropagationFlags.None, AccessControlType.Allow));
            Directory.SetAccessControl(Root, acl);
        }

        internal void InstallLexicon(UpdateOffer offer)
        {
            PrepareRoot();
            using (FileStream guard = new FileStream(Path.Combine(Root, "install.lock"), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None))
            {
                // Recheck while locked; another settings window/helper may have installed it.
                if (CompareVersions(offer.Version, CurrentLexiconVersion()) <= 0) return;
                var manifest = Object(Json.DeserializeObject(offer.Manifest));
                var checksum = Artifact(manifest, "checksum", true);
                string list = Utf8.GetString(Fetch(AllowedUrl(Text(checksum, "url"), LexiconRepository + offer.Version + "/"), 1024 * 1024));
                if (ListedDigest(list, offer.Filename) != offer.Sha256)
                    throw new InvalidDataException("詞庫 manifest 與 GitHub 校驗清單不一致。");
                byte[] database = Fetch(offer.Url, 512L * 1024 * 1024);
                if (Hash(database) != offer.Sha256) throw new InvalidDataException("詞庫 SHA-256 不符，未安裝。");
                string lexicons = Path.Combine(Root, "Lexicons");
                string directory = Path.Combine(lexicons, "versions", offer.Version + "-" + offer.Sha256.Substring(0, 12) + "-" + Guid.NewGuid().ToString("N"));
                Directory.CreateDirectory(directory);
                string path = Path.Combine(directory, "ChiaKeySource.db");
                bool activated = false;
                try
                {
                    File.WriteAllBytes(path, database);
                    ValidateDatabase(path); CoreValidator(path);
                    var metadata = Artifact(manifest, "metadata", false);
                    if (metadata != null)
                    {
                        string filename = Component(Text(metadata, "filename"));
                        if (filename == "ChiaKeySource.db" || filename == "lexicon-manifest.json")
                            throw new InvalidDataException("metadata 檔名與詞庫檔案衝突。");
                        byte[] bytes = Fetch(AllowedUrl(Text(metadata, "url"), LexiconCdn, LexiconRepository + offer.Version + "/"), 8 * 1024 * 1024);
                        string digest = Digest(Text(metadata, "sha256"));
                        if (Hash(bytes) != digest) throw new InvalidDataException("metadata SHA-256 不符。");
                        // Mac permits optional metadata absent from SHA256SUMS; when present it must agree.
                        string listed = ListedDigest(list, filename, false);
                        if (listed != null && listed != digest) throw new InvalidDataException("metadata 校驗清單不一致。");
                        File.WriteAllBytes(Path.Combine(directory, filename), bytes);
                    }
                    File.WriteAllText(Path.Combine(directory, "lexicon-manifest.json"), offer.Manifest, Utf8);
                    string pointer = Path.Combine(lexicons, "active.txt");
                    string[] existing = File.Exists(pointer) ? File.ReadAllLines(pointer, Utf8) : new string[0];
                    string previous = existing.Length > 0 && existing[0] != "" ? Component(existing[0]) : "";
                    AtomicText(pointer, Path.GetFileName(directory) + "\n" + previous + "\n");
                    activated = true;
                }
                finally { if (!activated) Directory.Delete(directory, true); }
            }
        }

        internal void RecoverLexicon()
        {
            string lexicons = Path.Combine(Root, "Lexicons");
            string pointer = Path.Combine(lexicons, "active.txt");
            if (!File.Exists(pointer)) return;
            using (FileStream guard = new FileStream(Path.Combine(Root, "install.lock"), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None))
            {
                string[] candidates = File.ReadAllLines(pointer, Utf8);
                for (int index = 0; index < Math.Min(candidates.Length, 2); ++index)
                {
                    if (candidates[index] == "") continue;
                    try
                    {
                        string directory = Path.Combine(lexicons, "versions", Component(candidates[index]));
                        string path = Path.Combine(directory, "ChiaKeySource.db");
                        string manifestPath = Path.Combine(directory, "lexicon-manifest.json");
                        if (!File.Exists(manifestPath)) throw new InvalidDataException("外部詞庫缺少 manifest。");
                        var manifest = Object(Json.DeserializeObject(File.ReadAllText(manifestPath, Utf8)));
                        CompareVersions(Text(manifest, "version"), "0");
                        ValidateDatabase(path); CoreValidator(path);
                        if (index > 0) AtomicText(pointer, candidates[index] + "\n\n");
                        return;
                    }
                    catch (Exception error)
                    {
                        if (!(error is IOException || error is InvalidDataException || error is XmlException ||
                            error is ArgumentException || error is OverflowException)) throw;
                    }
                }
                // Missing both external versions restores bundled input and lets the next
                // check retry the release rather than mistaking rejected data for current.
                AtomicText(pointer, "\n\n");
            }
        }

        internal string DownloadApp(UpdateOffer offer)
        {
            PrepareRoot();
            byte[] bytes = Fetch(offer.Url, 256L * 1024 * 1024);
            if (Hash(bytes) != offer.Sha256 || bytes.Length < 2 || bytes[0] != 'M' || bytes[1] != 'Z')
                throw new InvalidDataException("本體更新校驗失敗，未執行安裝器。");
            string directory = Path.Combine(Root, "Downloads", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            string path = Path.Combine(directory, Component(offer.Filename));
            File.WriteAllBytes(path, bytes);
            return path;
        }

        internal static void InstallApp(string path, string expectedHash)
        {
            // Recheck immediately before elevation, including cached downloads.
            if (Hash(File.ReadAllBytes(path)) != expectedHash) throw new InvalidDataException("安裝檔已改變，請重新下載。");
            Process.Start(new ProcessStartInfo(path, "/SP- /NORESTART") { UseShellExecute = true, Verb = "runas" });
        }

        internal void Configure(bool app, bool lexicon, bool includeBeta)
        {
            PrepareRoot();
            Plist preferences = Preferences;
            preferences.SetBool("AutoUpdateApp", app); preferences.SetBool("AutoUpdateLexicon", lexicon);
            preferences.SetBool("IncludeBetaReleases", includeBeta);
            preferences.Save();
            RegisterStartup();
            if (app || lexicon)
                Process.Start(new ProcessStartInfo(Executable, "/update-background") {
                    UseShellExecute = false, CreateNoWindow = true, WindowStyle = ProcessWindowStyle.Hidden });
        }

        internal void RegisterStartup()
        {
            Plist settings = Preferences;
            using (RegistryKey run = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows\CurrentVersion\Run"))
            {
                if (settings.GetBool("AutoUpdateApp", true) || settings.GetBool("AutoUpdateLexicon", true))
                    run.SetValue("ChiaKeyUpdates", "\"" + Executable + "\" /update-background");
                else run.DeleteValue("ChiaKeyUpdates", false);
            }
        }

        private void Status(string value)
        {
            AtomicText(Path.Combine(Root, "status.txt"), DateTime.Now.ToString("yyyy-MM-dd HH:mm", CultureInfo.InvariantCulture) + "  " + value);
        }

        internal void AutomaticPass()
        {
            Plist options = Preferences;
            bool app = options.GetBool("AutoUpdateApp", true), lexicon = options.GetBool("AutoUpdateLexicon", true);
            if (!app && !lexicon) return;
            PrepareRoot();
            // Cross-process lock also serializes the persisted daily gate.
            using (FileStream guard = new FileStream(Path.Combine(Root, "check.lock"), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None))
            {
                string stamp = Path.Combine(Root, "last-check.txt"); DateTime checkedAt;
                if (File.Exists(stamp) && DateTime.TryParse(File.ReadAllText(stamp), CultureInfo.InvariantCulture,
                    DateTimeStyles.RoundtripKind, out checkedAt) && DateTime.UtcNow - checkedAt < TimeSpan.FromDays(1)) return;
                AtomicText(stamp, DateTime.UtcNow.ToString("o", CultureInfo.InvariantCulture));
                List<string> results = new List<string>();
                if (lexicon)
                {
                    try
                    {
                        RecoverLexicon();
                        UpdateOffer offer = CheckLexicon();
                        if (offer != null && DateTime.UtcNow - offer.Published >= TimeSpan.FromDays(3))
                        {
                            InstallLexicon(offer); results.Add("詞庫已更新至 " + offer.Version);
                        }
                        else results.Add(offer == null ? "詞庫已是最新" : "詞庫等待發布滿三天");
                    }
                    catch (Exception error) { results.Add("詞庫更新失敗：" + error.Message); }
                }
                if (app)
                {
                    try
                    {
                        UpdateOffer offer = CheckApp();
                        if (offer != null && DateTime.UtcNow - offer.Published >= TimeSpan.FromDays(3))
                        {
                            string installer = DownloadApp(offer);
                            InstallApp(installer, offer.Sha256);
                            results.Add("已開啟 " + offer.Version + " 安裝器；完成後重新開啟應用程式");
                        }
                        else results.Add(offer == null ? "本體已是最新" : "本體等待發布滿三天");
                    }
                    catch (Exception error) { results.Add("本體更新失敗：" + error.Message); }
                }
                Status(string.Join("；", results.ToArray()));
            }
        }

        internal static void RunBackground()
        {
            bool created;
            using (Mutex single = new Mutex(true, "Local\\ChiaKey.UpdateBackground", out created))
            {
                // The previous version notices the changed startup command within a minute.
                // Let the new helper take over after it exits instead of losing the scheduler.
                if (!created)
                {
                    try { if (!single.WaitOne(70000)) return; }
                    catch (AbandonedMutexException) { }
                }
                UpdateService service = Default();
                while (true)
                {
                    // Upgrade registration points at the new version; leave old helper processes.
                    using (RegistryKey run = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Run"))
                    {
                        string command = run == null ? "" : run.GetValue("ChiaKeyUpdates", "") as string;
                        if (command != "\"" + service.Executable + "\" /update-background") return;
                    }
                    Plist options = service.Preferences;
                    if (!options.GetBool("AutoUpdateApp", true) && !options.GetBool("AutoUpdateLexicon", true)) return;
                    try { service.AutomaticPass(); }
                    catch (Exception error) { try { service.Status("更新失敗：" + error.Message); } catch (IOException) { } }
                    // A one-minute tick notices uninstall/upgrade/opt-out without another network request.
                    Thread.Sleep(60000);
                }
            }
        }

        [DllImport("ChiaKeyTsf.dll", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        private static extern int ChiaKeyUpdatesValidateCore(string path, string temporary);
        private static void ValidateCore(string path)
        {
            string temporary = Path.Combine(Path.GetTempPath(), "ChiaKeyUpdateProbe-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(temporary);
            try
            {
                if (ChiaKeyUpdatesValidateCore(path, temporary) == 0)
                    throw new InvalidDataException("詞庫無法由輸入引擎載入，保留原詞庫。");
            }
            finally { Directory.Delete(temporary, true); }
        }

        internal static void ValidateDatabase(string path)
        {
            using (Sqlite db = new Sqlite(path))
            {
                Require(db.Scalar("PRAGMA integrity_check;") == "ok", "SQLite integrity_check");
                foreach (string table in new[] { "cooked_information", "prepopulated_service_data", "unigrams", "bigrams", "Mandarin-bpmf-cin", "chiaki_db_metadata", "chiaki_db_sources" })
                    Require(db.Scalar("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='" + table + "';") == "1", table);
                Require(db.Scalar("SELECT value FROM chiaki_db_metadata WHERE key='schema_version';") == "1", "schema_version");
                Require(db.Scalar("SELECT COUNT(*) FROM cooked_information WHERE key='version' AND value!='';") == "1", "詞庫版本");
                Minimum(db, "SELECT COUNT(*) FROM unigrams;", 1000);
                Minimum(db, "SELECT COUNT(*) FROM 'Mandarin-bpmf-cin';", 1000);
                Minimum(db, "SELECT COUNT(*) FROM unigrams WHERE qstring='_punctuation_list';", 50);
                Minimum(db, "SELECT COUNT(*) FROM 'Mandarin-bpmf-cin' WHERE key='_punctuation_list';", 50);
                Minimum(db, "SELECT COUNT(*) FROM prepopulated_service_data WHERE key='canned_messages_timestamp' AND CAST(value AS INTEGER)>0;", 1);
                foreach (string key in new[] { "_punctuation_<", "_punctuation_Standard_<" })
                {
                    Require(db.Scalar("SELECT current FROM unigrams WHERE qstring='" + key + "' ORDER BY probability DESC,current LIMIT 1;") == "，", "全形標點");
                    Require(db.Scalar("SELECT value FROM 'Mandarin-bpmf-cin' WHERE key='" + key + "' ORDER BY rowid LIMIT 1;") == "，", "注音全形標點");
                }
                Require(db.Scalar("SELECT COUNT(*) FROM prepopulated_service_data WHERE key IN ('onekey_services','onekey_services_timestamp');") == "0", "禁止的 OneKey 資料");
                string canned = db.Scalar("SELECT value FROM prepopulated_service_data WHERE key='canned_messages' LIMIT 1;");
                Require(canned != null && canned.Length > 1000, "符號表資料");
                XmlDocument document = new XmlDocument(); document.XmlResolver = null;
                XmlReaderSettings settings = new XmlReaderSettings { DtdProcessing = DtdProcessing.Ignore, XmlResolver = null };
                using (XmlReader reader = XmlReader.Create(new StringReader(canned), settings)) document.Load(reader);
                Require(document.SelectNodes("/plist/dict/key[.='CannedMessages']/following-sibling::*[1][self::array]/dict").Count > 0, "符號表分類");
            }
        }
        private static void Require(bool condition, string name)
        {
            if (!condition) throw new InvalidDataException("詞庫驗證失敗：" + name);
        }
        private static void Minimum(Sqlite db, string sql, long minimum)
        {
            long count; Require(long.TryParse(db.Scalar(sql), out count) && count >= minimum, sql);
        }

        // Windows 10 ships WinSQLite. No downloaded code or extra runtime is needed for validation.
        internal sealed class Sqlite : IDisposable
        {
            private IntPtr database;
            [DllImport("winsqlite3.dll", CallingConvention = CallingConvention.Cdecl)] private static extern int sqlite3_open_v2(byte[] path, out IntPtr db, int flags, IntPtr vfs);
            [DllImport("winsqlite3.dll", CallingConvention = CallingConvention.Cdecl)] private static extern int sqlite3_close(IntPtr db);
            [DllImport("winsqlite3.dll", CallingConvention = CallingConvention.Cdecl)] private static extern int sqlite3_prepare_v2(IntPtr db, byte[] sql, int length, out IntPtr statement, IntPtr tail);
            [DllImport("winsqlite3.dll", CallingConvention = CallingConvention.Cdecl)] private static extern int sqlite3_step(IntPtr statement);
            [DllImport("winsqlite3.dll", CallingConvention = CallingConvention.Cdecl)] private static extern IntPtr sqlite3_column_text(IntPtr statement, int column);
            [DllImport("winsqlite3.dll", CallingConvention = CallingConvention.Cdecl)] private static extern int sqlite3_column_bytes(IntPtr statement, int column);
            [DllImport("winsqlite3.dll", CallingConvention = CallingConvention.Cdecl)] private static extern int sqlite3_finalize(IntPtr statement);
            internal Sqlite(string path)
            {
                if (sqlite3_open_v2(Utf8.GetBytes(path + "\0"), out database, 1, IntPtr.Zero) != 0)
                { Dispose(); throw new InvalidDataException("無法以唯讀模式開啟詞庫。"); }
            }
            internal string Scalar(string sql)
            {
                IntPtr statement;
                if (sqlite3_prepare_v2(database, Utf8.GetBytes(sql + "\0"), -1, out statement, IntPtr.Zero) != 0)
                    throw new InvalidDataException("詞庫缺少必要資料或欄位。");
                try
                {
                    int step = sqlite3_step(statement);
                    if (step == 101) return null;
                    if (step != 100) throw new InvalidDataException("詞庫查詢失敗。");
                    IntPtr value = sqlite3_column_text(statement, 0);
                    int size = sqlite3_column_bytes(statement, 0);
                    if (size > 8 * 1024 * 1024) throw new InvalidDataException("詞庫資料超過限制。");
                    byte[] bytes = new byte[size]; if (size > 0) Marshal.Copy(value, bytes, 0, size);
                    string result = Utf8.GetString(bytes);
                    Require(sqlite3_step(statement) == 101, "查詢結果不唯一");
                    return result;
                }
                finally { sqlite3_finalize(statement); }
            }
            public void Dispose() { if (database != IntPtr.Zero) { sqlite3_close(database); database = IntPtr.Zero; } }
        }
    }
}
