# Turns each RawArt/Icons/<name>_black.png + <name>_white.png pair (the same shot over unlit
# black and unlit white backdrops) into <name>.png with a real alpha channel:
#   alpha = 1 - (white - black) / (Wc - Bc)   with Wc / Bc the backdrop levels read from the
#                                            shot's corners (exposure and the tone curve never
#                                            leave them at a true 255 / 0)
#   color = (black - Bc * (1 - alpha)) / alpha   (un-premultiply the over-black shot, less the
#                                               backdrop's bleed through the edge pixels)
# Then trims to the item's bounding box plus a margin and resizes to $Out px square.
# The per-pixel work is compiled C# (a PowerShell loop over a megapixel takes minutes).
param([int]$Out = 256, [int]$Margin = 6)
$dir = "C:\Dev\Games\RepliCan\RawArt\Icons"
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
public static class IconMatte
{
    // Both inputs and the output are 32bpp ARGB rows (B,G,R,A in memory). Returns
    // { minx, miny, maxx, maxy } over the pixels that are at least half opaque.
    public static int[] Run(byte[] black, byte[] white, byte[] dst, int w, int h, int stride)
    {
        double bc = 0.0, wc = 0.0;
        int[,] corners = { { 2, 2 }, { w - 3, 2 }, { 2, h - 3 }, { w - 3, h - 3 } };
        for (int c = 0; c < 4; c++)
        {
            int i = corners[c, 1] * stride + corners[c, 0] * 4;
            bc += (black[i] + black[i + 1] + black[i + 2]) / 3.0;
            wc += (white[i] + white[i + 1] + white[i + 2]) / 3.0;
        }
        bc /= 4.0; wc /= 4.0;
        double span = Math.Max(1.0, wc - bc);
        int minx = w, miny = h, maxx = -1, maxy = -1;
        for (int y = 0; y < h; y++)
        {
            int row = y * stride;
            for (int x = 0; x < w; x++)
            {
                int i = row + x * 4;
                int bB = black[i], bG = black[i + 1], bR = black[i + 2];
                double d = (((white[i] - bB) + (white[i + 1] - bG) + (white[i + 2] - bR)) / 3.0) / span;
                double a = 1.0 - d;
                if (a < 0.0) a = 0.0; else if (a > 1.0) a = 1.0;
                if (a < 0.04) continue;                       // stays fully transparent
                double bleed = bc * (1.0 - a);
                int r = (int)((bR - bleed) / a), g = (int)((bG - bleed) / a), bl = (int)((bB - bleed) / a);
                dst[i] = (byte)(bl < 0 ? 0 : bl > 255 ? 255 : bl);
                dst[i + 1] = (byte)(g < 0 ? 0 : g > 255 ? 255 : g);
                dst[i + 2] = (byte)(r < 0 ? 0 : r > 255 ? 255 : r);
                dst[i + 3] = (byte)(255.0 * a);
                if (a > 0.5)
                {
                    if (x < minx) minx = x;
                    if (x > maxx) maxx = x;
                    if (y < miny) miny = y;
                    if (y > maxy) maxy = y;
                }
            }
        }
        return new int[] { minx, miny, maxx, maxy, (int)bc, (int)wc };
    }
}
'@
function Load32($path) {
  $src = New-Object System.Drawing.Bitmap $path
  $bmp = New-Object System.Drawing.Bitmap $src.Width, $src.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.DrawImage($src, 0, 0, $src.Width, $src.Height); $g.Dispose(); $src.Dispose()
  return $bmp
}
function Bytes($bmp) {
  $rect = New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
  $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $bytes = New-Object byte[] ($data.Stride * $bmp.Height)
  [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
  $bmp.UnlockBits($data)
  return ,$bytes
}
Get-ChildItem "$dir\*_black.png" | ForEach-Object {
  $name = $_.Name -replace '_black\.png$', ''
  $wpath = "$dir\$name`_white.png"; if (-not (Test-Path $wpath)) { return }
  $b = Load32 $_.FullName; $w = Load32 $wpath
  $wid = $b.Width; $hgt = $b.Height; $stride = $wid * 4
  $pb = Bytes $b; $pw = Bytes $w
  $dstBytes = New-Object byte[] ($stride * $hgt)
  $r = [IconMatte]::Run($pb, $pw, $dstBytes, $wid, $hgt, $stride)
  $minx = $r[0]; $miny = $r[1]; $maxx = $r[2]; $maxy = $r[3]
  if ($maxx -lt 0) { Write-Output "$name : empty"; $b.Dispose(); $w.Dispose(); return }
  $o = New-Object System.Drawing.Bitmap $wid, $hgt, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $rect = New-Object System.Drawing.Rectangle 0, 0, $wid, $hgt
  $od = $o.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  [System.Runtime.InteropServices.Marshal]::Copy($dstBytes, 0, $od.Scan0, $dstBytes.Length)
  $o.UnlockBits($od)
  # square crop around the item, then resize to the icon size
  $cw = $maxx - $minx + 1; $ch = $maxy - $miny + 1; $side = [Math]::Max($cw, $ch) + 2 * $Margin * [Math]::Max($cw, $ch) / 100.0
  $side = [int][Math]::Min([Math]::Max($side, 8), [Math]::Max($wid, $hgt))
  $cx = ($minx + $maxx) / 2.0; $cy = ($miny + $maxy) / 2.0
  $sx = [int][Math]::Max(0, [Math]::Min($wid - $side, $cx - $side / 2.0)); $sy = [int][Math]::Max(0, [Math]::Min($hgt - $side, $cy - $side / 2.0))
  $crop = $o.Clone((New-Object System.Drawing.Rectangle $sx, $sy, $side, $side), $o.PixelFormat)
  $icon = New-Object System.Drawing.Bitmap $Out, $Out, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g2 = [System.Drawing.Graphics]::FromImage($icon); $g2.InterpolationMode = 'HighQualityBicubic'; $g2.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
  $g2.DrawImage($crop, 0, 0, $Out, $Out); $g2.Dispose()
  $icon.Save("$dir\$name.png", [System.Drawing.Imaging.ImageFormat]::Png)
  $b.Dispose(); $w.Dispose(); $o.Dispose(); $crop.Dispose(); $icon.Dispose()
  Write-Output ("{0} : {1} x {2} item px, backdrops {3}/{4} -> {5} px icon" -f $name, $cw, $ch, $r[4], $r[5], $Out)
}
