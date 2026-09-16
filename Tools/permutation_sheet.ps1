# Tiles RawArt/Perms into one labelled grid: bodies down the side, head options across the top.
# Reading a grid is the point -- the asset names cannot tell you whether a given head actually
# sits properly on a given body, and this shows all of it at once.
param([int]$Cell = 250)

Add-Type -AssemblyName System.Drawing
$src = "C:\Dev\Games\RepliCan\RawArt\Perms"
$dst = "C:\Dev\Games\RepliCan\RawArt\SoldierPermutations.png"
$files = Get-ChildItem $src -Filter "*.png"
if ($files.Count -eq 0) { Write-Output "no renders in $src"; exit 1 }

# body__head.png
$bodies = @(); $heads = @()
foreach ($f in $files) {
  $parts = $f.BaseName -split '__'
  if ($bodies -notcontains $parts[0]) { $bodies += $parts[0] }
  if ($heads -notcontains $parts[1]) { $heads += $parts[1] }
}
# keep a sensible reading order rather than alphabetical
$headOrder = @('bare','HeadMale','HeadFemale','HelmetMale','HelmetFemale','Armour')
$heads = $headOrder | Where-Object { $heads -contains $_ }
$bodyOrder = @('Male_01','Female_01','BR_Male_01')
$bodies = $bodyOrder | Where-Object { $bodies -contains $_ }

$labelL = 120           # room for the body name down the left
$labelT = 34
$pad = 6
$W = $labelL + $heads.Count * ($Cell + $pad) + $pad
$H = $labelT + $bodies.Count * ($Cell + $pad) + $pad + 26

$bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'; $g.InterpolationMode = 'HighQualityBicubic'; $g.TextRenderingHint = 'AntiAlias'
$g.Clear([System.Drawing.Color]::FromArgb(255, 10, 14, 11))
$font = New-Object System.Drawing.Font 'Bahnschrift Condensed', 17, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$small = New-Object System.Drawing.Font 'Bahnschrift Condensed', 15, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$ink = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 214, 234, 218))
$dim = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 126, 166, 134))
$sf = New-Object System.Drawing.StringFormat; $sf.Alignment = 'Center'

$g.DrawString('SpaceSoldier bodies x head options  -  red palette', $small, $dim, 8, 6)
for ($c = 0; $c -lt $heads.Count; $c++) {
  $x = $labelL + $c * ($Cell + $pad)
  $g.DrawString($heads[$c], $font, $ink, (New-Object System.Drawing.RectangleF $x, ($labelT - 20), $Cell, 20), $sf)
}
for ($r = 0; $r -lt $bodies.Count; $r++) {
  $y = $labelT + $r * ($Cell + $pad)
  $g.DrawString($bodies[$r], $font, $ink, 8, ($y + $Cell / 2 - 10))
  for ($c = 0; $c -lt $heads.Count; $c++) {
    $x = $labelL + $c * ($Cell + $pad)
    $p = Join-Path $src ("{0}__{1}.png" -f $bodies[$r], $heads[$c])
    if (-not (Test-Path $p)) { continue }
    $img = [System.Drawing.Image]::FromFile($p)
    $g.DrawImage($img, (New-Object System.Drawing.Rectangle $x, $y, $Cell, $Cell))
    $img.Dispose()
  }
}
$bmp.Save($dst, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "wrote $dst"
