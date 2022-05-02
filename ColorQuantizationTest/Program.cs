using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using ColorQuantizationSharp;

const string BitmapFilename = @"Z:\yande.re 96993 akino_momiji cuffs gayarou loli naked nipples pussy_juice sakura_musubi wallpaper.png";
uint[] forceColors = { 0x000000, 0xffffff, 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0xff00ff, 0x00ffff };

using (var bitmap = new Bitmap(BitmapFilename)) {
    PaletteMap(bitmap, 8, forceColors);
    bitmap.Save(@"Z:\map.png");
}

using (var bitmap = new Bitmap(BitmapFilename)) {
    PaletteDither(bitmap, 8, forceColors);
    bitmap.Save(@"Z:\dither.png");
}



unsafe static (uint[] ColorTable, byte[] Indexes) PaletteMap(Bitmap bitmap, int colorCount = 256, ReadOnlySpan<uint> forceColors = default) {
    using var extractor = new SpaceShockColorExtractor();
    var data = bitmap.LockBits(new Rectangle(0, 0, bitmap.Width, bitmap.Height), ImageLockMode.ReadWrite, PixelFormat.Format32bppRgb);
    try {
        var pixels = new ReadOnlySpan<uint>((void*)data.Scan0, data.Width * data.Height);
        extractor.AddBitmap(pixels);
        uint[] colorTable = extractor.GetColorTable(colorCount, forceColors);
        using var palette = new Palette(colorTable, optimize: true);
        byte[] indexes = palette.Map(pixels);

        ref uint palettePtr = ref MemoryMarshal.GetArrayDataReference(colorTable);
        ref byte indexPtr = ref MemoryMarshal.GetArrayDataReference(indexes);
        uint* p = (uint*)data.Scan0;
        nint bitmapLength = bitmap.Width * bitmap.Height;

        for (nint i = 0; i < bitmapLength; i++) {
            p[i] = Unsafe.Add(ref palettePtr, Unsafe.Add(ref indexPtr, i));
        }

        return (colorTable, indexes);
    } finally {
        bitmap.UnlockBits(data);
    }
}

unsafe static (uint[] ColorTable, byte[] Indexes) PaletteDither(Bitmap bitmap, int colorCount = 256, ReadOnlySpan<uint> forceColors = default) {
    using var extractor = new SpaceShockColorExtractor();
    var data = bitmap.LockBits(new Rectangle(0, 0, bitmap.Width, bitmap.Height), ImageLockMode.ReadWrite, PixelFormat.Format32bppRgb);
    try {
        var pixels = new Span<uint>((void*)data.Scan0, data.Width * data.Height);
        extractor.AddBitmap(pixels);
        uint[] colorTable = extractor.GetColorTable(colorCount, forceColors);
        using var palette = new Palette(colorTable, optimize: true);
        byte[] indexes = palette.Dither(pixels, data.Width, data.Height);
        return (colorTable, indexes);
    } finally {
        bitmap.UnlockBits(data);
    }
}