using System;
using System.Collections;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

// Inspect the real controls without showing windows or saving user preferences.
// Separate processes prevent static choice lists from retaining another locale.
internal static class PreferenceLayoutTests
{
    private const BindingFlags Fields = BindingFlags.Instance | BindingFlags.NonPublic;

    private static int Check(Control parent)
    {
        int count = 0;
        foreach (Control child in parent.Controls)
        {
            if (child.Text.Contains("狀態列") || child.Text.Contains("状态列") ||
                child.Text.ToLowerInvariant().Contains("status bar"))
                throw new Exception("Floating status option remains");
            ScrollableControl scroll = parent as ScrollableControl;
            if (child.Right > parent.ClientSize.Width + 2 && (scroll == null || !scroll.AutoScroll))
                throw new Exception("Overflow: " + child.GetType().Name + " " + child.Text +
                    " right=" + child.Right + " parent=" + parent.GetType().Name +
                    " width=" + parent.ClientSize.Width);
            count += Check(child);
            if (child is CheckBox || child is Label)
            {
                if (child.Height + 2 < child.GetPreferredSize(new Size(child.Width, 0)).Height)
                    throw new Exception("Clipped text: " + child.Text);
                count++;
            }
        }
        return count;
    }

    [STAThread]
    private static int Main(string[] args)
    {
        string temporary = Path.GetFullPath(Path.GetTempPath()).TrimEnd(Path.DirectorySeparatorChar);
        string root = Path.GetFullPath(Path.Combine(temporary, "ChiaKeyLayout-" + Guid.NewGuid().ToString("N")));
        try
        {
            Directory.CreateDirectory(root);
            Application.EnableVisualStyles();
            Assembly assembly = Assembly.LoadFrom(args[0]);
            assembly.GetType("ChiaKey.Settings.Ui").GetProperty("Language",
                BindingFlags.Static | BindingFlags.NonPublic).SetValue(null, args[1], null);
            Type type = assembly.GetType("ChiaKey.Settings.SettingsForm");
            Type tabType = assembly.GetType("ChiaKey.Settings.ToolbarItem");
            using (Control tab = (Control)Activator.CreateInstance(tabType, new object[] { "Test", null }))
            {
                int clicks = 0;
                tab.Click += delegate { clicks++; };
                MethodInfo keyDown = tabType.GetMethod("OnKeyDown", Fields);
                MethodInfo inputKey = tabType.GetMethod("IsInputKey", Fields);
                if (!(bool)inputKey.Invoke(tab, new object[] { Keys.Enter }))
                    throw new Exception("Toolbar Enter must not activate the form's default OK button");
                foreach (Keys key in new[] { Keys.Control | Keys.Space, Keys.Shift | Keys.Space })
                    keyDown.Invoke(tab, new object[] { new KeyEventArgs(key) });
                if (clicks != 0) throw new Exception("Toolbar must not consume modified Space");
                foreach (Keys key in new[] { Keys.Space, Keys.Enter })
                {
                    KeyEventArgs activation = new KeyEventArgs(key);
                    keyDown.Invoke(tab, new object[] { activation });
                    if (!activation.SuppressKeyPress) throw new Exception("Toolbar activation must suppress the character");
                }
                if (clicks != 2) throw new Exception("Toolbar keyboard activation failed");
            }
            var watch = System.Diagnostics.Stopwatch.StartNew();
            using (Form form = (Form)Activator.CreateInstance(type, new object[] { root, 0 }))
            {
                Console.WriteLine("Constructor ms: " + watch.ElapsedMilliseconds);
                foreach (string field in new[] { "tableMatchOne", "tableMatchMany" })
                {
                    TextBox wildcard = (TextBox)type.GetField(field, Fields).GetValue(form);
                    if (wildcard.MaxLength != 1 || wildcard.TextAlign != HorizontalAlignment.Center)
                        throw new Exception("Wildcard input must retain its single-character constraint");
                }
                IList panes = (IList)type.GetField("panes", Fields).GetValue(form);
                int count = 0;
                for (int i = 0; i < panes.Count; i++)
                {
                    // Updates have asynchronous network behavior, tested separately.
                    if (i == 7) continue;
                    Control page = (Control)panes[i];
                    // Detaching must retain the production font, not the default
                    // ambient font of a standalone WinForms control.
                    Font bodyFont = form.Font;
                    page.Font = bodyFont;
                    Label heading = (Label)((TableLayoutPanel)page.Tag).Controls[0];
                    Font headingFont = heading.Font;
                    page.Parent.Controls.Remove(page);
                    page.Visible = true;
                    float previousScale = 1F;
                    foreach (float scale in new[] { 1F, 1.5F, 2F })
                    {
                        page.Scale(new SizeF(scale / previousScale, scale / previousScale));
                        previousScale = scale;
                        page.Font = new Font(bodyFont.FontFamily, bodyFont.Size * scale, bodyFont.Style);
                        heading.Font = new Font(headingFont.FontFamily, headingFont.Size * scale, headingFont.Style);
                        foreach (int width in new[] { 600, 720, 900 })
                        {
                            page.Size = new Size((int)(width * scale), (int)(430 * scale));
                            // Resizing cascades through the nested table layout panels.
                            // Do not invalidate every descendant's measurement cache.
                            page.PerformLayout();
                            count += Check(page);
                            if (((Panel)page).HorizontalScroll.Visible)
                                throw new Exception("Horizontal scrollbar on pane " + i + " " + width + " " + scale);
                        }
                    }
                    page.Dispose();
                }
                Console.WriteLine("PASS " + args[1] + ": eight pages, 100/150/200% scale, " +
                    "600/720/900px widths; " + count + " text checks");
            }
            return 0;
        }
        catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        finally
        {
            if (Directory.Exists(root) && string.Equals(Path.GetDirectoryName(root), temporary,
                    StringComparison.OrdinalIgnoreCase) && Path.GetFileName(root).StartsWith("ChiaKeyLayout-"))
                Directory.Delete(root, true);
        }
    }
}
