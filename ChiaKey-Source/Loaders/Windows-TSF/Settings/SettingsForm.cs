// after Yahoo! KeyKey's TakaoPreference; C# 5 so the csc.exe in .NET Framework 4.x builds it

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Globalization;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

namespace ChiaKey.Settings
{
    internal sealed class Choice
    {
        public readonly string Value;
        public readonly string Label;

        public Choice(string value, string label)
        {
            Value = value;
            Label = label;
        }

        public override string ToString()
        {
            return Label;
        }
    }

    // icon over label, as the original toolbar draws its items
    internal sealed class ToolbarItem : Control
    {
        private readonly Image icon;
        private bool selected;

        public ToolbarItem(string text, Image icon)
        {
            Text = text;
            this.icon = icon;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.UserPaint | ControlStyles.SupportsTransparentBackColor, true);
            BackColor = Color.Transparent;
            AccessibleRole = AccessibleRole.PageTab;
            Cursor = Cursors.Hand;
        }

        public bool Selected
        {
            get { return selected; }
            set { selected = value; Invalidate(); }
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Modifiers == Keys.None && (e.KeyCode == Keys.Space || e.KeyCode == Keys.Enter))
            {
                OnClick(EventArgs.Empty);
                e.Handled = true;
            }
            base.OnKeyDown(e);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            if (Focused) ControlPaint.DrawFocusRectangle(g, ClientRectangle);
            if (selected)
            {
                using (SolidBrush brush = new SolidBrush(Color.FromArgb(48, 0, 0, 0)))
                {
                    g.FillRectangle(brush, ClientRectangle);
                }
            }
            // sized from the scaled bounds: icon and label sit together, centered
            Size textSize = TextRenderer.MeasureText(g, Text, Font);
            int iconSize = Height / 2;
            int gap = Height / 24;
            int top = (Height - iconSize - gap - textSize.Height) / 2;
            if (icon != null)
                g.DrawImage(icon, (Width - iconSize) / 2, top, iconSize, iconSize);
            Rectangle textRect = new Rectangle(2, top + iconSize + gap, Width - 4, Height - top - iconSize - gap);
            TextRenderer.DrawText(g, Text, Font, textRect, Color.Black,
                                  TextFormatFlags.HorizontalCenter | TextFormatFlags.Top | TextFormatFlags.WordBreak);
        }
    }

    internal sealed class SettingsForm : Form
    {
        private const string WindowTitle = "千秋輸入法 偏好設定";
        // past the widest caption, 視窗背景顏色：
        private const int ControlLeft = 140;
        private const int ToolbarHeight = 70;

        private static readonly Choice[] Layouts = {
            new Choice("Standard", Ui.Text("標準")),
            new Choice("ETen", Ui.Text("倚天")),
            new Choice("Hsu", Ui.Text("許氏")),
            new Choice("ETen26", Ui.Text("倚天 26 鍵")),
            new Choice("HanyuPinyin", Ui.Text("漢語拼音")),
        };
        private static readonly Choice[] SelectionKeys = {
            new Choice("", Ui.Text("依鍵盤配置")),
            new Choice("12345678", "12345678"),
            new Choice("asdfghjk", "asdfghjk"),
            new Choice("asdfzxcv", "asdfzxcv"),
            new Choice("aoeuidht", "aoeuidht"),
            new Choice("aoeu;qjk", "aoeu;qjk"),
        };
        private static readonly Choice[] HighlightColors = {
            new Choice("Purple", Ui.Text("紫色")),
            new Choice("Green", Ui.Text("綠色")),
            new Choice("Yellow", Ui.Text("黃色")),
            new Choice("Red", Ui.Text("紅色")),
        };
        private static readonly Choice[] BackgroundColors = {
            new Choice("Black", Ui.Text("黑色")),
            new Choice("White", Ui.Text("白色")),
        };
        private static readonly Choice[] TextColors = {
            new Choice("White", Ui.Text("白色")),
            new Choice("Black", Ui.Text("黑色")),
        };

        private readonly Plist frontend;
        private readonly Plist smartMandarin;
        private readonly Plist traditionalMandarin;
        private readonly Plist cangjie;
        private readonly Plist simplex;
        private readonly string tablesPath;

        private readonly Panel toolbar = new Panel();
        private readonly Panel content = new Panel();
        private readonly List<ToolbarItem> toolbarItems = new List<ToolbarItem>();
        private readonly List<Control> panes = new List<Control>();
        private readonly Button applyButton = new Button();
        private bool loading;
        private UpdatePane updates;

        private static readonly Choice[] CangjiePunctuations = {
            new Choice("", Ui.Text("全部使用全形標點")),
            new Choice("Punctuations-cj-mixedwidth-cin", Ui.Text("除了逗號與句號外，使用半形標點")),
            new Choice("Punctuations-cj-halfwidth-cin", Ui.Text("全部使用半形標點")),
        };
        // the menu order, as InputMethods() in ChiaKeyEngine.cpp lists them
        private static readonly Choice[] BuiltInInputMethods = {
            new Choice("SmartMandarin", Ui.Text("好打注音")),
            new Choice("TraditionalMandarin", Ui.Text("傳統注音")),
            new Choice("Generic-cj-cin", Ui.Text("倉頡")),
            new Choice("Generic-simplex-cin", Ui.Text("簡易")),
        };
        private const string AllCharacters = "使用全字庫罕用字（CNS11643）";

        private readonly string preferencesPath;
        private CheckBox controlBackslash;
        private CheckBox shiftTogglesEnglish;
        private CheckBox capsLockTogglesEnglish;
        private ComboBox converterShortcut;
        private ComboBox repeatShortcut;
        private ComboBox reverseLookup;
        private CheckBox notifications;
        private CheckBox associatedPhrases;
        private CheckBox simplifiedOutput;
        private CheckedListBox menuInputMethods;
        private CheckBox smartAllCharacters;
        private CheckBox traditionalAllCharacters;
        private CheckBox cangjieAllCharacters;
        private CheckBox simplexAllCharacters;
        private ComboBox cangjiePunctuation;
        private GroupBox tableSettings;
        private NumericUpDown tableMaximumLength;
        private TextBox tableMatchOne;
        private TextBox tableMatchMany;
        private CheckBox tableCommitAtMaximum;
        private CheckBox tableClearOnError;
        private CheckBox tableComposeWhileTyping;
        private CheckBox tableDynamicFrequency;
        private CheckBox tableSpaceFirst;
        // one per user table, read when it is first selected
        private readonly Dictionary<string, Plist> tablePlists = new Dictionary<string, Plist>();
        private Plist shownTable;
        private ComboBox smartLayout;
        private ComboBox selectionKeys;
        private NumericUpDown bufferSize;
        private CheckBox spaceShowsCandidates;
        private CheckBox escClears;
        private CheckBox cursorAtEnd;
        private CheckBox shiftUppercase;
        private ComboBox traditionalLayout;
        private ComboBox highlightColor;
        private ComboBox backgroundColor;
        private ComboBox textColor;
        private CheckBox backgroundPattern;
        private CheckBox beep;
        private CheckBox keyboardFollowsCursor;
        private CheckBox wordCountEnabled;
        private ComboBox uiLanguage;
        private static readonly Choice[] UiLanguages = {
            new Choice("zh-TW", Ui.Text("繁體中文")), new Choice("zh-CN", Ui.Text("簡體中文")),
            new Choice("en", "English") };
        private Label wordCounts;
        [System.Runtime.InteropServices.DllImport("ChiaKeyTsf.dll", CallingConvention = System.Runtime.InteropServices.CallingConvention.StdCall)]
        private static extern bool ChiaKeyWordCounts(out long today, out long week, out long total);
        [System.Runtime.InteropServices.DllImport("ChiaKeyTsf.dll", CallingConvention = System.Runtime.InteropServices.CallingConvention.StdCall)]
        private static extern bool ChiaKeyClearWordCounts();
        private RadioButton defaultSound;
        private RadioButton customSound;
        private TextBox soundPath;
        private CheckBox cangjieCommitAtMaximum;
        private CheckBox cangjieComposeWhileTyping;
        private CheckBox cangjieClearOnError;
        private CheckBox cangjieDynamicFrequency;
        private CheckBox simplexComposeWhileTyping;
        private CheckBox simplexClearOnError;
        private ListBox userTables;

        public SettingsForm(string dataPath, int initialPane = 0)
        {
            preferencesPath = Path.Combine(dataPath, "Preferences");
            // the engine names Tables\Generic\x.cin the Generic-x-cin input method
            tablesPath = Path.Combine(dataPath, "Tables", "Generic");
            frontend = new Plist(Path.Combine(preferencesPath, "Windows.plist"));
            smartMandarin = new Plist(Path.Combine(preferencesPath, "SmartMandarin.plist"));
            traditionalMandarin = new Plist(Path.Combine(preferencesPath, "TraditionalMandarin.plist"));
            cangjie = new Plist(Path.Combine(preferencesPath, "Generic-cj-cin.plist"));
            simplex = new Plist(Path.Combine(preferencesPath, "Generic-simplex-cin.plist"));

            // laid out at 96 DPI and scaled on ResumeLayout; the manifest makes the process DPI aware
            SuspendLayout();
            AutoScaleDimensions = new SizeF(96F, 96F);
            AutoScaleMode = AutoScaleMode.Dpi;
            Text = Ui.Text(WindowTitle);
            Font = new Font("Microsoft JhengHei UI", 9F);
            Icon = PhraseEditorForm.LoadIcon("app.ico");
            FormBorderStyle = FormBorderStyle.Sizable;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(720, 700);
            MinimumSize = new Size(620, 480);
            BackColor = Color.White;

            BuildToolbar();
            BuildButtons();
            content.Dock = DockStyle.Fill;
            Controls.Add(content);
            content.BringToFront();

            AddPane(Ui.Text("一般(&G)"), "general.tiff", BuildGeneralPane());
            AddPane(Ui.Text("注音(&P)"), "phonetic.tiff", BuildPhoneticPane());
            AddPane(Ui.Text("倉頡(&J)"), "cangjie.tiff", BuildCangjiePane());
            AddPane(Ui.Text("簡易(&S)"), "simplex.tiff", BuildSimplexPane());
            AddPane(Ui.Text("泛用(&E)"), "generic.tiff", BuildGenericPane());
            AddPane(Ui.Text("詞彙(&H)"), "phrase.tiff", BuildPhrasePane());
            AddPane(Ui.Text("其他(&M)"), "plugin.tiff", BuildMiscPane());
            updates = new UpdatePane(Changed);
            AddPane(Ui.Text("更新(&U)"), "update.tiff", updates);
            AddPane(Ui.Text("關於(&B)"), "app.ico", BuildAboutPane());

            LoadSettings();
            ShowPane(Math.Max(0, Math.Min(initialPane, panes.Count - 1)));
            ResumeLayout(false);
            PerformLayout();
            Shown += delegate
            {
                Rectangle area = Screen.FromControl(this).WorkingArea;
                if (Height > area.Height) Height = area.Height;
                if (Width > area.Width) Width = area.Width;
                Location = new Point(Math.Max(area.Left, area.Left + (area.Width - Width) / 2),
                                     Math.Max(area.Top, area.Top + (area.Height - Height) / 2));
            };
        }

        private static Image LoadIcon(string name)
        {
            if (name.EndsWith(".ico", StringComparison.OrdinalIgnoreCase))
                return PhraseEditorForm.LoadIcon(name).ToBitmap();
            Stream stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(name);
            return stream != null ? Image.FromStream(stream) : null;
        }

        private void BuildToolbar()
        {
            toolbar.Dock = DockStyle.Top;
            toolbar.Height = ToolbarHeight;
            toolbar.SizeChanged += delegate { LayoutToolbar(); };
            toolbar.Paint += delegate(object sender, PaintEventArgs e)
            {
                using (LinearGradientBrush brush = new LinearGradientBrush(
                           toolbar.ClientRectangle, Color.LightGray, Color.White,
                           LinearGradientMode.Vertical))
                {
                    e.Graphics.FillRectangle(brush, toolbar.ClientRectangle);
                }
                e.Graphics.DrawLine(Pens.DarkGray, 0, toolbar.Height - 1, toolbar.Width,
                                    toolbar.Height - 1);
            };
            Controls.Add(toolbar);
        }

        private void BuildButtons()
        {
            FlowLayoutPanel footer = new FlowLayoutPanel { Dock = DockStyle.Bottom,
                Height = 52, FlowDirection = FlowDirection.RightToLeft, WrapContents = false,
                Padding = new Padding(12, 10, 12, 10), BackColor = SystemColors.Control };
            Button okButton = PreferenceLayout.Button(Ui.Text("確定(&O)"));
            okButton.Click += delegate { if (SaveSettings()) Close(); };
            Button cancelButton = PreferenceLayout.Button(Ui.Text("取消(&C)"));
            cancelButton.Click += delegate { Close(); };
            applyButton.Text = Ui.Text("套用(&A)");
            applyButton.AutoSize = true;
            applyButton.MinimumSize = new Size(96, 28);
            applyButton.Padding = new Padding(8, 2, 8, 2);
            applyButton.Enabled = false;
            applyButton.Click += delegate { if (SaveSettings()) applyButton.Enabled = false; };
            footer.Controls.AddRange(new Control[] { applyButton, cancelButton, okButton });
            Controls.Add(footer);
            AcceptButton = okButton;
            CancelButton = cancelButton;
        }

        private void LayoutToolbar()
        {
            if (toolbarItems.Count == 0) return;
            int height = toolbar.ClientSize.Height;
            for (int i = 0; i < toolbarItems.Count; ++i)
            {
                int left = i * toolbar.ClientSize.Width / toolbarItems.Count;
                int right = (i + 1) * toolbar.ClientSize.Width / toolbarItems.Count;
                toolbarItems[i].Bounds = new Rectangle(left, 2, right - left, height - 4);
            }
        }

        private void AddPane(string title, string icon, Control pane)
        {
            int index = toolbarItems.Count;
            ToolbarItem item = new ToolbarItem(title, LoadIcon(icon));
            item.AccessibleRole = AccessibleRole.PageTab;
            item.TabStop = true;
            item.Click += delegate { item.Focus(); ShowPane(index); };
            toolbar.Controls.Add(item);
            toolbarItems.Add(item);
            LayoutToolbar();
            pane.Dock = DockStyle.Fill;
            pane.Visible = false;
            content.Controls.Add(pane);
            panes.Add(pane);
            PreferenceLayout.Finish(pane);
        }

        protected override bool ProcessMnemonic(char charCode)
        {
            for (int index = 0; index < toolbarItems.Count; ++index)
            {
                if (IsMnemonic(charCode, toolbarItems[index].Text))
                {
                    ShowPane(index);
                    toolbarItems[index].Focus();
                    return true;
                }
            }
            return base.ProcessMnemonic(charCode);
        }

        private void ShowPane(int index)
        {
            for (int i = 0; i < panes.Count; ++i)
            {
                panes[i].Visible = i == index;
                toolbarItems[i].Selected = i == index;
            }
        }

        private static Panel Page(string title)
        {
            return PreferenceLayout.Page(Ui.Text(title));
        }

        private static GroupBox Group(Control parent, string text)
        {
            return PreferenceLayout.Section(parent, Ui.Text(text));
        }

        private CheckBox Check(Control parent, string text)
        {
            CheckBox box = new CheckBox { Text = Ui.Text(text), AutoSize = true };
            box.CheckedChanged += delegate { Changed(); };
            PreferenceLayout.Row(parent, box);
            return box;
        }

        private ComboBox Combo(Control parent, string label, Choice[] choices)
        {
            ComboBox box = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList };
            box.Items.AddRange(choices);
            box.SelectedIndexChanged += delegate { Changed(); };
            PreferenceLayout.Row(parent, PreferenceLayout.Label(Ui.Text(label)), box);
            return box;
        }

        private Control BuildGeneralPane()
        {
            Panel pane = Page("一般設定");
            GroupBox basic = Group(pane, "基本功能");
            controlBackslash = Check(basic, "使用 Ctrl + \\ 切換到下一個輸入法");
            shiftTogglesEnglish = Check(basic, "使用單擊 Shift 按鍵切換中英模式");
            capsLockTogglesEnglish = Check(basic, "使用 Caps Lock 按鍵切換中英文");
            capsLockTogglesEnglish.CheckedChanged += delegate { shiftTogglesEnglish.Enabled = !capsLockTogglesEnglish.Checked; };
            associatedPhrases = Check(basic, "輸入後顯示聯想詞");
            simplifiedOutput = Check(basic, "簡體輸出");
            notifications = Check(basic, "使用提示視窗");
            Choice[] shortcuts = new Choice[27];
            shortcuts[0] = new Choice("", Ui.Text("無"));
            for (int i = 0; i < 26; ++i)
                shortcuts[i + 1] = new Choice(((char)('a' + i)).ToString(), "Ctrl + Alt + " + (char)('A' + i));
            GroupBox keys = Group(pane, "快速鍵");
            converterShortcut = Combo(keys, "簡繁中文切換快速鍵：", shortcuts);
            repeatShortcut = Combo(keys, "送出最近一次輸入的文字：", shortcuts);
            reverseLookup = Combo(keys, "字根反查功能：", new Choice[] {
                new Choice("", Ui.Text("無")), new Choice("ReverseLookup-Generic-cj-cin", Ui.Text("倉頡")),
                new Choice("ReverseLookup-Mandarin-bpmf-cin", Ui.Text("注音")),
                new Choice("ReverseLookup-Mandarin-bpmf-cin-HanyuPinyin", Ui.Text("漢語拼音")) });
            GroupBox menu = Group(pane, "輸入法選單管理");
            PreferenceLayout.Hint(menu, Ui.Text("取消勾選的輸入法不會出現在輸入選單中（使用中的除外）。"));
            menuInputMethods = new CheckedListBox { CheckOnClick = true, IntegralHeight = false, Height = 88 };
            menuInputMethods.ItemCheck += delegate { Changed(); };
            PreferenceLayout.Row(menu, menuInputMethods);
            GroupBox language = Group(pane, "介面語言：");
            uiLanguage = Combo(language, "介面語言：", UiLanguages);
            PreferenceLayout.Hint(language, Ui.Text("重新開啟設定與使用中的應用程式後生效。"));
            return pane;
        }

        // the identifiers the engine gives Tables\Generic\x.cin, as OVCINDatabaseService names it
        private static string TableIdentifier(string path)
        {
            return "Generic-" + Path.GetFileName(path).Replace('.', '-');
        }

        private void LoadMenuInputMethods(List<string> hidden)
        {
            bool wasLoading = loading;
            loading = true;
            menuInputMethods.Items.Clear();
            List<Choice> methods = new List<Choice>(BuiltInInputMethods);
            if (Directory.Exists(tablesPath))
            {
                foreach (string path in Directory.GetFiles(tablesPath, "*.cin"))
                    methods.Add(new Choice(TableIdentifier(path), TableName(path)));
            }
            foreach (Choice method in methods)
                menuInputMethods.Items.Add(method, !hidden.Contains(method.Value));
            // a table that is gone keeps its entry, so it stays hidden if it comes back
            foreach (string identifier in hidden)
            {
                if (!methods.Exists(delegate(Choice method) { return method.Value == identifier; }))
                    menuInputMethods.Items.Add(new Choice(identifier, identifier), false);
            }
            menuInputMethods.TopIndex = 0;
            loading = wasLoading;
        }

        private List<string> HiddenInputMethods()
        {
            List<string> hidden = new List<string>();
            for (int index = 0; index < menuInputMethods.Items.Count; ++index)
            {
                if (!menuInputMethods.GetItemChecked(index))
                    hidden.Add(((Choice)menuInputMethods.Items[index]).Value);
            }
            return hidden;
        }

        private Control BuildPhoneticPane()
        {
            Panel pane = Page("注音輸入法設定");
            GroupBox smart = Group(pane, "好打注音");
            smartLayout = Combo(smart, "鍵盤配置：", Layouts);
            selectionKeys = Combo(smart, "選字鍵設定：", SelectionKeys);
            bufferSize = new NumericUpDown { Minimum = 10, Maximum = 20, Width = 72 };
            bufferSize.ValueChanged += delegate { Changed(); };
            PreferenceLayout.Row(smart, PreferenceLayout.Label(Ui.Text("輸入緩衝區：")),
                PreferenceLayout.Inline(bufferSize, PreferenceLayout.Label(Ui.Text("字（最多 20 字）"))));
            spaceShowsCandidates = Check(smart, "使用空白鍵選字");
            escClears = Check(smart, "按下 ESC 按鍵後清除全部編輯區內容");
            cursorAtEnd = Check(smart, "選字時游標放在詞尾");
            shiftUppercase = Check(smart, "按住 Shift 時輸入大寫英文");
            smartAllCharacters = Check(smart, AllCharacters);
            GroupBox traditional = Group(pane, "傳統注音");
            traditionalLayout = Combo(traditional, "鍵盤配置：", Layouts);
            traditionalAllCharacters = Check(traditional, AllCharacters);
            return pane;
        }

        private const string ExclusiveNote =
            "請注意：不能夠同時勾選「組字錯誤時清除字根」與「打字時同時組字」這兩個選項。";

        private static void Note(Control parent, string text)
        {
            PreferenceLayout.Hint(parent, Ui.Text(text));
        }

        // the original panels enforce this: composing as you type has nothing to clear
        private static void Exclusive(CheckBox first, CheckBox second)
        {
            first.CheckedChanged += delegate { if (first.Checked) second.Checked = false; };
            second.CheckedChanged += delegate { if (second.Checked) first.Checked = false; };
        }

        private Control BuildCangjiePane()
        {
            Panel pane = Page("倉頡輸入法設定");
            GroupBox typing = Group(pane, "打字功能");
            cangjieCommitAtMaximum = Check(typing, "打到字根最大長度時立刻組字");
            cangjieComposeWhileTyping = Check(typing, "打字時同時組字");
            cangjieClearOnError = Check(typing, "組字錯誤時清除字根");
            cangjieDynamicFrequency = Check(typing, "使用動態字頻調整（將常用字移動到選字列表前方）");
            cangjieAllCharacters = Check(typing, AllCharacters);
            Exclusive(cangjieComposeWhileTyping, cangjieClearOnError);
            GroupBox punctuation = Group(pane, "標點符號");
            cangjiePunctuation = Combo(punctuation, "標點符號樣式：", CangjiePunctuations);
            Note(pane, ExclusiveNote);
            return pane;
        }

        private Control BuildSimplexPane()
        {
            Panel pane = Page("簡易輸入法設定");
            GroupBox typing = Group(pane, "打字功能");
            simplexComposeWhileTyping = Check(typing, "打字時同時組字");
            simplexClearOnError = Check(typing, "組字錯誤時清除字根");
            simplexAllCharacters = Check(typing, AllCharacters);
            Exclusive(simplexComposeWhileTyping, simplexClearOnError);
            Note(pane, ExclusiveNote);
            return pane;
        }

        private Control BuildGenericPane()
        {
            Panel pane = Page("泛用輸入法設定");
            GroupBox tables = Group(pane, "自訂字表（.cin）");
            userTables = new ListBox { IntegralHeight = false, Height = 112 };
            userTables.SelectedIndexChanged += delegate { ShowTableSettings(); };
            PreferenceLayout.Row(tables, userTables);
            Button import = PreferenceLayout.Button(Ui.Text("匯入…"));
            import.Click += delegate { ImportTable(); };
            Button remove = PreferenceLayout.Button(Ui.Text("移除"));
            remove.Click += delegate { RemoveTable(); };
            Button open = PreferenceLayout.Button(Ui.Text("開啟資料夾"));
            open.Click += delegate {
                Directory.CreateDirectory(tablesPath);
                System.Diagnostics.Process.Start("explorer.exe", "\"" + tablesPath + "\"");
            };
            PreferenceLayout.Row(tables, PreferenceLayout.Inline(import, remove, open));
            tableSettings = Group(pane, "選取字表的設定");
            tableSettings.Enabled = false;
            tableMaximumLength = new NumericUpDown { Minimum = 1, Maximum = 128, Width = 72 };
            tableMaximumLength.ValueChanged += delegate { Changed(); };
            PreferenceLayout.Row(tableSettings, PreferenceLayout.Label(Ui.Text("字根組合最大長度：")),
                PreferenceLayout.Inline(tableMaximumLength));
            tableMatchOne = Wildcard(tableSettings, Ui.Text("單一長度"));
            tableMatchMany = Wildcard(tableSettings, Ui.Text("不限長度"));
            tableCommitAtMaximum = Check(tableSettings, "打到字根最大長度時立刻組字");
            tableClearOnError = Check(tableSettings, "組字錯誤時清除字根");
            tableComposeWhileTyping = Check(tableSettings, "打字時同時組字");
            tableDynamicFrequency = Check(tableSettings, "使用動態字頻調整");
            tableSpaceFirst = Check(tableSettings, "空白鍵選一字，第一選字鍵選第二字");
            Exclusive(tableComposeWhileTyping, tableClearOnError);
            Note(pane, Ui.Text("大易、行列、嘸蝦米等字表不隨附，請自行匯入 .cin 檔。\n") +
                Ui.Text("新增或移除字表後，需要重新開啟正在使用的程式，才會出現在輸入法選單中。"));
            return pane;
        }

        private TextBox Wildcard(Control parent, string label)
        {
            TextBox box = new TextBox { Width = 72, MaxLength = 1,
                TextAlign = HorizontalAlignment.Center };
            box.TextChanged += delegate { Changed(); };
            PreferenceLayout.Row(parent, PreferenceLayout.Label(Ui.Text("萬用字元：") + " " + label),
                PreferenceLayout.Inline(box));
            return box;
        }

        private void StoreTableSettings()
        {
            if (shownTable == null)
                return;
            shownTable.SetInt("MaximumRadicalLength", (int)tableMaximumLength.Value);
            shownTable.SetString("MatchOneChar", tableMatchOne.Text);
            shownTable.SetString("MatchZeroOrMoreChar", tableMatchMany.Text);
            shownTable.SetBool("ShouldCommitAtMaximumRadicalLength", tableCommitAtMaximum.Checked);
            shownTable.SetBool("ClearReadingBufferAtCompositionError", tableClearOnError.Checked);
            shownTable.SetBool("ComposeWhileTyping", tableComposeWhileTyping.Checked);
            shownTable.SetBool("UseDynamicFrequency", tableDynamicFrequency.Checked);
            shownTable.SetBool("UseSpaceAsFirstCandidateSelectionKey", tableSpaceFirst.Checked);
        }

        // the defaults are OVIMGeneric's for a table it has no preset for
        private void ShowTableSettings()
        {
            StoreTableSettings();
            shownTable = null;
            TableEntry entry = userTables.SelectedItem as TableEntry;
            tableSettings.Enabled = entry != null;
            if (entry == null)
                return;
            string identifier = TableIdentifier(entry.Path);
            Plist plist;
            if (!tablePlists.TryGetValue(identifier, out plist))
            {
                plist = new Plist(Path.Combine(preferencesPath, identifier + ".plist"));
                tablePlists[identifier] = plist;
            }
            bool wasLoading = loading;
            loading = true;
            int length = plist.GetInt("MaximumRadicalLength", 128);
            if (length <= 0)
                length = 128;
            tableMaximumLength.Maximum = Math.Max(128, length);
            tableMaximumLength.Value = length;
            tableMatchOne.Text = plist.GetString("MatchOneChar", "");
            tableMatchMany.Text = plist.GetString("MatchZeroOrMoreChar", "");
            tableCommitAtMaximum.Checked = plist.GetBool("ShouldCommitAtMaximumRadicalLength", false);
            tableClearOnError.Checked = plist.GetBool("ClearReadingBufferAtCompositionError", false);
            tableComposeWhileTyping.Checked = plist.GetBool("ComposeWhileTyping", false);
            tableDynamicFrequency.Checked = plist.GetBool("UseDynamicFrequency", true);
            tableSpaceFirst.Checked = plist.GetBool("UseSpaceAsFirstCandidateSelectionKey", false);
            loading = wasLoading;
            shownTable = plist;
        }

        private sealed class TableEntry
        {
            public readonly string Path;
            public readonly string Label;

            public TableEntry(string path, string label)
            {
                Path = path;
                Label = label;
            }

            public override string ToString()
            {
                return Label;
            }
        }

        // the %cname line is what the input method menu shows
        private static string TableName(string path)
        {
            try
            {
                using (StreamReader reader = new StreamReader(path, System.Text.Encoding.UTF8))
                {
                    for (int line = 0; line < 200; ++line)
                    {
                        string text = reader.ReadLine();
                        if (text == null || text.StartsWith("%chardef"))
                            break;
                        if (text.StartsWith("%cname"))
                            return text.Substring(6).Trim();
                    }
                }
            }
            catch (IOException)
            {
            }
            return System.IO.Path.GetFileNameWithoutExtension(path);
        }

        private void LoadTables()
        {
            StoreTableSettings();
            shownTable = null;
            userTables.Items.Clear();
            if (Directory.Exists(tablesPath))
            {
                foreach (string path in Directory.GetFiles(tablesPath, "*.cin"))
                {
                    userTables.Items.Add(new TableEntry(path, TableName(path) + "（" +
                                                              Path.GetFileName(path) + "）"));
                }
            }
            ShowTableSettings();
            // an imported table joins the menu list with the boxes ticked so far kept
            if (menuInputMethods.Items.Count > 0)
                LoadMenuInputMethods(HiddenInputMethods());
        }

        private void ImportTable()
        {
            using (OpenFileDialog dialog = new OpenFileDialog())
            {
                dialog.Filter = Ui.Text("CIN 字表 (*.cin)|*.cin");
                dialog.Title = Ui.Text("匯入字表");
                if (dialog.ShowDialog(this) != DialogResult.OK)
                    return;
                string target = Path.Combine(tablesPath, Path.GetFileName(dialog.FileName));
                if (File.Exists(target) &&
                    MessageBox.Show(this, Ui.Text("已經有同名的字表，要取代嗎？"), Text, MessageBoxButtons.YesNo,
                                    MessageBoxIcon.Question) != DialogResult.Yes)
                    return;
                try
                {
                    Directory.CreateDirectory(tablesPath);
                    File.Copy(dialog.FileName, target, true);
                }
                catch (Exception error)
                {
                    MessageBox.Show(this, Ui.Text("無法匯入字表：") + error.Message, Text, MessageBoxButtons.OK,
                                    MessageBoxIcon.Error);
                }
                LoadTables();
            }
        }

        private void RemoveTable()
        {
            TableEntry entry = userTables.SelectedItem as TableEntry;
            if (entry == null)
                return;
            if (MessageBox.Show(this, Ui.Text("要移除「") + entry.Label + Ui.Text("」嗎？"), Text, MessageBoxButtons.YesNo,
                                MessageBoxIcon.Question) != DialogResult.Yes)
                return;
            try
            {
                File.Delete(entry.Path);
                Plist removed;
                if (tablePlists.TryGetValue(TableIdentifier(entry.Path), out removed))
                {
                    if (shownTable == removed)
                        shownTable = null;
                    tablePlists.Remove(TableIdentifier(entry.Path));
                }
            }
            catch (Exception error)
            {
                MessageBox.Show(this, Ui.Text("無法移除字表：") + error.Message, Text, MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
            }
            LoadTables();
        }

        // TakaoPhrases and Yahoo's PanelPhrases: the editor is a window of its own
        private Control BuildPhrasePane()
        {
            Panel pane = Page("詞彙設定");
            GroupBox phrases = Group(pane, "自訂詞彙");
            Note(phrases, "加入、修改或移除自己的詞彙，也可以匯入或匯出詞彙檔。");
            Note(phrases, "詞彙檔與 Mac 版千秋輸入法、Yahoo! 奇摩輸入法通用。");
            Button editor = PreferenceLayout.Button(Ui.Text("開啟詞彙編輯器…"));
            editor.Click += delegate { System.Diagnostics.Process.Start(Application.ExecutablePath, PhraseEditorArgument); };
            PreferenceLayout.Row(phrases, PreferenceLayout.Inline(editor));
            GroupBox messages = Group(pane, "符號表常用語");
            Note(messages, "符號表最後一頁的自訂訊息，一行一則。");
            Button edit = PreferenceLayout.Button(Ui.Text("編輯常用語…"));
            edit.Click += delegate { EditCannedMessages(); };
            PreferenceLayout.Row(messages, PreferenceLayout.Inline(edit));
            return pane;
        }

        // the same header ChiaKey::Runtime::userCannedMessagesPath() writes; its first line is skipped
        private void EditCannedMessages()
        {
            string path = Path.Combine(Path.GetDirectoryName(preferencesPath), "UserCannedMessages.txt");
            try
            {
                if (!File.Exists(path))
                {
                    File.WriteAllText(path,
                        "=== 請從本行以下加入自定訊息，一行一則，每行不超過 80 中文或英數字，並請保留這一行 ===\n" +
                        "你好！\n", new System.Text.UTF8Encoding(true));
                }
                System.Diagnostics.Process.Start("notepad.exe", "\"" + path + "\"");
            }
            catch (Exception error)
            {
                MessageBox.Show(this, Ui.Text("無法開啟常用語檔案：") + error.Message, Text, MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
            }
        }

        private Control BuildMiscPane()
        {
            Panel pane = Page("其他設定");
            GroupBox candidate = Group(pane, "選字窗設定");
            highlightColor = Combo(candidate, "提示顏色：", HighlightColors);
            backgroundColor = Combo(candidate, "視窗背景顏色：", BackgroundColors);
            textColor = Combo(candidate, "文字顏色：", TextColors);
            EnableCustomColor(highlightColor);
            EnableCustomColor(backgroundColor);
            EnableCustomColor(textColor);
            backgroundPattern = Check(candidate, "使用背景花紋");
            GroupBox extra = Group(pane, "額外設定");
            beep = Check(extra, "錯誤時發出聲響");
            defaultSound = new RadioButton { Text = Ui.Text("使用系統預設提示聲"), AutoSize = true };
            customSound = new RadioButton { Text = Ui.Text("使用自訂提示聲："), AutoSize = true };
            // Both radio buttons share a parent to keep their selection exclusive.
            PreferenceLayout.Row(extra, PreferenceLayout.Inline(defaultSound, customSound));
            soundPath = new TextBox { ReadOnly = true };
            Button browse = PreferenceLayout.Button(Ui.Text("瀏覽…"));
            Button test = PreferenceLayout.Button(Ui.Text("測試"));
            PreferenceLayout.Row(extra, soundPath);
            PreferenceLayout.Row(extra, PreferenceLayout.Inline(browse, test));
            keyboardFollowsCursor = Check(extra, "標點螢幕鍵盤跟隨游標");
            defaultSound.CheckedChanged += delegate { Changed(); };
            customSound.CheckedChanged += delegate { Changed(); };
            browse.Click += delegate
            {
                using (OpenFileDialog dialog = new OpenFileDialog())
                {
                    dialog.Filter = Ui.Text("提示聲 (*.wav)|*.wav");
                    dialog.InitialDirectory = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "Media");
                    if (dialog.ShowDialog(this) != DialogResult.OK) return;
                    soundPath.Text = dialog.FileName;
                    customSound.Checked = true;
                    Changed();
                }
            };
            test.Click += delegate
            {
                try
                {
                    if (defaultSound.Checked || soundPath.Text.Length == 0) System.Media.SystemSounds.Beep.Play();
                    else new System.Media.SoundPlayer(soundPath.Text).Play();
                }
                catch (Exception error) { MessageBox.Show(this, Ui.Text("無法播放提示聲：") + error.Message, Text); }
            };
            beep.CheckedChanged += delegate
            {
                defaultSound.Enabled = customSound.Enabled = browse.Enabled = test.Enabled = beep.Checked;
            };
            return pane;
        }

        private void EnableCustomColor(ComboBox box)
        {
            box.Items.Add(new Choice("Custom", Ui.Text("自訂…")));
            box.SelectedIndexChanged += delegate
            {
                if (Selected(box) != "Custom") { box.Tag = box.SelectedItem; return; }
                if (loading) return;
                using (ColorDialog dialog = new ColorDialog())
                {
                    dialog.FullOpen = true;
                    if (dialog.ShowDialog(this) != DialogResult.OK)
                    {
                        box.SelectedItem = box.Tag ?? box.Items[0];
                        return;
                    }
                    string value = "Color " + dialog.Color.ToArgb().ToString(CultureInfo.InvariantCulture);
                    int index = box.Items.Add(new Choice(value, Ui.Text("自訂顏色（") + dialog.Color.Name + "）"));
                    box.SelectedIndex = index;
                }
            };
        }

        private Control BuildAboutPane()
        {
            Panel pane = Page("關於千秋輸入法");
            GroupBox about = Group(pane, Ui.Text("關於千秋輸入法"));
            PictureBox logo = new PictureBox { Image = Icon.ToBitmap(), SizeMode = PictureBoxSizeMode.Zoom,
                Size = new Size(72, 72), MaximumSize = new Size(72, 72) };
            Label description = PreferenceLayout.Label(Ui.Text("千秋輸入法\n版本 ") + UpdateService.Default().AppReleaseVersion +
                Ui.Text("\n\n源自 Yahoo! 奇摩輸入法（KeyKey）。\nWindows 前端依原版設計開發，使用 Windows TSF 與千秋共用核心。"));
            PreferenceLayout.Row(about, logo, description);
            LinkLabel project = new LinkLabel { Text = Ui.Text("專案網站與原始碼"), AutoSize = true };
            project.LinkClicked += delegate { System.Diagnostics.Process.Start("https://github.com/chiakich/ChiaKey"); };
            PreferenceLayout.Row(about, project);
            GroupBox counts = Group(pane, Ui.Text("字數統計"));
            wordCountEnabled = Check(counts, "啟用字數統計");
            wordCounts = PreferenceLayout.Label(Ui.Text("讀取字數統計…"));
            PreferenceLayout.Row(counts, wordCounts);
            Button refreshCounts = PreferenceLayout.Button(Ui.Text("重新整理"));
            Button clearCounts = PreferenceLayout.Button(Ui.Text("清除統計"));
            refreshCounts.Click += delegate { RefreshWordCounts(); };
            clearCounts.Click += delegate {
                if (MessageBox.Show(this, Ui.Text("要清除全部字數統計嗎？"), Text, MessageBoxButtons.YesNo,
                    MessageBoxIcon.Question) != DialogResult.Yes) return;
                try { if (!ChiaKeyClearWordCounts()) throw new IOException(Ui.Text("無法清除統計。")); RefreshWordCounts(); }
                catch (Exception error) { MessageBox.Show(this, error.Message, Text); }
            };
            PreferenceLayout.Row(counts, PreferenceLayout.Inline(refreshCounts, clearCounts));
            return pane;
        }

        private void RefreshWordCounts()
        {
            try {
                long today, week, total;
                if (!ChiaKeyWordCounts(out today, out week, out total)) throw new IOException(Ui.Text("無法讀取字數統計。"));
                wordCounts.Text = string.Format(Ui.Text("今日：{0:N0}　最近七天：{1:N0}\r\n累計：{2:N0}"), today, week, total);
            }
            catch (Exception error) { wordCounts.Text = error.Message; }
        }

        private void Changed()
        {
            if (!loading)
                applyButton.Enabled = true;
        }

        // a value the list lacks stays selectable, so saving another setting keeps it
        private static void Select(ComboBox box, Choice[] choices, string value)
        {
            for (int i = 0; i < box.Items.Count; ++i)
            {
                Choice choice = box.Items[i] as Choice;
                if (choice != null && string.Equals(choice.Value, value, StringComparison.OrdinalIgnoreCase))
                {
                    box.SelectedIndex = i;
                    return;
                }
            }
            if (string.IsNullOrEmpty(value))
            {
                box.SelectedIndex = 0;
                return;
            }
            box.SelectedIndex = box.Items.Add(new Choice(value, value + Ui.Text("（自訂）")));
        }

        // other spellings BopomofoKeyboardLayout::LayoutForName accepts; the mac preferences write some
        private static string CanonicalLayout(string layout)
        {
            switch (layout.ToLowerInvariant())
            {
                case "hanyu pinyin":
                case "hanyu-pinyin":
                case "pinyin":
                    return "HanyuPinyin";
                case "bpmfdtnlgkhjvcjvcrzasexuyhgeiawomnklldfjs":
                    return "Hsu";
                case "bpmfdtnlvkhgvcgycjqwsexuaorwiqzpmntlhfjkd":
                    return "ETen26";
                default:
                    return layout;
            }
        }

        private static string Selected(ComboBox box)
        {
            Choice choice = box.SelectedItem as Choice;
            return choice != null ? choice.Value : "";
        }

        // the defaults are the engine's, so an untouched file reads the same
        private void LoadSettings()
        {
            loading = true;
            controlBackslash.Checked = frontend.GetBool("ToggleInputMethodWithControlBackslash", true);
            shiftTogglesEnglish.Checked = frontend.GetBool("ShiftTogglesTemporaryEnglish", true);
            capsLockTogglesEnglish.Checked = frontend.GetBool("EnablesCapsLockAsAlphanumericModeToggle", false);
            Select(converterShortcut, new Choice[0], frontend.GetString("ChineseConverterToggleKey", "s"));
            Select(repeatShortcut, new Choice[0], frontend.GetString("RepeatLastCommitTextKey", "g"));
            Select(reverseLookup, new Choice[0], frontend.GetString("ReverseLookupMethod", ""));
            notifications.Checked = frontend.GetBool("ShouldUseNotifyWindow", true);
            keyboardFollowsCursor.Checked = frontend.GetBool("KeyboardFormShouldFollowCursor", false);
            wordCountEnabled.Checked = frontend.GetBool("WordCountEnabled", false);
            Select(uiLanguage, UiLanguages, Ui.Normalize(frontend.GetString("UiLanguage", "zh-TW")));
            RefreshWordCounts();
            associatedPhrases.Checked = frontend.GetBool("EnableAssociatedPhrases", false);
            simplifiedOutput.Checked = frontend.GetBool("SimplifiedOutput", false);
            LoadMenuInputMethods(frontend.GetStringArray("ModulesSuppressedFromUI"));

            Select(smartLayout, Layouts,
                   CanonicalLayout(smartMandarin.GetString("KeyboardLayout", "Standard")));
            Select(selectionKeys, SelectionKeys, smartMandarin.GetString("CandidateSelectionKeys", ""));
            int buffer = smartMandarin.GetInt("ComposingTextBufferSize", 20);
            if (buffer <= 0)
                buffer = 20;
            // widened rather than clamped, or saving would rewrite a size set by hand
            bufferSize.Minimum = Math.Min(bufferSize.Minimum, buffer);
            bufferSize.Maximum = Math.Max(bufferSize.Maximum, buffer);
            bufferSize.Value = buffer;
            spaceShowsCandidates.Checked = smartMandarin.GetBool("ShowCandidateListWithSpace", true);
            escClears.Checked = smartMandarin.GetBool("ClearComposingTextWithEsc", false);
            cursorAtEnd.Checked = smartMandarin.GetBool("CandidateCursorAtEndOfTargetBlock", false);
            shiftUppercase.Checked =
                smartMandarin.GetBool("ShiftKeyAlwaysCommitUppercaseCharacters", false);
            smartAllCharacters.Checked = AllowsAllCharacters(smartMandarin);

            Select(traditionalLayout, Layouts,
                   CanonicalLayout(traditionalMandarin.GetString("KeyboardLayout", "Standard")));
            traditionalAllCharacters.Checked = AllowsAllCharacters(traditionalMandarin);

            Select(highlightColor, HighlightColors, frontend.GetString("HighlightColor", "Purple"));
            Select(backgroundColor, BackgroundColors, frontend.GetString("BackgroundColor", "Black"));
            Select(textColor, TextColors, frontend.GetString("TextColor", "White"));
            backgroundPattern.Checked = frontend.GetBool("BackgroundPattern", false);
            beep.Checked = frontend.GetBool("ShouldPlaySoundOnTypingError", true);
            string sound = frontend.GetString("SoundFilename", "Default");
            defaultSound.Checked = sound == "Default" || sound.Length == 0;
            customSound.Checked = !defaultSound.Checked;
            soundPath.Text = defaultSound.Checked ? "" : sound;

            // OVIMGeneric's per-table defaults for cj and simplex
            cangjieCommitAtMaximum.Checked = cangjie.GetBool("ShouldCommitAtMaximumRadicalLength", false);
            cangjieComposeWhileTyping.Checked = cangjie.GetBool("ComposeWhileTyping", false);
            cangjieClearOnError.Checked = cangjie.GetBool("ClearReadingBufferAtCompositionError", true);
            cangjieDynamicFrequency.Checked = cangjie.GetBool("UseDynamicFrequency", true);
            simplexComposeWhileTyping.Checked = simplex.GetBool("ComposeWhileTyping", false);
            simplexClearOnError.Checked = simplex.GetBool("ClearReadingBufferAtCompositionError", true);
            cangjieAllCharacters.Checked = AllowsAllCharacters(cangjie);
            simplexAllCharacters.Checked = AllowsAllCharacters(simplex);
            Select(cangjiePunctuation, CangjiePunctuations, cangjie.GetString("UseOverrideTable", ""));
            LoadTables();
            loading = false;
        }

        // the modules ignore an encoding they do not know, so only BIG-5 restricts
        private static bool AllowsAllCharacters(Plist plist)
        {
            return !string.Equals(plist.GetString("UseCharactersSupportedByEncoding", ""), "BIG-5",
                                  StringComparison.OrdinalIgnoreCase);
        }

        private static void SetAllCharacters(Plist plist, bool allowed)
        {
            plist.SetString("UseCharactersSupportedByEncoding", allowed ? "" : "BIG-5");
        }

        private bool SaveSettings()
        {
            if (Selected(converterShortcut).Length > 0 && Selected(converterShortcut) == Selected(repeatShortcut))
            {
                MessageBox.Show(this, Ui.Text("簡繁切換與重送文字不能使用相同的快速鍵。"), Text, MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
                return false;
            }
            frontend.SetBool("ToggleInputMethodWithControlBackslash", controlBackslash.Checked);
            frontend.SetBool("ShiftTogglesTemporaryEnglish", shiftTogglesEnglish.Checked);
            frontend.SetBool("EnablesCapsLockAsAlphanumericModeToggle", capsLockTogglesEnglish.Checked);
            frontend.SetString("ChineseConverterToggleKey", Selected(converterShortcut));
            frontend.SetString("RepeatLastCommitTextKey", Selected(repeatShortcut));
            frontend.SetString("ReverseLookupMethod", Selected(reverseLookup));
            frontend.SetBool("ShouldUseNotifyWindow", notifications.Checked);
            frontend.SetBool("KeyboardFormShouldFollowCursor", keyboardFollowsCursor.Checked);
            frontend.SetBool("WordCountEnabled", wordCountEnabled.Checked);
            frontend.SetString("UiLanguage", Selected(uiLanguage));
            frontend.SetString("SoundFilename", customSound.Checked && soundPath.Text.Length > 0 ? soundPath.Text : "Default");
            frontend.SetBool("EnableAssociatedPhrases", associatedPhrases.Checked);
            frontend.SetBool("SimplifiedOutput", simplifiedOutput.Checked);
            frontend.SetStringArray("ModulesSuppressedFromUI", HiddenInputMethods());
            frontend.SetString("HighlightColor", Selected(highlightColor));
            frontend.SetString("BackgroundColor", Selected(backgroundColor));
            frontend.SetString("TextColor", Selected(textColor));
            frontend.SetBool("BackgroundPattern", backgroundPattern.Checked);
            frontend.SetBool("ShouldPlaySoundOnTypingError", beep.Checked);

            smartMandarin.SetString("KeyboardLayout", Selected(smartLayout));
            smartMandarin.SetString("CandidateSelectionKeys", Selected(selectionKeys));
            smartMandarin.SetInt("ComposingTextBufferSize", (int)bufferSize.Value);
            smartMandarin.SetBool("ShowCandidateListWithSpace", spaceShowsCandidates.Checked);
            smartMandarin.SetBool("ClearComposingTextWithEsc", escClears.Checked);
            smartMandarin.SetBool("CandidateCursorAtEndOfTargetBlock", cursorAtEnd.Checked);
            smartMandarin.SetBool("ShiftKeyAlwaysCommitUppercaseCharacters", shiftUppercase.Checked);
            SetAllCharacters(smartMandarin, smartAllCharacters.Checked);

            traditionalMandarin.SetString("KeyboardLayout", Selected(traditionalLayout));
            SetAllCharacters(traditionalMandarin, traditionalAllCharacters.Checked);

            cangjie.SetBool("ShouldCommitAtMaximumRadicalLength", cangjieCommitAtMaximum.Checked);
            cangjie.SetBool("ComposeWhileTyping", cangjieComposeWhileTyping.Checked);
            cangjie.SetBool("ClearReadingBufferAtCompositionError", cangjieClearOnError.Checked);
            cangjie.SetBool("UseDynamicFrequency", cangjieDynamicFrequency.Checked);
            simplex.SetBool("ComposeWhileTyping", simplexComposeWhileTyping.Checked);
            simplex.SetBool("ClearReadingBufferAtCompositionError", simplexClearOnError.Checked);
            SetAllCharacters(cangjie, cangjieAllCharacters.Checked);
            SetAllCharacters(simplex, simplexAllCharacters.Checked);
            cangjie.SetString("UseOverrideTable", Selected(cangjiePunctuation));
            StoreTableSettings();

            try
            {
                frontend.Save();
                smartMandarin.Save();
                traditionalMandarin.Save();
                cangjie.Save();
                simplex.Save();
                foreach (Plist table in tablePlists.Values)
                    table.Save();
                updates.Save();
                return true;
            }
            catch (Exception error)
            {
                MessageBox.Show(this, Ui.Text("無法儲存設定：") + error.Message, Text, MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
                return false;
            }
        }

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr FindWindow(string className, string windowName);

        [DllImport("user32.dll")]
        private static extern bool SetForegroundWindow(IntPtr window);

        public const string PhraseEditorArgument = "/phrases";

        [STAThread]
        private static void Main(string[] args)
        {
            if (args.Length > 0 && args[0] == "/update-background")
            {
                UpdateService.RunBackground();
                return;
            }
            if (args.Length > 0 && args[0] == "/update-register")
            {
                UpdateService service = UpdateService.Default();
                service.RegisterStartup();
                if (service.Preferences.GetBool("AutoUpdateApp", true) || service.Preferences.GetBool("AutoUpdateLexicon", true))
                    UpdateService.StartBackgroundUpdater(Application.ExecutablePath);
                return;
            }
            // the input menu opens the phrase editor on its own, as Yahoo's separate PhraseEditor.exe
            bool phrases = args.Length > 0 &&
                           string.Equals(args[0], PhraseEditorArgument, StringComparison.OrdinalIgnoreCase);
            bool dictionary = args.Length > 0 && string.Equals(args[0], "/dictionary", StringComparison.OrdinalIgnoreCase);
            string canonicalTitle = dictionary ? DictionaryForm.WindowTitle : phrases ? PhraseEditorForm.WindowTitle : WindowTitle;
            string title = Ui.Text(canonicalTitle);
            bool created;
            using (Mutex single = new Mutex(true, dictionary ? "ChiaKey.Dictionary" : phrases ? "ChiaKey.PhraseEditor" : "ChiaKey.Settings",
                                            out created))
            {
                // a second launch from the language bar brings the open window up instead
                if (!created)
                {
                    IntPtr existing = FindWindow(null, title);
                    foreach (string locale in new[] { "zh-TW", "zh-CN", "en" })
                        if (existing == IntPtr.Zero) existing = FindWindow(null, Ui.Translate(canonicalTitle, locale));
                    if (existing != IntPtr.Zero)
                        SetForegroundWindow(existing);
                    return;
                }
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                if (dictionary)
                {
                    Application.Run(new DictionaryForm());
                    return;
                }
                if (phrases)
                {
                    Application.Run(new PhraseEditorForm());
                    return;
                }
                string data = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "ChiaKey");
                bool about = args.Length > 0 && string.Equals(args[0], "/about", StringComparison.OrdinalIgnoreCase);
                Application.Run(new SettingsForm(data, about ? 8 : 0));
            }
        }
    }
}
