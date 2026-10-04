using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Xml;

namespace ChiaKey.Settings
{
    internal static class Ui
    {
        private static string language;
        private static readonly Dictionary<string, string> english = LoadEnglish();

        internal static string Language
        {
            get
            {
                if (language == null)
                {
                    string data = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                        "ChiaKey", "Preferences", "Windows.plist");
                    language = Normalize(new Plist(data).GetString("UiLanguage", "zh-TW"));
                }
                return language;
            }
            set { language = Normalize(value); }
        }

        internal static string Normalize(string value)
        {
            if (string.Equals(value, "en", StringComparison.OrdinalIgnoreCase)) return "en";
            if (string.Equals(value, "zh-CN", StringComparison.OrdinalIgnoreCase)) return "zh-CN";
            return "zh-TW";
        }

        private static Dictionary<string, string> LoadEnglish()
        {
            Dictionary<string, string> result = new Dictionary<string, string>(StringComparer.Ordinal);
            using (Stream stream = typeof(Ui).Assembly.GetManifestResourceStream("UiStrings.xml"))
            {
                if (stream == null) return result;
                XmlDocument document = new XmlDocument(); document.XmlResolver = null;
                document.Load(stream);
                foreach (XmlNode node in document.DocumentElement.ChildNodes)
                    result[node["source"].InnerText] = node["en"].InnerText;
            }
            return result;
        }

        [DllImport("ChiaKeyTsf.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern int ChiaKeySimplifyText(string source, StringBuilder output, int capacity);

        internal static string Text(string source) { return Translate(source, Language); }

        internal static string Translate(string source, string target)
        {
            if (string.IsNullOrEmpty(source)) return source;
            if (Normalize(target) == "en")
            {
                string translated;
                return english.TryGetValue(source, out translated) ? translated : source;
            }
            if (Normalize(target) == "zh-CN")
            {
                StringBuilder output = new StringBuilder(source.Length + 1);
                if (ChiaKeySimplifyText(source, output, output.Capacity) > 0) return output.ToString();
            }
            return source;
        }
    }
}
