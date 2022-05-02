#include <cinttypes>
#include <cstdlib>
#include <cstddef>
#include <vector>
#include <array>
#include <iterator>
#include <algorithm>
#include <cmath>
#include <limits>
#include "default_init_allocator.h"

#define EXPORT_API extern "C" __declspec(dllexport)

#if __clang__
#define LIKELY [[likely]]
#else
#define LIKELY
#endif

using byte = uint8_t;
using byte_vector = std::vector<byte>;
using color_t = uint32_t;

constexpr size_t N = 16;
constexpr size_t CubeSize = 256 / N;
constexpr size_t CubeCount = N * N * N;

static constexpr int square_sum(int x, int y, int z) { return x * x + y * y + z * z; }

static constexpr int square_sum(int x, int y) { return x * x + y * y; }

static constexpr int square_sum(int x) { return x * x; }

static constexpr int color_distance(int r1, int g1, int b1, int r2, int g2, int b2) {
    int dr = r1 - r2;
    int dg = g1 - g2;
    int db = b1 - b2;
    return square_sum(dr, dg, db);
}

struct ListHead {
    uint32_t count, index;
};


struct Palette {
    const std::vector<color_t> colorTable;

    Palette(const color_t* colorTable, size_t tableLength) : colorTable(color_table(colorTable, tableLength)) {
    }

    virtual void palette_map(const color_t* pixels, byte* indexes, size_t length) = 0;

    virtual void palette_dither(color_t* pixels, byte* indexes, size_t width, size_t height) = 0;


    virtual ~Palette() noexcept {}

private:
    static std::vector<color_t> color_table(const color_t* colorTable, size_t tableLength) {
        std::vector<color_t> table(colorTable, colorTable + tableLength);
        std::for_each(table.begin(), table.end(), [](color_t& c) { c &= 0xffffff; });
        return table;
    }
};

template<typename TPalette>
struct PaletteImpl : public Palette {
    PaletteImpl(const color_t* colorTable, size_t tableLength) : Palette(colorTable, tableLength) {

    }

    void palette_map(const color_t* pixels, byte* indexes, size_t length) override final {
        palette_map_no_dither(static_cast<TPalette*>(this), pixels, indexes, length);
    }

    void palette_dither(color_t* pixels, byte* indexes, size_t width, size_t height) override final {
        palette_map_dither(static_cast<TPalette*>(this), pixels, indexes, width, height);
    }


    static void palette_map_no_dither(TPalette* palette, const color_t* pixels, byte* indexes, size_t length) {
        for (size_t i = 0; i < length; i++) {
            indexes[i] = palette->palette_index(pixels[i] & 0xffffff);
        }
    }

    static void palette_map_dither(TPalette* palette, color_t* pixels, byte* indexes, size_t width, size_t height);
};

struct OptimizationPalette {
    byte_vector list;

    OptimizationPalette(const color_t* colorTable, size_t tableLength);

    byte slow_map(const std::vector<color_t>& colorTable, color_t pixel) const {
        int r = reinterpret_cast<uint8_t*>(&pixel)[2];
        int g = reinterpret_cast<uint8_t*>(&pixel)[1];
        int b = reinterpret_cast<uint8_t*>(&pixel)[0];
        int minDist = std::numeric_limits<int>::max();
        int findIndex = 0;

        int cubeIndex = r / CubeSize * N * N + g / CubeSize * N + b / CubeSize;
        const ListHead& listHead = reinterpret_cast<const ListHead*>(list.data())[cubeIndex];
        uint32_t count = listHead.count;
        const byte* colorList = &list[CubeCount * sizeof(ListHead) + listHead.index];
        if (count == 1) return colorList[0];

        for (uint32_t i = 0; i < count; i++) {
            int colorIndex = colorList[i];
            int r0 = reinterpret_cast<const uint8_t*>(&colorTable[colorIndex])[2];
            int g0 = reinterpret_cast<const uint8_t*>(&colorTable[colorIndex])[1];
            int b0 = reinterpret_cast<const uint8_t*>(&colorTable[colorIndex])[0];
            int dist = color_distance(r0, g0, b0, r, g, b);
            if (dist == 0) return static_cast<byte>(colorIndex);
            if (dist < minDist) {
                minDist = dist;
                findIndex = colorIndex;
            }
        }

        return static_cast<byte>(findIndex);
    }
};

