using System;
using System.Collections.Generic;

namespace ChiaKey.Settings
{
    // The original dictionary kept eight unique queries for the lifetime of its window.
    // Keep query text in memory; do not send anything until the user searches.
    internal sealed class DictionaryQuery
    {
        private readonly List<string> history = new List<string>();
        internal string Current { get; private set; }
        internal string[] History { get { return history.ToArray(); } }

        internal Uri Search(string text)
        {
            string query = (text ?? "").Trim();
            if (query.Length == 0) throw new ArgumentException("Empty dictionary query.");
            if (query.Length > 2048) throw new ArgumentException("Dictionary query is too long.");
            // Escape a query component, never an entire URI (the old implementation did the latter).
            Uri target = new Uri("https://tw.dictionary.search.yahoo.com/search?p=" + Uri.EscapeDataString(query));
            Current = query;
            if (!history.Contains(query))
            {
                history.Add(query);
                if (history.Count > 8) history.RemoveAt(0);
            }
            return target;
        }

        internal static bool IsWebAddress(string text)
        {
            Uri uri;
            return Uri.TryCreate(text, UriKind.Absolute, out uri) &&
                   uri.Scheme == Uri.UriSchemeHttps && !string.IsNullOrEmpty(uri.Host) &&
                   string.IsNullOrEmpty(uri.UserInfo);
        }
    }
}
