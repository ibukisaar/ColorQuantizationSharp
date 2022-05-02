#include <cinttypes>
#include <cstdlib>
#include <vector>
#include <array>
#include <map>
#include <iterator>
#include <algorithm>
#include <cmath>
#include <limits>
#include "default_init_allocator.h"

#define EXPORT_API extern "C" __declspec(dllexport)

using color_t = uint32_t;

using u32allocator = default_init_allocator<uint32_t>;
using u16allocator = default_init_allocator<uint16_t>;

struct CountNode {
    uint32_t count;
    uint32_t node;
};

struct SpaceShockColorExtractor {
    std::vector<color_t> colorList;
    size_t pixelTotalCount;
    std::array<CountNode, 0x1000000> colorCounts;

    SpaceShockColorExtractor() : pixelTotalCount(0) {
        colorCounts.fill({});
        colorList.reserve(0x100000);
    }

    SpaceShockColorExtractor(const SpaceShockColorExtractor&) = delete;

    SpaceShockColorExtractor& operator=(const SpaceShockColorExtractor&) = delete;
};


EXPORT_API
SpaceShockColorExtractor* create() {
    return new SpaceShockColorExtractor();
}

EXPORT_API
SpaceShockColorExtractor* reset(SpaceShockColorExtractor* extractor) {
    if (extractor == nullptr) return create();

    for (color_t color : extractor->colorList) {
        extractor->colorCounts[color] = {};
    }

    extractor->colorList.clear();
    extractor->pixelTotalCount = 0;
    return extractor;
}

EXPORT_API
void destroy(SpaceShockColorExtractor* extractor) {
    delete extractor;
}

EXPORT_API
void add_bitmap(SpaceShockColorExtractor& extractor, const color_t* pixels, size_t pixelCount) {
    for (size_t i = 0; i < pixelCount; i++) {
        color_t pixel = pixels[i] & 0xffffff;
        uint32_t count = extractor.colorCounts[pixel].count++;
        if (count == 0) {
            extractor.colorList.push_back(pixel);
        }
    }

    extractor.pixelTotalCount += pixelCount;
}

constexpr size_t BaseLength = 1024;

static std::pair<std::vector<uint32_t, u32allocator>, std::map<uint32_t, uint32_t>> sort_colors(SpaceShockColorExtractor& extractor) {
    std::vector<uint32_t, u32allocator> sortedBuffer;
    std::map<uint32_t, uint32_t> sortedMap;
    sortedBuffer.reserve(BaseLength * 1024);
    sortedBuffer.resize(BaseLength, 0);
    uint32_t lastNewIndex = BaseLength;

    for (color_t color : extractor.colorList) {
        uint32_t lastIndex;
        uint32_t count = extractor.colorCounts[color].count;

        if (count <= BaseLength) {
            uint32_t index = count - 1;
            lastIndex = sortedBuffer[index];
            if (lastIndex != 0) {
                sortedBuffer[lastIndex + 2] = lastNewIndex;
            }
            sortedBuffer[index] = lastNewIndex;
            sortedBuffer.resize(lastNewIndex + 3);
        } else {
            uint32_t& listHead = sortedMap[count];
            if (listHead == 0) {
                sortedBuffer.resize(lastNewIndex + 4);
                listHead = lastNewIndex++;
                lastIndex = 0;
            } else {
                sortedBuffer.resize(lastNewIndex + 3);
                lastIndex = sortedBuffer[listHead];
                sortedBuffer[lastIndex + 2] = lastNewIndex;
            }
            sortedBuffer[listHead] = lastNewIndex;
        }

        sortedBuffer[lastNewIndex + 0] = color;
        sortedBuffer[lastNewIndex + 1] = lastIndex;
        sortedBuffer[lastNewIndex + 2] = 0;
        extractor.colorCounts[color].node = lastNewIndex;
        lastNewIndex += 3;
    }

    return std::make_pair(std::move(sortedBuffer), std::move(sortedMap));
}

