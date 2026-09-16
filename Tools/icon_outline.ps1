# Puts a solid green rim around every inventory icon, the way an inspected object is outlined in
# game. Run AFTER Tools/icon_matte.ps1: the silhouette is the icon's alpha, so a weapon's black
# panels are outlined like its bright ones (a brightness test lost them into the backdrop).
#
#   powershell Tools/icon_outline.ps1            (after Tools/render_weapon_icons.py)
#   powershell Tools/icon_outline.ps1 -Thickness 5 -R 60 -G 255 -B 110
#
# Done on the rendered PNG rather than in the shader, because the silhouette is exactly "the
# pixels that are not the black backdrop", which is trivially known here and would need a custom
# depth pass and an outline post-process material to work out in the renderer.
param([int]$Thickness = 9, [int]$R = 64, [int]$G = 232, [int]$B = 104, [int]$Threshold = 30)

Add-Type -AssemblyName System.Drawing
$src = "C:\Dev\Games\RepliCan\RawArt\Icons"
if (-not (Test-Path $src)) { Write-Output "no icons at $src"; exit 1 }

Add-Type -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.IO;

public static class IconOutline
{
    // Adds one background pixel to the flood if it is dark and not already taken.
    static int TryPush(byte[] buf, int stride, int W, int H, bool[] outside, int[] stack, int sp, int x, int y, int threshold)
    {
        int i = y * W + x;
        if (outside[i]) return sp;
        int o = y * stride + x * 4;
        // The matted icon carries the silhouette in its alpha (Tools/icon_matte.ps1), so the
        // subject is what is not transparent -- a black stock included.
        int v = buf[o + 3];
        if (v > threshold) return sp;     // this is the subject: the flood stops here
        outside[i] = true;
        stack[sp++] = i;
        return sp;
    }

