# Regenerates the icon SVGs from jf open huninn (SIL OFL 1.1), e.g.
#   powershell -STA -File make-icons.ps1 -FontPath jf-openhuninn-2.1.ttf -OutDir . -PreviewPath preview.png
param([string] $FontPath, [string] $OutDir, [string] $PreviewPath)
Add-Type -AssemblyName PresentationCore, WindowsBase
$ErrorActionPreference = 'Stop'
$typeface = New-Object System.Windows.Media.GlyphTypeface (New-Object Uri $FontPath)
$inv = [Globalization.CultureInfo]::InvariantCulture
New-Item -ItemType Directory -Force $OutDir | Out-Null

# fills a box of the given size, keeping the glyph's own proportions
function GlyphGeometry([string] $char, [double] $box, [double] $margin) {
    $index = $typeface.CharacterToGlyphMap[[int][char]$char]
    $outline = $typeface.GetGlyphOutline($index, 100, 100)
    $bounds = $outline.Bounds
    $scale = ($box - 2 * $margin) / [Math]::Max($bounds.Width, $bounds.Height)
    $group = New-Object System.Windows.Media.TransformGroup
    $group.Children.Add((New-Object System.Windows.Media.TranslateTransform (-$bounds.X - $bounds.Width / 2), (-$bounds.Y - $bounds.Height / 2)))
    $group.Children.Add((New-Object System.Windows.Media.ScaleTransform $scale, $scale))
    $group.Children.Add((New-Object System.Windows.Media.TranslateTransform ($box / 2), ($box / 2)))
    # a group bakes its children's transforms into the figures; NonZero, since CJK
    # glyphs often overlap their own strokes
    $holder = New-Object System.Windows.Media.GeometryGroup
    $holder.FillRule = 'Nonzero'
    $outline = $outline.Clone()
    $outline.Transform = $group
    $holder.Children.Add($outline)
    return [System.Windows.Media.PathGeometry]::CreateFromGeometry($holder)
}

function PathData($geometry) {
    # WPF writes "F1 M..."; SVG takes the fill rule as an attribute instead
    $data = $geometry.ToString($inv) -replace '^F[01]\s*', ''
    # two decimals is far below a pixel at any icon size
    return [regex]::Replace($data, '-?\d+\.\d+', { param($m) [Math]::Round([double]::Parse($m.Value, $inv), 2).ToString($inv) })
}

$glyphs = [ordered]@{ qian = '千'; chinese = '中'; english = '英'; zhuyin = '注'; cangjie = '倉'; simplex = '簡'; full = '全'; half = '半' }
$geometries = [ordered]@{}
foreach ($name in $glyphs.Keys) {
    $g = GlyphGeometry $glyphs[$name] 32 1
    $geometries[$name] = $g
    $svg = "<svg xmlns=`"http://www.w3.org/2000/svg`" viewBox=`"0 0 32 32`">`n" +
           "  <path fill=`"currentColor`" d=`"$(PathData $g)`"/>`n</svg>`n"
    [IO.File]::WriteAllText((Join-Path $OutDir "$name.svg"), $svg, (New-Object Text.UTF8Encoding($false)))
}

# white glyph, not a cut-out: the input switcher has one icon for both themes
$badgeGlyph = GlyphGeometry '千' 32 6
$svg = "<svg xmlns=`"http://www.w3.org/2000/svg`" viewBox=`"0 0 32 32`">`n" +
       "  <rect width=`"32`" height=`"32`" rx=`"7`" fill=`"#1A1A1A`"/>`n" +
       "  <path fill=`"#FFFFFF`" d=`"$(PathData $badgeGlyph)`"/>`n</svg>`n"
[IO.File]::WriteAllText((Join-Path $OutDir 'badge.svg'), $svg, (New-Object Text.UTF8Encoding($false)))

# preview: every icon at 16/20/24/32 px on a light and a dark taskbar
$sizes = 16, 20, 24, 32
$cell = 40
$width = $geometries.Count * $cell + 70
$height = 2 * ($sizes.Count * $cell) + 10
$visual = New-Object System.Windows.Media.DrawingVisual
$dc = $visual.RenderOpen()
$themes = @(@{ bg = '#F3F3F3'; fg = '#1A1A1A'; top = 0 }, @{ bg = '#202020'; fg = '#FFFFFF'; top = $sizes.Count * $cell + 10 })
foreach ($theme in $themes) {
    $bg = (New-Object System.Windows.Media.BrushConverter).ConvertFromString($theme.bg)
    $fg = (New-Object System.Windows.Media.BrushConverter).ConvertFromString($theme.fg)
    $dc.DrawRectangle($bg, $null, (New-Object System.Windows.Rect 0, $theme.top, $width, ($sizes.Count * $cell)))
    $row = 0
    foreach ($size in $sizes) {
        $label = New-Object System.Windows.Media.FormattedText "$size", $inv, 'LeftToRight', (New-Object System.Windows.Media.Typeface 'Segoe UI'), 11, $fg
        $dc.DrawText($label, (New-Object System.Windows.Point 6, ($theme.top + $row * $cell + 14)))
        $col = 0
        foreach ($name in $geometries.Keys) {
            $g = $geometries[$name].Clone()
            $s = $size / 32.0
            $tg = New-Object System.Windows.Media.TransformGroup
            $tg.Children.Add((New-Object System.Windows.Media.ScaleTransform $s, $s))
            $tg.Children.Add((New-Object System.Windows.Media.TranslateTransform (60 + $col * $cell + ($cell - $size) / 2), ($theme.top + $row * $cell + ($cell - $size) / 2)))
            $g.Transform = $tg
            $dc.DrawGeometry($fg, $null, $g)
            $col++
        }
        $row++
    }
}
$dc.Close()
$bitmap = New-Object System.Windows.Media.Imaging.RenderTargetBitmap $width, $height, 96, 96, ([System.Windows.Media.PixelFormats]::Pbgra32)
$bitmap.Render($visual)
$encoder = New-Object System.Windows.Media.Imaging.PngBitmapEncoder
$encoder.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
$stream = [IO.File]::Create($PreviewPath); $encoder.Save($stream); $stream.Close()
"wrote: " + ((Get-ChildItem $OutDir -Filter *.svg).Name -join ', ')