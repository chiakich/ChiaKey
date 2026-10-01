using System;
using System.Drawing;
using System.IO;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ChiaKey.Settings
{
    internal sealed class UpdatePane : Panel
    {
        private readonly UpdateService service = UpdateService.Default();
        private readonly CheckBox app = new CheckBox(), lexicon = new CheckBox();
        private readonly Label current = new Label(), status = new Label();
        private readonly Button check = new Button(), installApp = new Button(), installLexicon = new Button();
        private UpdateOffer appOffer, lexiconOffer;
        private bool busy;

        internal UpdatePane(Action changed)
        {
            Label title = new Label { Text = "更新", Font = new Font("Microsoft JhengHei UI", 11F, FontStyle.Bold),
                AutoSize = true, Location = new Point(16, 10) };
            Controls.Add(title);
            current.SetBounds(16, 48, 465, 60); Controls.Add(current);
            app.Text = "自動更新千秋輸入法（安裝時需 Windows 權限確認）";
            app.SetBounds(16, 112, 465, 26); Controls.Add(app);
            lexicon.Text = "自動更新詞庫"; lexicon.SetBounds(16, 144, 465, 26); Controls.Add(lexicon);
            app.Checked = service.Preferences.GetBool("AutoUpdateApp", false);
            lexicon.Checked = service.Preferences.GetBool("AutoUpdateLexicon", false);
            app.CheckedChanged += delegate { changed(); }; lexicon.CheckedChanged += delegate { changed(); };
            Label note = new Label { Text = "開啟後每天背景檢查一次，發布滿三天才自動更新。\n" +
                "更新只連線至千秋輸入法的發布來源，不傳送輸入內容。\n" +
                "詞庫在組字結束後切換；本體安裝後請重新開啟應用程式。",
                Location = new Point(16, 180), Size = new Size(465, 65) };
            Controls.Add(note);
            check.Text = "檢查更新"; check.SetBounds(16, 258, 110, 28); Controls.Add(check);
            installApp.Text = "更新本體"; installApp.SetBounds(142, 258, 110, 28); Controls.Add(installApp);
            installLexicon.Text = "更新詞庫"; installLexicon.SetBounds(268, 258, 110, 28); Controls.Add(installLexicon);
            status.SetBounds(16, 306, 465, 100); Controls.Add(status);
            check.Click += async delegate { await Check(); };
            installApp.Click += async delegate { await Install(false); };
            installLexicon.Click += async delegate { await Install(true); };
            VisibleChanged += delegate { if (Visible && !busy) RefreshCurrent(); };
            RefreshCurrent(); Buttons();
        }

        private void RefreshCurrent()
        {
            string version;
            try { version = service.CurrentLexiconVersion(); } catch (Exception error) { version = "無法讀取：" + error.Message; }
            current.Text = "本體版本：" + service.AppVersion.ToString(3) + "\n詞庫版本：" + version;
            string path = Path.Combine(service.Root, "status.txt");
            if (File.Exists(path))
                try { status.Text = File.ReadAllText(path); } catch (IOException) { }
        }

        private void Buttons()
        {
            check.Enabled = !busy; installApp.Enabled = !busy && appOffer != null;
            installLexicon.Enabled = !busy && lexiconOffer != null;
        }

        private async Task Check()
        {
            busy = true; Buttons(); status.Text = "正在檢查更新…";
            appOffer = null; lexiconOffer = null;
            string appResult = "", lexiconResult = "";
            await Task.Run(delegate
            {
                try { appOffer = service.CheckApp(); appResult = appOffer == null ? "本體已是最新。" : "本體可更新至 " + appOffer.Version; }
                catch (Exception error) { appResult = "本體檢查失敗：" + error.Message; }
                try { service.RecoverLexicon(); lexiconOffer = service.CheckLexicon(); lexiconResult = lexiconOffer == null ? "詞庫已是最新。" : "詞庫可更新至 " + lexiconOffer.Version; }
                catch (Exception error) { lexiconResult = "詞庫檢查失敗：" + error.Message; }
            });
            if (IsDisposed) return;
            status.Text = appResult + "\n" + lexiconResult;
            busy = false; Buttons();
        }

        private async Task Install(bool isLexicon)
        {
            UpdateOffer offer = isLexicon ? lexiconOffer : appOffer;
            if (offer == null) return;
            busy = true; Buttons(); status.Text = "正在下載並驗證更新…";
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
                else { lexiconOffer = null; RefreshCurrent(); }
                status.Text = isLexicon ? "詞庫已更新至 " + offer.Version + "，組字結束後載入新版。" :
                    "已開啟安裝器。完成後請重新開啟應用程式以載入新版。";
            }
            catch (Exception error) { if (!IsDisposed) status.Text = "更新失敗：" + error.Message; }
            finally { busy = false; if (!IsDisposed) Buttons(); }
        }

        internal void Save() { service.Configure(app.Checked, lexicon.Checked); }
    }
}
