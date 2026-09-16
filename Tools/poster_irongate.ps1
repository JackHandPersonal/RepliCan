# The IronGate board (was scratch posters2.ps1; moved into Tools so the state lives with the project).
# 1700 x 1050 landscape, saved upright (_Upright, for review) and quarter-turned
# (Rotate270) for the pitched board planes, the same convention as the rest of RawArt.
#   T_Poster_IronGate : the gate runs edge to edge, the three-spike roundel badged over it,
#   fire behind the bars. Then: powershell Tools/poster_grime.ps1 ; then the poster import.
#   (the labour board is its own script, poster_labour.ps1)
$dir = "C:\Dev\Games\RepliCan\RawArt"
Add-Type -AssemblyName System.Drawing
$W = 1700; $H = 1050
function C($r, $g, $b) { return [System.Drawing.Color]::FromArgb(255, $r, $g, $b) }
function CA($a, $r, $g, $b) { return [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function Font($name, $size, $bold) { $st = if ($bold) { [System.Drawing.FontStyle]::Bold } else { [System.Drawing.FontStyle]::Regular }; return New-Object System.Drawing.Font $name, $size, $st, ([System.Drawing.GraphicsUnit]::Pixel) }
function Centre($g, $text, $font, $color, $y, $spacing, $width = 0) {
  $b = New-Object System.Drawing.SolidBrush $color
  if ($spacing -gt 0) { $text = ($text.ToCharArray() -join (' ' * $spacing)) }
  if ($width -le 0) { $width = $W }
  $sf = New-Object System.Drawing.StringFormat; $sf.Alignment = 'Center'; $sf.LineAlignment = 'Center'
  $g.DrawString($text, $font, $b, (New-Object System.Drawing.RectangleF 0, $y, $width, ($font.Size * 1.3)), $sf)
}
# Blackletter set letter by letter so it can be tracked out: the gaps are what make it readable.
function Tracked($g, $text, $font, $color, $cx, $y, $track) {
  $b = New-Object System.Drawing.SolidBrush $color
  $sf = [System.Drawing.StringFormat]::GenericTypographic
  $widths = @(); $total = 0.0
  foreach ($ch in $text.ToCharArray()) { $sz = $g.MeasureString([string]$ch, $font, 2000, $sf); $widths += $sz.Width; $total += $sz.Width + $track }
  $total -= $track
  $x = $cx - $total / 2.0
  for ($i = 0; $i -lt $text.Length; $i++) { $g.DrawString([string]$text[$i], $font, $b, $x, $y, $sf); $x += $widths[$i] + $track }
}
function Save($bmp, $name) {
  $bmp.Save("$dir\$name`_Upright.png", [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.RotateFlip([System.Drawing.RotateFlipType]::Rotate270FlipNone)
  $bmp.Save("$dir\$name.png", [System.Drawing.Imaging.ImageFormat]::Png)
}
function New-Board($bg) {
  $bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAlias'; $g.Clear($bg)
  return @($bmp, $g)
}
$black = C 12 10 11; $red = C 168 22 28; $darkred = C 96 12 16; $white = C 238 234 228; $grey = C 70 66 66; $cream = C 232 223 202
# The footer line sat on near-black in a dark grey that closed up into it, so it runs light.
# The tagline stays the brand red the gate and the mark are drawn in -- lightening it read pink.
$litegrey = C 196 190 184

# =====================================================================  IRONGATE
$r = New-Board $black; $bmp = $r[0]; $g = $r[1]
$bBlack = New-Object System.Drawing.SolidBrush $black
$bWhite = New-Object System.Drawing.SolidBrush $white
$bRed = New-Object System.Drawing.SolidBrush $red
$bDark = New-Object System.Drawing.SolidBrush $darkred
$fenceBottom = 470
$g.FillRectangle($bRed, 0, 0, $W, $fenceBottom)

# FIRE BEHIND THE GATE: tongues of flame rising from the foot of the red field, in reds a shade
# off the field's own, so they read as shapes in the red rather than as a picture of a fire.
# Two ranks, the back one darker, the front one lighter and warmer; a few embers drifting up.
# The white rule, the bars, the rails and the roundel are drawn after, over them. Seeded, so
# re-runs match; the grime pass (Tools/poster_grime.ps1) goes over the top as before.
$rand = New-Object System.Random 7
function Tongue($g, $brush, $x, $base, $h, $w, $lean) {
  $p = New-Object System.Drawing.Drawing2D.GraphicsPath
  $tipX = $x + $lean; $tipY = $base - $h
  $p.StartFigure()
  $p.AddBezier([single]($x - $w), [single]$base, [single]($x - $w * 0.9), [single]($base - $h * 0.45), [single]($tipX - $w * 0.35), [single]($base - $h * 0.55), [single]$tipX, [single]$tipY)
  $p.AddBezier([single]$tipX, [single]$tipY, [single]($tipX + $w * 0.15), [single]($base - $h * 0.6), [single]($x + $w * 1.05), [single]($base - $h * 0.35), [single]($x + $w), [single]$base)
  $p.CloseFigure()
  $g.FillPath($brush, $p); $p.Dispose()
}
$bFlameBack = New-Object System.Drawing.SolidBrush (C 152 20 26)
$bFlameFront = New-Object System.Drawing.SolidBrush (C 188 42 30)
for ($fx = -40; $fx -lt ($W + 40); $fx += 70) { $fh = 150 + $rand.Next(0, 170); Tongue $g $bFlameBack ($fx + $rand.Next(-20, 20)) ($fenceBottom + 1) $fh (34 + $rand.Next(0, 26)) ($rand.Next(-40, 40)) }
for ($fx = 0; $fx -lt ($W + 40); $fx += 95) { $fh = 90 + $rand.Next(0, 140); Tongue $g $bFlameFront ($fx + $rand.Next(-25, 25)) ($fenceBottom + 1) $fh (26 + $rand.Next(0, 22)) ($rand.Next(-30, 30)) }
for ($i = 0; $i -lt 18; $i++) { $ex = $rand.Next(40, $W - 40); $ey = 90 + $rand.Next(0, 250); $er = 3 + $rand.Next(0, 5); $g.FillEllipse($bFlameFront, ($ex - $er), ($ey - $er), (2 * $er), (2 * $er)) }
$pW = New-Object System.Drawing.Pen $white, 5; $g.DrawLine($pW, 0, $fenceBottom, $W, $fenceBottom)
$pW2 = New-Object System.Drawing.Pen $white, 2; $g.DrawLine($pW2, 0, ($fenceBottom + 12), $W, ($fenceBottom + 12))

# One barbed spike: a lancet head with two barbs over a bar. $k scales it about its tip.
function Spike($g, $brush, $cx, $tip, $base, $k) {
  $barW = 26 * $k
  $g.FillRectangle($brush, ($cx - $barW / 2), ($tip + 120 * $k), $barW, ($base - $tip - 120 * $k))
  $pts = @(
    (New-Object System.Drawing.PointF $cx, $tip),
    (New-Object System.Drawing.PointF ($cx + 30 * $k), ($tip + 110 * $k)),
    (New-Object System.Drawing.PointF ($cx + 52 * $k), ($tip + 92 * $k)),
    (New-Object System.Drawing.PointF ($cx + 24 * $k), ($tip + 150 * $k)),
    (New-Object System.Drawing.PointF ($cx + $barW / 2), ($tip + 135 * $k)),
    (New-Object System.Drawing.PointF ($cx - $barW / 2), ($tip + 135 * $k)),
    (New-Object System.Drawing.PointF ($cx - 24 * $k), ($tip + 150 * $k)),
    (New-Object System.Drawing.PointF ($cx - 52 * $k), ($tip + 92 * $k)),
    (New-Object System.Drawing.PointF ($cx - 30 * $k), ($tip + 110 * $k)))
  $g.FillPolygon($brush, [System.Drawing.PointF[]]$pts)
}
# The gate: bars right across the board, alternating heights, two white rails through them.
$step = 120; $n = [int][Math]::Ceiling($W / $step) + 1
for ($i = 0; $i -lt $n; $i++) {
  $cx = $i * $step + 10
  $tall = if ($i % 2 -eq 0) { 1.0 } else { 0.78 }
  $tip = 60 + (1 - $tall) * 120
  Spike $g $bBlack $cx $tip ($fenceBottom + 40) 1.0
  $pE = New-Object System.Drawing.Pen $white, 3
  $g.DrawLine($pE, ($cx - 2), ($tip + 6), ($cx - 28), ($tip + 106))
}
$g.FillRectangle($bWhite, 0, 300, $W, 14)
$g.FillRectangle($bBlack, 0, 314, $W, 26)
$g.FillRectangle($bWhite, 0, 400, $W, 10)
$g.FillRectangle($bBlack, 0, 410, $W, 22)

# The mark, badged over the middle of the gate: three red spikes, the middle highest, in a
# white roundel ringed in red and studded with rivets. Its foot crosses into the black field.
$lx = $W / 2; $ly = 300; $R = 264
$g.FillEllipse($bWhite, ($lx - $R), ($ly - $R), (2 * $R), (2 * $R))
$pRing = New-Object System.Drawing.Pen $red, 15; $g.DrawEllipse($pRing, ($lx - $R + 26), ($ly - $R + 26), (2 * $R - 52), (2 * $R - 52))
$pRing2 = New-Object System.Drawing.Pen $black, 7; $g.DrawEllipse($pRing2, ($lx - $R + 5), ($ly - $R + 5), (2 * $R - 10), (2 * $R - 10))
for ($i = 0; $i -lt 40; $i++) { $t = $i * (360 / 40) * [Math]::PI / 180; $rx = $lx + ($R - 52) * [Math]::Cos($t); $ry = $ly + ($R - 52) * [Math]::Sin($t); $g.FillEllipse($bBlack, ($rx - 5), ($ry - 5), 10, 10) }
$k = 1.24; $base = $ly + 168
Spike $g $bRed ($lx - 84) ($ly - 104) $base $k
Spike $g $bRed $lx ($ly - 176) $base $k
Spike $g $bRed ($lx + 84) ($ly - 104) $base $k

# The name in blackletter, tracked out so it reads at a glance, with a red drop behind it.
$fName = Font 'Old English Text MT' 210 $false
Tracked $g 'IronGate' $fName $darkred ($W / 2 + 7) 585 16
Tracked $g 'IronGate' $fName $white ($W / 2) 578 16
$fTag = Font 'Bahnschrift SemiBold Condensed' 82 $false
Centre $g 'THE PRICE OF CERTAINTY' $fTag $red 862 1
$g.DrawLine($pW2, 150, 916, 330, 916); $g.DrawLine($pW2, ($W - 330), 916, ($W - 150), 916)
$fFoot = Font 'Bahnschrift Condensed' 38 $false
Centre $g 'PRIVATE SECURITY  //  ASSET RECOVERY  //  CONTRACT ENFORCEMENT' $fFoot $litegrey 972 0
Save $bmp 'T_Poster_IronGate'

Write-Output "irongate written"
