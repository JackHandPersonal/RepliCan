# Reads the swatches from Tools/soldier_palette_swatches.py and reports which alternate palette
# makes the soldier red. Hue is measured over the lit pixels only, so the black backdrop does not
# drag the average, and saturation is reported alongside because a desaturated near-red is not
# what "the red variation" means.
Add-Type -AssemblyName System.Drawing
$dir = "C:\Dev\Games\RepliCan\RawArt\Palette"
if (-not (Test-Path $dir)) { Write-Output "no swatches: run Tools/soldier_palette_swatches.py first"; exit 1 }

$rows = @()
foreach ($f in (Get-ChildItem $dir -Filter "*.png" | Sort-Object Name)) {
  $bmp = New-Object System.Drawing.Bitmap $f.FullName
  $sumX = 0.0; $sumY = 0.0; $sat = 0.0; $val = 0.0; $n = 0
  for ($y = 0; $y -lt $bmp.Height; $y += 2) {
    for ($x = 0; $x -lt $bmp.Width; $x += 2) {
      $p = $bmp.GetPixel($x, $y)
      $mx = [Math]::Max($p.R, [Math]::Max($p.G, $p.B)); $mn = [Math]::Min($p.R, [Math]::Min($p.G, $p.B))
      if ($mx -le 45) { continue }
      $s = ($mx - $mn) / $mx
      if ($s -lt 0.18) { continue }          # grey armour panels carry no hue worth averaging
      $h = $p.GetHue() * [Math]::PI / 180.0
      # Hue is circular, so it is averaged as a vector rather than as a number.
      $sumX += [Math]::Cos($h); $sumY += [Math]::Sin($h)
      $sat += $s; $val += $mx; $n++
    }
  }
  $bmp.Dispose()
  if ($n -lt 50) { $rows += [pscustomobject]@{ Name = $f.BaseName; Hue = -1; Sat = 0; Redness = 0 }; continue }
  $hue = [Math]::Atan2($sumY / $n, $sumX / $n) * 180.0 / [Math]::PI
  if ($hue -lt 0) { $hue += 360 }
  # Redness: how close the average hue is to red, weighted by how saturated it actually is.
  $dist = [Math]::Min([Math]::Abs($hue), 360 - [Math]::Abs($hue))
  $redness = [Math]::Max(0.0, 1.0 - $dist / 45.0) * ($sat / $n)
  $rows += [pscustomobject]@{ Name = $f.BaseName; Hue = [math]::Round($hue, 0); Sat = [math]::Round($sat / $n, 2); Redness = [math]::Round($redness, 3) }
}

$rows | Sort-Object -Property Redness -Descending | Format-Table -AutoSize
$best = ($rows | Sort-Object -Property Redness -Descending | Select-Object -First 1)
Write-Output ("REDDEST: {0}  (hue {1}, saturation {2})" -f $best.Name, $best.Hue, $best.Sat)