static void create_kernel(std::vector<uint16_t, u16allocator>& kernel, int kernelSize, double affect) {
    int length = kernelSize * 2 + 1;
    int len1 = length - 1;
    double cache[kernelSize + 1];

    for (int i = 0; i <= kernelSize; i++) {
        double v = (i - kernelSize) / (double)kernelSize;
        cache[i] = -(v * v / affect);
    }

    kernel.resize(length * length * length);

    int base2 = length * length;
    int base1 = length;

    for (int z = 0; z <= kernelSize; z++) {
        for (int y = 0; y <= kernelSize; y++) {
            for (int x = 0; x <= kernelSize; x++) {
                double w = exp(cache[x] + cache[y] + cache[z]);
                uint16_t v = static_cast<uint16_t>(w * 65535);

                kernel[z * base2 + y * base1 + x] = v;
                kernel[z * base2 + y * base1 + (len1 - x)] = v;
                kernel[z * base2 + (len1 - y) * base1 + x] = v;
                kernel[z * base2 + (len1 - y) * base1 + (len1 - x)] = v;
                kernel[(len1 - z) * base2 + y * base1 + x] = v;
                kernel[(len1 - z) * base2 + y * base1 + (len1 - x)] = v;
                kernel[(len1 - z) * base2 + (len1 - y) * base1 + x] = v;
                kernel[(len1 - z) * base2 + (len1 - y) * base1 + (len1 - x)] = v;
            }
        }
    }
}

static size_t absorb_color(SpaceShockColorExtractor& extractor, std::vector<uint32_t, u32allocator>& sortedBuffer, std::map<uint32_t, uint32_t>& sortedMap, const std::vector<uint16_t, u16allocator>& kernel, int kernelSize, color_t color, uint32_t kernelHeight) {
    int rCenter = reinterpret_cast<uint8_t*>(&color)[2];
    int gCenter = reinterpret_cast<uint8_t*>(&color)[1];
    int bCenter = reinterpret_cast<uint8_t*>(&color)[0];
    int rStart = std::max(rCenter - kernelSize, 0);
    int gStart = std::max(gCenter - kernelSize, 0);
    int bStart = std::max(bCenter - kernelSize, 0);
    int rEnd = std::min(rCenter + kernelSize, 255);
    int gEnd = std::min(gCenter + kernelSize, 255);
    int bEnd = std::min(bCenter + kernelSize, 255);

    int kernelBaseSize = kernelSize * 2 + 1;
    size_t pixelCount = 0;

    for (int r = rStart; r <= rEnd; r++) {
        const uint16_t* rKernel = &kernel[(r - rStart) * kernelBaseSize * kernelBaseSize];
        for (int g = gStart; g <= gEnd; g++) {
            const uint16_t* rgKernel = rKernel + (g - gStart) * kernelBaseSize;
            for (int b = bStart; b <= bEnd; b++) {
                color_t otherRgb = static_cast<color_t>((r << 16) | (g << 8) | b);
                uint32_t otherCount = extractor.colorCounts[otherRgb].count;
                if (otherCount == 0) continue;
                uint16_t weight = rgKernel[b - bStart];
                uint32_t newCount = static_cast<uint32_t>(std::max(otherCount - ((static_cast<int64_t>(kernelHeight) * weight) >> 16), 0LL));
                if (newCount == otherCount) continue;

                pixelCount += otherCount - newCount;

                extractor.colorCounts[otherRgb].count = newCount;
                uint32_t lastNode = extractor.colorCounts[otherRgb].node;
                uint32_t prevIndex = sortedBuffer[lastNode + 1];
                uint32_t nextIndex = sortedBuffer[lastNode + 2];
                uint32_t listHead = otherCount > BaseLength ? sortedMap[otherCount] : otherCount - 1;

                if (prevIndex != 0) {
                    sortedBuffer[prevIndex + 2] = nextIndex;
                    if (nextIndex != 0) {
                        sortedBuffer[nextIndex + 1] = prevIndex;
                    } else {
                        sortedBuffer[listHead] = prevIndex;
                    }
                } else {
                    if (nextIndex == 0) {
                        sortedBuffer[listHead] = 0;
                        if (otherCount > BaseLength) {
                            sortedMap.erase(otherCount);
                        }
                    } else {
                        sortedBuffer[nextIndex + 1] = prevIndex;
                    }
                }

                if (newCount != 0) {
                    if (newCount > BaseLength) {
                        uint32_t& oldListHead = sortedMap[newCount];
                        if (oldListHead == 0) {
                            listHead = sortedBuffer.size();
                            oldListHead = listHead;
                            sortedBuffer.push_back(0);
                        } else {
                            listHead = oldListHead;
                        }
                    } else {
                        listHead = newCount - 1;
                    }

                    if (sortedBuffer[listHead] != 0) {
                        sortedBuffer[sortedBuffer[listHead] + 2] = lastNode;
                    }
                    sortedBuffer[lastNode + 0] = otherRgb;
                    sortedBuffer[lastNode + 1] = sortedBuffer[listHead];
                    sortedBuffer[lastNode + 2] = 0;
                    sortedBuffer[listHead] = lastNode;
                }
            }
        }
    }

    return pixelCount;
}

