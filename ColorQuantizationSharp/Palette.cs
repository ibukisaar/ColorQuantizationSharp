using System;
using System.Runtime.InteropServices;

namespace ColorQuantizationSharp {
    public class Palette : IDisposable {
        private IntPtr ptr;

        unsafe public ReadOnlySpan<uint> ColorTable {
            get {
                var table = Native.PaletteColorTable(ptr, out nint count);
                return new ReadOnlySpan<uint>(table, (int)count);
            }
        }

        /// <summary>
        /// 构造一个调色板。
        /// <para>如果可以，请尽量复用对象以提升性能，调色板内有缓存，使用次数越多性能越高。</para>
        /// </summary>
        /// <param name="colorTable">调色板颜色表</param>
        /// <param name="optimize">如果该参数为true，则优化该调色板，查找颜色索引时只需要比较少量颜色，但会增加构造时间</param>
        /// <exception cref="ArgumentOutOfRangeException"></exception>
        public Palette(ReadOnlySpan<uint> colorTable, bool optimize = true) {
            if (colorTable.IsEmpty) throw new ArgumentOutOfRangeException(nameof(colorTable), "颜色表不能为空");

            ptr = Native.PaletteCreate(ref MemoryMarshal.GetReference(colorTable), colorTable.Length, optimize);
            if (ptr == IntPtr.Zero) throw new ArgumentOutOfRangeException(nameof(colorTable), "颜色表大小不能超过256");
        }

        /// <summary>
        /// 将每个像素映射到距离最近的颜色索引
        /// </summary>
        /// <param name="pixels"></param>
        /// <param name="outIndexes"></param>
        /// <exception cref="ArgumentOutOfRangeException"></exception>
        public void Map(ReadOnlySpan<uint> pixels, Span<byte> outIndexes) {
            if (outIndexes.Length < pixels.Length) throw new ArgumentOutOfRangeException(nameof(outIndexes), "存放索引的缓冲区太小");

            Native.PaletteMap(ptr, ref MemoryMarshal.GetReference(pixels), ref MemoryMarshal.GetReference(outIndexes), pixels.Length);
        }

        /// <summary>
        /// 将每个像素映射到距离最近的颜色索引
        /// </summary>
        /// <param name="pixels"></param>
        /// <returns></returns>
        public byte[] Map(ReadOnlySpan<uint> pixels) {
            byte[] indexes = GC.AllocateUninitializedArray<byte>(pixels.Length);
            Native.PaletteMap(ptr, ref MemoryMarshal.GetReference(pixels), ref MemoryMarshal.GetArrayDataReference(indexes), pixels.Length);
            return indexes;
        }

        /// <summary>
        /// 将每个像素映射到距离最近的颜色索引
        /// </summary>
        /// <param name="pixels"></param>
        /// <param name="width"></param>
        /// <param name="height"></param>
        /// <returns></returns>
        /// <exception cref="ArgumentOutOfRangeException"></exception>
        public byte[] Map(ReadOnlySpan<uint> pixels, int width, int height) {
            if (width <= 0) throw new ArgumentOutOfRangeException(nameof(width));
            if (height <= 0) throw new ArgumentOutOfRangeException(nameof(height));
            if (width * height > pixels.Length) throw new ArgumentOutOfRangeException(nameof(pixels));

            return Map(pixels);
        }

        /// <summary>
        /// 使用抖动处理来获得颜色索引
        /// </summary>
        /// <param name="pixels"></param>
        /// <param name="indexes"></param>
        /// <param name="width"></param>
        /// <param name="height"></param>
        /// <exception cref="ArgumentOutOfRangeException"></exception>
        public void Dither(Span<uint> pixels, Span<byte> indexes, int width, int height) {
            if (width <= 0) throw new ArgumentOutOfRangeException(nameof(width));
            if (height <= 0) throw new ArgumentOutOfRangeException(nameof(height));
            if (width * height > pixels.Length) throw new ArgumentOutOfRangeException(nameof(pixels));
            if (width * height > indexes.Length) throw new ArgumentOutOfRangeException(nameof(indexes));

            Native.PaletteDither(ptr, ref MemoryMarshal.GetReference(pixels), ref MemoryMarshal.GetReference(indexes), width, height);
        }

        /// <summary>
        /// 使用抖动处理来获得颜色索引
        /// </summary>
        /// <param name="pixels"></param>
        /// <param name="width"></param>
        /// <param name="height"></param>
        /// <returns></returns>
        /// <exception cref="ArgumentOutOfRangeException"></exception>
        public byte[] Dither(Span<uint> pixels, int width, int height) {
            if (width <= 0) throw new ArgumentOutOfRangeException(nameof(width));
            if (height <= 0) throw new ArgumentOutOfRangeException(nameof(height));
            if (width * height > pixels.Length) throw new ArgumentOutOfRangeException(nameof(pixels));

            byte[] indexes = GC.AllocateUninitializedArray<byte>(width * height);
            Native.PaletteDither(ptr, ref MemoryMarshal.GetReference(pixels), ref MemoryMarshal.GetArrayDataReference(indexes), width, height);
            return indexes;
        }

        protected virtual void Dispose(bool disposing) {
            if (ptr != IntPtr.Zero) {
                Native.PaletteDestroy(ptr);
                ptr = IntPtr.Zero;
            }
        }

        ~Palette() {
            Dispose(disposing: false);
        }

        public void Dispose() {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }
    }
}
