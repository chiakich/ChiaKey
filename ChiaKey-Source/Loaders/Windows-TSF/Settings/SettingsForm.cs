// after Yahoo! KeyKey's TakaoPreference; C# 5 so the csc.exe in .NET Framework 4.x builds it

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
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

        private CheckBox controlBackslash;
        private CheckBox associatedPhrases;
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
        private CheckBox cangjieCommitAtMaximum;
        private CheckBox cangjieComposeWhileTyping;
        private CheckBox cangjieClearOnError;
        private CheckBox cangjieDynamicFrequency;
        private CheckBox simplexComposeWhileTyping;
        private CheckBox simplexClearOnError;
        private ListBox userTables;

        public SettingsForm(string dataPath)
        {
            string preferencesPath = Path.Combine(dataPath, "Preferences");
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
            Icon = System.Drawing.Icon.ExtractAssociatedIcon(Application.ExecutablePath);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(500, 470);
            BackColor = Color.White;

            BuildToolbar();
            BuildButtons();
            content.Location = new Point(0, ToolbarHeight);
            content.Size = new Size(500, 420 - ToolbarHeight);
            Controls.Add(content);

            AddPane("一般(&G)", "general.tiff", BuildGeneralPane());
            AddPane("注音(&P)", "phonetic.tiff", BuildPhoneticPane());
            AddPane("倉頡(&J)", "cangjie.tiff", BuildCangjiePane());
            AddPane("簡易(&S)", "simplex.tiff", BuildSimplexPane());
            AddPane("泛用(&E)", "generic.tiff", BuildGenericPane());
            AddPane("其他(&M)", "plugin.tiff", BuildMiscPane());

            LoadSettings();
            ShowPane(0);
            ResumeLayout(false);
            PerformLayout();
        }

        private static Image LoadIcon(string name)
        {
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
            okButton.Bounds = new Rectangle(232, 432, 82, 26);
            okButton.Click += delegate { if (SaveSettings()) Close(); };
            Button cancelButton = new Button();
            cancelButton.Text = "取消(&C)";
            cancelButton.Bounds = new Rectangle(322, 432, 82, 26);
            cancelButton.Click += delegate { Close(); };
            applyButton.Text = "套用(&A)";
            applyButton.Bounds = new Rectangle(412, 432, 82, 26);
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
            item.Bounds = new Rectangle(8 + index * 72, 2, 68, ToolbarHeight - 4);
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

        // --- panes --------------------------------------------------------------

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
            Panel pane = new Panel();
            Title(pane, "一般設定");
            GroupBox basic = Group(pane, "基本功能", 44, 84);
            controlBackslash = Check(basic, "使用 Ctrl + \\ 切換中英模式", 14, 24);
            associatedPhrases = Check(basic, "輸入後顯示聯想詞", 14, 50);
            return pane;
        }

        private Control BuildPhoneticPane()
        {
            Panel pane = new Panel();
            Title(pane, "注音輸入法設定");
            GroupBox smart = Group(pane, "好打注音", 44, 228);
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

            GroupBox traditional = Group(pane, "傳統注音", 280, 60);
            traditionalLayout = Combo(traditional, "鍵盤配置：", 22, Layouts);
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
            GroupBox typing = Group(pane, "打字功能", 44, 136);
            cangjieCommitAtMaximum = Check(typing, "打到字根最大長度時立刻組字", 14, 24);
            cangjieComposeWhileTyping = Check(typing, "打字時同時組字", 14, 50);
            cangjieClearOnError = Check(typing, "組字錯誤時清除字根", 14, 76);
            cangjieDynamicFrequency = Check(typing, "使用動態字頻調整（將常用字移動到選字列表前方）", 14, 102);
            Exclusive(cangjieComposeWhileTyping, cangjieClearOnError);
            Note(pane, ExclusiveNote, 190);
            return pane;
        }

        private Control BuildSimplexPane()
        {
            Panel pane = new Panel();
            Title(pane, "簡易輸入法設定");
            GroupBox typing = Group(pane, "打字功能", 44, 84);
            simplexComposeWhileTyping = Check(typing, "打字時同時組字", 14, 24);
            simplexClearOnError = Check(typing, "組字錯誤時清除字根", 14, 50);
            Exclusive(simplexComposeWhileTyping, simplexClearOnError);
            Note(pane, ExclusiveNote, 138);
            return pane;
        }

        private Control BuildGenericPane()
        {
            Panel pane = new Panel();
            Title(pane, "泛用輸入法設定");
            GroupBox tables = Group(pane, "自訂字表（.cin）", 44, 200);
            userTables = new ListBox();
            userTables.Bounds = new Rectangle(14, 24, 330, 160);
            userTables.IntegralHeight = false;
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

            Label note = Note(pane, "大易、行列、嘸蝦米等字表不隨附，請自行匯入 .cin 檔。\n" +
                                    "新增或移除字表後，需要重新開啟正在使用的程式，才會出現在輸入法選單中。",
                              254);
            note.AutoSize = true;
            return pane;
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
            userTables.Items.Clear();
            if (!Directory.Exists(tablesPath))
                return;
            foreach (string path in Directory.GetFiles(tablesPath, "*.cin"))
            {
                userTables.Items.Add(new TableEntry(path, TableName(path) + "（" +
                                                          Path.GetFileName(path) + "）"));
            }
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
            }
            catch (Exception error)
            {
                MessageBox.Show(this, "無法移除字表：" + error.Message, Text, MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
            }
            LoadTables();
        }

        private Control BuildMiscPane()
        {
            Panel pane = new Panel();
            Title(pane, "其他設定");
            GroupBox candidate = Group(pane, "選字窗設定", 44, 150);
            highlightColor = Combo(candidate, "提示顏色：", 22, HighlightColors);
            backgroundColor = Combo(candidate, "視窗背景顏色：", 54, BackgroundColors);
            textColor = Combo(candidate, "文字顏色：", 86, TextColors);
            backgroundPattern = Check(candidate, "使用背景花紋", 14, 120);
            GroupBox extra = Group(pane, "額外設定", 204, 60);
            beep = Check(extra, "錯誤時發出聲響", 14, 24);
            return pane;
        }

        // --- settings -----------------------------------------------------------

        private void Changed()
        {
            if (!loading)
                applyButton.Enabled = true;
        }

        // a value the list lacks stays selectable, so saving another setting keeps it
        private static void Select(ComboBox box, Choice[] choices, string value)
        {
            for (int i = 0; i < choices.Length; ++i)
            {
                if (string.Equals(choices[i].Value, value, StringComparison.OrdinalIgnoreCase))
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
            associatedPhrases.Checked = frontend.GetBool("EnableAssociatedPhrases", false);

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

            Select(traditionalLayout, Layouts,
                   CanonicalLayout(traditionalMandarin.GetString("KeyboardLayout", "Standard")));

            Select(highlightColor, HighlightColors, frontend.GetString("HighlightColor", "Purple"));
            Select(backgroundColor, BackgroundColors, frontend.GetString("BackgroundColor", "Black"));
            Select(textColor, TextColors, frontend.GetString("TextColor", "White"));
            backgroundPattern.Checked = frontend.GetBool("BackgroundPattern", false);
            beep.Checked = frontend.GetBool("ShouldPlaySoundOnTypingError", true);

            // OVIMGeneric's per-table defaults for cj and simplex
            cangjieCommitAtMaximum.Checked = cangjie.GetBool("ShouldCommitAtMaximumRadicalLength", false);
            cangjieComposeWhileTyping.Checked = cangjie.GetBool("ComposeWhileTyping", false);
            cangjieClearOnError.Checked = cangjie.GetBool("ClearReadingBufferAtCompositionError", true);
            cangjieDynamicFrequency.Checked = cangjie.GetBool("UseDynamicFrequency", true);
            simplexComposeWhileTyping.Checked = simplex.GetBool("ComposeWhileTyping", false);
            simplexClearOnError.Checked = simplex.GetBool("ClearReadingBufferAtCompositionError", true);
            LoadTables();
            loading = false;
        }

        private bool SaveSettings()
        {
            frontend.SetBool("ToggleInputMethodWithControlBackslash", controlBackslash.Checked);
            frontend.SetBool("EnableAssociatedPhrases", associatedPhrases.Checked);
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

            traditionalMandarin.SetString("KeyboardLayout", Selected(traditionalLayout));

            cangjie.SetBool("ShouldCommitAtMaximumRadicalLength", cangjieCommitAtMaximum.Checked);
            cangjie.SetBool("ComposeWhileTyping", cangjieComposeWhileTyping.Checked);
            cangjie.SetBool("ClearReadingBufferAtCompositionError", cangjieClearOnError.Checked);
            cangjie.SetBool("UseDynamicFrequency", cangjieDynamicFrequency.Checked);
            simplex.SetBool("ComposeWhileTyping", simplexComposeWhileTyping.Checked);
            simplex.SetBool("ClearReadingBufferAtCompositionError", simplexClearOnError.Checked);

            try
            {
                frontend.Save();
                smartMandarin.Save();
                traditionalMandarin.Save();
                cangjie.Save();
                simplex.Save();
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

        [STAThread]
        private static void Main()
        {
            bool created;
            using (Mutex single = new Mutex(true, "ChiaKey.Settings", out created))
            {
                // a second launch from the language bar brings the open window up instead
                if (!created)
                {
                    IntPtr existing = FindWindow(null, WindowTitle);
                    if (existing != IntPtr.Zero)
                        SetForegroundWindow(existing);
                    return;
                }
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                string data = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "ChiaKey");
                Application.Run(new SettingsForm(data));
            }
        }
    }
}
