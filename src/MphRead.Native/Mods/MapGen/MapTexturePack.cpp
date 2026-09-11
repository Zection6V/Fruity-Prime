#include "MapTexturePack.hpp"

#include "../../Program.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ios>
#include <istream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{
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

    [[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path)
    {
#if defined(__cpp_char8_t)
        const std::u8string value = path.u8string();
        std::string result;
        result.reserve(value.size());
        for (char8_t ch : value)
        {
            result.push_back(static_cast<char>(ch));
        }
        return result;
#else
        return path.u8string();
#endif
    }

    [[nodiscard]] std::string GetFileName(std::string_view path)
    {
        return PathToUtf8(PathFromUtf8(path).filename());
    }

    void AppendReplacement(std::string& output)
    {
        output.push_back(static_cast<char>(0xEF));
        output.push_back(static_cast<char>(0xBF));
        output.push_back(static_cast<char>(0xBD));
    }

    [[nodiscard]] bool IsContinuation(std::uint8_t value) noexcept
    {
        return (value & 0xC0U) == 0x80U;
    }

    // Encoding.UTF8 uses replacement fallback rather than throwing. Preserve
    // valid UTF-8 byte-for-byte and replace each maximal invalid subpart with
    // U+FFFD, including a truncated valid prefix at the end of the name.
    [[nodiscard]] std::string DecodeUtf8(const std::vector<std::uint8_t>& bytes)
    {
        std::string output;
        output.reserve(bytes.size());

        for (std::size_t i = 0; i < bytes.size();)
        {
            const std::uint8_t first = bytes[i];
            if (first <= 0x7FU)
            {
                output.push_back(static_cast<char>(first));
                ++i;
                continue;
            }

            std::size_t length = 0;
            std::uint8_t secondLow = 0x80U;
            std::uint8_t secondHigh = 0xBFU;
            if (first >= 0xC2U && first <= 0xDFU)
            {
                length = 2;
            }
            else if (first >= 0xE0U && first <= 0xEFU)
            {
                length = 3;
                if (first == 0xE0U)
                {
                    secondLow = 0xA0U;
                }
                else if (first == 0xEDU)
                {
                    secondHigh = 0x9FU;
                }
            }
            else if (first >= 0xF0U && first <= 0xF4U)
            {
                length = 4;
                if (first == 0xF0U)
                {
                    secondLow = 0x90U;
                }
                else if (first == 0xF4U)
                {
                    secondHigh = 0x8FU;
                }
            }
            else
            {
                AppendReplacement(output);
                ++i;
                continue;
            }

            if (i + 1 >= bytes.size())
            {
                AppendReplacement(output);
                ++i;
                continue;
            }

            const std::uint8_t second = bytes[i + 1];
            if (second < secondLow || second > secondHigh)
            {
                AppendReplacement(output);
                ++i;
                continue;
            }

            std::size_t validPrefix = 2;
            bool valid = true;
            for (std::size_t part = 2; part < length; ++part)
            {
                if (i + part >= bytes.size())
                {
                    valid = false;
                    break;
                }
                if (!IsContinuation(bytes[i + part]))
                {
                    valid = false;
                    break;
                }
                ++validPrefix;
            }

            if (!valid)
            {
                AppendReplacement(output);
                i += validPrefix;
                continue;
            }

            for (std::size_t part = 0; part < length; ++part)
            {
                output.push_back(static_cast<char>(bytes[i + part]));
            }
            i += length;
        }
        return output;
    }

    class BinaryReader final
    {
    public:
        explicit BinaryReader(std::istream& stream) noexcept : _stream(stream)
        {
        }

        [[nodiscard]] std::uint16_t ReadUInt16()
        {
            std::vector<std::uint8_t> bytes = ReadBytes(2);
            if (bytes.size() != 2)
            {
                throw std::runtime_error("Unable to read beyond the end of the stream.");
            }
            return static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(bytes[0])
                | (static_cast<std::uint16_t>(bytes[1]) << 8));
        }

        [[nodiscard]] std::vector<std::uint8_t> ReadBytes(std::int32_t count)
        {
            if (count < 0)
            {
                throw std::out_of_range("count");
            }

            std::vector<std::uint8_t> result(static_cast<std::size_t>(count));
            std::size_t total = 0;
            while (total < result.size())
            {
                const std::size_t remaining = result.size() - total;
                const std::size_t maximum = static_cast<std::size_t>(
                    std::numeric_limits<std::streamsize>::max());
                const std::streamsize request = static_cast<std::streamsize>(
                    std::min(remaining, maximum));
                _stream.read(reinterpret_cast<char*>(result.data() + total), request);
                const std::streamsize read = _stream.gcount();
                if (read > 0)
                {
                    total += static_cast<std::size_t>(read);
                }
                if (read != request)
                {
                    if (_stream.bad())
                    {
                        throw std::ios_base::failure("I/O error while reading stream.");
                    }
                    break;
                }
            }
            result.resize(total);
            return result;
        }

    private:
        std::istream& _stream;
    };

    [[nodiscard]] std::int32_t PixelCount(std::uint16_t width, std::uint16_t height) noexcept
    {
        const std::uint32_t product = static_cast<std::uint32_t>(width)
            * static_cast<std::uint32_t>(height);
        if (product <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
        {
            return static_cast<std::int32_t>(product);
        }
        return static_cast<std::int32_t>(
            static_cast<std::int64_t>(product) - (static_cast<std::int64_t>(1) << 32));
    }
}

