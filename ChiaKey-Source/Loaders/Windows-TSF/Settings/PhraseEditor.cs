// after Yahoo! KeyKey's PhraseEditor (Utilities/PhraseEditor/Windows); the data is
// ChiaKey::UserPhraseStore, reached through ChiaKeyTsf.dll

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Text;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace ChiaKey.Settings
{
    internal static class PhraseStore
    {
        private const string Library = "ChiaKeyTsf.dll";

        [UnmanagedFunctionPointer(CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        public delegate void PhraseCallback(long rowid, string phrase, string reading);

        [UnmanagedFunctionPointer(CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        public delegate void StringCallback(string text);

        [DllImport(Library, CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr ChiaKeyPhrasesOpen();
        [DllImport(Library, CallingConvention = CallingConvention.StdCall)]
        public static extern void ChiaKeyPhrasesClose(IntPtr store);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall)]
        public static extern void ChiaKeyPhrasesBeginSession(IntPtr store);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall)]
        public static extern void ChiaKeyPhrasesRefreshSession(IntPtr store);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall)]
        public static extern void ChiaKeyPhrasesEndSession(IntPtr store);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        public static extern int ChiaKeyPhrasesCount(IntPtr store, string filter);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        public static extern int ChiaKeyPhrasesList(IntPtr store, string filter, int order, int ascending,
                                                    int offset, int limit, PhraseCallback callback);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        public static extern int ChiaKeyPhrasesContains(IntPtr store, string phrase);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        public static extern long ChiaKeyPhrasesAdd(IntPtr store, string phrase, string reading);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        public static extern int ChiaKeyPhrasesSetPhrase(IntPtr store, long rowid, string phrase);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        public static extern int ChiaKeyPhrasesSetPhraseAndReading(IntPtr store, long rowid, string phrase, string reading);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        public static extern int ChiaKeyPhrasesSetReading(IntPtr store, long rowid, string reading);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall)]
        public static extern int ChiaKeyPhrasesRemove(IntPtr store, long[] rowids, int count);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        public static extern int ChiaKeyPhrasesReadings(IntPtr store, string character,
                                                        StringCallback callback);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        public static extern int ChiaKeyPhrasesExport(IntPtr store, string path);
        [DllImport(Library, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        public static extern int ChiaKeyPhrasesImport(IntPtr store, string path);

        public static List<string> Readings(IntPtr store, string character)
        {
            List<string> result = new List<string>();
            StringCallback callback = delegate(string text) { result.Add(text); };
            ChiaKeyPhrasesReadings(store, character, callback);
            GC.KeepAlive(callback);
            return result;
        }

        // UTF-16 to characters, so a supplementary-plane character stays whole
        public static List<string> Characters(string phrase)
        {
            List<string> result = new List<string>();
            for (int index = 0; index < phrase.Length; ++index)
            {
                if (char.IsHighSurrogate(phrase[index]) && index + 1 < phrase.Length &&
                    char.IsLowSurrogate(phrase[index + 1]))
                {
                    result.Add(phrase.Substring(index, 2));
                    ++index;
                }
                else
                {
                    result.Add(phrase.Substring(index, 1));
                }
            }
            return result;
        }
    }

    internal sealed class PhraseRow
    {
        public long Rowid;
        public string Phrase;
        public string Reading;
    }

    // one line of text, as the original's add and edit dialogs
    internal sealed class TextDialog : Form
    {
        private readonly TextBox box = new TextBox();

        public TextDialog(string title, string prompt, string text)
        {
            SuspendLayout();
            AutoScaleDimensions = new SizeF(96F, 96F);
            AutoScaleMode = AutoScaleMode.Dpi;
            Text = title;
            Font = new Font("Microsoft JhengHei UI", 9F);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            ShowInTaskbar = false;
            StartPosition = FormStartPosition.CenterParent;
            ClientSize = new Size(340, 110);

            Label label = new Label();
            label.Text = prompt;
            label.AutoSize = true;
            label.Location = new Point(12, 14);
            Controls.Add(label);
            box.Bounds = new Rectangle(12, 38, 316, 24);
            box.Text = text;
            Controls.Add(box);

            Button ok = new Button();
            ok.Text = Ui.Text("確定(&O)");
            ok.DialogResult = DialogResult.OK;
            ok.Bounds = new Rectangle(160, 74, 82, 26);
            Controls.Add(ok);
            Button cancel = new Button();
            cancel.Text = Ui.Text("取消(&C)");
            cancel.DialogResult = DialogResult.Cancel;
            cancel.Bounds = new Rectangle(246, 74, 82, 26);
            Controls.Add(cancel);
            AcceptButton = ok;
            CancelButton = cancel;
            ResumeLayout(false);
        }

        public string Value
        {
            get { return box.Text.Trim(); }
        }
    }

    // ReadingForm: a syllable for every character, from what the lexicon knows of it
    internal sealed class ReadingDialog : Form
    {
        private readonly List<ComboBox> syllables = new List<ComboBox>();

        public ReadingDialog(IntPtr store, string phrase, string reading)
        {
            SuspendLayout();
            AutoScaleDimensions = new SizeF(96F, 96F);
            AutoScaleMode = AutoScaleMode.Dpi;
            Text = Ui.Text("編輯注音");
            Font = new Font("Microsoft JhengHei UI", 9F);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            ShowInTaskbar = false;
            StartPosition = FormStartPosition.CenterParent;

            List<string> characters = PhraseStore.Characters(phrase);
            string[] current = (reading ?? "").Split(',');
            FlowLayoutPanel row = new FlowLayoutPanel();
            row.Location = new Point(12, 12);
            row.AutoSize = true;
            row.MaximumSize = new Size(520, 0);
            row.WrapContents = true;
            for (int index = 0; index < characters.Count; ++index)
            {
                Panel cell = new Panel();
                cell.Size = new Size(86, 64);
                Label character = new Label();
                character.Text = characters[index];
                character.Font = new Font("Microsoft JhengHei UI", 14F);
                character.TextAlign = ContentAlignment.MiddleCenter;
                character.Bounds = new Rectangle(0, 0, 80, 30);
                cell.Controls.Add(character);
                ComboBox box = new ComboBox();
                box.DropDownStyle = ComboBoxStyle.DropDownList;
                box.Bounds = new Rectangle(0, 34, 80, 24);
                foreach (string syllable in PhraseStore.Readings(store, characters[index]))
                    box.Items.Add(syllable);
                string mine = index < current.Length ? current[index] : "";
                if (mine.Length > 0 && !box.Items.Contains(mine))
                    box.Items.Insert(0, mine);
                box.SelectedIndex = mine.Length > 0 ? box.Items.IndexOf(mine) : (box.Items.Count > 0 ? 0 : -1);
                cell.Controls.Add(box);
                row.Controls.Add(cell);
                syllables.Add(box);
            }
            Controls.Add(row);
            Size preferred = row.GetPreferredSize(new Size(520, 0));
            int width = Math.Max(250, preferred.Width + 24);
            int buttonTop = preferred.Height + 24;

            Button ok = new Button();
            ok.Text = Ui.Text("確定(&O)");
            ok.DialogResult = DialogResult.OK;
            ok.Enabled = syllables.Count > 0 && syllables.TrueForAll(box => box.SelectedIndex >= 0);
            foreach (ComboBox box in syllables)
                box.SelectedIndexChanged += delegate {
                    ok.Enabled = syllables.TrueForAll(item => item.SelectedIndex >= 0);
                };
            ok.Bounds = new Rectangle(width - 184, buttonTop, 82, 26);
            Controls.Add(ok);
            Button cancel = new Button();
            cancel.Text = Ui.Text("取消(&C)");
            cancel.DialogResult = DialogResult.Cancel;
            cancel.Bounds = new Rectangle(width - 96, buttonTop, 82, 26);
            Controls.Add(cancel);
            AcceptButton = ok;
            CancelButton = cancel;
            ClientSize = new Size(width, buttonTop + 40);
            ResumeLayout(false);
        }

        public string Reading
        {
            get
            {
                List<string> parts = new List<string>();
                foreach (ComboBox box in syllables)
                    parts.Add((string)box.SelectedItem);
                return string.Join(",", parts.ToArray());
            }
        }
    }

    internal sealed class PhraseEditorForm : Form
    {
        public const string WindowTitle = "千秋輸入法詞彙編輯程式";
        private const int PageSize = 200;
        // the engine takes a lock untouched for 30 minutes for a crashed editor's
        private const int SessionRefreshMilliseconds = 60 * 1000;

        private readonly IntPtr store;
        private readonly DataGridView grid = new DataGridView();
        private readonly ToolStripTextBox search = new ToolStripTextBox();
        private TextBoxBase clipboardTarget;
        private readonly ToolStripStatusLabel status = new ToolStripStatusLabel();
        private readonly Timer sessionTimer = new Timer();
        private readonly Timer searchTimer = new Timer();
        private readonly Dictionary<int, List<PhraseRow>> pages = new Dictionary<int, List<PhraseRow>>();
        private int order;
        // newest first, so a phrase just added or learned is at the top
        private bool ascending = false;
        private string filter = "";

        public static Icon LoadIcon(string name)
        {
            using (Stream stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(name))
            {
                if (stream == null) return null;
                using (Icon icon = new Icon(stream))
                    return (Icon)icon.Clone();
            }
        }

        // Decode the largest embedded image directly: .NET Framework Icon can
        // fall back to a small DIB even when the ICO includes a 256px PNG.
        public static Bitmap LoadIconImage(string name)
        {
            using (Stream stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(name))
            {
                if (stream == null) return null;
                using (BinaryReader reader = new BinaryReader(stream))
                {
                    reader.ReadUInt16();
                    reader.ReadUInt16();
                    int count = reader.ReadUInt16();
                    int largest = 0, length = 0, offset = 0;
                    for (int i = 0; i < count; i++)
                    {
                        int width = reader.ReadByte();
                        reader.ReadByte(); // height; embedded icon images are square
                        reader.ReadUInt16();
                        reader.ReadUInt16();
                        reader.ReadUInt16();
                        int bytes = reader.ReadInt32();
                        int start = reader.ReadInt32();
                        int dimension = width == 0 ? 256 : width;
                        if (dimension > largest)
                        {
                            largest = dimension; length = bytes; offset = start;
                        }
                    }
                    if (largest == 0 || length <= 0 || offset < 0 ||
                        offset > stream.Length || length > stream.Length - offset) return null;
                    stream.Position = offset;
                    byte[] imageBytes = reader.ReadBytes(length);
                    if (imageBytes.Length >= 8 && imageBytes[0] == 137 &&
                        imageBytes[1] == 80 && imageBytes[2] == 78 && imageBytes[3] == 71)
                    {
                        using (MemoryStream png = new MemoryStream(imageBytes))
                        using (Image image = Image.FromStream(png)) return new Bitmap(image);
                    }
                    stream.Position = 0;
                    using (Icon icon = new Icon(stream, largest, largest)) return icon.ToBitmap();
                }
            }
        }

        public PhraseEditorForm()
        {
            try
            {
                store = PhraseStore.ChiaKeyPhrasesOpen();
            }
            catch (Exception)
            {
                store = IntPtr.Zero;
            }

            SuspendLayout();
            AutoScaleDimensions = new SizeF(96F, 96F);
            AutoScaleMode = AutoScaleMode.Dpi;
            Text = Ui.Text(WindowTitle);
            Font = new Font("Microsoft JhengHei UI", 9F);
            Icon = LoadIcon("phrase-editor.ico");
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(480, 520);
            MinimumSize = new Size(360, 300);

            grid.Dock = DockStyle.Fill;
            grid.VirtualMode = true;
            grid.ReadOnly = true;
            grid.AllowUserToAddRows = false;
            grid.AllowUserToDeleteRows = false;
            grid.AllowUserToResizeRows = false;
            grid.RowHeadersVisible = false;
            grid.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
            grid.MultiSelect = true;
            grid.BackgroundColor = SystemColors.Window;
            grid.BorderStyle = BorderStyle.None;
            grid.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
            // the fixed default header height does not follow the DPI
            grid.ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            grid.Columns.Add(Column(Ui.Text("詞彙"), 1));
            grid.Columns.Add(Column(Ui.Text("注音"), 2));
            grid.CellValueNeeded += GridCellValueNeeded;
            grid.ColumnHeaderMouseClick += GridColumnHeaderMouseClick;
            grid.CellDoubleClick += delegate(object sender, DataGridViewCellEventArgs e)
            {
                if (e.RowIndex >= 0)
                    EditPhrase();
            };
            grid.KeyDown += delegate(object sender, KeyEventArgs e)
            {
                if (e.KeyCode == Keys.Delete)
                {
                    RemoveSelected();
                    e.Handled = true;
                }
            };
            Controls.Add(grid);

            ToolStrip tools = new ToolStrip();
            tools.GripStyle = ToolStripGripStyle.Hidden;
            tools.Items.Add(Tool(Ui.Text("加入新詞"), "add.ico", delegate { AddPhrase(); }));
            tools.Items.Add(Tool(Ui.Text("移除詞彙"), "remove.ico", delegate { RemoveSelected(); }));
            tools.Items.Add(Tool(Ui.Text("編輯詞彙"), "editPhrase.ico", delegate { EditPhrase(); }));
            tools.Items.Add(Tool(Ui.Text("編輯注音"), "editReading.ico", delegate { EditReading(); }));
            search.Alignment = ToolStripItemAlignment.Right;
            search.Width = 110;
            search.BorderStyle = BorderStyle.FixedSingle;
            search.ToolTipText = Ui.Text("以詞彙或注音搜尋");
            search.TextChanged += delegate
            {
                searchTimer.Stop();
                searchTimer.Start();
            };
            tools.Items.Add(search);
            ToolStripLabel searchLabel = new ToolStripLabel(Ui.Text("搜尋："));
            searchLabel.Alignment = ToolStripItemAlignment.Right;
            tools.Items.Add(searchLabel);
            Controls.Add(tools);

            MenuStrip menu = new MenuStrip();
            ToolStripMenuItem file = new ToolStripMenuItem(Ui.Text("檔案(&F)"));
            file.DropDownItems.Add(MenuItem(Ui.Text("加入新詞(&N)"), Keys.Control | Keys.N, delegate { AddPhrase(); }));
            file.DropDownItems.Add(new ToolStripSeparator());
            file.DropDownItems.Add(MenuItem(Ui.Text("匯入(&I)…"), Keys.None, delegate { Import(); }));
            file.DropDownItems.Add(MenuItem(Ui.Text("匯出(&E)…"), Keys.None, delegate { Export(); }));
            file.DropDownItems.Add(new ToolStripSeparator());
            file.DropDownItems.Add(MenuItem(Ui.Text("關閉(&X)"), Keys.None, delegate { Close(); }));
            ToolStripMenuItem edit = new ToolStripMenuItem(Ui.Text("編輯(&E)"));
            // Menu activation temporarily takes focus from the hosted textbox.
            search.TextBox.Enter += delegate { clipboardTarget = search.TextBox; };
            grid.Enter += delegate { clipboardTarget = null; };
            ToolStripMenuItem cut = MenuItem(Ui.Text("剪下(&T)"), Keys.Control | Keys.X, delegate {
                TextBoxBase text = FocusedTextBox(); if (text != null) text.Cut(); });
            ToolStripMenuItem copy = MenuItem(Ui.Text("複製(&C)"), Keys.Control | Keys.C, delegate { CopySelection(); });
            ToolStripMenuItem paste = MenuItem(Ui.Text("貼上(&P)"), Keys.Control | Keys.V, delegate {
                TextBoxBase text = FocusedTextBox(); if (text != null) text.Paste(); });
            edit.DropDownItems.AddRange(new ToolStripItem[] { cut, copy, paste, new ToolStripSeparator() });
            edit.DropDownOpening += delegate
            {
                TextBoxBase text = FocusedTextBox();
                cut.Enabled = text != null && !text.ReadOnly && text.SelectionLength > 0;
                paste.Enabled = text != null && !text.ReadOnly;
                copy.Enabled = text != null ? text.SelectionLength > 0 : grid.SelectedRows.Count > 0;
            };
            edit.DropDownItems.Add(MenuItem(Ui.Text("刪除(&D)"), Keys.None, delegate {
                TextBoxBase text = FocusedTextBox();
                if (text != null) text.SelectedText = ""; else RemoveSelected(); }));
            edit.DropDownItems.Add(new ToolStripSeparator());
            edit.DropDownItems.Add(MenuItem(Ui.Text("編輯詞彙(&E)"), Keys.F2, delegate { EditPhrase(); }));
            edit.DropDownItems.Add(MenuItem(Ui.Text("編輯注音(&R)"), Keys.Control | Keys.R, delegate { EditReading(); }));
            menu.Items.Add(file);
            menu.Items.Add(edit);
            ToolStripMenuItem help = new ToolStripMenuItem(Ui.Text("輔助說明(&H)"));
            help.DropDownItems.Add(MenuItem(Ui.Text("線上說明文件(&H)"), Keys.None, delegate {
                System.Diagnostics.Process.Start("https://github.com/chiakich/ChiaKey/blob/windows-tsf/Docs/WindowsImplementation.md"); }));
            help.DropDownItems.Add(MenuItem(Ui.Text("關於(&A)"), Keys.None, delegate {
                MessageBox.Show(this, Ui.Text("千秋輸入法詞彙編輯器\n版本 ") + UpdateService.Default().AppReleaseVersion +
                    Ui.Text("\n\n源自 Yahoo! KeyKey 詞彙編輯程式。"), Ui.Text("關於"), MessageBoxButtons.OK, MessageBoxIcon.Information); }));
            menu.Items.Add(help);
            Controls.Add(menu);
            MainMenuStrip = menu;

            StatusStrip bar = new StatusStrip();
            bar.Items.Add(status);
            Controls.Add(bar);
            ResumeLayout(false);
            PerformLayout();

            searchTimer.Interval = 300;
            searchTimer.Tick += delegate
            {
                searchTimer.Stop();
                filter = search.Text.Trim();
                Reload();
            };
            sessionTimer.Interval = SessionRefreshMilliseconds;
            sessionTimer.Tick += delegate { PhraseStore.ChiaKeyPhrasesRefreshSession(store); };

            if (store != IntPtr.Zero)
            {
                PhraseStore.ChiaKeyPhrasesBeginSession(store);
                sessionTimer.Start();
            }
            Reload();
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            if (store == IntPtr.Zero)
            {
                MessageBox.Show(this, Ui.Text("無法開啟使用者詞庫。"), Text, MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
                Close();
            }
        }

        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            sessionTimer.Stop();
            if (store != IntPtr.Zero)
            {
                // the engine resumes learning as soon as the lock is gone
                PhraseStore.ChiaKeyPhrasesEndSession(store);
                PhraseStore.ChiaKeyPhrasesClose(store);
            }
            base.OnFormClosed(e);
        }

        private static DataGridViewTextBoxColumn Column(string header, int order)
        {
            DataGridViewTextBoxColumn column = new DataGridViewTextBoxColumn();
            column.HeaderText = header;
            column.Tag = order;
            column.SortMode = DataGridViewColumnSortMode.Programmatic;
            return column;
        }

        private static ToolStripButton Tool(string text, string icon, EventHandler click)
        {
            ToolStripButton button = new ToolStripButton(text);
            Icon image = LoadIcon(icon);
            if (image != null)
                button.Image = image.ToBitmap();
            button.DisplayStyle = ToolStripItemDisplayStyle.ImageAndText;
            button.Click += click;
            return button;
        }

        private static ToolStripMenuItem MenuItem(string text, Keys keys, EventHandler click)
        {
            ToolStripMenuItem item = new ToolStripMenuItem(text);
            if (keys != Keys.None)
                item.ShortcutKeys = keys;
            item.Click += click;
            return item;
        }

        private void Reload()
        {
            pages.Clear();
            int count = store != IntPtr.Zero ? PhraseStore.ChiaKeyPhrasesCount(store, filter) : 0;
            grid.RowCount = count;
            grid.Invalidate();
            status.Text = filter.Length > 0 ? Ui.Text("找到 ") + count + Ui.Text(" 個詞彙") : Ui.Text("共 ") + count + Ui.Text(" 個詞彙");
        }

        private PhraseRow RowAt(int index)
        {
            int page = index / PageSize;
            List<PhraseRow> rows;
            if (!pages.TryGetValue(page, out rows))
            {
                rows = new List<PhraseRow>();
                List<PhraseRow> collected = rows;
                PhraseStore.PhraseCallback callback = delegate(long rowid, string phrase, string reading)
                {
                    PhraseRow row = new PhraseRow();
                    row.Rowid = rowid;
                    row.Phrase = phrase;
                    row.Reading = reading;
                    collected.Add(row);
                };
                PhraseStore.ChiaKeyPhrasesList(store, filter, order, ascending ? 1 : 0, page * PageSize,
                                               PageSize, callback);
                GC.KeepAlive(callback);
                pages[page] = rows;
            }
            int offset = index - page * PageSize;
            return offset < rows.Count ? rows[offset] : null;
        }

        private void GridCellValueNeeded(object sender, DataGridViewCellValueEventArgs e)
        {
            PhraseRow row = RowAt(e.RowIndex);
            if (row != null)
                e.Value = e.ColumnIndex == 0 ? row.Phrase : row.Reading;
        }

        private void GridColumnHeaderMouseClick(object sender, DataGridViewCellMouseEventArgs e)
        {
            DataGridViewColumn column = grid.Columns[e.ColumnIndex];
            int wanted = (int)column.Tag;
            ascending = order == wanted ? !ascending : wanted != 0;
            order = wanted;
            foreach (DataGridViewColumn other in grid.Columns)
                other.HeaderCell.SortGlyphDirection = SortOrder.None;
            column.HeaderCell.SortGlyphDirection = ascending ? SortOrder.Ascending : SortOrder.Descending;
            Reload();
        }

        private PhraseRow CurrentRow()
        {
            return grid.CurrentCell != null ? RowAt(grid.CurrentCell.RowIndex) : null;
        }

        private void AddPhrase()
        {
            string phrase;
            using (TextDialog dialog = new TextDialog(Ui.Text("加入新詞"), Ui.Text("請輸入要加入的詞彙："), ""))
            {
                if (dialog.ShowDialog(this) != DialogResult.OK || dialog.Value.Length == 0)
                    return;
                phrase = dialog.Value;
            }
            if (PhraseStore.ChiaKeyPhrasesContains(store, phrase) != 0 &&
                MessageBox.Show(this, "「" + phrase + Ui.Text("」已經在詞庫裡了，仍要再加一個不同讀音的嗎？"), Text,
                                MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes)
                return;
            // a character with several readings is the user's to pick
            using (ReadingDialog dialog = new ReadingDialog(store, phrase, ""))
            {
                if (dialog.ShowDialog(this) != DialogResult.OK)
                    return;
                if (PhraseStore.ChiaKeyPhrasesAdd(store, phrase, dialog.Reading) == 0)
                {
                    MessageBox.Show(this, Ui.Text("無法加入「") + phrase + "」。", Text, MessageBoxButtons.OK,
                                    MessageBoxIcon.Error);
                    return;
                }
            }
            Reload();
        }

        private void EditPhrase()
        {
            PhraseRow row = CurrentRow();
            if (row == null)
                return;
            string phrase;
            using (TextDialog dialog = new TextDialog(Ui.Text("編輯詞彙"), Ui.Text("請輸入新的詞彙："), row.Phrase))
            {
                if (dialog.ShowDialog(this) != DialogResult.OK || dialog.Value.Length == 0 ||
                    dialog.Value == row.Phrase)
                    return;
                phrase = dialog.Value;
            }
            // Choose the reading before writing, so Cancel preserves the old
            // phrase. One native update saves both fields or neither of them.
            using (ReadingDialog dialog = new ReadingDialog(store, phrase, ""))
            {
                if (dialog.ShowDialog(this) != DialogResult.OK)
                    return;
                if (PhraseStore.ChiaKeyPhrasesSetPhraseAndReading(store, row.Rowid, phrase, dialog.Reading) == 0)
                {
                    MessageBox.Show(this, Ui.Text("無法更新「") + phrase + Ui.Text("」與讀音。"), Text, MessageBoxButtons.OK,
                                    MessageBoxIcon.Error);
                    return;
                }
            }
            Reload();
        }

        private void EditReading()
        {
            PhraseRow row = CurrentRow();
            if (row == null)
                return;
            using (ReadingDialog dialog = new ReadingDialog(store, row.Phrase, row.Reading))
            {
                if (dialog.ShowDialog(this) != DialogResult.OK || dialog.Reading == row.Reading)
                    return;
                if (PhraseStore.ChiaKeyPhrasesSetReading(store, row.Rowid, dialog.Reading) == 0)
                {
                    MessageBox.Show(this, Ui.Text("無法更新「") + row.Phrase + Ui.Text("」的讀音。"), Text, MessageBoxButtons.OK,
                                    MessageBoxIcon.Error);
                    return;
                }
            }
            Reload();
        }

        private TextBoxBase FocusedTextBox()
        {
            if (search.TextBox.Focused) return search.TextBox;
            return (ActiveControl as TextBoxBase) ?? clipboardTarget;
        }

        private void CopySelection()
        {
            TextBoxBase text = FocusedTextBox();
            if (text != null) { text.Copy(); return; }
            StringBuilder result = new StringBuilder();
            foreach (DataGridViewRow selected in grid.SelectedRows)
            {
                PhraseRow row = RowAt(selected.Index);
                if (row != null) result.Append(row.Phrase).Append(' ').Append(row.Reading).Append("\r\n");
            }
            if (result.Length > 0) Clipboard.SetText(result.ToString());
        }

        private void RemoveSelected()
        {
            List<long> rowids = new List<long>();
            foreach (DataGridViewRow selected in grid.SelectedRows)
            {
                PhraseRow row = RowAt(selected.Index);
                if (row != null)
                    rowids.Add(row.Rowid);
            }
            if (rowids.Count == 0)
                return;
            string question = rowids.Count == 1
                ? Ui.Text("確定要移除「") + RowAt(grid.SelectedRows[0].Index).Phrase + Ui.Text("」嗎？")
                : Ui.Text("確定要移除選取的 ") + rowids.Count + Ui.Text(" 個詞彙嗎？");
            if (MessageBox.Show(this, question, Text, MessageBoxButtons.YesNo, MessageBoxIcon.Question) !=
                DialogResult.Yes)
                return;
            PhraseStore.ChiaKeyPhrasesRemove(store, rowids.ToArray(), rowids.Count);
            grid.ClearSelection();
            Reload();
        }

        private const string FileFilter = "詞彙檔 (*.txt)|*.txt|所有檔案 (*.*)|*.*";

        private void Import()
        {
            using (OpenFileDialog dialog = new OpenFileDialog())
            {
                dialog.Filter = Ui.Text(FileFilter);
                dialog.Title = Ui.Text("匯入詞彙");
                if (dialog.ShowDialog(this) != DialogResult.OK)
                    return;
                int result = PhraseStore.ChiaKeyPhrasesImport(store, dialog.FileName);
                Reload();
                if (result == 1)
                    MessageBox.Show(this, Ui.Text("詞彙已經成功匯入。"), Ui.Text("完成"), MessageBoxButtons.OK,
                                    MessageBoxIcon.Information);
                else if (result == 2)
                    MessageBox.Show(this, Ui.Text("詞彙已經匯入，但檔案裡的自動學習資料無法還原。"), Ui.Text("完成"),
                                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                else
                    MessageBox.Show(this, Ui.Text("詞彙匯入失敗。"), Ui.Text("錯誤"), MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void Export()
        {
            using (SaveFileDialog dialog = new SaveFileDialog())
            {
                dialog.Filter = Ui.Text(FileFilter);
                dialog.Title = Ui.Text("匯出詞彙");
                dialog.FileName = "ChiaKeyPhrases.txt";
                if (dialog.ShowDialog(this) != DialogResult.OK)
                    return;
                if (PhraseStore.ChiaKeyPhrasesExport(store, dialog.FileName) != 0)
                    MessageBox.Show(this, Ui.Text("詞彙已經成功匯出。"), Ui.Text("完成"), MessageBoxButtons.OK,
                                    MessageBoxIcon.Information);
                else
                    MessageBox.Show(this, Ui.Text("詞彙匯出失敗。"), Ui.Text("錯誤"), MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