struct NoOptimizationPalette {
    byte slow_map(const std::vector<color_t>& colorTable, color_t pixel) const {
        int r1 = reinterpret_cast<uint8_t*>(&pixel)[2];
        int g1 = reinterpret_cast<uint8_t*>(&pixel)[1];
        int b1 = reinterpret_cast<uint8_t*>(&pixel)[0];
        int minDist = std::numeric_limits<int>::max();
        size_t findIndex = 0;
        for (size_t i = 0; i < colorTable.size(); i++) {
            color_t pixel2 = colorTable[i];
            int r2 = reinterpret_cast<uint8_t*>(&pixel2)[2];
            int g2 = reinterpret_cast<uint8_t*>(&pixel2)[1];
            int b2 = reinterpret_cast<uint8_t*>(&pixel2)[0];
            int dist = color_distance(r1, g1, b1, r2, g2, b2);
            if (dist < minDist) {
                findIndex = i;
                minDist = dist;
                if (dist == 0) break;
            }
        }
        return static_cast<byte>(findIndex);
    }
};

struct DoubleCachePalette {
    std::array<byte, 0x1000000 / 8> masks;
    std::array<byte, 0x1000000> indexMap;

    DoubleCachePalette() {
        masks.fill(0);
        indexMap.fill(0);
    }
};

struct SingleCachePalette {
    std::array<byte, 0x1000000> indexMap;

    SingleCachePalette() {
        indexMap.fill(0);
    }
};




struct DoubleCacheOptimizationPalette : public PaletteImpl<DoubleCacheOptimizationPalette>, public OptimizationPalette, public DoubleCachePalette {
    DoubleCacheOptimizationPalette(const color_t* colorTable, size_t tableLength) : PaletteImpl(colorTable, tableLength), OptimizationPalette(colorTable, tableLength) {

    }

    byte palette_index(color_t pixel) {
        if (masks[pixel >> 3] & (1 << (pixel & 7))) LIKELY{
            return indexMap[pixel];
        }

        byte index = slow_map(colorTable, pixel);
        indexMap[pixel] = index;
        masks[pixel >> 3] |= 1 << (pixel & 7);
        return index;
    }
};

struct DoubleCacheEuclideanPalette : public PaletteImpl<DoubleCacheEuclideanPalette>, public NoOptimizationPalette, public DoubleCachePalette {
    DoubleCacheEuclideanPalette(const color_t* colorTable, size_t tableLength) : PaletteImpl(colorTable, tableLength) {

    }

    byte palette_index(color_t pixel) {
        if (masks[pixel >> 3] & (1 << (pixel & 7))) LIKELY{
            return indexMap[pixel];
        }

        byte index = slow_map(colorTable, pixel);
        indexMap[pixel] = index;
        masks[pixel >> 3] |= 1 << (pixel & 7);
        return index;
    }
};

struct SingleCacheOptimizationPalette : public PaletteImpl<SingleCacheOptimizationPalette>, public OptimizationPalette, public SingleCachePalette {
    SingleCacheOptimizationPalette(const color_t* colorTable, size_t tableLength) : PaletteImpl(colorTable, tableLength), OptimizationPalette(colorTable, tableLength) {

    }

    byte palette_index(color_t pixel) {
        if (indexMap[pixel]) LIKELY{
            return indexMap[pixel] - 1;
        }

        byte index = slow_map(colorTable, pixel);
        indexMap[pixel] = index + 1;
        return index;
    }
};

struct SingleCacheEuclideanPalette : public PaletteImpl<SingleCacheEuclideanPalette>, public NoOptimizationPalette, public SingleCachePalette {
    SingleCacheEuclideanPalette(const color_t* colorTable, size_t tableLength) : PaletteImpl(colorTable, tableLength) {

    }

    byte palette_index(color_t pixel) {
        if (indexMap[pixel]) LIKELY{
            return indexMap[pixel] - 1;
        }

        byte index = slow_map(colorTable, pixel);
        indexMap[pixel] = index + 1;
        return index;
    }
};

