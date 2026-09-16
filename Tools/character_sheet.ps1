# Tiles RawArt/Chars/*.png into labelled contact sheets. The label is the whole point: picking a
# Synty character off a wall of unnamed thumbnails is a memory test, and a named grid turns it
# into reading. Writes RawArt/CharacterSheet_<n>.png, at most ROWS x COLS per sheet.
param([int]$Cols = 8, [int]$Rows = 4, [int]$Cell = 230)

Add-Type -AssemblyName System.Drawing
$src = "C:\Dev\Games\RepliCan\RawArt\Chars"
$dst = "C:\Dev\Games\RepliCan\RawArt"
$files = Get-ChildItem $src -Filter "*.png" | Sort-Object Name
if ($files.Count -eq 0) { Write-Output "no renders in $src"; exit 1 }

$label = 34
$pad = 8
$tileW = $Cell + $pad
$tileH = $Cell + $label + $pad
$perSheet = $Cols * $Rows
$sheets = [math]::Ceiling($files.Count / $perSheet)
$font = New-Object System.Drawing.Font 'Bahnschrift Condensed', 15, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$fontSmall = New-Object System.Drawing.Font 'Bahnschrift Condensed', 13, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$ink = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 210, 230, 214))
$dim = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 120, 160, 128))
$back = [System.Drawing.Color]::FromArgb(255, 10, 14, 11)

for ($s = 0; $s -lt $sheets; $s++) {
  $slice = $files | Select-Object -Skip ($s * $perSheet) -First $perSheet
  $rowsUsed = [math]::Ceiling($slice.Count / $Cols)
  $W = $Cols * $tileW + $pad
  $H = $rowsUsed * $tileH + $pad + 30
  $bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = 'AntiAlias'; $g.InterpolationMode = 'HighQualityBicubic'; $g.TextRenderingHint = 'AntiAlias'
  $g.Clear($back)
  $g.DrawString(("PolygonSciFiSpace characters  -  sheet {0} of {1}" -f ($s + 1), $sheets), $font, $dim, 10, 8)

  $i = 0
  foreach ($f in $slice) {
    $cx = ($i % $Cols) * $tileW + $pad
    $cy = [math]::Floor($i / $Cols) * $tileH + $pad + 26
    $img = [System.Drawing.Image]::FromFile($f.FullName)
    $g.DrawImage($img, (New-Object System.Drawing.Rectangle $cx, $cy, $Cell, $Cell))
    $img.Dispose()
    # Name under the tile, split so a long asset name stays readable at this size.
    $name = $f.BaseName -replace '^SK_Chr_', ''
    $sf = New-Object System.Drawing.StringFormat; $sf.Alignment = 'Center'
    if ($name.Length -gt 22) {
      $cut = $name.LastIndexOf('_', [math]::Min(21, $name.Length - 1))
      if ($cut -lt 6) { $cut = 21 }
      $g.DrawString($name.Substring(0, $cut), $fontSmall, $ink, (New-Object System.Drawing.RectangleF $cx, ($cy + $Cell + 1), $Cell, 16), $sf)
      $g.DrawString($name.Substring($cut + 1), $fontSmall, $ink, (New-Object System.Drawing.RectangleF $cx, ($cy + $Cell + 15), $Cell, 16), $sf)
    } else {
      $g.DrawString($name, $font, $ink, (New-Object System.Drawing.RectangleF $cx, ($cy + $Cell + 4), $Cell, 18), $sf)
    }
    $i++
  }
  $out = Join-Path $dst ("CharacterSheet_{0}.png" -f ($s + 1))
  $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Output "wrote $out ($($slice.Count) characters)"
}
