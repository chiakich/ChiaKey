using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Threading.Tasks;
using System.Windows.Forms;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.WinForms;

namespace ChiaKey.Settings
{
    internal sealed class DictionaryForm : Form
    {
        internal const string WindowTitle = "千秋輸入法 字典";
        private readonly DictionaryQuery queries = new DictionaryQuery();
        private readonly TextBox query = new TextBox();
        private readonly ToolStripMenuItem history = new ToolStripMenuItem(Ui.Text("查詢歷史"));
        private readonly Label status = new Label();
        private WebView2 browser;
        private bool ready;

        internal DictionaryForm()
        {
            Text = Ui.Text(WindowTitle);
            FormBorderStyle = FormBorderStyle.SizableToolWindow;
            MaximizeBox = false;
            MinimizeBox = false;
            AutoScaleMode = AutoScaleMode.Dpi;
            ClientSize = new Size(460, 520);
            MinimumSize = new Size(360, 300);
            StartPosition = FormStartPosition.CenterScreen;

            MenuStrip menu = new MenuStrip();
            ToolStripMenuItem file = new ToolStripMenuItem(Ui.Text("檔案"));
            file.DropDownItems.Add(Ui.Text("關閉"), null, delegate { Close(); });
            ToolStripMenuItem edit = new ToolStripMenuItem(Ui.Text("編輯"));
            edit.DropDownItems.Add(Ui.Text("複製"), null, async delegate {
                if (query.ContainsFocus) query.Copy();
                else await BrowserCommand("copy");
            });
            edit.DropDownItems.Add(Ui.Text("全選"), null, async delegate {
                if (query.ContainsFocus) query.SelectAll();
                else await BrowserCommand("selectAll");
            });
            edit.DropDownItems.Add(new ToolStripSeparator());
            edit.DropDownItems.Add(history);
            history.Enabled = false;
            menu.Items.Add(file); menu.Items.Add(edit);
            MainMenuStrip = menu;

            TableLayoutPanel search = new TableLayoutPanel();
            search.Dock = DockStyle.Top; search.Height = 36; search.Padding = new Padding(4);
            search.ColumnCount = 3;
            search.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            search.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            search.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            query.Dock = DockStyle.Fill; query.MaxLength = 2048;
            query.KeyDown += delegate(object sender, KeyEventArgs e) {
                if (e.KeyCode != Keys.Enter) return;
                e.SuppressKeyPress = true; Search();
            };
            Button go = new Button { Text = Ui.Text("查詢"), AutoSize = true };
            go.Click += delegate { Search(); };
            Button external = new Button { Text = Ui.Text("瀏覽器開啟"), AutoSize = true };
            external.Click += delegate { OpenExternal(); };
            search.Controls.Add(query, 0, 0); search.Controls.Add(go, 1, 0); search.Controls.Add(external, 2, 0);
            status.Dock = DockStyle.Bottom; status.Height = 24; status.Padding = new Padding(4, 3, 4, 0);
            status.Text = Ui.Text("輸入要查詢的英文或中文。");
            Panel content = new Panel { Dock = DockStyle.Fill };
            Controls.Add(content); Controls.Add(status); Controls.Add(search); Controls.Add(menu);
            Shown += async delegate {
                try
                {
                    browser = new WebView2 { Dock = DockStyle.Fill };
                    content.Controls.Add(browser);
                    string profile = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                                                  "ChiaKey", "DictionaryWebView2");
                    CoreWebView2Environment environment = await CoreWebView2Environment.CreateAsync(null, profile);
                    await browser.EnsureCoreWebView2Async(environment);
                    if (IsDisposed) return;
                    browser.CoreWebView2.Settings.AreHostObjectsAllowed = false;
                    browser.CoreWebView2.Settings.IsWebMessageEnabled = false;
                    browser.CoreWebView2.PermissionRequested += delegate(object s, CoreWebView2PermissionRequestedEventArgs e) {
                        e.State = CoreWebView2PermissionState.Deny;
                    };
                    browser.CoreWebView2.DownloadStarting += delegate(object s, CoreWebView2DownloadStartingEventArgs e) { e.Cancel = true; };
                    browser.CoreWebView2.NavigationStarting += delegate(object s, CoreWebView2NavigationStartingEventArgs e) {
                        // No file/custom protocol navigation from remote content.
                        if (e.Uri != "about:blank" && !DictionaryQuery.IsWebAddress(e.Uri)) e.Cancel = true;
                    };
                    browser.CoreWebView2.NewWindowRequested += delegate(object s, CoreWebView2NewWindowRequestedEventArgs e) {
                        e.Handled = true;
                        if (e.IsUserInitiated && DictionaryQuery.IsWebAddress(e.Uri)) OpenUrl(e.Uri);
                    };
                    browser.CoreWebView2.NavigationCompleted += delegate(object s, CoreWebView2NavigationCompletedEventArgs e) {
                        status.Text = e.IsSuccess ? Ui.Text("Yahoo 字典") : Ui.Text("查詢頁面無法載入，請重試或使用瀏覽器開啟。");
                    };
                    ready = true;
                    if (queries.Current != null) Navigate(queries.Search(queries.Current));
                }
                catch (Exception)
                {
                    if (!IsDisposed)
                        status.Text = Ui.Text("內嵌字典無法啟動，請使用瀏覽器開啟，或安裝 WebView2 Runtime。");
                }
            };
        }

        private Uri Search(bool navigate = true)
        {
            if (string.IsNullOrWhiteSpace(query.Text)) return null;
            Uri target = queries.Search(query.Text);
            history.DropDownItems.Clear(); history.Enabled = true;
            foreach (string text in queries.History)
            {
                string entry = text;
                // A mnemonic ampersand must not alter the displayed query text.
                string caption = entry.Length > 80 ? entry.Substring(0, 80) + "…" : entry;
                ToolStripMenuItem item = new ToolStripMenuItem(caption.Replace("&", "&&")) { Checked = entry == queries.Current };
                item.Click += delegate { query.Text = entry; Search(); };
                history.DropDownItems.Add(item);
            }
            if (ready && navigate) Navigate(target);
            return target;
        }

        private void Navigate(Uri target)
        {
            status.Text = Ui.Text("正在查詢…");
            browser.CoreWebView2.Navigate(target.AbsoluteUri);
        }

        private void OpenExternal()
        {
            Uri target = Search(false);
            if (target != null) OpenUrl(target.AbsoluteUri);
        }

        private async Task BrowserCommand(string command)
        {
            if (!ready || IsDisposed) return;
            try { await browser.CoreWebView2.ExecuteScriptAsync("document.execCommand('" + command + "');"); }
            catch (Exception error)
            {
                if (!IsDisposed) MessageBox.Show(this, error.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void OpenUrl(string address)
        {
            try { Process.Start(new ProcessStartInfo(address) { UseShellExecute = true }); }
            catch (Exception error) { MessageBox.Show(this, error.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error); }
        }
    }
}
