#include "MapTextureBake.hpp"

#include "../../NativeRuntime/Stb/Image.hpp"
#include "../../NativeRuntime/System/ZipArchive.hpp"

#include "Q3Bsp.hpp"
#include "../../Formats/Types.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

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
#include "../../NativeRuntime/System/Sort.hpp"

using ::MphRead::NativeRuntime::FileExists;
using ::MphRead::NativeRuntime::FileInfoLength;
using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::PathGetExtension;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;

using ::MphRead::NativeRuntime::ManagedSort;
namespace MphRead::Mods::MapGen::MapTextureBakeInterop
{
    // ZipFile.OpenRead and ReFuel.Stb's RGB decode, as MapTextureBake.cs calls
    // them.
    using ZipArchive = ::MphRead::NativeRuntime::ZipArchive;

    struct DecodedImage final
    {
        std::int32_t Width = 0;
        std::int32_t Height = 0;
        std::vector<std::uint8_t> Pixels;
    };

    [[nodiscard]] inline std::shared_ptr<ZipArchive> OpenZipRead(const std::string& path)
    {
        return ZipArchive::OpenRead(path);
    }

    [[nodiscard]] inline std::size_t ZipEntryCount(const ZipArchive& archive)
    {
        return archive.Count();
    }

    [[nodiscard]] inline std::string ZipEntryFullName(
        const ZipArchive& archive, std::size_t index)
    {
        return archive.FullName(index);
    }

    [[nodiscard]] inline std::vector<std::uint8_t> ReadZipEntry(
        const ZipArchive& archive, std::size_t index)
    {
        return archive.Read(index);
    }

    [[nodiscard]] inline DecodedImage LoadRgb(std::span<const std::uint8_t> bytes)
    {
        const ::MphRead::NativeRuntime::Image image = ::MphRead::NativeRuntime::LoadPng(
            std::vector<std::uint8_t>(bytes.begin(), bytes.end()), 3);
        DecodedImage result;
        result.Width = image.Width;
        result.Height = image.Height;
        result.Pixels = image.Pixels;
        return result;
    }
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

        const std::int32_t resultLength = UncheckedMultiply(
            UncheckedMultiply(size, size), 3);
        std::vector<std::uint8_t> result(ArrayLength(resultLength));

        for (std::int32_t y = 0; y < size; ++y)
        {
            const std::int32_t y0
                = UncheckedMultiply(y, height) / size;
            const std::int32_t y1 = std::max(
                UncheckedAdd(y0, 1),
                UncheckedMultiply(UncheckedAdd(y, 1), height) / size);
            for (std::int32_t x = 0; x < size; ++x)
            {
                const std::int32_t x0
                    = UncheckedMultiply(x, width) / size;
                const std::int32_t x1 = std::max(
                    UncheckedAdd(x0, 1),
                    UncheckedMultiply(UncheckedAdd(x, 1), width) / size);

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
                        const std::int32_t offset = UncheckedMultiply(
                            UncheckedAdd(UncheckedMultiply(sy, width), sx), 3);
                        r = UncheckedAdd(r, AtByte(image.Pixels, offset));
                        g = UncheckedAdd(g, AtByte(
                            image.Pixels, UncheckedAdd(offset, 1)));
                        b = UncheckedAdd(b, AtByte(
                            image.Pixels, UncheckedAdd(offset, 2)));
                        count = UncheckedAdd(count, 1);
                    }
                }

                const std::int32_t target = UncheckedMultiply(
                    UncheckedAdd(UncheckedMultiply(y, size), x), 3);
                const std::int32_t divisor = std::max<std::int32_t>(1, count);
                SetByte(result, target,
                    static_cast<std::uint8_t>(r / divisor));
                SetByte(result, UncheckedAdd(target, 1),
                    static_cast<std::uint8_t>(g / divisor));
                SetByte(result, UncheckedAdd(target, 2),
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
            const std::int32_t leftOffset = UncheckedAdd(
                UncheckedMultiply(left, 3), _channel);
            const std::int32_t rightOffset = UncheckedAdd(
                UncheckedMultiply(right, 3), _channel);
            const std::int32_t a = AtByte(_rgb, leftOffset);
            const std::int32_t b = AtByte(_rgb, rightOffset);
            return a < b ? -1 : (a > b ? 1 : 0);
        }

    private:
        const std::vector<std::uint8_t>& _rgb;
        std::int32_t _channel;
    };

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
        ManagedSort(keys, compare);
    }

    [[nodiscard]] std::pair<
        std::vector<std::uint16_t>,
        std::vector<std::uint8_t>>
        Quantize(
            const std::vector<std::uint8_t>& rgb,
            std::int32_t size)
    {
        const std::int32_t count = UncheckedMultiply(size, size);
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
                    const std::int32_t end = UncheckedAdd(
                        box.Start, box.Length);
                    for (std::int32_t j = box.Start; j < end; ++j)
                    {
                        const std::int32_t index = indices.at(
                            static_cast<std::size_t>(j));
                        const std::int32_t offset = UncheckedAdd(
                            UncheckedMultiply(index, 3), channel);
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
                UncheckedAdd(selected.Start, half),
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
            const std::int32_t end = UncheckedAdd(box.Start, box.Length);
            for (std::int32_t j = box.Start; j < end; ++j)
            {
                const std::int32_t index = indices.at(
                    static_cast<std::size_t>(j));
                const std::int32_t offset = UncheckedMultiply(index, 3);
                r = UncheckedAdd(r, AtByte(rgb, offset));
                g = UncheckedAdd(g, AtByte(rgb, UncheckedAdd(offset, 1)));
                b = UncheckedAdd(b, AtByte(rgb, UncheckedAdd(offset, 2)));
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
                    PathGetExtension(*path), ".bsp"))
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
        result->Bytes = FileInfoLength(path);
        return result;
    }
}
