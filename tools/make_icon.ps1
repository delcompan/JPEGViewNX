# Regenerate res/app.ico from the image the user dropped into icon\.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\make_icon.ps1            # newest image in icon\
#   powershell -ExecutionPolicy Bypass -File tools\make_icon.ps1 <image>    # explicit image
#
# The image is center-cropped to a square and written as a multi-size ICO
# (PNG-compressed entries, Windows Vista+). build.bat calls this automatically,
# so replacing the image in icon\ and rebuilding is enough.
param(
    [string]$Source = ""
)

Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot          # project root
$iconDir = Join-Path $root "icon"
$outIco = Join-Path $root "res\app.ico"

if ([string]::IsNullOrWhiteSpace($Source)) {
    $candidates = Get-ChildItem -Path $iconDir -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -match '^\.(png|jpg|jpeg|webp|bmp|gif|tif|tiff)$' } |
        Sort-Object LastWriteTime -Descending
    if (-not $candidates) {
        Write-Error "No source image found in $iconDir (expected png/jpg/jpeg/webp/bmp/gif/tif)."
        exit 1
    }
    $Source = $candidates[0].FullName
}

if (-not (Test-Path -LiteralPath $Source)) {
    Write-Error "Source image not found: $Source"
    exit 1
}

Write-Output "icon source: $Source"

$img = [System.Drawing.Image]::FromFile((Resolve-Path -LiteralPath $Source))
$side = [Math]::Min($img.Width, $img.Height)
$crop = New-Object System.Drawing.Bitmap($side, $side, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$gc = [System.Drawing.Graphics]::FromImage($crop)
$gc.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$gc.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$gc.Clear([System.Drawing.Color]::Transparent)
$srcRect = New-Object System.Drawing.Rectangle(([int](($img.Width - $side) / 2)), ([int](($img.Height - $side) / 2)), $side, $side)
$dstRect = New-Object System.Drawing.Rectangle(0, 0, $side, $side)
$gc.DrawImage($img, $dstRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
$gc.Dispose()
$img.Dispose()

$sizes = @(256, 128, 96, 64, 48, 40, 32, 24, 20, 16)
$pngs = New-Object System.Collections.ArrayList

foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap($s, $s, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($crop, (New-Object System.Drawing.Rectangle(0, 0, $s, $s)))
    $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    [void]$pngs.Add($ms.ToArray())
    $ms.Dispose()
    $bmp.Dispose()
}
$crop.Dispose()

$resDir = Split-Path -Parent $outIco
if (-not (Test-Path -LiteralPath $resDir)) { New-Item -ItemType Directory -Path $resDir | Out-Null }

$fs = [System.IO.File]::Create($outIco)
$bw = New-Object System.IO.BinaryWriter($fs)
$bw.Write([UInt16]0)                 # reserved
$bw.Write([UInt16]1)                 # type: icon
$bw.Write([UInt16]$sizes.Count)      # image count

$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $dim = if ($s -ge 256) { 0 } else { $s }
    $bw.Write([byte]$dim)            # width (0 = 256)
    $bw.Write([byte]$dim)            # height
    $bw.Write([byte]0)               # palette colours
    $bw.Write([byte]0)               # reserved
    $bw.Write([UInt16]1)             # colour planes
    $bw.Write([UInt16]32)            # bits per pixel
    $bw.Write([UInt32]$pngs[$i].Length)
    $bw.Write([UInt32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $bw.Write($p) }
$bw.Close()
$fs.Close()

Write-Output ("icon written: {0} ({1} sizes: {2})" -f $outIco, $sizes.Count, ($sizes -join ", "))
