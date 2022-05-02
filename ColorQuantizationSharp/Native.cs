using System.Runtime.InteropServices;
using System.Security;

namespace ColorQuantizationSharp {
    [SuppressUnmanagedCodeSecurity]
    unsafe internal static class Native {
        const string Dll = "ColorQuantization.dll";

        [DllImport(Dll, EntryPoint = "create")]
        public static extern IntPtr Create();

        [DllImport(Dll, EntryPoint = "reset")]
        public static extern IntPtr Reset(IntPtr extractorPtr);

        [DllImport(Dll, EntryPoint = "destroy")]
        public static extern void Destroy(IntPtr extractorPtr);

        [DllImport(Dll, EntryPoint = "add_bitmap")]
        public static extern void AddBitmap(IntPtr extractorPtr, uint* pixels, nint pixelCount);

        [DllImport(Dll, EntryPoint = "add_bitmap")]
        public static extern void AddBitmap(IntPtr extractorPtr, ref uint pixels, nint pixelCount);

        [DllImport(Dll, EntryPoint = "get_color_table")]
        public static extern nint GetColorTable(IntPtr extractorPtr, uint* colorTable, nint tableLength, uint* forceColors, nint forceColorCount);

        [DllImport(Dll, EntryPoint = "get_color_table")]
        public static extern nint GetColorTable(IntPtr extractorPtr, ref uint colorTable, nint tableLength, ref uint forceColors, nint forceColorCount);

        // ========== palette ==========
        [DllImport(Dll, EntryPoint = "palette_create")]
        public static extern IntPtr PaletteCreate(uint* colorTable, nint tableLength, bool optimize);

        [DllImport(Dll, EntryPoint = "palette_create")]
        public static extern IntPtr PaletteCreate(ref uint colorTable, nint tableLength, bool optimize);

        [DllImport(Dll, EntryPoint = "palette_destroy")]
        public static extern void PaletteDestroy(IntPtr palettePtr);

        [DllImport(Dll, EntryPoint = "palette_map")]
        public static extern void PaletteMap(IntPtr palettePtr, uint* pixels, byte* indexes, nint length);

        [DllImport(Dll, EntryPoint = "palette_map")]
        public static extern void PaletteMap(IntPtr palettePtr, ref uint pixels, ref byte indexes, nint length);

        [DllImport(Dll, EntryPoint = "palette_dither")]
        public static extern void PaletteDither(IntPtr palettePtr, uint* pixels, byte* indexes, nint width, nint height);

        [DllImport(Dll, EntryPoint = "palette_dither")]
        public static extern void PaletteDither(IntPtr palettePtr, ref uint pixels, ref byte indexes, nint width, nint height);

        [DllImport(Dll, EntryPoint = "palette_color_table")]
        public static extern uint* PaletteColorTable(IntPtr palettePtr, out nint tableLength);
    }
}