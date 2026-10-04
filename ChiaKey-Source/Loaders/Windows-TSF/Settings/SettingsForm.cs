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
            Cursor = Cursors.Hand;
        }

        public bool Selected
        {
            get { return selected; }
            set { selected = value; Invalidate(); }
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
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
            Rectangle textRect = new Rectangle(0, top + iconSize + gap, Width, textSize.Height);
            TextRenderer.DrawText(g, Text, Font, textRect, Color.Black,
                                  TextFormatFlags.HorizontalCenter | TextFormatFlags.Top);
        }
    }

    internal sealed class SettingsForm : Form
    {
        private const string WindowTitle = "千秋輸入法 偏好設定";
        // past the widest caption, 視窗背景顏色：
        private const int ControlLeft = 140;
        private const int ToolbarHeight = 50;

        private static readonly Choice[] Layouts = {
            new Choice("Standard", "標準"),
            new Choice("ETen", "倚天"),
            new Choice("Hsu", "許氏"),
            new Choice("ETen26", "倚天 26 鍵"),
            new Choice("HanyuPinyin", "漢語拼音"),
        };
        private static readonly Choice[] SelectionKeys = {
            new Choice("", "依鍵盤配置"),
            new Choice("12345678", "12345678"),
            new Choice("asdfghjk", "asdfghjk"),
            new Choice("asdfzxcv", "asdfzxcv"),
            new Choice("aoeuidht", "aoeuidht"),
            new Choice("aoeu;qjk", "aoeu;qjk"),
        };
        private static readonly Choice[] HighlightColors = {
            new Choice("Purple", "紫色"),
            new Choice("Green", "綠色"),
            new Choice("Yellow", "黃色"),
            new Choice("Red", "紅色"),
        };
        private static readonly Choice[] BackgroundColors = {
            new Choice("Black", "黑色"),
            new Choice("White", "白色"),
        };
        private static readonly Choice[] TextColors = {
            new Choice("White", "白色"),
            new Choice("Black", "黑色"),
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
            new Choice("", "全部使用全形標點"),
            new Choice("Punctuations-cj-mixedwidth-cin", "除了逗號與句號外，使用半形標點"),
            new Choice("Punctuations-cj-halfwidth-cin", "全部使用半形標點"),
        };
        // the menu order, as InputMethods() in ChiaKeyEngine.cpp lists them
        private static readonly Choice[] BuiltInInputMethods = {
            new Choice("SmartMandarin", "好打注音"),
            new Choice("TraditionalMandarin", "傳統注音"),
            new Choice("Generic-cj-cin", "倉頡"),
            new Choice("Generic-simplex-cin", "簡易"),
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
        private CheckBox transparentStatusBar;
        private CheckBox statusBarInTray;
        private CheckBox wordCountEnabled;
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
            Text = WindowTitle;
            Font = new Font("Microsoft JhengHei UI", 9F);
            Icon = PhraseEditorForm.LoadIcon("app.ico");
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(500, 520);
            BackColor = Color.White;

            BuildToolbar();
            BuildButtons();
            content.Location = new Point(0, ToolbarHeight);
            content.Size = new Size(500, 470 - ToolbarHeight);
            Controls.Add(content);

            AddPane("一般(&G)", "general.tiff", BuildGeneralPane());
            AddPane("注音(&P)", "phonetic.tiff", BuildPhoneticPane());
            AddPane("倉頡(&J)", "cangjie.tiff", BuildCangjiePane());
            AddPane("簡易(&S)", "simplex.tiff", BuildSimplexPane());
            AddPane("泛用(&E)", "generic.tiff", BuildGenericPane());
            AddPane("詞彙(&H)", "phrase.tiff", BuildPhrasePane());
            AddPane("其他(&M)", "plugin.tiff", BuildMiscPane());
            updates = new UpdatePane(Changed);
            AddPane("更新(&U)", "update.tiff", updates);
            AddPane("關於(&B)", "app.ico", BuildAboutPane());

            LoadSettings();
            ShowPane(Math.Max(0, Math.Min(initialPane, panes.Count - 1)));
            ResumeLayout(false);
            PerformLayout();
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
            toolbar.Location = new Point(0, 0);
            toolbar.Size = new Size(500, ToolbarHeight);
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
            Button okButton = new Button();
            okButton.Text = "確定(&O)";
            okButton.Bounds = new Rectangle(232, 482, 82, 26);
            okButton.Click += delegate { if (SaveSettings()) Close(); };
            Button cancelButton = new Button();
            cancelButton.Text = "取消(&C)";
            cancelButton.Bounds = new Rectangle(322, 482, 82, 26);
            cancelButton.Click += delegate { Close(); };
            applyButton.Text = "套用(&A)";
            applyButton.Bounds = new Rectangle(412, 482, 82, 26);
            applyButton.Enabled = false;
            applyButton.Click += delegate { if (SaveSettings()) applyButton.Enabled = false; };
            Controls.Add(okButton);
            Controls.Add(cancelButton);
            Controls.Add(applyButton);
            AcceptButton = okButton;
            CancelButton = cancelButton;
        }

        private void AddPane(string title, string icon, Control pane)
        {
            int index = toolbarItems.Count;
            ToolbarItem item = new ToolbarItem(title, LoadIcon(icon));
            item.Bounds = new Rectangle(2 + index * 55, 2, 53, ToolbarHeight - 4);
            item.Click += delegate { ShowPane(index); };
            toolbar.Controls.Add(item);
            toolbarItems.Add(item);
            pane.Dock = DockStyle.Fill;
            pane.Visible = false;
            content.Controls.Add(pane);
            panes.Add(pane);
        }

        protected override bool ProcessMnemonic(char charCode)
        {
            for (int index = 0; index < toolbarItems.Count; ++index)
            {
                if (IsMnemonic(charCode, toolbarItems[index].Text))
                {
                    ShowPane(index);
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

        private static Label Title(Control parent, string text)
        {
            Label label = new Label();
            label.Text = text;
            label.Font = new Font("Microsoft JhengHei UI", 11F, FontStyle.Bold);
            label.AutoSize = true;
            label.Location = new Point(16, 10);
            parent.Controls.Add(label);
            return label;
        }

        private static GroupBox Group(Control parent, string text, int top, int height)
        {
            GroupBox group = new GroupBox();
            group.Text = text;
            group.Bounds = new Rectangle(16, top, 468, height);
            parent.Controls.Add(group);
            return group;
        }

        private CheckBox Check(Control parent, string text, int left, int top)
        {
            CheckBox box = new CheckBox();
            box.Text = text;
            box.AutoSize = true;
            box.Location = new Point(left, top);
            box.CheckedChanged += delegate { Changed(); };
            parent.Controls.Add(box);
            return box;
        }

        private ComboBox Combo(Control parent, string label, int top, Choice[] choices)
        {
            Label caption = new Label();
            caption.Text = label;
            caption.AutoSize = true;
            caption.Location = new Point(14, top + 4);
            parent.Controls.Add(caption);
            ComboBox box = new ComboBox();
            box.DropDownStyle = ComboBoxStyle.DropDownList;
            box.Bounds = new Rectangle(ControlLeft, top, 160, 24);
            box.Items.AddRange(choices);
            box.SelectedIndexChanged += delegate { Changed(); };
            parent.Controls.Add(box);
            return box;
        }

        private Control BuildGeneralPane()
        {
            Panel pane = new Panel { AutoScroll = true };
            Title(pane, "一般設定");
            GroupBox basic = Group(pane, "基本功能", 44, 162);
            controlBackslash = Check(basic, "使用 Ctrl + \\ 切換到下一個輸入法", 14, 24);
            shiftTogglesEnglish = Check(basic, "使用單擊 Shift 按鍵切換中英模式", 14, 50);
            capsLockTogglesEnglish = Check(basic, "使用 Caps Lock 按鍵切換中英文", 14, 76);
            capsLockTogglesEnglish.CheckedChanged += delegate
            {
                shiftTogglesEnglish.Enabled = !capsLockTogglesEnglish.Checked;
            };
            associatedPhrases = Check(basic, "輸入後顯示聯想詞", 14, 102);
            simplifiedOutput = Check(basic, "簡體輸出", 14, 128);
            notifications = Check(basic, "使用提示視窗", 250, 128);

            Choice[] shortcuts = new Choice[27];
            shortcuts[0] = new Choice("", "無");
            for (int index = 0; index < 26; ++index)
                shortcuts[index + 1] = new Choice(((char)('a' + index)).ToString(),
                    "Ctrl + Alt + " + (char)('A' + index));
            GroupBox keys = Group(pane, "快速鍵", 216, 124);
            converterShortcut = Combo(keys, "簡繁中文切換快速鍵：", 22, shortcuts);
            repeatShortcut = Combo(keys, "送出最近一次輸入的文字：", 54, shortcuts);
            converterShortcut.Left = repeatShortcut.Left = 205;
            reverseLookup = Combo(keys, "字根反查功能：", 86, new Choice[] {
                new Choice("", "無"), new Choice("ReverseLookup-Generic-cj-cin", "倉頡"),
                new Choice("ReverseLookup-Mandarin-bpmf-cin", "注音"),
                new Choice("ReverseLookup-Mandarin-bpmf-cin-HanyuPinyin", "漢語拼音") });
            reverseLookup.Left = 205;

            GroupBox menu = Group(pane, "輸入法選單管理", 350, 130);
            Label hint = new Label();
            hint.Text = "取消勾選的輸入法不會出現在輸入選單中（使用中的除外）。";
            hint.AutoSize = true;
            hint.Location = new Point(12, 24);
            menu.Controls.Add(hint);
            menuInputMethods = new CheckedListBox();
            menuInputMethods.CheckOnClick = true;
            menuInputMethods.IntegralHeight = false;
            menuInputMethods.Bounds = new Rectangle(14, 50, 440, 170);
            menuInputMethods.ItemCheck += delegate { Changed(); };
            menu.Controls.Add(menuInputMethods);
            menu.Layout += delegate
            {
                menuInputMethods.Height = menu.ClientSize.Height - menuInputMethods.Top -
                                          menuInputMethods.Left;
            };
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
            Panel pane = new Panel();
            Title(pane, "注音輸入法設定");
            GroupBox smart = Group(pane, "好打注音", 44, 254);
            smartLayout = Combo(smart, "鍵盤配置：", 22, Layouts);
            selectionKeys = Combo(smart, "選字鍵設定：", 54, SelectionKeys);

            Label bufferLabel = new Label();
            bufferLabel.Text = "輸入緩衝區：";
            bufferLabel.AutoSize = true;
            bufferLabel.Location = new Point(14, 90);
            smart.Controls.Add(bufferLabel);
            bufferSize = new NumericUpDown();
            bufferSize.Minimum = 10;
            bufferSize.Maximum = 20;
            bufferSize.Bounds = new Rectangle(ControlLeft, 86, 60, 24);
            bufferSize.ValueChanged += delegate { Changed(); };
            smart.Controls.Add(bufferSize);
            Label bufferUnit = new Label();
            bufferUnit.Text = "字（最多 20 字）";
            bufferUnit.AutoSize = true;
            bufferUnit.Location = new Point(ControlLeft + 66, 90);
            smart.Controls.Add(bufferUnit);

            Label typing = new Label();
            typing.Text = "打字功能：";
            typing.AutoSize = true;
            typing.Location = new Point(14, 122);
            smart.Controls.Add(typing);
            spaceShowsCandidates = Check(smart, "使用空白鍵選字", ControlLeft, 120);
            escClears = Check(smart, "按下 ESC 按鍵後清除全部編輯區內容", ControlLeft, 146);
            cursorAtEnd = Check(smart, "選字時游標放在詞尾", ControlLeft, 172);
            shiftUppercase = Check(smart, "按住 Shift 時輸入大寫英文", ControlLeft, 198);
            smartAllCharacters = Check(smart, AllCharacters, ControlLeft, 224);

            GroupBox traditional = Group(pane, "傳統注音", 306, 86);
            traditionalLayout = Combo(traditional, "鍵盤配置：", 22, Layouts);
            traditionalAllCharacters = Check(traditional, AllCharacters, ControlLeft, 54);
            return pane;
        }

        private const string ExclusiveNote =
            "請注意：不能夠同時勾選「組字錯誤時清除字根」與「打字時同時組字」這兩個選項。";

        private static Label Note(Control parent, string text, int top)
        {
            Label label = new Label();
            label.Text = text;
            label.ForeColor = Color.DimGray;
            label.Bounds = new Rectangle(16, top, 468, 40);
            parent.Controls.Add(label);
            return label;
        }

        // the original panels enforce this: composing as you type has nothing to clear
        private static void Exclusive(CheckBox first, CheckBox second)
        {
            first.CheckedChanged += delegate { if (first.Checked) second.Checked = false; };
            second.CheckedChanged += delegate { if (second.Checked) first.Checked = false; };
        }

        private Control BuildCangjiePane()
        {
            Panel pane = new Panel();
            Title(pane, "倉頡輸入法設定");
            GroupBox typing = Group(pane, "打字功能", 44, 162);
            cangjieCommitAtMaximum = Check(typing, "打到字根最大長度時立刻組字", 14, 24);
            cangjieComposeWhileTyping = Check(typing, "打字時同時組字", 14, 50);
            cangjieClearOnError = Check(typing, "組字錯誤時清除字根", 14, 76);
            cangjieDynamicFrequency = Check(typing, "使用動態字頻調整（將常用字移動到選字列表前方）", 14, 102);
            cangjieAllCharacters = Check(typing, AllCharacters, 14, 128);
            Exclusive(cangjieComposeWhileTyping, cangjieClearOnError);
            GroupBox punctuation = Group(pane, "標點符號", 214, 60);
            cangjiePunctuation = Combo(punctuation, "標點符號樣式：", 22, CangjiePunctuations);
            cangjiePunctuation.Width = 300;
            Note(pane, ExclusiveNote, 284);
            return pane;
        }

        private Control BuildSimplexPane()
        {
            Panel pane = new Panel();
            Title(pane, "簡易輸入法設定");
            GroupBox typing = Group(pane, "打字功能", 44, 110);
            simplexComposeWhileTyping = Check(typing, "打字時同時組字", 14, 24);
            simplexClearOnError = Check(typing, "組字錯誤時清除字根", 14, 50);
            simplexAllCharacters = Check(typing, AllCharacters, 14, 76);
            Exclusive(simplexComposeWhileTyping, simplexClearOnError);
            Note(pane, ExclusiveNote, 164);
            return pane;
        }

        private Control BuildGenericPane()
        {
            Panel pane = new Panel();
            Title(pane, "泛用輸入法設定");
            GroupBox tables = Group(pane, "自訂字表（.cin）", 44, 150);
            userTables = new ListBox();
            userTables.Bounds = new Rectangle(14, 24, 330, 110);
            userTables.IntegralHeight = false;
            userTables.SelectedIndexChanged += delegate { ShowTableSettings(); };
            tables.Controls.Add(userTables);
            // a ListBox does not keep its height through DPI scaling, so it follows the group
            tables.Layout += delegate
            {
                userTables.Height = tables.ClientSize.Height - userTables.Top - userTables.Left;
            };

            Button import = new Button();
            import.Text = "匯入…";
            import.Bounds = new Rectangle(356, 24, 98, 26);
            import.Click += delegate { ImportTable(); };
            tables.Controls.Add(import);
            Button remove = new Button();
            remove.Text = "移除";
            remove.Bounds = new Rectangle(356, 56, 98, 26);
            remove.Click += delegate { RemoveTable(); };
            tables.Controls.Add(remove);
            Button open = new Button();
            open.Text = "開啟資料夾";
            open.Bounds = new Rectangle(356, 88, 98, 26);
            open.Click += delegate
            {
                Directory.CreateDirectory(tablesPath);
                System.Diagnostics.Process.Start("explorer.exe", "\"" + tablesPath + "\"");
            };
            tables.Controls.Add(open);

            // TakaoGenericSettings, one table at a time
            tableSettings = Group(pane, "選取字表的設定", 202, 164);
            tableSettings.Enabled = false;
            Label lengthLabel = new Label();
            lengthLabel.Text = "字根組合最大長度：";
            lengthLabel.AutoSize = true;
            lengthLabel.Location = new Point(14, 26);
            tableSettings.Controls.Add(lengthLabel);
            tableMaximumLength = new NumericUpDown();
            tableMaximumLength.Minimum = 1;
            tableMaximumLength.Maximum = 128;
            tableMaximumLength.Bounds = new Rectangle(ControlLeft, 22, 60, 24);
            tableMaximumLength.ValueChanged += delegate { Changed(); };
            tableSettings.Controls.Add(tableMaximumLength);

            Label wildcardLabel = new Label();
            wildcardLabel.Text = "萬用字元：";
            wildcardLabel.AutoSize = true;
            wildcardLabel.Location = new Point(14, 56);
            tableSettings.Controls.Add(wildcardLabel);
            tableMatchOne = Wildcard(tableSettings, "單一長度", ControlLeft, 52);
            tableMatchMany = Wildcard(tableSettings, "不限長度", ControlLeft + 120, 52);

            tableCommitAtMaximum = Check(tableSettings, "打到字根最大長度時立刻組字", 14, 82);
            tableClearOnError = Check(tableSettings, "組字錯誤時清除字根", 250, 82);
            tableComposeWhileTyping = Check(tableSettings, "打字時同時組字", 14, 108);
            tableDynamicFrequency = Check(tableSettings, "使用動態字頻調整", 250, 108);
            tableSpaceFirst = Check(tableSettings, "空白鍵選一字，第一選字鍵選第二字", 14, 134);
            Exclusive(tableComposeWhileTyping, tableClearOnError);

            Label note = Note(pane, "大易、行列、嘸蝦米等字表不隨附，請自行匯入 .cin 檔。\n" +
                                    "新增或移除字表後，需要重新開啟正在使用的程式，才會出現在輸入法選單中。",
                              374);
            note.AutoSize = true;
            return pane;
        }

        private TextBox Wildcard(Control parent, string label, int left, int top)
        {
            Label caption = new Label();
            caption.Text = label;
            caption.AutoSize = true;
            caption.Location = new Point(left, top + 4);
            parent.Controls.Add(caption);
            TextBox box = new TextBox();
            box.MaxLength = 1;
            box.TextAlign = HorizontalAlignment.Center;
            box.Bounds = new Rectangle(left + 64, top, 30, 24);
            box.TextChanged += delegate { Changed(); };
            parent.Controls.Add(box);
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
                dialog.Filter = "CIN 字表 (*.cin)|*.cin";
                dialog.Title = "匯入字表";
                if (dialog.ShowDialog(this) != DialogResult.OK)
                    return;
                string target = Path.Combine(tablesPath, Path.GetFileName(dialog.FileName));
                if (File.Exists(target) &&
                    MessageBox.Show(this, "已經有同名的字表，要取代嗎？", Text, MessageBoxButtons.YesNo,
                                    MessageBoxIcon.Question) != DialogResult.Yes)
                    return;
                try
                {
                    Directory.CreateDirectory(tablesPath);
                    File.Copy(dialog.FileName, target, true);
                }
                catch (Exception error)
                {
                    MessageBox.Show(this, "無法匯入字表：" + error.Message, Text, MessageBoxButtons.OK,
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
            if (MessageBox.Show(this, "要移除「" + entry.Label + "」嗎？", Text, MessageBoxButtons.YesNo,
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
                MessageBox.Show(this, "無法移除字表：" + error.Message, Text, MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
            }
            LoadTables();
        }

        // TakaoPhrases and Yahoo's PanelPhrases: the editor is a window of its own
        private Control BuildPhrasePane()
        {
            Panel pane = new Panel();
            Title(pane, "詞彙設定");
            GroupBox phrases = Group(pane, "自訂詞彙", 44, 120);
            Label about = new Label();
            about.Text = "加入、修改或移除自己的詞彙，也可以匯入或匯出詞彙檔。";
            about.AutoSize = true;
            about.Location = new Point(12, 26);
            phrases.Controls.Add(about);
            Label format = new Label();
            format.Text = "詞彙檔與 Mac 版千秋輸入法、Yahoo! 奇摩輸入法通用。";
            format.ForeColor = Color.DimGray;
            format.AutoSize = true;
            format.Location = new Point(12, 48);
            phrases.Controls.Add(format);
            Button editor = new Button();
            editor.Text = "開啟詞彙編輯器…";
            editor.Bounds = new Rectangle(14, 78, 160, 28);
            editor.Click += delegate
            {
                System.Diagnostics.Process.Start(Application.ExecutablePath, PhraseEditorArgument);
            };
            phrases.Controls.Add(editor);

            GroupBox messages = Group(pane, "符號表常用語", 174, 98);
            Label messagesAbout = new Label();
            messagesAbout.Text = "符號表最後一頁的自訂訊息，一行一則。";
            messagesAbout.AutoSize = true;
            messagesAbout.Location = new Point(12, 26);
            messages.Controls.Add(messagesAbout);
            Button edit = new Button();
            edit.Text = "編輯常用語…";
            edit.Bounds = new Rectangle(14, 56, 160, 28);
            edit.Click += delegate { EditCannedMessages(); };
            messages.Controls.Add(edit);
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
                MessageBox.Show(this, "無法開啟常用語檔案：" + error.Message, Text, MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
            }
        }

        private Control BuildMiscPane()
        {
            Panel pane = new Panel();
            Title(pane, "其他設定");
            pane.AutoScroll = true;
            pane.AutoScrollMinSize = new Size(470, 458);
            GroupBox candidate = Group(pane, "選字窗設定", 44, 150);
            highlightColor = Combo(candidate, "提示顏色：", 22, HighlightColors);
            backgroundColor = Combo(candidate, "視窗背景顏色：", 54, BackgroundColors);
            textColor = Combo(candidate, "文字顏色：", 86, TextColors);
            EnableCustomColor(highlightColor);
            EnableCustomColor(backgroundColor);
            EnableCustomColor(textColor);
            backgroundPattern = Check(candidate, "使用背景花紋", 14, 120);
            GroupBox extra = Group(pane, "額外設定", 204, 162);
            keyboardFollowsCursor = Check(pane, "標點螢幕鍵盤跟隨游標", 20, 374);
            transparentStatusBar = Check(pane, "使用半透明狀態列", 20, 400);
            statusBarInTray = Check(pane, "狀態列最小化到系統匣（雙擊狀態列收合）", 20, 426);
            beep = Check(extra, "錯誤時發出聲響", 14, 24);
            defaultSound = new RadioButton { Text = "使用系統預設提示聲", AutoSize = true,
                Location = new Point(32, 51) };
            customSound = new RadioButton { Text = "使用自訂提示聲：", AutoSize = true,
                Location = new Point(32, 78) };
            soundPath = new TextBox { ReadOnly = true, Bounds = new Rectangle(32, 105, 322, 24) };
            Button browse = new Button { Text = "瀏覽…", Bounds = new Rectangle(364, 105, 82, 25) };
            Button test = new Button { Text = "測試", Bounds = new Rectangle(364, 24, 82, 25) };
            extra.Controls.AddRange(new Control[] { defaultSound, customSound, soundPath, browse, test });
            defaultSound.CheckedChanged += delegate { Changed(); };
            customSound.CheckedChanged += delegate { Changed(); };
            browse.Click += delegate
            {
                using (OpenFileDialog dialog = new OpenFileDialog())
                {
                    dialog.Filter = "提示聲 (*.wav)|*.wav";
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
                catch (Exception error) { MessageBox.Show(this, "無法播放提示聲：" + error.Message, Text); }
            };
            beep.CheckedChanged += delegate
            {
                defaultSound.Enabled = customSound.Enabled = browse.Enabled = test.Enabled = beep.Checked;
            };
            return pane;
        }

        private void EnableCustomColor(ComboBox box)
        {
            box.Items.Add(new Choice("Custom", "自訂…"));
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
                    int index = box.Items.Add(new Choice(value, "自訂顏色（" + dialog.Color.Name + "）"));
                    box.SelectedIndex = index;
                }
            };
        }

        private Control BuildAboutPane()
        {
            Panel pane = new Panel();
            Title(pane, "關於千秋輸入法");
            PictureBox logo = new PictureBox { Image = Icon.ToBitmap(), SizeMode = PictureBoxSizeMode.Zoom,
                Bounds = new Rectangle(24, 56, 72, 72) };
            Label description = new Label { Bounds = new Rectangle(116, 58, 360, 190),
                Text = "千秋輸入法\n版本 " + UpdateService.Default().AppReleaseVersion +
                    "\n\n源自 Yahoo! 奇摩輸入法（KeyKey）。\nWindows 前端依原版設計開發，使用 Windows TSF 與千秋共用核心。" };
            LinkLabel project = new LinkLabel { Text = "專案網站與原始碼", AutoSize = true,
                Location = new Point(24, 272) };
            project.LinkClicked += delegate { System.Diagnostics.Process.Start("https://github.com/chiakich/ChiaKey"); };
            pane.Controls.AddRange(new Control[] { logo, description, project });
            wordCountEnabled = Check(pane, "啟用字數統計", 20, 250);
            wordCounts = new Label { Bounds = new Rectangle(20, 280, 430, 55), Text = "讀取字數統計…" };
            Button refreshCounts = new Button { Text = "重新整理", Bounds = new Rectangle(20, 344, 100, 26) };
            Button clearCounts = new Button { Text = "清除統計", Bounds = new Rectangle(130, 344, 100, 26) };
            refreshCounts.Click += delegate { RefreshWordCounts(); };
            clearCounts.Click += delegate {
                if (MessageBox.Show(this, "要清除全部字數統計嗎？", Text, MessageBoxButtons.YesNo,
                    MessageBoxIcon.Question) != DialogResult.Yes) return;
                try { if (!ChiaKeyClearWordCounts()) throw new IOException("無法清除統計。"); RefreshWordCounts(); }
                catch (Exception error) { MessageBox.Show(this, error.Message, Text); }
            };
            pane.Controls.AddRange(new Control[] { wordCounts, refreshCounts, clearCounts });
            return pane;
        }

        private void RefreshWordCounts()
        {
            try {
                long today, week, total;
                if (!ChiaKeyWordCounts(out today, out week, out total)) throw new IOException("無法讀取字數統計。");
                wordCounts.Text = string.Format("今日：{0:N0}　最近七天：{1:N0}\r\n累計：{2:N0}", today, week, total);
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
            box.SelectedIndex = box.Items.Add(new Choice(value, value + "（自訂）"));
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
            transparentStatusBar.Checked = frontend.GetBool("ShouldUseTransparentStatusBar", false);
            statusBarInTray.Checked = frontend.GetBool("ShouldUseSystemTray", false);
            wordCountEnabled.Checked = frontend.GetBool("WordCountEnabled", false);
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
                MessageBox.Show(this, "簡繁切換與重送文字不能使用相同的快速鍵。", Text, MessageBoxButtons.OK,
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
            frontend.SetBool("ShouldUseTransparentStatusBar", transparentStatusBar.Checked);
            frontend.SetBool("ShouldUseSystemTray", statusBarInTray.Checked);
            frontend.SetBool("WordCountEnabled", wordCountEnabled.Checked);
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
                MessageBox.Show(this, "無法儲存設定：" + error.Message, Text, MessageBoxButtons.OK,
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
            string title = phrases ? PhraseEditorForm.WindowTitle : WindowTitle;
            bool created;
            using (Mutex single = new Mutex(true, phrases ? "ChiaKey.PhraseEditor" : "ChiaKey.Settings",
                                            out created))
            {
                // a second launch from the language bar brings the open window up instead
                if (!created)
                {
                    IntPtr existing = FindWindow(null, title);
                    if (existing != IntPtr.Zero)
                        SetForegroundWindow(existing);
                    return;
                }
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
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
