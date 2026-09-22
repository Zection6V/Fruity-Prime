#include "MapTextureBake.hpp"

#include "Q3Bsp.hpp"
#include "../../Formats/Types.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace MphRead::Mods::MapGen::MapTextureBakeInterop
{
    // Declaration-only bridges for the external APIs used by MapTextureBake.cs.
    // They add no fallback or Native-only policy: the platform owner must supply
    // System.IO.Compression-equivalent ZIP reads and ReFuel.Stb RGB decoding.
    class ZipArchive;

    struct DecodedImage final
    {
        std::int32_t Width = 0;
        std::int32_t Height = 0;
        std::vector<std::uint8_t> Pixels;
    };

    [[nodiscard]] std::shared_ptr<ZipArchive> OpenZipRead(const std::string& path);
    [[nodiscard]] std::size_t ZipEntryCount(const ZipArchive& archive);
    [[nodiscard]] std::string ZipEntryFullName(
        const ZipArchive& archive, std::size_t index);
    [[nodiscard]] std::vector<std::uint8_t> ReadZipEntry(
        const ZipArchive& archive, std::size_t index);
    [[nodiscard]] DecodedImage LoadRgb(std::span<const std::uint8_t> bytes);
}

namespace System
{
    class IndexOutOfRangeException final : public std::out_of_range
    {
    public:
        IndexOutOfRangeException()
            : std::out_of_range("Index was outside the bounds of the array.")
        {
        }
    };
}

namespace
{
    using MphRead::Mods::MapGen::MapTextureBakeInterop::DecodedImage;
    using MphRead::Mods::MapGen::MapTextureBakeInterop::ZipArchive;

    constexpr std::int32_t PaletteSize = 256;
    constexpr std::array<std::string_view, 7> SkySuffixes{
        "_1", "_2", "_ft", "_bk", "_lf", "_rt", "_up"
    };
    constexpr std::array<std::string_view, 4> Extensions{
        ".tga", ".jpg", ".jpeg", ".png"
    };

