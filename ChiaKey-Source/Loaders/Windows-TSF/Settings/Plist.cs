// C# 5 so the csc.exe in .NET Framework 4.x builds it

using System;
using System.IO;
using System.Text;
using System.Xml;

namespace ChiaKey.Settings
{
    // the files PVPropertyList reads; keys this class never sets are written back untouched
    public sealed class Plist
    {
        private readonly string path;
        private readonly XmlDocument document = new XmlDocument();
        private XmlElement dictionary;

        public Plist(string path)
        {
            this.path = path;
            document.XmlResolver = null;
            if (File.Exists(path))
            {
                try
                {
                    XmlReaderSettings settings = new XmlReaderSettings();
                    settings.DtdProcessing = DtdProcessing.Ignore;
                    settings.XmlResolver = null;
                    using (XmlReader reader = XmlReader.Create(path, settings))
                    {
                        document.Load(reader);
                    }
                    if (document.DocumentElement != null)
                        dictionary = document.DocumentElement["dict"];
                }
                catch (XmlException)
                {
                    dictionary = null;
                }
            }
            if (dictionary == null)
            {
                document.RemoveAll();
                document.AppendChild(document.CreateXmlDeclaration("1.0", "UTF-8", null));
                XmlElement root = document.CreateElement("plist");
                root.SetAttribute("version", "1.0");
                document.AppendChild(root);
                dictionary = document.CreateElement("dict");
                root.AppendChild(dictionary);
            }
        }

        private XmlElement ValueElement(string key)
        {
            foreach (XmlNode node in dictionary.ChildNodes)
            {
                if (node.Name != "key" || node.InnerText != key)
                    continue;
                XmlNode next = node.NextSibling;
                while (next != null && !(next is XmlElement))
                    next = next.NextSibling;
                return next as XmlElement;
            }
            return null;
        }

        public string GetString(string key, string fallback)
        {
            XmlElement value = ValueElement(key);
            return value != null && value.Name == "string" ? value.InnerText : fallback;
        }

        // matches OVKeyValueMap::isKeyTrue
        public bool GetBool(string key, bool fallback)
        {
            string value = GetString(key, null);
            if (value == null)
                return fallback;
            int number;
            return value == "true" || (int.TryParse(value, out number) && number > 0);
        }

        public int GetInt(string key, int fallback)
        {
            int result;
            return int.TryParse(GetString(key, null), out result) ? result : fallback;
        }

        public void SetString(string key, string value)
        {
            XmlElement element = ValueElement(key);
            if (element == null)
            {
                XmlElement keyElement = document.CreateElement("key");
                keyElement.InnerText = key;
                dictionary.AppendChild(keyElement);
                element = document.CreateElement("string");
                dictionary.AppendChild(element);
            }
            else if (element.Name != "string")
            {
                XmlElement replacement = document.CreateElement("string");
                dictionary.ReplaceChild(replacement, element);
                element = replacement;
            }
            element.InnerText = value;
        }

        public void SetBool(string key, bool value)
        {
            SetString(key, value ? "true" : "false");
        }

        public void SetInt(string key, int value)
        {
            SetString(key, value.ToString());
        }

        public void Save()
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            XmlWriterSettings settings = new XmlWriterSettings();
            settings.Indent = true;
            settings.IndentChars = "\t";
            settings.Encoding = new UTF8Encoding(false);
            // a reader in another process must never see half a file
            string temporary = path + ".tmp";
            using (XmlWriter writer = XmlWriter.Create(temporary, settings))
            {
                document.Save(writer);
            }
            if (File.Exists(path))
                File.Replace(temporary, path, null);
            else
                File.Move(temporary, path);
        }
    }
}
