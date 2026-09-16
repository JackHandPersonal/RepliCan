# Bullet-hole decal textures: three 128 px variants in RawArt (T_BulletHole_01..03.png), RGBA.
# A hole, not a scuff: a near-black ragged core, a darker ring of torn metal round it, a
# lighter chipped lip outside that, lit from the upper left so the lip catches light and the
# core's lower right is in shadow, and transparent beyond. Import with Tools/make_bullet_holes.py.
#   powershell Tools/make_bullet_hole_textures.ps1
$dir = "C:\Dev\Games\RepliCan\RawArt"
Add-Type -AssemblyName System.Drawing
$N = 128
for ($v = 1; $v -le 3; $v++) {
  $rand = New-Object System.Random ($v * 13)
  $bmp = New-Object System.Drawing.Bitmap $N, $N, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  # ragged radius: a base radius modulated by a few sines with random phase (in units of the half-size)
  $k = @(); for ($i = 0; $i -lt 5; $i++) { $k += @(($rand.NextDouble() * 6.28), (0.012 + $rand.NextDouble() * 0.025), (4 + $i * 2)) }
  function rag($ang) { $r = 1.0; for ($i = 0; $i -lt 5; $i++) { $r += $k[$i*3+1] * [Math]::Sin($k[$i*3+2] * $ang + $k[$i*3]) }; return $r }
  for ($y = 0; $y -lt $N; $y++) { for ($x = 0; $x -lt $N; $x++) {
    $dx = ($x + 0.5) / $N - 0.5; $dy = ($y + 0.5) / $N - 0.5
    $d = [Math]::Sqrt($dx*$dx + $dy*$dy) * 2.0          # 0 at centre, 1 at the edge
    $ang = [Math]::Atan2($dy, $dx)
    $rg = rag $ang
    $core = 0.30 * $rg; $ring = 0.46 * $rg; $lip = 0.62 * $rg
    # light from the upper left: positive where the surface faces it
    $lit = (-$dx - $dy) / ([Math]::Max(0.001, [Math]::Sqrt($dx*$dx + $dy*$dy)))
    if ($d -lt $core) {
      $g = 0.02 + 0.03 * [Math]::Max(0, $lit)           # the hole: black, a hint of light on the near wall
      $a = 1.0
    } elseif ($d -lt $ring) {
      $t = ($d - $core) / ($ring - $core)
      $g = 0.06 + 0.10 * $t - 0.05 * $lit               # torn metal, in shadow on the lit side (it slopes away)
      $a = 1.0
    } elseif ($d -lt $lip) {
      $t = ($d - $ring) / ($lip - $ring)
      $g = 0.42 + 0.30 * [Math]::Max(0, $lit) - 0.12 * [Math]::Max(0, -$lit)   # the chipped lip catches the light
      $g = $g * (1.0 - 0.35 * $t)
      $a = 1.0 - 0.75 * $t                               # feathers out into the wall
    } else { $g = 0; $a = 0 }
    # a little grain so no ring reads as a perfect circle
    $g = [Math]::Max(0.0, [Math]::Min(1.0, $g + ($rand.NextDouble() - 0.5) * 0.06))
    $c = [int](255 * $g); $ai = [int](255 * [Math]::Max(0.0, [Math]::Min(1.0, $a)))
    $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($ai, $c, [int]($c * 0.96), [int]($c * 0.9)))
  } }
  $bmp.Save("$dir\T_BulletHole_0$v.png", [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
  Write-Output "wrote T_BulletHole_0$v.png"
}
