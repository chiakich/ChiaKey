using System;
using System.Drawing;
using System.Windows.Forms;

namespace ChiaKey.Settings
{
    // Shared metrics for the Windows preference pages. Sections grow with their
    // contents; only the page scrolls when the available work area is too small.
    internal static class PreferenceLayout
    {
        internal static Panel Page(string title)
        {
            Panel page = new Panel { AutoScroll = true, Padding = new Padding(20, 16, 20, 16),
                Size = new Size(700, 600), BackColor = Color.White };
            page.SuspendLayout();
            TableLayoutPanel stack = new TableLayoutPanel { AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Top,
                ColumnCount = 1, Margin = Padding.Empty, Padding = Padding.Empty };
            stack.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            stack.SuspendLayout();
            page.Tag = stack;
            page.Controls.Add(stack);
            Label heading = new Label { Text = title, AutoSize = true,
                Font = new Font("Microsoft JhengHei UI", 11F, FontStyle.Bold),
                Margin = new Padding(0, 0, 0, 14) };
            Add(stack, heading);
            return page;
        }

        internal static GroupBox Section(Control page, string title)
        {
            GroupBox section = new GroupBox { Text = title, AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Fill,
                Padding = new Padding(14, 8, 14, 10), Margin = new Padding(0, 0, 0, 12) };
            section.SuspendLayout();
            TableLayoutPanel rows = new TableLayoutPanel { AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Top,
                ColumnCount = 1, Margin = Padding.Empty, Padding = Padding.Empty };
            rows.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            rows.SuspendLayout();
            section.Tag = rows;
            section.Controls.Add(rows);
            Add((TableLayoutPanel)page.Tag, section);
            return section;
        }

        private static void Add(TableLayoutPanel table, Control control)
        {
            int row = table.RowCount++;
            table.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            table.Controls.Add(control, 0, row);
        }

        internal static void Row(Control parent, params Control[] controls)
        {
            TableLayoutPanel row = new TableLayoutPanel { AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Top,
                ColumnCount = controls.Length, RowCount = 1,
                Margin = new Padding(0, 1, 0, 1), Padding = Padding.Empty };
            row.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            row.SuspendLayout();
            bool caption = controls.Length == 2 && controls[0] is Label;
            bool image = controls.Length == 2 && controls[0] is PictureBox;
            for (int i = 0; i < controls.Length; ++i)
            {
                row.ColumnStyles.Add(image && i == 0 ? new ColumnStyle(SizeType.AutoSize)
                    : new ColumnStyle(SizeType.Percent,
                        image ? 100 : caption ? (i == 0 ? 34 : 66) : 100F / controls.Length));
                Control control = controls[i];
                control.Margin = new Padding(0, 2, i + 1 == controls.Length ? 0 : 12, 2);
                control.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
                row.Controls.Add(control, i, 0);
            }
            // Constrain text before measuring its height: translated labels and
            // checkboxes must wrap instead of forcing a horizontal scrollbar.
            int previousWidth = -1;
            row.SizeChanged += delegate
            {
                if (previousWidth == row.ClientSize.Width || row.ClientSize.Width <= 0) return;
                previousWidth = row.ClientSize.Width;
                for (int i = 0; i < controls.Length; ++i)
                {
                    Control control = controls[i];
                    if (control is Label || control is CheckBox || control is RadioButton)
                    {
                        float fraction = caption ? (i == 0 ? 0.34F : 0.66F) : 1F / controls.Length;
                        int width = image ? row.ClientSize.Width - controls[0].Width - controls[0].Margin.Horizontal
                            : (int)(row.ClientSize.Width * fraction);
                        Size maximum = new Size(Math.Max(1,
                            width - control.Margin.Horizontal), 0);
                        if (control.MaximumSize != maximum) control.MaximumSize = maximum;
                    }
                }
            };
            Add((TableLayoutPanel)parent.Tag, row);
        }

        internal static void Finish(Control control)
        {
            foreach (Control child in control.Controls) Finish(child);
            control.ResumeLayout(false);
        }

        internal static Label Label(string text)
        {
            return new Label { Text = text, AutoSize = true };
        }

        internal static void Hint(Control parent, string text)
        {
            Label hint = Label(text);
            hint.ForeColor = SystemColors.GrayText;
            Row(parent, hint);
        }

        internal static TableLayoutPanel Inline(params Control[] controls)
        {
            TableLayoutPanel flow = new TableLayoutPanel { AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Fill,
                ColumnCount = controls.Length, RowCount = 1,
                Margin = Padding.Empty, Padding = Padding.Empty };
            flow.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            flow.SuspendLayout();
            for (int i = 0; i < controls.Length; ++i)
            {
                Control control = controls[i];
                flow.ColumnStyles.Add(i + 1 == controls.Length
                    ? new ColumnStyle(SizeType.Percent, 100)
                    : new ColumnStyle(SizeType.AutoSize));
                control.Margin = new Padding(0, 0, 10, 4);
                control.Anchor = AnchorStyles.Top | AnchorStyles.Left;
                flow.Controls.Add(control, i, 0);
            }
            return flow;
        }

        internal static Button Button(string text)
        {
            return new Button { Text = text, AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink, MinimumSize = new Size(96, 28),
                Padding = new Padding(8, 2, 8, 2), UseVisualStyleBackColor = true };
        }
    }
}
