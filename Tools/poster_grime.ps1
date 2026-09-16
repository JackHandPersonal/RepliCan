# Subtle dirt on every billboard. Runs AFTER the poster generators, over their *_Upright.png
# output, and writes both the upright review copy and the quarter-turned texture the board
# planes use. Re-running a generator wipes the grime, so run this again afterwards.
#
#   powershell Tools/poster_grime.ps1
#   python Tools/ue_remote.py --file <scratch>/import_posters.py
#
# Four things, all gentle. A poster on a wall for years is not evenly dirty: it collects blotches
# where damp sat, runs where water came down it, a general grey haze, and wear at the edges where
# people and equipment brushed past. Done in one compiled pass because a per-pixel PowerShell
# loop over a 1700 x 1050 image takes minutes.
param([double]$Strength = 1.0)

Add-Type -AssemblyName System.Drawing
$dir = "C:\Dev\Games\RepliCan\RawArt"

Add-Type -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class PosterGrime
{
    // Value noise: a lattice of random values with smooth interpolation between them. Cheap,
    // and the octaves give the blotching a natural range of sizes.
    static double[,] Lattice(int w, int h, Random rnd)
    {
        double[,] g = new double[w + 1, h + 1];
        for (int y = 0; y <= h; y++)
            for (int x = 0; x <= w; x++)
                g[x, y] = rnd.NextDouble();
        return g;
    }

    static double Sample(double[,] g, int gw, int gh, double u, double v)
    {
        double fx = u * gw, fy = v * gh;
        int x0 = (int)fx, y0 = (int)fy;
        if (x0 >= gw) x0 = gw - 1;
        if (y0 >= gh) y0 = gh - 1;
        double tx = fx - x0, ty = fy - y0;
        tx = tx * tx * (3 - 2 * tx);           // smoothstep, or the lattice shows as squares
        ty = ty * ty * (3 - 2 * ty);
        double a = g[x0, y0] * (1 - tx) + g[x0 + 1, y0] * tx;
        double b = g[x0, y0 + 1] * (1 - tx) + g[x0 + 1, y0 + 1] * tx;
        return a * (1 - ty) + b * ty;
    }

    public static void Run(string src, string dst, int seed, double strength)
    {
        using (Bitmap bmp = new Bitmap(src))
        {
            int W = bmp.Width, H = bmp.Height;
            Random rnd = new Random(seed);
            // Three octaves of blotching, coarse to fine.
            int[] sizes = { 5, 13, 31 };
            double[] weights = { 0.55, 0.30, 0.15 };
            double[][,] octaves = new double[sizes.Length][,];
            for (int i = 0; i < sizes.Length; i++) octaves[i] = Lattice(sizes[i], sizes[i], rnd);

            // Vertical runs: a handful of columns where something ran down the board.
            int runCount = 14;
            double[] runX = new double[runCount];
            double[] runW = new double[runCount];
            double[] runTop = new double[runCount];
            double[] runLen = new double[runCount];
            for (int i = 0; i < runCount; i++)
            {
                runX[i] = rnd.NextDouble();
                runW[i] = 0.004 + rnd.NextDouble() * 0.012;
                runTop[i] = rnd.NextDouble() * 0.5;
                runLen[i] = 0.25 + rnd.NextDouble() * 0.6;
            }

            BitmapData bd = bmp.LockBits(new Rectangle(0, 0, W, H), ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
            int bytes = Math.Abs(bd.Stride) * H;
            byte[] buf = new byte[bytes];
            Marshal.Copy(bd.Scan0, buf, 0, bytes);

            for (int y = 0; y < H; y++)
            {
                double v = (double)y / H;
                int row = y * bd.Stride;
                for (int x = 0; x < W; x++)
                {
                    double u = (double)x / W;
                    double n = 0;
                    for (int i = 0; i < sizes.Length; i++) n += weights[i] * Sample(octaves[i], sizes[i], sizes[i], u, v);

                    // Blotches: only the darker half of the noise dirties anything, so most of
                    // the board is left alone and the dirt reads as patches rather than a wash.
                    double blotch = Math.Max(0.0, n - 0.52) * 1.9;

                    // Runs, fading out as they go down.
                    double run = 0;
                    for (int i = 0; i < runCount; i++)
                    {
                        double dx = Math.Abs(u - runX[i]);
                        if (dx > runW[i]) continue;
                        if (v < runTop[i] || v > runTop[i] + runLen[i]) continue;
                        double across = 1.0 - dx / runW[i];
                        double along = 1.0 - (v - runTop[i]) / runLen[i];
                        run = Math.Max(run, across * across * along * 0.5);
                    }

                    // Edge wear: strongest in the corners.
                    double ex = Math.Max(0.0, 1.0 - Math.Min(u, 1 - u) / 0.14);
                    double ey = Math.Max(0.0, 1.0 - Math.Min(v, 1 - v) / 0.14);
                    double edge = Math.Max(ex, ey);
                    edge = edge * edge * 0.42;

                    double dirt = (blotch * 0.55 + run + edge) * strength;
                    if (dirt > 0.75) dirt = 0.75;

                    int i0 = row + x * 4;
                    double b = buf[i0], g = buf[i0 + 1], r = buf[i0 + 2];

                    // Dirt darkens and desaturates toward a grey-brown, rather than just
                    // painting brown over the art, so the poster still reads underneath it.
                    double lum = 0.299 * r + 0.587 * g + 0.114 * b;
                    double tr = lum * 0.58 + 26, tg = lum * 0.56 + 24, tb = lum * 0.52 + 21;
                    r = r + (tr - r) * dirt;
                    g = g + (tg - g) * dirt;
                    b = b + (tb - b) * dirt;

                    // A fine speckle of grit over everything, very slight.
                    double sp = (rnd.NextDouble() - 0.5) * 16.0 * strength;
                    r += sp; g += sp; b += sp;

                    buf[i0] = (byte)(b < 0 ? 0 : b > 255 ? 255 : b);
                    buf[i0 + 1] = (byte)(g < 0 ? 0 : g > 255 ? 255 : g);
                    buf[i0 + 2] = (byte)(r < 0 ? 0 : r > 255 ? 255 : r);
                }
            }
            Marshal.Copy(buf, 0, bd.Scan0, bytes);
            bmp.UnlockBits(bd);
            bmp.Save(dst, ImageFormat.Png);
        }
    }
}
'@ -ReferencedAssemblies System.Drawing

$sources = Get-ChildItem $dir -Filter "T_Poster_*_Upright.png"
if ($sources.Count -eq 0) { Write-Output "no T_Poster_*_Upright.png in $dir"; exit 1 }

$seed = 1
foreach ($f in $sources) {
  $base = $f.BaseName -replace '_Upright$', ''
  $clean = Join-Path $dir ("{0}_Clean.png" -f $base)
  # Keep an untouched copy the first time, so re-running does not pile dirt on dirt.
  if (-not (Test-Path $clean)) { Copy-Item $f.FullName $clean }
  $out = Join-Path $dir ("{0}_Upright.png" -f $base)
  [PosterGrime]::Run($clean, $out, $seed, $Strength)
  # The board planes are pitched, so the texture is stored quarter-turned, same as the generators.
  $bmp = New-Object System.Drawing.Bitmap $out
  $bmp.RotateFlip([System.Drawing.RotateFlipType]::Rotate270FlipNone)
  $bmp.Save((Join-Path $dir ("{0}.png" -f $base)), [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Output ("grimed {0}" -f $base)
  $seed += 7
}