    public static void Run(string path, int thickness, int r, int g, int b, int threshold)
    {
        // Loaded through a MemoryStream and copied, NOT with new Bitmap(path): GDI+ keeps the
        // source file open for the lifetime of a Bitmap constructed from a path, so saving back
        // over it throws "a generic error occurred in GDI+" and the icon is left untouched.
        Bitmap bmp;
        using (MemoryStream ms = new MemoryStream(File.ReadAllBytes(path)))
        using (Bitmap loaded = new Bitmap(ms))
        {
            bmp = new Bitmap(loaded);
        }
        using (bmp)
        {
            int W = bmp.Width, H = bmp.Height;
            BitmapData bd = bmp.LockBits(new Rectangle(0, 0, W, H), ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
            int bytes = Math.Abs(bd.Stride) * H;
            byte[] buf = new byte[bytes];
            Marshal.Copy(bd.Scan0, buf, 0, bytes);

            // 1. The silhouette, found by flooding the BACKGROUND inward from the border
            //    rather than by asking whether a pixel is bright. A weapon has black panels on
            //    it, and a brightness test calls those background and fills them with rim colour
            //    in the middle of the gun. Anything the flood cannot reach from the edge of the
            //    frame is part of the subject, however dark it is.
            bool[] outside = new bool[W * H];
            int[] stack = new int[W * H];
            int sp = 0;
            // Marked at PUSH time, not at pop: a pixel reached from two neighbours would
            // otherwise be pushed twice and the stack can then outrun W*H entries.
            for (int x = 0; x < W; x++)
            {
                sp = TryPush(buf, bd.Stride, W, H, outside, stack, sp, x, 0, threshold);
                sp = TryPush(buf, bd.Stride, W, H, outside, stack, sp, x, H - 1, threshold);
            }
            for (int y = 0; y < H; y++)
            {
                sp = TryPush(buf, bd.Stride, W, H, outside, stack, sp, 0, y, threshold);
                sp = TryPush(buf, bd.Stride, W, H, outside, stack, sp, W - 1, y, threshold);
            }
            while (sp > 0)
            {
                int i = stack[--sp];
                int y = i / W, x = i - y * W;
                if (x > 0) sp = TryPush(buf, bd.Stride, W, H, outside, stack, sp, x - 1, y, threshold);
                if (x < W - 1) sp = TryPush(buf, bd.Stride, W, H, outside, stack, sp, x + 1, y, threshold);
                if (y > 0) sp = TryPush(buf, bd.Stride, W, H, outside, stack, sp, x, y - 1, threshold);
                if (y < H - 1) sp = TryPush(buf, bd.Stride, W, H, outside, stack, sp, x, y + 1, threshold);
            }
            bool[] solid = new bool[W * H];
            for (int i = 0; i < solid.Length; i++) solid[i] = !outside[i];

            // 2. Distance to the nearest subject pixel, by two sweeps of a chamfer transform.
            //    Cheaper and smoother than dilating once per pixel of thickness.
            int INF = 1 << 20;
            int[] dist = new int[W * H];
            for (int i = 0; i < dist.Length; i++) dist[i] = solid[i] ? 0 : INF;
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++)
                {
                    int i = y * W + x; int d = dist[i];
                    if (x > 0) d = Math.Min(d, dist[i - 1] + 5);
                    if (y > 0) d = Math.Min(d, dist[i - W] + 5);
                    if (x > 0 && y > 0) d = Math.Min(d, dist[i - W - 1] + 7);
                    if (x < W - 1 && y > 0) d = Math.Min(d, dist[i - W + 1] + 7);
                    dist[i] = d;
                }
            for (int y = H - 1; y >= 0; y--)
                for (int x = W - 1; x >= 0; x--)
                {
                    int i = y * W + x; int d = dist[i];
                    if (x < W - 1) d = Math.Min(d, dist[i + 1] + 5);
                    if (y < H - 1) d = Math.Min(d, dist[i + W] + 5);
                    if (x < W - 1 && y < H - 1) d = Math.Min(d, dist[i + W + 1] + 7);
                    if (x > 0 && y < H - 1) d = Math.Min(d, dist[i + W - 1] + 7);
                    dist[i] = d;
                }

            // 3. The rim: outside the subject, within thickness of it. Solid, not a glow --
            //    a soft halo on a black ground just looks like bloom.
            int reach = thickness * 5;
            for (int y = 0; y < H; y++)
            {
                int row = y * bd.Stride;
                for (int x = 0; x < W; x++)
                {
                    int i = y * W + x;
                    if (!outside[i] || dist[i] > reach) continue;
                    int o = row + x * 4;
                    // One pixel of feather at the outer edge only, so the rim is not stair-stepped.
                    double a = dist[i] > reach - 5 ? (double)(reach - dist[i]) / 5.0 : 1.0;
                    if (a < 0) a = 0; if (a > 1) a = 1;
                    buf[o]     = (byte)(buf[o]     + (b - buf[o])     * a);
                    buf[o + 1] = (byte)(buf[o + 1] + (g - buf[o + 1]) * a);
                    buf[o + 2] = (byte)(buf[o + 2] + (r - buf[o + 2]) * a);
                    buf[o + 3] = (byte)Math.Max(buf[o + 3], (int)(255 * a));   // the rim is opaque
                }
            }
            Marshal.Copy(buf, 0, bd.Scan0, bytes);
            bmp.UnlockBits(bd);
            bmp.Save(path, ImageFormat.Png);
        }
    }
}
'@ -ReferencedAssemblies System.Drawing

$n = 0; $failed = 0
foreach ($f in (Get-ChildItem $src -Filter "T_Icon_*.png" | Where-Object { $_.Name -notmatch "_(black|white)\.png$" })) {
  try { [IconOutline]::Run($f.FullName, $Thickness, $R, $G, $B, $Threshold); $n++ }
  catch { $failed++; Write-Output ("FAILED {0}: {1}" -f $f.Name, $_.Exception.Message) }
}
Write-Output ("outlined {0} icons, {1} failed  (thickness {2}px, rgb {3},{4},{5})" -f $n, $failed, $Thickness, $R, $G, $B)
if ($failed -gt 0) { exit 1 }