static int distance_to_rect(int r, int g, int b, int rs, int re, int gs, int ge, int bs, int be) {
    if (r < rs) {
        if (g < gs) {
            if (b < bs) {
                return square_sum(r - rs, g - gs, b - bs);
            } else if (b > be) {
                return square_sum(r - rs, g - gs, b - be);
            } else {
                return square_sum(r - rs, g - gs);
            }
        } else if (g > ge) {
            if (b < bs) {
                return square_sum(r - rs, g - ge, b - bs);
            } else if (b > be) {
                return square_sum(r - rs, g - ge, b - be);
            } else {
                return square_sum(r - rs, g - ge);
            }
        } else {
            if (b < bs) {
                return square_sum(r - rs, b - bs);
            } else if (b > be) {
                return square_sum(r - rs, b - be);
            } else {
                return square_sum(r - rs);
            }
        }
    } else if (r > re) {
        if (g < gs) {
            if (b < bs) {
                return square_sum(r - re, g - gs, b - bs);
            } else if (b > be) {
                return square_sum(r - re, g - gs, b - be);
            } else {
                return square_sum(r - re, g - gs);
            }
        } else if (g > ge) {
            if (b < bs) {
                return square_sum(r - re, g - ge, b - bs);
            } else if (b > be) {
                return square_sum(r - re, g - ge, b - be);
            } else {
                return square_sum(r - re, g - ge);
            }
        } else {
            if (b < bs) {
                return square_sum(r - re, b - bs);
            } else if (b > be) {
                return square_sum(r - re, b - be);
            } else {
                return square_sum(r - re);
            }
        }
    } else {
        if (g < gs) {
            if (b < bs) {
                return square_sum(g - gs, b - bs);
            } else if (b > be) {
                return square_sum(g - gs, b - be);
            } else {
                return square_sum(g - gs);
            }
        } else if (g > ge) {
            if (b < bs) {
                return square_sum(g - ge, b - bs);
            } else if (b > be) {
                return square_sum(g - ge, b - be);
            } else {
                return square_sum(g - ge);
            }
        } else {
            if (b < bs) {
                return square_sum(b - bs);
            } else if (b > be) {
                return square_sum(b - be);
            } else {
                return 0;
            }
        }
    }
}

static int distance_to_rect_farthest(int r, int g, int b, int rs, int re, int gs, int ge, int bs, int be) {
    if (r < (rs + re) >> 1) {
        if (g < (gs + ge) >> 1) {
            if (b < (bs + be) >> 1) {
                return color_distance(r, g, b, re, ge, be);
            } else {
                return color_distance(r, g, b, re, ge, bs);
            }
        } else {
            if (b < (bs + be) >> 1) {
                return color_distance(r, g, b, re, gs, be);
            } else {
                return color_distance(r, g, b, re, gs, bs);
            }
        }
    } else {
        if (g < (gs + ge) >> 1) {
            if (b < (bs + be) >> 1) {
                return color_distance(r, g, b, rs, ge, be);
            } else {
                return color_distance(r, g, b, rs, ge, bs);
            }
        } else {
            if (b < (bs + be) >> 1) {
                return color_distance(r, g, b, rs, gs, be);
            } else {
                return color_distance(r, g, b, rs, gs, bs);
            }
        }
    }
}

