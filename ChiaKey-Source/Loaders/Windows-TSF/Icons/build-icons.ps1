# Rasterizes the SVGs here into the .ico files the TIP embeds; run by CMake.
param(
    [Parameter(Mandatory = $true)] [string] $SvgDir,
    [Parameter(Mandatory = $true)] [string] $OutDir
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName PresentationCore, WindowsBase
$invariant = [Globalization.CultureInfo]::InvariantCulture

# glyph color per taskbar theme; the badge carries its own colors
$onLight = '#1A1A1A'
$onDark = '#FFFFFF'
$modeSizes = 16, 20, 24, 32, 40, 48
$badgeSizes = 16, 20, 24, 32, 40, 48, 64, 256
$targets = @(
    @{ Name = 'badge'; Svg = 'badge'; Color = $onLight; Sizes = $badgeSizes },
    @{ Name = 'chinese-on-light'; Svg = 'chinese'; Color = $onLight; Sizes = $modeSizes },
    @{ Name = 'chinese-on-dark'; Svg = 'chinese'; Color = $onDark; Sizes = $modeSizes },
    @{ Name = 'english-on-light'; Svg = 'english'; Color = $onLight; Sizes = $modeSizes },
    @{ Name = 'english-on-dark'; Svg = 'english'; Color = $onDark; Sizes = $modeSizes },
    @{ Name = 'full-on-light'; Svg = 'full'; Color = $onLight; Sizes = $modeSizes },
    @{ Name = 'full-on-dark'; Svg = 'full'; Color = $onDark; Sizes = $modeSizes },
    @{ Name = 'half-on-light'; Svg = 'half'; Color = $onLight; Sizes = $modeSizes },
    @{ Name = 'half-on-dark'; Svg = 'half'; Color = $onDark; Sizes = $modeSizes },
    @{ Name = 'zhuyin-on-light'; Svg = 'zhuyin'; Color = $onLight; Sizes = $modeSizes },
    @{ Name = 'zhuyin-on-dark'; Svg = 'zhuyin'; Color = $onDark; Sizes = $modeSizes },
    @{ Name = 'cangjie-on-light'; Svg = 'cangjie'; Color = $onLight; Sizes = $modeSizes },
    @{ Name = 'cangjie-on-dark'; Svg = 'cangjie'; Color = $onDark; Sizes = $modeSizes },
    @{ Name = 'simplex-on-light'; Svg = 'simplex'; Color = $onLight; Sizes = $modeSizes },
    @{ Name = 'simplex-on-dark'; Svg = 'simplex'; Color = $onDark; Sizes = $modeSizes }
)

function Brush([string] $fill, [string] $currentColor) {
    if (-not $fill -or $fill -eq 'currentColor') { $fill = $currentColor }
    return (New-Object System.Windows.Media.BrushConverter).ConvertFromString($fill)
}

# only what these files use: <rect width height rx fill> and <path d fill>
function Render([xml] $svg, [string] $color, [int] $size) {
    $visual = New-Object System.Windows.Media.DrawingVisual
    $dc = $visual.RenderOpen()
    $dc.PushTransform((New-Object System.Windows.Media.ScaleTransform ($size / 32.0), ($size / 32.0)))
    foreach ($node in $svg.svg.ChildNodes) {
        if ($node.LocalName -eq 'rect') {
            $rx = if ($node.rx) { [double]::Parse($node.rx, $invariant) } else { 0 }
            $rect = New-Object System.Windows.Rect 0, 0, ([double]::Parse($node.width, $invariant)), ([double]::Parse($node.height, $invariant))
            $dc.DrawRoundedRectangle((Brush $node.fill $color), $null, $rect, $rx, $rx)
        } elseif ($node.LocalName -eq 'path') {
            $geometry = [System.Windows.Media.Geometry]::Parse($node.d)
            $dc.DrawGeometry((Brush $node.fill $color), $null, $geometry)
        }
    }
    $dc.Pop()
    $dc.Close()
    $bitmap = New-Object System.Windows.Media.Imaging.RenderTargetBitmap $size, $size, 96, 96, ([System.Windows.Media.PixelFormats]::Pbgra32)
    $bitmap.Render($visual)
    return $bitmap
}

# a 32-bit DIB entry: bottom-up straight-alpha BGRA, then an empty AND mask
function DibBytes($bitmap, [int] $size) {
    $straight = New-Object System.Windows.Media.Imaging.FormatConvertedBitmap $bitmap, ([System.Windows.Media.PixelFormats]::Bgra32), $null, 0
    $stride = $size * 4
    $pixels = New-Object byte[] ($stride * $size)
    $straight.CopyPixels($pixels, $stride, 0)
    $maskStride = [int]([Math]::Ceiling($size / 32.0) * 4)
    $stream = New-Object IO.MemoryStream
    $writer = New-Object IO.BinaryWriter $stream
    $writer.Write([int]40); $writer.Write([int]$size); $writer.Write([int]($size * 2))
    $writer.Write([int16]1); $writer.Write([int16]32); $writer.Write([int]0)
    $writer.Write([int]($stride * $size + $maskStride * $size))
    $writer.Write([int]0); $writer.Write([int]0); $writer.Write([int]0); $writer.Write([int]0)
    for ($row = $size - 1; $row -ge 0; $row--) { $writer.Write($pixels, $row * $stride, $stride) }
    $writer.Write((New-Object byte[] ($maskStride * $size)))
    $writer.Flush()
    return $stream.ToArray()
}

function PngBytes($bitmap) {
    $encoder = New-Object System.Windows.Media.Imaging.PngBitmapEncoder
    $encoder.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
    $stream = New-Object IO.MemoryStream
    $encoder.Save($stream)
    return $stream.ToArray()
}

New-Item -ItemType Directory -Force $OutDir | Out-Null
foreach ($target in $targets) {
    [xml] $svg = [IO.File]::ReadAllText((Join-Path $SvgDir ($target.Svg + '.svg')), [Text.Encoding]::UTF8)
    $images = @()
    foreach ($size in $target.Sizes) {
        $bitmap = Render $svg $target.Color $size
        # Vista and later read PNG entries; only 256 is worth compressing
        $images += , @($size, $(if ($size -ge 256) { PngBytes $bitmap } else { DibBytes $bitmap $size }))
    }
    $stream = [IO.File]::Create((Join-Path $OutDir ($target.Name + '.ico')))
    $writer = New-Object IO.BinaryWriter $stream
    $writer.Write([int16]0); $writer.Write([int16]1); $writer.Write([int16]$images.Count)
    $offset = 6 + 16 * $images.Count
    foreach ($image in $images) {
        $dimension = if ($image[0] -ge 256) { 0 } else { $image[0] }
        $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
        $writer.Write([byte]0); $writer.Write([byte]0)
        $writer.Write([int16]1); $writer.Write([int16]32)
        $writer.Write([int]$image[1].Length); $writer.Write([int]$offset)
        $offset += $image[1].Length
    }
    foreach ($image in $images) { $writer.Write([byte[]]$image[1]) }
    $writer.Close()
}
"built $($targets.Count) icons in $OutDir"
