# A tileable value-noise texture for the cheap ground fog (Tools/make_ground_fog.py): 256 x 256,
# three octaves on a periodic lattice, so the plane's panner never shows a seam.
#   powershell Tools/make_fog_texture.ps1
$dir = "C:\Dev\Games\RepliCan\RawArt"
Add-Type -AssemblyName System.Drawing
$S = 256
$rnd = New-Object System.Random 19
function Lattice($n) { $a = New-Object 'double[,]' $n, $n; for ($j = 0; $j -lt $n; $j++) { for ($i = 0; $i -lt $n; $i++) { $a[$i, $j] = $rnd.NextDouble() } }; return ,$a }
function Smooth($t) { return $t * $t * (3.0 - 2.0 * $t) }
function Sample($a, $n, $u, $v) {
  $x = $u * $n; $y = $v * $n
  $x0 = [Math]::Floor($x); $y0 = [Math]::Floor($y)
  $fx = Smooth ($x - $x0); $fy = Smooth ($y - $y0)
  $i0 = [int]$x0 % $n; $j0 = [int]$y0 % $n; $i1 = ($i0 + 1) % $n; $j1 = ($j0 + 1) % $n
  $a00 = $a[$i0, $j0]; $a10 = $a[$i1, $j0]; $a01 = $a[$i0, $j1]; $a11 = $a[$i1, $j1]
  $top = $a00 + ($a10 - $a00) * $fx; $bot = $a01 + ($a11 - $a01) * $fx
  return $top + ($bot - $top) * $fy
}
$L1 = Lattice 4; $L2 = Lattice 8; $L3 = Lattice 16
$bmp = New-Object System.Drawing.Bitmap $S, $S, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
for ($y = 0; $y -lt $S; $y++) {
  for ($x = 0; $x -lt $S; $x++) {
    $u = $x / $S; $v = $y / $S
    $n = 0.55 * (Sample $L1 4 $u $v) + 0.30 * (Sample $L2 8 $u $v) + 0.15 * (Sample $L3 16 $u $v)
    $n = [Math]::Max(0.0, [Math]::Min(1.0, ($n - 0.25) * 1.6))
    $g = [int](255 * $n)
    $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $g, $g, $g))
  }
}
$bmp.Save("$dir\T_FogNoise.png", [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "wrote T_FogNoise.png"