namespace MphRead::Mods::MapGen
{
    MapTexturePack::Entry::Entry(
        std::int32_t sourceIndex,
        std::string name,
        std::uint16_t width,
        std::uint16_t height,
        std::vector<std::uint16_t> palette,
        std::vector<std::uint8_t> pixels)
        : _sourceIndex(sourceIndex),
          _name(std::move(name)),
          _width(width),
          _height(height),
          _palette(std::move(palette)),
          _pixels(std::move(pixels))
    {
    }

    std::int32_t MapTexturePack::Entry::SourceIndex() const noexcept
    {
        return _sourceIndex;
    }

    const std::string& MapTexturePack::Entry::Name() const noexcept
    {
        return _name;
    }

    std::uint16_t MapTexturePack::Entry::Width() const noexcept
    {
        return _width;
    }

    std::uint16_t MapTexturePack::Entry::Height() const noexcept
    {
        return _height;
    }

    const std::vector<std::uint16_t>& MapTexturePack::Entry::Palette() const noexcept
    {
        return _palette;
    }

    const std::vector<std::uint8_t>& MapTexturePack::Entry::Pixels() const noexcept
    {
        return _pixels;
    }

    std::int32_t MapTexturePack::SourceIndexDictionary::Count() const noexcept
    {
        return static_cast<std::int32_t>(_values.size());
    }

    bool MapTexturePack::SourceIndexDictionary::TryGetValue(
        std::int32_t key,
        std::int32_t& value) const noexcept
    {
        for (const Value& pair : _values)
        {
            if (pair.first == key)
            {
                value = pair.second;
                return true;
            }
        }
        value = 0;
        return false;
    }

    const std::int32_t& MapTexturePack::SourceIndexDictionary::operator[](std::int32_t key) const
    {
        for (const Value& pair : _values)
        {
            if (pair.first == key)
            {
                return pair.second;
            }
        }
        throw std::out_of_range("The given key was not present in the dictionary.");
    }

    MapTexturePack::SourceIndexDictionary::const_iterator
    MapTexturePack::SourceIndexDictionary::begin() const noexcept
    {
        return _values.begin();
    }

    MapTexturePack::SourceIndexDictionary::const_iterator
    MapTexturePack::SourceIndexDictionary::end() const noexcept
    {
        return _values.end();
    }

    void MapTexturePack::SourceIndexDictionary::Set(std::int32_t key, std::int32_t value)
    {
        for (Value& pair : _values)
        {
            if (pair.first == key)
            {
                pair.second = value;
                return;
            }
        }
        _values.emplace_back(key, value);
    }

    MapTexturePack::MapTexturePack(std::vector<Entry> entries)
        : _entries(std::move(entries))
    {
        for (std::size_t i = 0; i < _entries.size(); ++i)
        {
            _bySourceIndex.Set(
                _entries[i].SourceIndex(),
                static_cast<std::int32_t>(i));
        }
    }

    const std::vector<MapTexturePack::Entry>& MapTexturePack::Entries() const noexcept
    {
        return _entries;
    }

    const MapTexturePack::SourceIndexDictionary& MapTexturePack::BySourceIndex() const noexcept
    {
        return _bySourceIndex;
    }

    MapTexturePack MapTexturePack::Load(
        const std::vector<std::uint8_t>& bytes,
        const std::string& name)
    {
        const std::string storage(bytes.begin(), bytes.end());
        std::istringstream memory(storage, std::ios::in | std::ios::binary);
        return Load(memory, name);
    }

    MapTexturePack MapTexturePack::Load(const std::string& path)
    {
        std::ifstream stream(PathFromUtf8(path), std::ios::in | std::ios::binary);
        if (!stream.is_open())
        {
            throw std::ios_base::failure("Could not open file for reading: " + path);
        }
        stream.exceptions(std::ios::badbit);
        return Load(stream, GetFileName(path));
    }

    MapTexturePack MapTexturePack::Load(std::istream& stream, const std::string& path)
    {
        BinaryReader reader(stream);
        const std::vector<std::uint8_t> magic = reader.ReadBytes(4);
        if (magic.size() != 4
            || magic[0] != static_cast<std::uint8_t>('F')
            || magic[1] != static_cast<std::uint8_t>('P')
            || magic[2] != static_cast<std::uint8_t>('T')
            || magic[3] != static_cast<std::uint8_t>('X'))
        {
            throw ProgramException(GetFileName(path) + " is not a texture pack.");
        }

        const std::uint16_t version = reader.ReadUInt16();
        if (version != 1)
        {
            throw ProgramException(
                GetFileName(path) + " is version " + std::to_string(version)
                + "; this build reads 1.");
        }

        const std::int32_t count = reader.ReadUInt16();
        std::vector<Entry> entries;
        entries.reserve(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count; ++i)
        {
            const std::uint16_t sourceIndex = reader.ReadUInt16();
            const std::uint16_t width = reader.ReadUInt16();
            const std::uint16_t height = reader.ReadUInt16();
            const std::int32_t paletteLength = reader.ReadUInt16();
            const std::int32_t nameLength = reader.ReadUInt16();
            const std::string name = DecodeUtf8(reader.ReadBytes(nameLength));

            std::vector<std::uint16_t> palette(static_cast<std::size_t>(paletteLength));
            for (std::int32_t p = 0; p < paletteLength; ++p)
            {
                palette[static_cast<std::size_t>(p)] = reader.ReadUInt16();
            }

            std::vector<std::uint8_t> pixels = reader.ReadBytes(PixelCount(width, height));
            entries.emplace_back(
                static_cast<std::int32_t>(sourceIndex),
                name,
                width,
                height,
                std::move(palette),
                std::move(pixels));
        }
        return MapTexturePack(std::move(entries));
    }
}
