# Keep-out art for the out-of-order cabin door, and the laser's scorch mask. Three PNGs in RawArt:
#   T_Sign_OutOfOrder.png  a paper notice, red on off-white, a little grubby (512 x 360; the door
#                          plane is scaled 0.42 x 0.295 to match)
#   T_HazardTape.png       yellow and black diagonal stripes, worn (512 x 64, tiles along a strip)
#   T_Scorch_Soft.png      a soft round mask with a rough edge (64 x 64): the laser's heated track
# Then: python Tools/ue_remote.py --file Tools/import_keepout.py  (textures + materials)
#   powershell Tools/make_keepout.ps1
$dir = "C:\Dev\Games\RepliCan\RawArt"
Add-Type -AssemblyName System.Drawing
function C($r, $g, $b, $a = 255) { return [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function Font($name, $size, $bold) { $st = if ($bold) { [System.Drawing.FontStyle]::Bold } else { [System.Drawing.FontStyle]::Regular }; return New-Object System.Drawing.Font $name, $size, $st, ([System.Drawing.GraphicsUnit]::Pixel) }
$rnd = New-Object System.Random 7

# ---- the sign ------------------------------------------------------------------------------
$W = 512; $H = 360
$bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp); $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAlias'
$paper = C 236 229 210; $red = C 178 38 34; $ink = C 40 38 36
$g.Clear($paper)
$bRed = New-Object System.Drawing.SolidBrush $red; $bInk = New-Object System.Drawing.SolidBrush $ink; $bPaper = New-Object System.Drawing.SolidBrush $paper
$pRed = New-Object System.Drawing.Pen $red, 10
$g.DrawRectangle($pRed, 12, 12, ($W - 24), ($H - 24))
$g.FillRectangle($bRed, 12, 12, ($W - 24), 96)
$sf = New-Object System.Drawing.StringFormat; $sf.Alignment = 'Center'; $sf.LineAlignment = 'Center'
$g.DrawString('OUT OF ORDER', (Font 'Bahnschrift SemiBold Condensed' 74 $true), $bPaper, (New-Object System.Drawing.RectangleF 12, 12, ($W - 24), 96), $sf)
$g.DrawString('DO NOT ENTER', (Font 'Bahnschrift SemiBold Condensed' 58 $true), $bRed, (New-Object System.Drawing.RectangleF 12, 118, ($W - 24), 70), $sf)
$g.DrawString('Door 6  -  actuator fault', (Font 'Bahnschrift' 27 $false), $bInk, (New-Object System.Drawing.RectangleF 12, 200, ($W - 24), 36), $sf)
$g.DrawString('Maintenance ticket 4471-B', (Font 'Bahnschrift' 27 $false), $bInk, (New-Object System.Drawing.RectangleF 12, 238, ($W - 24), 36), $sf)
$g.DrawString('P C S   F A C I L I T I E S', (Font 'Bahnschrift SemiBold Condensed' 24 $true), $bRed, (New-Object System.Drawing.RectangleF 12, 292, ($W - 24), 40), $sf)
# grubby: a few translucent grey blotches and a darker smear along the bottom edge
for ($i = 0; $i -lt 14; $i++) {
  $b = New-Object System.Drawing.SolidBrush (C 90 80 60 ($rnd.Next(14, 40)))
  $x = $rnd.Next(0, $W); $y = $rnd.Next(0, $H); $r = $rnd.Next(20, 90)
  $g.FillEllipse($b, ($x - $r), ($y - $r / 2), (2 * $r), $r)
}
$g.FillRectangle((New-Object System.Drawing.SolidBrush (C 70 60 45 40)), 0, ($H - 26), $W, 26)
$bmp.Save("$dir\T_Sign_OutOfOrder.png", [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "wrote T_Sign_OutOfOrder.png"

# ---- the tape ------------------------------------------------------------------------------
$W = 512; $H = 64
$bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp); $g.SmoothingMode = 'AntiAlias'
$black = C 28 26 24; $yellow = C 232 190 42
$g.Clear($black)
$bYellow = New-Object System.Drawing.SolidBrush $yellow
for ($x = -80; $x -lt $W + 80; $x += 80) {
  $pts = @((New-Object System.Drawing.PointF $x, 0), (New-Object System.Drawing.PointF ($x + 40), 0), (New-Object System.Drawing.PointF ($x + 40 - 64), $H), (New-Object System.Drawing.PointF ($x - 64), $H))
  $g.FillPolygon($bYellow, [System.Drawing.PointF[]]$pts)
}
for ($i = 0; $i -lt 90; $i++) {   # worn: specks of dirt, and a few scuffs that show the black through the yellow
  $b = New-Object System.Drawing.SolidBrush (C 40 34 28 ($rnd.Next(30, 110)))
  $g.FillEllipse($b, $rnd.Next(0, $W), $rnd.Next(0, $H), $rnd.Next(2, 7), $rnd.Next(1, 4))
}
$bmp.Save("$dir\T_HazardTape.png", [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "wrote T_HazardTape.png"

# ---- the scorch mask ------------------------------------------------------------------------
$S = 64
$bmp = New-Object System.Drawing.Bitmap $S, $S, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
for ($y = 0; $y -lt $S; $y++) {
  for ($x = 0; $x -lt $S; $x++) {
    $dx = ($x + 0.5) / $S - 0.5; $dy = ($y + 0.5) / $S - 0.5
    $d = [Math]::Sqrt($dx * $dx + $dy * $dy) * 2.0
    $edge = 0.82 + 0.16 * [Math]::Sin(11.0 * [Math]::Atan2($dy, $dx)) * [Math]::Sin(3.0 * [Math]::Atan2($dy, $dx) + 1.3)   # a ragged rim
    $a = 1.0 - [Math]::Max(0.0, [Math]::Min(1.0, ($d - $edge * 0.55) / ($edge * 0.45)))
    $a = $a * $a * (3.0 - 2.0 * $a)
    $bmp.SetPixel($x, $y, (C 255 255 255 ([int](255 * $a))))
  }
}
$bmp.Save("$dir\T_Scorch_Soft.png", [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "wrote T_Scorch_Soft.png"