static bool vertical_intersect_to_rect(color_t color, color_t other, int r1, int r2, int g1, int g2, int b1, int b2) {
    uint8_t colorR = reinterpret_cast<uint8_t*>(&color)[2];
    uint8_t colorG = reinterpret_cast<uint8_t*>(&color)[1];
    uint8_t colorB = reinterpret_cast<uint8_t*>(&color)[0];
    uint8_t otherR = reinterpret_cast<uint8_t*>(&other)[2];
    uint8_t otherG = reinterpret_cast<uint8_t*>(&other)[1];
    uint8_t otherB = reinterpret_cast<uint8_t*>(&other)[0];
    int rc = otherR - (otherR - colorR) / 2;
    int gc = otherG - (otherG - colorG) / 2;
    int bc = otherB - (otherB - colorB) / 2;
    int vr = colorR - rc;
    int vg = colorG - gc;
    int vb = colorB - bc;
    int dpr1 = vr * (r1 - rc);
    int dpr2 = vr * (r2 - rc);
    int dpg1 = vg * (g1 - gc);
    int dpg2 = vg * (g2 - gc);
    int dpb1 = vb * (b1 - bc);
    int dpb2 = vb * (b2 - bc);
    if (dpr1 + dpg1 + dpb1 < 0) return true;
    if (dpr1 + dpg1 + dpb2 < 0) return true;
    if (dpr1 + dpg2 + dpb1 < 0) return true;
    if (dpr1 + dpg2 + dpb2 < 0) return true;
    if (dpr2 + dpg1 + dpb1 < 0) return true;
    if (dpr2 + dpg1 + dpb2 < 0) return true;
    if (dpr2 + dpg2 + dpb1 < 0) return true;
    if (dpr2 + dpg2 + dpb2 < 0) return true;
    return false;
}

struct DistanceInfo {
    int otherIndex;
    int distance;

    bool operator<(const DistanceInfo& other) const {
        return distance < other.distance;
    }
};