struct ColorInfo {
    double r, g, b, count;
};

static constexpr double smooth(double x) {
    double e = exp(x);
    double ie = 1 / e;
    double y = (e - ie) / (e + ie);
    return y;
}

EXPORT_API
size_t get_color_table(SpaceShockColorExtractor& extractor, color_t* colorTable, size_t tableLength, const color_t* forceColors, size_t forceColorCount) {
    if (tableLength < forceColorCount) return static_cast<size_t>(-1);

    constexpr double E = 2.7182818284590451;
    constexpr double PI = 3.1415926535897931;
    constexpr uint32_t SkipMinCount = 3;

    color_t forceColorList[forceColorCount];
    for (size_t i = 0; i < forceColorCount; i++) {
        forceColorList[i] = forceColors[i] & 0xffffff;
    }

    ColorInfo counter[tableLength];
    std::vector<uint16_t, u16allocator> kernel;
    auto [sortedBuffer, sortedMap] = sort_colors(extractor);

    uint32_t maxPixelCount;
    if (!sortedMap.empty()) {
        maxPixelCount = sortedMap.crbegin()->first;
    } else {
        for (maxPixelCount = BaseLength; maxPixelCount != 0; maxPixelCount--) {
            if (sortedBuffer[maxPixelCount - 1] != 0) break;
        }
    }


    const int MaxKernelSize = 28;
    const int MinKernelSize = std::clamp<int>(static_cast<int>(MaxKernelSize * exp((tableLength - forceColorCount) / -64.0)), 2, MaxKernelSize - 1);
    size_t outIndex = 0;
    double pixelTotalCount = extractor.pixelTotalCount;

    if (forceColorCount) {
        create_kernel(kernel, MinKernelSize, 1 / PI);

        for (color_t color : forceColorList) {
            pixelTotalCount -= absorb_color(extractor, sortedBuffer, sortedMap, kernel, MinKernelSize, color, maxPixelCount);
            colorTable[outIndex++] = color;
        }
    }


    if (outIndex == tableLength) return outIndex;

    uint32_t pixelCount;
    uint32_t decrementPixelCount = BaseLength;
    uint32_t listHead, lastNode;
    color_t rgb;
    uint32_t prevKernelSize = 0;
    double prevAffect;
    double x0 = 0;
    double consumePixelCount = 0;

    for (; outIndex < tableLength; outIndex++) {
        if (!sortedMap.empty()) {
            std::tie(pixelCount, listHead) = *sortedMap.crbegin();
            lastNode = sortedBuffer[listHead];
            rgb = sortedBuffer[lastNode];
            sortedBuffer[listHead] = sortedBuffer[lastNode + 1];
            if (sortedBuffer[listHead] == 0) {
                sortedMap.erase(--sortedMap.cend());
            }
        } else {
            for (;; decrementPixelCount--) {
                if (decrementPixelCount == 0) goto ReduceBegin;
                listHead = decrementPixelCount - 1;
                lastNode = sortedBuffer[listHead];
                if (lastNode != 0) break;
            }

            rgb = sortedBuffer[lastNode];
            pixelCount = decrementPixelCount;
            sortedBuffer[listHead] = sortedBuffer[lastNode + 1];
            if (sortedBuffer[listHead] != 0) {
                sortedBuffer[sortedBuffer[listHead] + 2] = 0;
            }
        }

        extractor.colorCounts[rgb].count = 0;
        colorTable[outIndex] = rgb;
        ColorInfo& colorInfo = counter[outIndex - forceColorCount];

        constexpr double A = 2;

        size_t kernelSize = std::clamp(static_cast<int>(x0 * MaxKernelSize), MinKernelSize, MaxKernelSize);
        double affect = std::clamp((x0 * (PI - 1) + 1) / E, 1 / E, PI / E);
        double a0 = (double)outIndex / tableLength;
        double b0 = consumePixelCount / pixelTotalCount;
        double reference = std::min<double>(smooth(A * a0) * 1.08, 1);
        double dx = smooth(A * (a0 + 1.0 / tableLength)) - smooth(A * a0);
        dx += dx * smooth(8 * ((reference - b0) * (1 - a0) + (reference - x0) * a0));
        x0 = std::clamp<double>(x0 + dx, 0, 1);

        if (prevKernelSize != kernelSize || abs(prevAffect - affect) > 0.01) {
            create_kernel(kernel, kernelSize, affect);
            prevKernelSize = kernelSize;
            prevAffect = affect;
        }


        size_t absorbCount = absorb_color(extractor, sortedBuffer, sortedMap, kernel, kernelSize, rgb, pixelCount);
        consumePixelCount += pixelCount + absorbCount;
        colorInfo.r = reinterpret_cast<uint8_t*>(&rgb)[2];
        colorInfo.g = reinterpret_cast<uint8_t*>(&rgb)[1];
        colorInfo.b = reinterpret_cast<uint8_t*>(&rgb)[0];
        colorInfo.count = pixelCount + absorbCount;
    }


ReduceBegin:
    if (outIndex < tableLength) return outIndex;

    size_t infoCount = tableLength - forceColorCount;

    while (true) {
        if (!sortedMap.empty()) {
            std::tie(pixelCount, listHead) = *sortedMap.crbegin();
            lastNode = sortedBuffer[listHead];
            rgb = sortedBuffer[lastNode];
            sortedBuffer[listHead] = sortedBuffer[lastNode + 1];
            if (sortedBuffer[listHead] == 0) {
                sortedMap.erase(--sortedMap.cend());
            }
        } else {
            for (;; decrementPixelCount--) {
                if (decrementPixelCount <= SkipMinCount) goto Return;
                listHead = decrementPixelCount - 1;
                lastNode = sortedBuffer[listHead];
                if (lastNode != 0) break;
            }

            rgb = sortedBuffer[lastNode];
            pixelCount = decrementPixelCount;
            sortedBuffer[listHead] = sortedBuffer[lastNode + 1];
            if (sortedBuffer[listHead] != 0) {
                sortedBuffer[sortedBuffer[listHead] + 2] = 0;
            }
        }

        double r = reinterpret_cast<uint8_t*>(&rgb)[2];
        double g = reinterpret_cast<uint8_t*>(&rgb)[1];
        double b = reinterpret_cast<uint8_t*>(&rgb)[0];
        double minVar = std::numeric_limits<double>::infinity();
        size_t reduceIndex;

        for (size_t i = 0; i < infoCount; i++) {
            double dr = r - counter[i].r;
            double dg = g - counter[i].g;
            double db = b - counter[i].b;
            double currVar = dr * dr + dg * dg + db * db;
            if (currVar < minVar) {
                minVar = currVar;
                reduceIndex = i;
            }
        }

        double newPixelCount = pixelCount;
        double totalCount = counter[reduceIndex].count + newPixelCount;
        counter[reduceIndex].r = (counter[reduceIndex].r * counter[reduceIndex].count + r * newPixelCount) / totalCount;
        counter[reduceIndex].g = (counter[reduceIndex].g * counter[reduceIndex].count + g * newPixelCount) / totalCount;
        counter[reduceIndex].b = (counter[reduceIndex].b * counter[reduceIndex].count + b * newPixelCount) / totalCount;
        counter[reduceIndex].count = totalCount;
    }

Return:
    for (size_t i = 0; i < infoCount; i++) {
        uint32_t r = static_cast<uint32_t>(round(counter[i].r));
        uint32_t g = static_cast<uint32_t>(round(counter[i].g));
        uint32_t b = static_cast<uint32_t>(round(counter[i].b));
        colorTable[forceColorCount + i] = (r << 16) | (g << 8) | b;
    }
    return tableLength;
}