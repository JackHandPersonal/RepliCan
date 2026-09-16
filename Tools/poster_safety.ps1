# Safety billboard: a cartoon miner ("Digger Dan") giving a mining safety tip in a speech bubble
# over safety yellow, a dark header band, a hazard-stripe foot. Same 1700 x 1050 convention as
# the other boards (upright copy for review, quarter-turned copy for the pitched board planes);
# Tools/poster_grime.ps1 -Only T_Poster_Safety weathers it afterwards.
#   powershell Tools/poster_safety.ps1
$dir = "C:\Dev\Games\RepliCan\RawArt"
Add-Type -AssemblyName System.Drawing
$W = 1700; $H = 1050
function C($r, $g, $b) { return [System.Drawing.Color]::FromArgb(255, $r, $g, $b) }
function Font($name, $size, $bold) { $st = if ($bold) { [System.Drawing.FontStyle]::Bold } else { [System.Drawing.FontStyle]::Regular }; return New-Object System.Drawing.Font $name, $size, $st, ([System.Drawing.GraphicsUnit]::Pixel) }
function RoundRect($x, $y, $w, $h, $r) {
  $p = New-Object System.Drawing.Drawing2D.GraphicsPath
  $p.AddArc($x, $y, 2*$r, 2*$r, 180, 90); $p.AddArc($x + $w - 2*$r, $y, 2*$r, 2*$r, 270, 90)
  $p.AddArc($x + $w - 2*$r, $y + $h - 2*$r, 2*$r, 2*$r, 0, 90); $p.AddArc($x, $y + $h - 2*$r, 2*$r, 2*$r, 90, 90); $p.CloseFigure(); return $p
}
function Save($bmp, $name) {
  $bmp.Save("$dir\$name`_Upright.png", [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.RotateFlip([System.Drawing.RotateFlipType]::Rotate270FlipNone)
  $bmp.Save("$dir\$name.png", [System.Drawing.Imaging.ImageFormat]::Png)
}
$yellow = C 232 184 48; $ink = C 26 28 32; $navy = C 34 48 68; $white = C 244 240 230; $skin = C 236 200 160
$orange = C 232 120 40; $blue = C 58 96 150; $blueDark = C 40 68 110; $red = C 200 48 40; $stripe = C 20 20 20

$bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp); $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAlias'
$g.Clear($yellow)
$bInk = New-Object System.Drawing.SolidBrush $ink; $bNavy = New-Object System.Drawing.SolidBrush $navy; $bWhite = New-Object System.Drawing.SolidBrush $white
$bSkin = New-Object System.Drawing.SolidBrush $skin; $bOrange = New-Object System.Drawing.SolidBrush $orange; $bBlue = New-Object System.Drawing.SolidBrush $blue
$bBlueDark = New-Object System.Drawing.SolidBrush $blueDark; $bRed = New-Object System.Drawing.SolidBrush $red; $bYellow = New-Object System.Drawing.SolidBrush $yellow
$pInk = New-Object System.Drawing.Pen $ink, 7

# ---- header band and hazard foot
$g.FillRectangle($bNavy, 0, 0, $W, 150)
$sf = New-Object System.Drawing.StringFormat; $sf.Alignment = 'Center'; $sf.LineAlignment = 'Center'
$g.DrawString('S A F E T Y   F I R S T', (Font 'Bahnschrift SemiBold Condensed' 92 $true), $bWhite, (New-Object System.Drawing.RectangleF 0, 22, $W, 110), $sf)
$g.FillRectangle($bInk, 0, ($H - 110), $W, 110)
$bStripe = New-Object System.Drawing.SolidBrush $yellow
for ($x = -120; $x -lt $W + 120; $x += 120) {
  $pts = @((New-Object System.Drawing.PointF $x, ($H - 110)), (New-Object System.Drawing.PointF ($x + 60), ($H - 110)), (New-Object System.Drawing.PointF ($x + 60 - 110), $H), (New-Object System.Drawing.PointF ($x - 110), $H))
  $g.FillPolygon($bStripe, [System.Drawing.PointF[]]$pts)
}
$g.FillRectangle($bInk, 0, ($H - 110), $W, 8)

# ---- Digger Dan: a round-faced miner in a hard hat, one finger raised
$cx = 430; $cy = 560
# body: coveralls, a rounded block, with a reflective chest stripe
$body = RoundRect ($cx - 210) ($cy + 40) 420 300 90; $g.FillPath($bBlue, $body); $g.DrawPath($pInk, $body)
$g.FillRectangle($bYellow, ($cx - 210), ($cy + 150), 420, 34); $g.FillRectangle($bInk, ($cx - 210), ($cy + 150), 420, 5); $g.FillRectangle($bInk, ($cx - 210), ($cy + 179), 420, 5)
$g.FillRectangle($bBlueDark, ($cx - 30), ($cy + 60), 60, 280)          # the zip
# left arm on the hip
$armL = RoundRect ($cx - 330) ($cy + 110) 150 90 45; $g.FillPath($bBlue, $armL); $g.DrawPath($pInk, $armL)
$g.FillEllipse($bSkin, ($cx - 360), ($cy + 100), 110, 110); $g.DrawEllipse($pInk, ($cx - 360), ($cy + 100), 110, 110)
# right arm up, index finger raised
$armR = RoundRect ($cx + 150) ($cy - 170) 100 300 50; $g.FillPath($bBlue, $armR); $g.DrawPath($pInk, $armR)
$g.FillEllipse($bSkin, ($cx + 130), ($cy - 260), 140, 130); $g.DrawEllipse($pInk, ($cx + 130), ($cy - 260), 140, 130)
$finger = RoundRect ($cx + 175) ($cy - 380) 50 150 25; $g.FillPath($bSkin, $finger); $g.DrawPath($pInk, $finger)
$g.FillEllipse($bSkin, ($cx + 160), ($cy - 250), 60, 60)                 # covers the seam
# head
$g.FillEllipse($bSkin, ($cx - 170), ($cy - 300), 340, 340); $g.DrawEllipse($pInk, ($cx - 170), ($cy - 300), 340, 340)
# hard hat: dome, brim, lamp
$g.FillPie($bOrange, ($cx - 195), ($cy - 360), 390, 300, 180, 180); $g.DrawArc($pInk, ($cx - 195), ($cy - 360), 390, 300, 180, 180)
$brim = RoundRect ($cx - 240) ($cy - 225) 480 44 22; $g.FillPath($bOrange, $brim); $g.DrawPath($pInk, $brim)
$g.FillEllipse($bInk, ($cx - 45), ($cy - 330), 90, 90); $g.FillEllipse($bWhite, ($cx - 32), ($cy - 317), 64, 64); $g.FillEllipse($bYellow, ($cx - 18), ($cy - 303), 36, 36)
# eyes, brows, grin
$g.FillEllipse($bWhite, ($cx - 120), ($cy - 150), 100, 110); $g.FillEllipse($bWhite, ($cx + 20), ($cy - 150), 100, 110)
$g.DrawEllipse($pInk, ($cx - 120), ($cy - 150), 100, 110); $g.DrawEllipse($pInk, ($cx + 20), ($cy - 150), 100, 110)
$g.FillEllipse($bInk, ($cx - 80), ($cy - 110), 40, 46); $g.FillEllipse($bInk, ($cx + 60), ($cy - 110), 40, 46)
$g.FillEllipse($bWhite, ($cx - 66), ($cy - 102), 14, 14); $g.FillEllipse($bWhite, ($cx + 74), ($cy - 102), 14, 14)
$pBrow = New-Object System.Drawing.Pen $ink, 14; $pBrow.StartCap = 'Round'; $pBrow.EndCap = 'Round'
$g.DrawLine($pBrow, ($cx - 125), ($cy - 178), ($cx - 30), ($cy - 168)); $g.DrawLine($pBrow, ($cx + 30), ($cy - 172), ($cx + 125), ($cy - 190))
$g.FillPie($bInk, ($cx - 95), ($cy - 100), 190, 120, 0, 180)              # the grin: the lower half of an ellipse, inside the face
$g.FillRectangle($bWhite, ($cx - 78), ($cy - 40), 156, 24)                  # teeth
$g.FillEllipse($bRed, ($cx - 40), ($cy - 12), 80, 30)                        # tongue
$g.FillEllipse($bRed, ($cx + 120), ($cy - 30), 40, 40)                    # a rosy cheek

# ---- the bubble
$bub = RoundRect 800 210 820 520 60; $g.FillPath($bWhite, $bub); $g.DrawPath($pInk, $bub)
$tail = @((New-Object System.Drawing.PointF 800, 520), (New-Object System.Drawing.PointF 690, 470), (New-Object System.Drawing.PointF 800, 430))
$g.FillPolygon($bWhite, [System.Drawing.PointF[]]$tail); $g.DrawLine($pInk, 800, 520, 690, 470); $g.DrawLine($pInk, 690, 470, 800, 430)
$g.FillRectangle($bWhite, 796, 425, 12, 100)
$sl = New-Object System.Drawing.StringFormat; $sl.Alignment = 'Near'; $sl.LineAlignment = 'Near'
$g.DrawString('DIGGER DAN SAYS:', (Font 'Bahnschrift SemiBold Condensed' 40 $true), $bNavy, (New-Object System.Drawing.RectangleF 850, 240, 740, 50), $sl)
$g.DrawString('CHECK YOUR SEALS', (Font 'Bahnschrift SemiBold Condensed' 110 $true), $bRed, (New-Object System.Drawing.RectangleF 846, 288, 780, 130), $sl)
$g.DrawString("BEFORE THE AIRLOCK,`nNOT AFTER.", (Font 'Bahnschrift SemiBold Condensed' 88 $true), $bInk, (New-Object System.Drawing.RectangleF 850, 410, 760, 200), $sl)
$g.DrawString("Vacuum doesn't care whose fault it was.", (Font 'Bahnschrift Condensed' 42 $false), $bNavy, (New-Object System.Drawing.RectangleF 850, 640, 760, 60), $sl)

# ---- the line beneath
$g.DrawString('SAFETY IS EVERYONE''S JOB   //   ASTEROID OPERATIONS   //   REPORT EVERY LEAK', (Font 'Bahnschrift SemiBold Condensed' 36 $false), $bInk, (New-Object System.Drawing.RectangleF 680, 870, 1010, 60), $sf)
Save $bmp 'T_Poster_Safety'
Write-Output "safety poster written"