OptimizationPalette::OptimizationPalette(const color_t* colorTable, size_t tableLength) {
    list.reserve(CubeCount * (sizeof(ListHead) + 4));
    list.resize(CubeCount * sizeof(ListHead), 0);

    uint16_t bucket[CubeCount + tableLength];
    int distCache[tableLength][tableLength];
    DistanceInfo sortedDistCache[tableLength][tableLength];

    for (size_t i = 0; i < tableLength; i++) {
        for (size_t j = 0; j < i; j++) {
            int r1 = reinterpret_cast<const uint8_t*>(colorTable + i)[2];
            int g1 = reinterpret_cast<const uint8_t*>(colorTable + i)[1];
            int b1 = reinterpret_cast<const uint8_t*>(colorTable + i)[0];
            int r2 = reinterpret_cast<const uint8_t*>(colorTable + j)[2];
            int g2 = reinterpret_cast<const uint8_t*>(colorTable + j)[1];
            int b2 = reinterpret_cast<const uint8_t*>(colorTable + j)[0];
            int dist = color_distance(r1, g1, b1, r2, g2, b2);

            sortedDistCache[i][j].otherIndex = j;
            sortedDistCache[i][j].distance = dist;
            sortedDistCache[j][i].otherIndex = i;
            sortedDistCache[j][i].distance = dist;
            distCache[i][j] = dist;
            distCache[j][i] = dist;
        }
        sortedDistCache[i][i].otherIndex = i;
        sortedDistCache[i][i].distance = 0;
        distCache[i][i] = 0;
    }

    for (size_t i = 0; i < tableLength; i++) {
        std::sort(sortedDistCache[i], sortedDistCache[i] + tableLength);
    }

    std::fill_n(bucket, CubeCount, 0);

    for (size_t i = 0; i < tableLength; i++) {
        uint8_t ri = reinterpret_cast<const uint8_t*>(colorTable + i)[2] / CubeSize;
        uint8_t gi = reinterpret_cast<const uint8_t*>(colorTable + i)[1] / CubeSize;
        uint8_t bi = reinterpret_cast<const uint8_t*>(colorTable + i)[0] / CubeSize;
        uint16_t bucketIndex = ri * N * N + gi * N + bi;
        bucket[CubeCount + i] = bucket[bucketIndex];
        bucket[bucketIndex] = static_cast<uint16_t>(CubeCount + i);
    }

    int bucketOffset = 0;
    for (int r = 0; r < N; r++) {
        int rs = r * CubeSize, re = rs + CubeSize;
        for (int g = 0; g < N; g++) {
            int gs = g * CubeSize, ge = gs + CubeSize;
            for (int b = 0; b < N; b++, bucketOffset++) {
                int bs = b * CubeSize, be = bs + CubeSize;

                int dist;
                int searchRange = 0;
                int findIndex = 0;

                if (bucket[bucketOffset] != 0) {
                    uint16_t currIndex = bucket[bucketOffset];
                    do {
                        int i = currIndex - CubeCount;
                        int r0 = reinterpret_cast<const uint8_t*>(colorTable + i)[2];
                        int g0 = reinterpret_cast<const uint8_t*>(colorTable + i)[1];
                        int b0 = reinterpret_cast<const uint8_t*>(colorTable + i)[0];
                        int dist = distance_to_rect_farthest(r0, g0, b0, rs, re, gs, ge, bs, be);
                        if (dist > searchRange) {
                            searchRange = dist;
                            findIndex = i;
                        }
                        currIndex = bucket[currIndex];
                    } while (currIndex != 0);

                    searchRange <<= 2;
                } else {
                    int minDist = std::numeric_limits<int>::max();

                    for (int i = 0; i < tableLength; i++) {
                        int r0 = reinterpret_cast<const uint8_t*>(colorTable + i)[2];
                        int g0 = reinterpret_cast<const uint8_t*>(colorTable + i)[1];
                        int b0 = reinterpret_cast<const uint8_t*>(colorTable + i)[0];
                        int dist = distance_to_rect(r0, g0, b0, rs, re, gs, ge, bs, be);
                        if (dist < minDist) {
                            minDist = dist;
                            findIndex = i;
                        }
                    }

                    int r0 = reinterpret_cast<const uint8_t*>(colorTable + findIndex)[2];
                    int g0 = reinterpret_cast<const uint8_t*>(colorTable + findIndex)[1];
                    int b0 = reinterpret_cast<const uint8_t*>(colorTable + findIndex)[0];
                    searchRange = distance_to_rect_farthest(r0, g0, b0, rs, re, gs, ge, bs, be) << 2;
                }


                size_t indexesOffset = list.size();
                uint32_t indexesCount = 1;
                reinterpret_cast<ListHead*>(&list[bucketOffset * sizeof(ListHead)])->index = static_cast<uint32_t>(indexesOffset - CubeCount * sizeof(ListHead));
                list.push_back(static_cast<byte>(findIndex));
                const DistanceInfo* sortedDistLine = sortedDistCache[findIndex];

                for (size_t i = 1; i < tableLength; i++) {
                    if (sortedDistLine[i].distance >= searchRange) break;
                    int otherIndex = sortedDistLine[i].otherIndex;
                    const int* distCacheLine = distCache[otherIndex];
                    int minDistIndex = findIndex;
                    int minDist = distCacheLine[findIndex];
                    const byte* indexes = &list[indexesOffset];

                    for (uint32_t j = 1; j < indexesCount; j++) {
                        dist = distCacheLine[indexes[j]];
                        if (dist < minDist) {
                            minDist = dist;
                            minDistIndex = indexes[j];
                        }
                    }

                    if (vertical_intersect_to_rect(colorTable[minDistIndex], colorTable[otherIndex], rs, re, gs, ge, bs, be)) {
                        list.push_back(static_cast<byte>(otherIndex));
                        indexesCount++;
                    }
                }

                reinterpret_cast<ListHead*>(&list[bucketOffset * sizeof(ListHead)])->count = indexesCount;
            }
        }
    }
}


