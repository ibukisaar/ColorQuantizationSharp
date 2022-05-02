using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using ColorQuantizationSharp;

unsafe {
    const string OutDir = @"Z:";

    string filename = @"E:\H\图片\84780582_p0.jpg";
    var shortFilename = Path.GetFileNameWithoutExtension(filename);

    var origBitmap = new Bitmap(filename);
    origBitmap = new Bitmap(origBitmap, origBitmap.Size);


    using var extractor = new SpaceShockColorExtractor();

    foreach (int tableLength in new int[] { 4, 8, 16, 32, 64, 128, 256 }) {
        extractor.Reset();

        var origData = origBitmap.LockBits(new Rectangle(0, 0, origBitmap.Width, origBitmap.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppRgb);
        var origPixels = new ReadOnlySpan<uint>((void*)origData.Scan0, origBitmap.Width * origBitmap.Height);
        extractor.AddBitmap(origPixels);
        origBitmap.UnlockBits(origData);

        uint[] colorTable = extractor.GetColorTable(tableLength);

        using var palette = new Palette(colorTable);

        using (var indexedBitmap = PaletteMap(origBitmap, palette)) {
            Save(origBitmap, indexedBitmap, colorTable, Path.Combine(OutDir, $"{shortFilename}-map-{tableLength}.png"));
        }

        using (var indexedBitmap = PaletteDither(origBitmap, palette)) {
            Save(origBitmap, indexedBitmap, colorTable, Path.Combine(OutDir, $"{shortFilename}-dither-{tableLength}.png"));
        }
    }

    /// =======================================================================================


    
    static Bitmap PaletteMap(Bitmap bitmap, Palette palette) {
        Bitmap cloneBitmap = (Bitmap)bitmap.Clone();
        var data = cloneBitmap.LockBits(new Rectangle(0, 0, cloneBitmap.Width, cloneBitmap.Height), ImageLockMode.ReadWrite, PixelFormat.Format32bppRgb);
        var pixels = new Span<uint>((void*)data.Scan0, data.Width * data.Height);

        byte[] indexes = palette.Map(pixels);

        ref uint palettePtr = ref MemoryMarshal.GetReference(palette.ColorTable);
        ref byte indexPtr = ref MemoryMarshal.GetArrayDataReference(indexes);

        for (int i = 0; i < pixels.Length; i++) {
            pixels[i] = Unsafe.Add(ref palettePtr, Unsafe.Add(ref indexPtr, i));
        }

        cloneBitmap.UnlockBits(data);
        return cloneBitmap;
    }

    static Bitmap PaletteDither(Bitmap bitmap, Palette palette) {
        Bitmap cloneBitmap = (Bitmap)bitmap.Clone();
        var data = cloneBitmap.LockBits(new Rectangle(0, 0, cloneBitmap.Width, cloneBitmap.Height), ImageLockMode.ReadWrite, PixelFormat.Format32bppRgb);
        var pixels = new Span<uint>((void*)data.Scan0, data.Width * data.Height);

        palette.Dither(pixels, data.Width, data.Height);

        cloneBitmap.UnlockBits(data);
        return cloneBitmap;
    }

    static void Save(Bitmap origBitmap, Bitmap indexedBitmap, ReadOnlySpan<uint> colorTable, string filename) {
        int numCols = 16;
        int numRows = (colorTable.Length + (numCols - 1)) / numCols;
        const int size = 50;
        const int margin = 5;
        using Bitmap paletteBitmap = new Bitmap(margin + (size + margin) * numCols, margin + (size + margin) * numRows);

        using (var g = Graphics.FromImage(paletteBitmap)) {
            int i = 0;
            for (int row = 0; row < numRows; row++) {
                for (int col = 0; col < numCols && i < colorTable.Length; col++, i++) {
                    g.FillRectangle(new SolidBrush(Color.FromArgb((int)(colorTable[i] | 0xff000000u))), new Rectangle(margin + (size + margin) * col, margin + (size + margin) * row, size, size));
                }
            }
        }

        int finalWidth = origBitmap.Width + indexedBitmap.Width + paletteBitmap.Width;
        int finalHeight = Math.Max(Math.Max(origBitmap.Height, indexedBitmap.Height), paletteBitmap.Height);
        using Bitmap finalBitmap = new Bitmap(finalWidth, finalHeight);
        using (var g = Graphics.FromImage(finalBitmap)) {
            g.DrawImageUnscaled(origBitmap, new Point());
            g.DrawImageUnscaled(indexedBitmap, new Point(origBitmap.Width, 0));
            g.DrawImageUnscaled(paletteBitmap, new Point(origBitmap.Width + indexedBitmap.Width, 0));
        }

        finalBitmap.Save(filename, ImageFormat.Png);
        Console.WriteLine($"save: {filename}");
    }
}