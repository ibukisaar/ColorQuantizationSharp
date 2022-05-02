using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace ColorQuantizationSharp {
    unsafe public class SpaceShockColorExtractor : IDisposable {
        private IntPtr ptr;

        public SpaceShockColorExtractor() {
            ptr = Native.Create();
        }

        /// <summary>
        /// 将图像添加进<see cref="SpaceShockColorExtractor"/>对象中。
        /// <para>此方法允许被多次调用来反复添加图像，但需要考虑溢出，内部计数类型为uint32。</para>
        /// </summary>
        /// <param name="pixels"></param>
        public void AddBitmap(ReadOnlySpan<uint> pixels) {
            Native.AddBitmap(ptr, ref MemoryMarshal.GetReference(pixels), pixels.Length);
        }

        /// <summary>
        /// 获得调色板颜色表。
        /// <para>注意：此方法具有副作用，如需复用对象请先调用<see cref="Reset"/>方法。</para>
        /// </summary>
        /// <param name="colorTable"></param>
        /// <param name="forceColors">强制颜色表，调色板中一定会出现这些颜色。</param>
        /// <returns></returns>
        /// <exception cref="ArgumentOutOfRangeException"></exception>
        public int GetColorTable(Span<uint> colorTable, ReadOnlySpan<uint> forceColors = default) {
            int tableLength = (int)Native.GetColorTable(ptr, ref MemoryMarshal.GetReference(colorTable), colorTable.Length, ref MemoryMarshal.GetReference(forceColors), forceColors.Length);
            if (tableLength < 0) throw new ArgumentOutOfRangeException(nameof(colorTable), "颜色表空间不能小于强制颜色表的大小");
            return tableLength;
        }

        /// <summary>
        /// 获得调色板颜色表。
        /// <para>注意：此方法具有副作用，如需复用对象请先调用<see cref="Reset"/>方法。</para>
        /// </summary>
        /// <param name="tableLength"></param>
        /// <param name="forceColors">强制颜色表，调色板中一定会出现这些颜色。</param>
        /// <returns></returns>
        /// <exception cref="ArgumentOutOfRangeException"></exception>
        public uint[] GetColorTable(int tableLength, ReadOnlySpan<uint> forceColors = default) {
            uint* colorTable = stackalloc uint[tableLength];
            tableLength = (int)Native.GetColorTable(ptr, ref Unsafe.AsRef<uint>(colorTable), tableLength, ref MemoryMarshal.GetReference(forceColors), forceColors.Length);
            if (tableLength < 0) throw new ArgumentOutOfRangeException(nameof(tableLength), "不能小于强制颜色表的大小");
            return new Span<uint>(colorTable, tableLength).ToArray();
        }

        /// <summary>
        /// 将内部所有缓存清零。
        /// <para>需要使用<see cref="AddBitmap"/>重新添加图像才可以再获取调色板颜色表。</para>
        /// </summary>
        public void Reset() {
            ptr = Native.Reset(ptr);
        }

        protected virtual void Dispose(bool disposing) {
            if (ptr != IntPtr.Zero) {
                Native.Destroy(ptr);
                ptr = IntPtr.Zero;
            }
        }

        ~SpaceShockColorExtractor() {
            Dispose(disposing: false);
        }

        public void Dispose() {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }
    }
}