template<typename TPalette>
void PaletteImpl<TPalette>::palette_map_dither(TPalette* palette, color_t* pixels, byte* indexes, size_t width, size_t height) {
    constexpr double Attenuation = 0.75;
    constexpr size_t Rows = 2;
    constexpr size_t Cols = 3;
    constexpr int DitherMat[Rows][Cols]{
        { 0, 0, 7 },
        { 3, 5, 1 },
    };

    int pixelOffsets[Rows * Cols];
    uint16_t weights[Rows * Cols];
    int weightSum = 0;
    size_t weightCount = 0;

    for (size_t y = 0; y < Rows; y++) {
        for (size_t x = 0; x < Cols; x++) {
            if (DitherMat[y][x] == 0) continue;
            weightSum += DitherMat[y][x];
            weights[weightCount] = DitherMat[y][x];
            pixelOffsets[weightCount] = static_cast<int>(y * width + x - Cols / 2);
            weightCount++;
        }
    }

    for (size_t i = 0; i < weightCount; i++) {
        weights[i] = static_cast<uint16_t>(weights[i] * 65535 * Attenuation / weightSum);
    }

    if (width <= Cols || height <= Rows) {
        for (size_t i = 0; i < width * height; i++) {
            indexes[i] = palette->palette_index(pixels[i] & 0xffffff);
            pixels[i] = palette->colorTable[indexes[i]];
        }
        return;
    }


    for (size_t y = 0; y < height - Rows + 1; y++, pixels += width, indexes += width) {
        size_t x = 0;
        for (; x < Cols / 2; x++) {
            indexes[x] = palette->palette_index(pixels[x] & 0xffffff);
            pixels[x] = palette->colorTable[indexes[x]];
        }

        for (; x < width - Cols / 2; x++) LIKELY{
            color_t oldPixel = pixels[x] & 0xffffff;
            byte paletteIndex = palette->palette_index(oldPixel);
            color_t newPixel = palette->colorTable[paletteIndex];
            pixels[x] = newPixel;
            indexes[x] = paletteIndex;

            int64_t errR = (static_cast<int64_t>(oldPixel) & 0xff0000) - (static_cast<int64_t>(newPixel) & 0xff0000);
            int64_t errG = (static_cast<int64_t>(oldPixel) & 0x00ff00) - (static_cast<int64_t>(newPixel) & 0x00ff00);
            int64_t errB = (static_cast<int64_t>(oldPixel) & 0x0000ff) - (static_cast<int64_t>(newPixel) & 0x0000ff);

            for (size_t i = 0; i < weightCount; i++) {
                int pixelOffset = pixelOffsets[i];
                color_t dstPixel = pixels[x + pixelOffset];
                int64_t newR = (dstPixel & 0xff0000) + (errR * weights[i] >> 16);
                int64_t newG = (dstPixel & 0x00ff00) + (errG * weights[i] >> 16);
                int64_t newB = (dstPixel & 0x0000ff) + (errB * weights[i] >> 16);
                if (newR & ~0xffffffLL) newR = ~(newR >> 63);
                if (newG & ~0x00ffffLL) newG = ~(newG >> 63);
                if (newB & ~0x0000ffLL) newB = ~(newB >> 63) & 0x0000ff;
                newR &= 0xff0000;
                newG &= 0x00ff00;
                pixels[x + pixelOffset] = static_cast<color_t>(newR | newG | newB);
            }
        }

        for (; x < width; x++) {
            indexes[x] = palette->palette_index(pixels[x] & 0xffffff);
            pixels[x] = palette->colorTable[indexes[x]];
        }
    }

    for (size_t y = height - Rows + 1; y < height; y++) {
        for (size_t x = 0; x < width; x++, pixels++, indexes++) {
            *indexes = palette->palette_index(*pixels & 0xffffff);
            *pixels = palette->colorTable[*indexes];
        }
    }
}


EXPORT_API
Palette* palette_create(const color_t* colorTable, size_t tableLength, bool optimize) {
    Palette* palette = nullptr;

    if (tableLength < 8) {
        palette = new SingleCacheEuclideanPalette(colorTable, tableLength);
    } else if (optimize) {
        if (tableLength < 256) {
            palette = new SingleCacheOptimizationPalette(colorTable, tableLength);
        } else if (tableLength == 256) {
            palette = new DoubleCacheOptimizationPalette(colorTable, tableLength);
        }
    } else {
        if (tableLength < 256) {
            palette = new SingleCacheEuclideanPalette(colorTable, tableLength);
        } else if (tableLength == 256) {
            palette = new DoubleCacheEuclideanPalette(colorTable, tableLength);
        }
    }

    return palette;
}

EXPORT_API
void palette_destroy(Palette* palette) {
    delete palette;
}

EXPORT_API
void palette_map(Palette& palette, const color_t* pixels, byte* indexes, size_t length) {
    palette.palette_map(pixels, indexes, length);
}

EXPORT_API
void palette_dither(Palette& palette, color_t* pixels, byte* indexes, size_t width, size_t height) {
    palette.palette_dither(pixels, indexes, width, height);
}

EXPORT_API
const color_t* palette_color_table(const Palette& palette, size_t* tableLength) {
    *tableLength = palette.colorTable.size();
    return palette.colorTable.data();
}