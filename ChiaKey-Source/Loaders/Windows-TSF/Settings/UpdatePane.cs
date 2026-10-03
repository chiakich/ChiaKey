using System;
using System.Drawing;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ChiaKey.Settings
{
    internal sealed class UpdatePane : Panel
    {
        private readonly UpdateService service = UpdateService.Default();

        private sealed class Section : GroupBox
        {
            internal readonly CheckBox automatic = new CheckBox();
            internal readonly Label latest = new Label(), current = new Label(), checkedAt = new Label(), status = new Label();
            internal readonly Button check = new Button(), install = new Button();
            internal readonly FlowLayoutPanel options = new FlowLayoutPanel();
            internal UpdateOffer offer;
            internal bool busy;

            internal Section(string title, string option)
            {
                SuspendLayout();
                DoubleBuffered = true;
                UseCompatibleTextRendering = true;
                Text = title;
                Dock = DockStyle.Top;
                AutoSize = true;
                AutoSizeMode = AutoSizeMode.GrowAndShrink;
                Padding = new Padding(12, 6, 12, 10);
                Margin = new Padding(0, 0, 0, 12);
                TableLayoutPanel rows = new TableLayoutPanel {
                    Dock = DockStyle.Top, AutoSize = true, ColumnCount = 3, RowCount = 5,
                    Margin = Padding.Empty, Padding = Padding.Empty };
                rows.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
                rows.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
                rows.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
                for (int i = 0; i < 5; ++i) rows.RowStyles.Add(new RowStyle(SizeType.AutoSize));
                Row(rows, "最新版本：", latest, 0);
                Row(rows, "目前版本：", current, 1);
                Row(rows, "上次檢查：", checkedAt, 2);
                latest.Text = "尚未檢查"; checkedAt.Text = "尚未檢查";
                automatic.Text = option; automatic.AutoSize = true;
                automatic.UseCompatibleTextRendering = true;
                automatic.Margin = new Padding(0, 8, 0, 4);
                options.AutoSize = true; options.Dock = DockStyle.Fill;
                options.Margin = Padding.Empty;
                options.Controls.Add(automatic);
                rows.Controls.Add(options, 0, 3); rows.SetColumnSpan(options, 3);
                check.Text = "檢查更新";
                install.Text = "下載安裝";
                foreach (Button button in new[] { check, install })
                {
                    button.FlatStyle = FlatStyle.System;
                    button.UseVisualStyleBackColor = true;
                    button.AutoSize = true;
                    button.AutoSizeMode = AutoSizeMode.GrowAndShrink;
                    button.MinimumSize = new Size(112, 30);
                    button.Margin = new Padding(12, 0, 0, 4);
                }
                rows.Controls.Add(check, 2, 0); rows.Controls.Add(install, 2, 1);
                status.AutoSize = true; status.UseCompatibleTextRendering = true;
                status.Margin = new Padding(0, 4, 0, 0);
                status.Visible = false;
                status.TextChanged += delegate { status.Visible = status.Text.Length > 0; };
                rows.Controls.Add(status, 0, 4); rows.SetColumnSpan(status, 3);
                rows.SizeChanged += delegate { status.MaximumSize = new Size(rows.ClientSize.Width, 0); };
                Controls.Add(rows);
                Buttons();
                ResumeLayout(true);
            }

            private void Row(TableLayoutPanel rows, string text, Label value, int row)
            {
                Label caption = new Label { Text = text, AutoSize = true,
                    UseCompatibleTextRendering = true, Margin = new Padding(0, 4, 6, 4) };
                value.AutoSize = true; value.UseCompatibleTextRendering = true;
                value.Margin = new Padding(0, 4, 0, 4);
                value.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
                value.AutoEllipsis = true;
                rows.Controls.Add(caption, 0, row); rows.Controls.Add(value, 1, row);
            }

            internal void Buttons()
            {
                check.Enabled = !busy;
                install.Visible = offer != null;
                install.Enabled = !busy && offer != null;
            }
        }

        private readonly Section app;
        private readonly Section lexicon;
        private readonly CheckBox beta = new CheckBox { Text = "接受 Beta 版", AutoSize = true,
            UseCompatibleTextRendering = true, Margin = new Padding(16, 8, 0, 4) };

        internal UpdatePane(Action changed)
        {
            SuspendLayout();
            DoubleBuffered = true;
            AutoScroll = true;
            app = new Section("輸入法更新", "自動更新輸入法");
            lexicon = new Section("詞庫更新", "自動更新詞庫");
            TableLayoutPanel sections = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true,
                ColumnCount = 1, RowCount = 2, Padding = new Padding(16), Margin = Padding.Empty };
            sections.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
            sections.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            sections.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            sections.Controls.Add(app, 0, 0); sections.Controls.Add(lexicon, 0, 1);
            Controls.Add(sections);
            app.automatic.Checked = service.Preferences.GetBool("AutoUpdateApp", true);
            lexicon.automatic.Checked = service.Preferences.GetBool("AutoUpdateLexicon", true);
            beta.Checked = service.Preferences.GetBool("IncludeBetaReleases", false);
            app.options.Controls.Add(beta);
            beta.CheckedChanged += async delegate
            {
                changed();
                app.offer = null; app.Buttons();
                if (Visible) await Check(app, false);
            };
            app.automatic.CheckedChanged += delegate { changed(); };
            lexicon.automatic.CheckedChanged += delegate { changed(); };
            app.check.Click += async delegate { await Check(app, false); };
            lexicon.check.Click += async delegate { await Check(lexicon, true); };
            app.install.Click += async delegate { await Install(app, false); };
            lexicon.install.Click += async delegate { await Install(lexicon, true); };
            VisibleChanged += async delegate
            {
                if (!Visible) return;
                RefreshCurrent();
                await Task.WhenAll(Check(app, false), Check(lexicon, true));
            };
            RefreshCurrent();
            ResumeLayout(true);
        }

        private void RefreshCurrent()
        {
            app.current.Text = service.AppReleaseVersion;
            try { lexicon.current.Text = service.CurrentLexiconVersion(); }
            catch (Exception error) { lexicon.status.Text = "無法讀取詞庫：" + error.Message; }
        }

        private void SetBusy(Section section, bool isLexicon, bool busy)
        {
            section.busy = busy;
            if (!IsDisposed)
            {
                section.Buttons();
                if (!isLexicon) beta.Enabled = !busy;
            }
        }

        private async Task Check(Section section, bool isLexicon)
        {
            if (section.busy || IsDisposed) return;
            bool includeBeta = beta.Checked;
            section.offer = null;
            SetBusy(section, isLexicon, true);
            section.status.Text = "正在檢查更新…";
            try
            {
                UpdateOffer offer = await Task.Run(delegate
                {
                    if (isLexicon) { service.RecoverLexicon(); return service.CheckLexicon(); }
                    return service.CheckApp(includeBeta);
                });
                if (IsDisposed) return;
                section.offer = offer;
                section.latest.Text = offer == null ? section.current.Text : offer.Version;
                section.checkedAt.Text = DateTime.Now.ToString("yyyy-MM-dd HH:mm");
                section.status.Text = offer == null ? "已是最新版本。" : "有新版本可供下載。";
            }
            catch (Exception error) { if (!IsDisposed) section.status.Text = "檢查失敗：" + error.Message; }
            finally { SetBusy(section, isLexicon, false); }
        }

        private async Task Install(Section section, bool isLexicon)
        {
            UpdateOffer offer = section.offer;
            if (offer == null) return;
            SetBusy(section, isLexicon, true);
            section.status.Text = "正在下載更新…";
            try
            {
                string installer = null;
                await Task.Run(delegate
                {
                    if (isLexicon) service.InstallLexicon(offer);
                    else installer = service.DownloadApp(offer);
                });
                if (IsDisposed) return;
                if (!isLexicon) UpdateService.InstallApp(installer, offer.Sha256);
                else { section.offer = null; RefreshCurrent(); }
                section.status.Text = isLexicon ? "詞庫已更新。" : "已開啟安裝器。";
            }
            catch (Exception error) { if (!IsDisposed) section.status.Text = "更新失敗：" + error.Message; }
            finally { SetBusy(section, isLexicon, false); }
        }

        internal void Save() { service.Configure(app.automatic.Checked, lexicon.automatic.Checked, beta.Checked); }
    }
}