    [[nodiscard]] std::int32_t WrapAdd(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left)
            + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] std::int32_t WrapMultiply(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left)
            * static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] std::int32_t ManagedShiftRight(
        std::int32_t value, unsigned count) noexcept
    {
        count &= 31U;
        if (count == 0)
        {
            return value;
        }
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
        std::uint32_t shifted = bits >> count;
        if ((bits & 0x80000000U) != 0)
        {
            shifted |= 0xFFFFFFFFU << (32U - count);
        }
        return std::bit_cast<std::int32_t>(shifted);
    }

    [[nodiscard]] std::int32_t ManagedShiftLeft(
        std::int32_t value, unsigned count) noexcept
    {
        const std::uint32_t shifted
            = std::bit_cast<std::uint32_t>(value) << (count & 31U);
        return std::bit_cast<std::int32_t>(shifted);
    }

    [[nodiscard]] std::int32_t ManagedOr(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            std::bit_cast<std::uint32_t>(left)
            | std::bit_cast<std::uint32_t>(right));
    }

    [[nodiscard]] std::size_t ArrayLength(std::int32_t value)
    {
        if (value < 0)
        {
            throw System::OverflowException();
        }
        return static_cast<std::size_t>(value);
    }

    [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value)
    {
#if defined(__cpp_char8_t)
        std::u8string converted;
        converted.reserve(value.size());
        for (unsigned char ch : value)
        {
            converted.push_back(static_cast<char8_t>(ch));
        }
        return std::filesystem::path(converted);
#else
        return std::filesystem::u8path(value.begin(), value.end());
#endif
    }

    [[nodiscard]] bool FileExists(
        const std::optional<std::string>& path) noexcept
    {
        if (!path.has_value() || path->empty())
        {
            return false;
        }
        try
        {
            std::error_code error;
            const bool regular = std::filesystem::is_regular_file(
                PathFromUtf8(*path), error);
            return regular && !error;
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] bool IsDirectorySeparator(char value) noexcept
    {
#if defined(_WIN32)
        return value == '/' || value == '\\';
#else
        return value == '/';
#endif
    }

    [[nodiscard]] std::string GetExtension(std::string_view path)
    {
        for (std::size_t i = path.size(); i > 0; --i)
        {
            const char value = path[i - 1];
            if (value == '.')
            {
                return i == path.size()
                    ? std::string()
                    : std::string(path.substr(i - 1));
            }
            if (IsDirectorySeparator(value))
            {
                break;
            }
        }
        return {};
    }

    [[nodiscard]] bool OrdinalIgnoreCaseEquals(
        const std::string& left, const std::string& right) noexcept
    {
        return MphRead::Mods::MapGen::Q3StringEqual{}(left, right);
    }

    class ArchiveGuard final
    {
    public:
        std::vector<std::shared_ptr<ZipArchive>> Values;

        ~ArchiveGuard()
        {
            for (std::shared_ptr<ZipArchive>& archive : Values)
            {
                archive.reset();
            }
        }

        ArchiveGuard() = default;
        ArchiveGuard(const ArchiveGuard&) = delete;
        ArchiveGuard& operator=(const ArchiveGuard&) = delete;
    };

    struct FileEntry final
    {
        ZipArchive* Archive = nullptr;
        std::size_t Index = 0;
        std::string FullName;
    };

    struct BakedEntry final
    {
        std::int32_t Index = 0;
        std::string Name;
        std::vector<std::uint16_t> Palette;
        std::vector<std::uint8_t> Pixels;
    };

    [[nodiscard]] const FileEntry* FindFile(
        const std::vector<FileEntry>& files, const std::string& name) noexcept
    {
        for (const FileEntry& file : files)
        {
            if (OrdinalIgnoreCaseEquals(file.FullName, name))
            {
                return std::addressof(file);
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<std::pair<std::int32_t, std::string>>
        UsedTextures(
            const std::shared_ptr<MphRead::Mods::MapGen::Q3Bsp>& bsp,
            bool sky)
    {
        using namespace MphRead::Mods::MapGen;

        if (!bsp)
        {
            throw System::NullReferenceException();
        }

        std::unordered_set<std::int32_t> seen;
        std::vector<std::pair<std::int32_t, std::string>> results;
        for (const std::shared_ptr<Q3Face>& faceRef : bsp->Faces())
        {
            if (!faceRef)
            {
                throw System::NullReferenceException();
            }
            const Q3Face& face = *faceRef;
            if (face.Type() != 1 && face.Type() != 2 && face.Type() != 3)
            {
                continue;
            }

            const std::int32_t textureIndex = face.Texture();
            if (!seen.emplace(textureIndex).second)
            {
                continue;
            }

            const Q3Bsp::TextureList& textures = bsp->Textures();
            if (textureIndex < 0
                || static_cast<std::uint64_t>(textureIndex) >= textures.size())
            {
                throw System::ArgumentOutOfRangeException();
            }
            const std::shared_ptr<Q3Texture>& textureRef
                = textures[static_cast<std::size_t>(textureIndex)];
            if (!textureRef)
            {
                throw System::NullReferenceException();
            }
            const Q3Texture& texture = *textureRef;
            if ((texture.Flags()
                & (Q3Bsp::SurfaceNoDraw
                    | Q3Bsp::SurfaceHint
                    | Q3Bsp::SurfaceSkip)) != 0)
            {
                continue;
            }
            if ((texture.Flags() & Q3Bsp::SurfaceSky) != 0 && !sky)
            {
                continue;
            }
            results.emplace_back(textureIndex, texture.Name());
        }

        std::sort(results.begin(), results.end(),
            [](const auto& left, const auto& right)
            {
                return left.first < right.first;
            });
        return results;
    }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> Find(
        const std::vector<FileEntry>& files, const std::string& name)
    {
        using namespace MphRead::Mods::MapGen::MapTextureBakeInterop;

        for (std::size_t suffixIndex = 0;
            suffixIndex <= SkySuffixes.size(); ++suffixIndex)
        {
            const std::string_view suffix = suffixIndex == 0
                ? std::string_view()
                : SkySuffixes[suffixIndex - 1];
            for (std::string_view extension : Extensions)
            {
                std::string candidate = name;
                candidate += suffix;
                candidate += extension;
                if (const FileEntry* file = FindFile(files, candidate))
                {
                    return ReadZipEntry(*file->Archive, file->Index);
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::uint8_t AtByte(
        const std::vector<std::uint8_t>& values, std::int32_t index)
    {
        if (index < 0
            || static_cast<std::uint64_t>(index) >= values.size())
        {
            throw System::IndexOutOfRangeException();
        }
        return values[static_cast<std::size_t>(index)];
    }

    void SetByte(
        std::vector<std::uint8_t>& values,
        std::int32_t index,
        std::uint8_t value)
    {
        if (index < 0
            || static_cast<std::uint64_t>(index) >= values.size())
        {
            throw System::IndexOutOfRangeException();
        }
        values[static_cast<std::size_t>(index)] = value;
    }

    [[nodiscard]] std::vector<std::uint8_t> Decode(
        const std::vector<std::uint8_t>& raw, std::int32_t size)
    {
        using namespace MphRead::Mods::MapGen::MapTextureBakeInterop;

        DecodedImage image = LoadRgb(raw);
        const std::int32_t width = image.Width;
        const std::int32_t height = image.Height;

        const std::int32_t resultLength = WrapMultiply(
            WrapMultiply(size, size), 3);
        std::vector<std::uint8_t> result(ArrayLength(resultLength));

        for (std::int32_t y = 0; y < size; ++y)
        {
            const std::int32_t y0
                = WrapMultiply(y, height) / size;
            const std::int32_t y1 = std::max(
                WrapAdd(y0, 1),
                WrapMultiply(WrapAdd(y, 1), height) / size);
            for (std::int32_t x = 0; x < size; ++x)
            {
                const std::int32_t x0
                    = WrapMultiply(x, width) / size;
                const std::int32_t x1 = std::max(
                    WrapAdd(x0, 1),
                    WrapMultiply(WrapAdd(x, 1), width) / size);

                std::int32_t r = 0;
                std::int32_t g = 0;
                std::int32_t b = 0;
                std::int32_t count = 0;
                for (std::int32_t sy = y0;
                    sy < y1 && sy < height; ++sy)
                {
                    for (std::int32_t sx = x0;
                        sx < x1 && sx < width; ++sx)
                    {
                        const std::int32_t offset = WrapMultiply(
                            WrapAdd(WrapMultiply(sy, width), sx), 3);
                        r = WrapAdd(r, AtByte(image.Pixels, offset));
                        g = WrapAdd(g, AtByte(
                            image.Pixels, WrapAdd(offset, 1)));
                        b = WrapAdd(b, AtByte(
                            image.Pixels, WrapAdd(offset, 2)));
                        count = WrapAdd(count, 1);
                    }
                }

                const std::int32_t target = WrapMultiply(
                    WrapAdd(WrapMultiply(y, size), x), 3);
                const std::int32_t divisor = std::max<std::int32_t>(1, count);
                SetByte(result, target,
                    static_cast<std::uint8_t>(r / divisor));
                SetByte(result, WrapAdd(target, 1),
                    static_cast<std::uint8_t>(g / divisor));
                SetByte(result, WrapAdd(target, 2),
                    static_cast<std::uint8_t>(b / divisor));
            }
        }
        return result;
    }

    struct BoxRange final
    {
        std::int32_t Start = 0;
        std::int32_t Length = 0;
    };

    class PixelComparison final
    {
    public:
        PixelComparison(
            const std::vector<std::uint8_t>& rgb,
            std::int32_t channel) noexcept
            : _rgb(rgb), _channel(channel)
        {
        }

        [[nodiscard]] std::int32_t operator()(
            std::int32_t left, std::int32_t right) const
        {
            const std::int32_t leftOffset = WrapAdd(
                WrapMultiply(left, 3), _channel);
            const std::int32_t rightOffset = WrapAdd(
                WrapMultiply(right, 3), _channel);
            const std::int32_t a = AtByte(_rgb, leftOffset);
            const std::int32_t b = AtByte(_rgb, rightOffset);
            return a < b ? -1 : (a > b ? 1 : 0);
        }

    private:
        const std::vector<std::uint8_t>& _rgb;
        std::int32_t _channel;
    };

    template <typename Compare>
    void SwapIfGreater(
        std::span<std::int32_t> keys,
        const Compare& compare,
        std::size_t i,
        std::size_t j)
    {
        if (compare(keys[i], keys[j]) > 0)
        {
            std::swap(keys[i], keys[j]);
        }
    }

    template <typename Compare>
    void InsertionSort(
        std::span<std::int32_t> keys,
        const Compare& compare)
    {
        for (std::size_t i = 0; i + 1 < keys.size(); ++i)
        {
            const std::int32_t value = keys[i + 1];
            std::ptrdiff_t j = static_cast<std::ptrdiff_t>(i);
            while (j >= 0
                && compare(value, keys[static_cast<std::size_t>(j)]) < 0)
            {
                keys[static_cast<std::size_t>(j + 1)]
                    = keys[static_cast<std::size_t>(j)];
                --j;
            }
            keys[static_cast<std::size_t>(j + 1)] = value;
        }
    }

    template <typename Compare>
    void DownHeap(
        std::span<std::int32_t> keys,
        std::size_t i,
        std::size_t n,
        const Compare& compare)
    {
        const std::int32_t value = keys[i - 1];
        while (i <= n >> 1)
        {
            std::size_t child = 2 * i;
            if (child < n
                && compare(keys[child - 1], keys[child]) < 0)
            {
                ++child;
            }
            if (!(compare(value, keys[child - 1]) < 0))
            {
                break;
            }
            keys[i - 1] = keys[child - 1];
            i = child;
        }
        keys[i - 1] = value;
    }

    template <typename Compare>
    void HeapSort(
        std::span<std::int32_t> keys,
        const Compare& compare)
    {
        const std::size_t n = keys.size();
        for (std::size_t i = n >> 1; i >= 1; --i)
        {
            DownHeap(keys, i, n, compare);
            if (i == 1)
            {
                break;
            }
        }
        for (std::size_t i = n; i > 1; --i)
        {
            std::swap(keys[0], keys[i - 1]);
            DownHeap(keys, 1, i - 1, compare);
        }
    }

    template <typename Compare>
    [[nodiscard]] std::size_t PickPivotAndPartition(
        std::span<std::int32_t> keys,
        const Compare& compare)
    {
        const std::size_t hi = keys.size() - 1;
        const std::size_t middle = hi >> 1;

        SwapIfGreater(keys, compare, 0, middle);
        SwapIfGreater(keys, compare, 0, hi);
        SwapIfGreater(keys, compare, middle, hi);

        const std::int32_t pivot = keys[middle];
        std::swap(keys[middle], keys[hi - 1]);
        std::size_t left = 0;
        std::size_t right = hi - 1;

        while (left < right)
        {
            do
            {
                ++left;
            }
            while (compare(keys[left], pivot) < 0);

            do
            {
                --right;
            }
            while (compare(pivot, keys[right]) < 0);

            if (left >= right)
            {
                break;
            }
            std::swap(keys[left], keys[right]);
        }

        if (left != hi - 1)
        {
            std::swap(keys[left], keys[hi - 1]);
        }
        return left;
    }

    template <typename Compare>
    void IntroSort(
        std::span<std::int32_t> keys,
        std::int32_t depthLimit,
        const Compare& compare)
    {
        std::size_t partitionSize = keys.size();
        while (partitionSize > 1)
        {
            if (partitionSize <= 16)
            {
                const std::span<std::int32_t> partition
                    = keys.first(partitionSize);
                if (partitionSize == 2)
                {
                    SwapIfGreater(partition, compare, 0, 1);
                    return;
                }
                if (partitionSize == 3)
                {
                    SwapIfGreater(partition, compare, 0, 1);
                    SwapIfGreater(partition, compare, 0, 2);
                    SwapIfGreater(partition, compare, 1, 2);
                    return;
                }
                InsertionSort(partition, compare);
                return;
            }

            if (depthLimit == 0)
            {
                HeapSort(keys.first(partitionSize), compare);
                return;
            }
            --depthLimit;

            const std::size_t pivot = PickPivotAndPartition(
                keys.first(partitionSize), compare);
            IntroSort(
                keys.subspan(
                    pivot + 1,
                    partitionSize - (pivot + 1)),
                depthLimit,
                compare);
            partitionSize = pivot;
        }
    }

    void DotNetArraySort(
        std::vector<std::int32_t>& values,
        std::int32_t start,
        std::int32_t length,
        const PixelComparison& compare)
    {
        if (start < 0 || length < 0
            || static_cast<std::uint64_t>(start)
                + static_cast<std::uint64_t>(length)
                > values.size())
        {
            throw std::out_of_range(
                "Offset and length were out of bounds for the array.");
        }
        if (length <= 1)
        {
            return;
        }

        std::span<std::int32_t> keys(
            values.data() + static_cast<std::size_t>(start),
            static_cast<std::size_t>(length));
        const std::int32_t depth = static_cast<std::int32_t>(
            2U * std::bit_width(static_cast<std::uint32_t>(length)));
        IntroSort(keys, depth, compare);
    }

    [[nodiscard]] std::pair<
        std::vector<std::uint16_t>,
        std::vector<std::uint8_t>>
        Quantize(
            const std::vector<std::uint8_t>& rgb,
            std::int32_t size)
    {
        const std::int32_t count = WrapMultiply(size, size);
        std::vector<std::int32_t> indices(ArrayLength(count));
        for (std::int32_t i = 0; i < count; ++i)
        {
            indices[static_cast<std::size_t>(i)] = i;
        }

        std::vector<BoxRange> boxes;
        boxes.push_back(BoxRange{0, count});
        while (boxes.size() < static_cast<std::size_t>(PaletteSize))
        {
            std::int32_t widest = -1;
            std::int32_t widestSpread = 0;
            std::int32_t widestChannel = 0;
            for (std::size_t i = 0; i < boxes.size(); ++i)
            {
                const BoxRange box = boxes[i];
                if (box.Length < 2)
                {
                    continue;
                }
                for (std::int32_t channel = 0; channel < 3; ++channel)
                {
                    std::int32_t low = 255;
                    std::int32_t high = 0;
                    const std::int32_t end = WrapAdd(
                        box.Start, box.Length);
                    for (std::int32_t j = box.Start; j < end; ++j)
                    {
                        const std::int32_t index = indices.at(
                            static_cast<std::size_t>(j));
                        const std::int32_t offset = WrapAdd(
                            WrapMultiply(index, 3), channel);
                        const std::int32_t value = AtByte(rgb, offset);
                        low = std::min(low, value);
                        high = std::max(high, value);
                    }
                    const std::int32_t spread = high - low;
                    if (spread > widestSpread)
                    {
                        widestSpread = spread;
                        widest = static_cast<std::int32_t>(i);
                        widestChannel = channel;
                    }
                }
            }

            if (widest < 0 || widestSpread == 0)
            {
                break;
            }

            const BoxRange selected = boxes.at(
                static_cast<std::size_t>(widest));
            DotNetArraySort(
                indices,
                selected.Start,
                selected.Length,
                PixelComparison(rgb, widestChannel));

            const std::int32_t half = selected.Length / 2;
            boxes[static_cast<std::size_t>(widest)]
                = BoxRange{selected.Start, half};
            boxes.push_back(BoxRange{
                WrapAdd(selected.Start, half),
                selected.Length - half
            });
        }

        const std::size_t paletteLength
            = std::max<std::size_t>(1, boxes.size());
        std::vector<std::uint16_t> palette(paletteLength);
        std::vector<std::uint8_t> lookup(ArrayLength(count));

        for (std::size_t i = 0; i < boxes.size(); ++i)
        {
            const BoxRange box = boxes[i];
            std::int32_t r = 0;
            std::int32_t g = 0;
            std::int32_t b = 0;
            const std::int32_t end = WrapAdd(box.Start, box.Length);
            for (std::int32_t j = box.Start; j < end; ++j)
            {
                const std::int32_t index = indices.at(
                    static_cast<std::size_t>(j));
                const std::int32_t offset = WrapMultiply(index, 3);
                r = WrapAdd(r, AtByte(rgb, offset));
                g = WrapAdd(g, AtByte(rgb, WrapAdd(offset, 1)));
                b = WrapAdd(b, AtByte(rgb, WrapAdd(offset, 2)));
            }
            const std::int32_t divisor
                = std::max<std::int32_t>(1, box.Length);
            r /= divisor;
            g /= divisor;
            b /= divisor;

            const std::int32_t packed = ManagedOr(
                ManagedOr(
                    ManagedShiftLeft(ManagedShiftRight(b, 3), 10),
                    ManagedShiftLeft(ManagedShiftRight(g, 3), 5)),
                ManagedShiftRight(r, 3));
            palette[i] = static_cast<std::uint16_t>(
                std::bit_cast<std::uint32_t>(packed));

            for (std::int32_t j = box.Start; j < end; ++j)
            {
                const std::int32_t index = indices.at(
                    static_cast<std::size_t>(j));
                if (index < 0
                    || static_cast<std::uint64_t>(index) >= lookup.size())
                {
                    throw System::IndexOutOfRangeException();
                }
                lookup[static_cast<std::size_t>(index)]
                    = static_cast<std::uint8_t>(i);
            }
        }
        return {std::move(palette), std::move(lookup)};
    }

    void WriteU16(std::ostream& stream, std::uint16_t value)
    {
        const std::array<char, 2> bytes{
            static_cast<char>(value & 0xFFU),
            static_cast<char>((value >> 8) & 0xFFU)
        };
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    [[nodiscard]] std::string RequirePath(
        const std::optional<std::string>& value)
    {
        if (!value.has_value())
        {
            throw System::ArgumentNullException("path");
        }
        return *value;
    }

    void EnsureOutputDirectory(
        const std::optional<std::string>& outputPath)
    {
        const std::string path = RequirePath(outputPath);
        if (path.empty())
        {
            throw System::ArgumentException();
        }
        if (path.find('\0') != std::string::npos)
        {
            throw System::ArgumentException();
        }

        const std::filesystem::path absolute
            = std::filesystem::absolute(PathFromUtf8(path)).lexically_normal();
        if (absolute == absolute.root_path())
        {
            throw System::ArgumentNullException("path");
        }
        const std::filesystem::path directory = absolute.parent_path();
        if (directory.empty())
        {
            throw System::ArgumentNullException("path");
        }
        std::filesystem::create_directories(directory);
    }

    void WritePack(
        const std::string& outputPath,
        const std::vector<BakedEntry>& entries,
        std::int32_t size)
    {
        std::ofstream stream(
            PathFromUtf8(outputPath),
            std::ios::binary | std::ios::trunc);
        stream.exceptions(std::ios::failbit | std::ios::badbit);

        static constexpr std::array<char, 4> Magic{'F', 'P', 'T', 'X'};
        stream.write(Magic.data(), static_cast<std::streamsize>(Magic.size()));
        WriteU16(stream, 1);
        WriteU16(stream, static_cast<std::uint16_t>(entries.size()));

        for (const BakedEntry& entry : entries)
        {
            const std::string& encoded = entry.Name;
            WriteU16(stream, static_cast<std::uint16_t>(entry.Index));
            WriteU16(stream, static_cast<std::uint16_t>(size));
            WriteU16(stream, static_cast<std::uint16_t>(size));
            WriteU16(stream, static_cast<std::uint16_t>(entry.Palette.size()));
            WriteU16(stream, static_cast<std::uint16_t>(encoded.size()));
            stream.write(
                encoded.data(),
                static_cast<std::streamsize>(encoded.size()));
            for (std::uint16_t colour : entry.Palette)
            {
                WriteU16(stream, colour);
            }
            if (!entry.Pixels.empty())
            {
                stream.write(
                    reinterpret_cast<const char*>(entry.Pixels.data()),
                    static_cast<std::streamsize>(entry.Pixels.size()));
            }
        }
        stream.close();
    }

    [[nodiscard]] std::int64_t FileLength(const std::string& path)
    {
        const std::uintmax_t value
            = std::filesystem::file_size(PathFromUtf8(path));
        if (value > static_cast<std::uintmax_t>(
            std::numeric_limits<std::int64_t>::max()))
        {
            throw std::overflow_error(
                "File length exceeds Int64.MaxValue.");
        }
        return static_cast<std::int64_t>(value);
    }

    [[nodiscard]] std::shared_ptr<const std::vector<std::string>>
        EmptyStrings()
    {
        static const auto value
            = std::make_shared<const std::vector<std::string>>();
        return value;
    }
}

namespace MphRead::Mods::MapGen
{
    MapTextureBake::Result::Result()
        : Missing(EmptyStrings())
    {
    }

    std::shared_ptr<MapTextureBake::Result> MapTextureBake::Bake(
        const std::shared_ptr<Q3Bsp>& bsp,
        const std::shared_ptr<
            const std::vector<std::optional<std::string>>>& archivePaths,
        const std::optional<std::string>& outputPath,
        std::int32_t size,
        bool sky)
    {
        using namespace MapTextureBakeInterop;

        ArchiveGuard archives;
        if (!archivePaths)
        {
            throw System::NullReferenceException();
        }

        for (const std::optional<std::string>& path : *archivePaths)
        {
            if (FileExists(path)
                && !OrdinalIgnoreCaseEquals(
                    GetExtension(*path), ".bsp"))
            {
                archives.Values.push_back(OpenZipRead(*path));
            }
        }

        std::vector<FileEntry> files;
        for (const std::shared_ptr<ZipArchive>& archive : archives.Values)
        {
            if (!archive)
            {
                throw System::NullReferenceException();
            }
            const std::size_t count = ZipEntryCount(*archive);
            for (std::size_t i = 0; i < count; ++i)
            {
                std::string fullName = ZipEntryFullName(*archive, i);
                if (FindFile(files, fullName) == nullptr)
                {
                    files.push_back(FileEntry{
                        archive.get(), i, std::move(fullName)
                    });
                }
            }
        }

        std::vector<BakedEntry> entries;
        auto missing = std::make_shared<std::vector<std::string>>();
        for (const auto& [index, name] : UsedTextures(bsp, sky))
        {
            std::optional<std::vector<std::uint8_t>> raw
                = Find(files, name);
            if (!raw.has_value())
            {
                missing->push_back(name);
                continue;
            }

            auto [palette, pixels]
                = Quantize(Decode(*raw, size), size);
            entries.push_back(BakedEntry{
                index,
                name,
                std::move(palette),
                std::move(pixels)
            });
        }

        EnsureOutputDirectory(outputPath);
        const std::string path = RequirePath(outputPath);
        WritePack(path, entries, size);

        auto result = std::make_shared<Result>();
        result->Baked = static_cast<std::int32_t>(entries.size());
        result->Missing = missing;
        result->Bytes = FileLength(path);
        return result;
    }
}
