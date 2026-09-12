#include "Extract.hpp"

#include "../Metadata/SoundMeta.hpp"
#include "../Program.hpp"
#include "../Read.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace MphRead::ExtractDependency
{
    // These are the only unavoidable dependency adapters in this pair.
    // The corresponding Native counterparts do not exist yet on develop2.
    void PathsUpdatePaths();
    void PathsSetPath(const std::string& key, const std::string& value);
    [[nodiscard]] const std::string& PathsMphKey();
    [[nodiscard]] const std::string& PathsValue(const std::string& key);

    void SetNormalFontData(
        const std::vector<std::uint8_t>& widths,
        const std::vector<std::uint8_t>& offsets,
        const std::vector<std::uint8_t>& characters,
        std::int32_t minChar);

    void LzBackwardDecompress(const std::string& source, const std::string& destination);

    struct NcsfTag final
    {
        std::string Name;
        std::string Value;
    };

    class NcsfSdat
    {
    public:
        virtual ~NcsfSdat() = default;

        virtual void Read(const std::string& filename, std::span<const std::uint8_t> bytes) = 0;
        [[nodiscard]] virtual std::unique_ptr<NcsfSdat> Add(const NcsfSdat& other) const = 0;
        virtual void FixOffsetsAndSizes() = 0;
        [[nodiscard]] virtual std::uint32_t Size() const = 0;
        virtual void Write(std::span<std::uint8_t> bytes) const = 0;

        [[nodiscard]] virtual std::size_t SequenceCount() const = 0;
        [[nodiscard]] virtual std::uint32_t SequenceOffset(std::size_t index) const = 0;
        [[nodiscard]] virtual bool SequencePresent(std::size_t index) const = 0;
        [[nodiscard]] virtual std::string SequenceFilename(std::size_t index) const = 0;
        virtual void SetSequenceFilename(std::size_t index, const std::string& filename) = 0;
        [[nodiscard]] virtual std::string SequenceSseqOriginalFilename(std::size_t index) const = 0;
        [[nodiscard]] virtual std::string SequenceSdatNumber(std::size_t index) const = 0;
        [[nodiscard]] virtual std::string SequenceFullFilename(
            std::size_t index, bool multipleSdats) const = 0;
    };

    class AlbumGain
    {
    public:
        virtual ~AlbumGain() = default;
    };

    [[nodiscard]] std::unique_ptr<NcsfSdat> CreateNcsfSdat();
    [[nodiscard]] std::unique_ptr<AlbumGain> CreateAlbumGain();
    void MakeNcsf(
        const std::string& filename,
        std::span<const std::uint8_t> reservedSection,
        std::span<const std::uint8_t> programSection);
    void MakeNcsf(
        const std::string& filename,
        std::span<const std::uint8_t> reservedSection,
        std::span<const std::uint8_t> programSection,
        const std::vector<NcsfTag>& tags);
}

namespace
{
    using namespace MphRead;

    struct RomDataValues final
    {
        std::string File;
        std::int32_t Offset;
        std::int32_t Size;

        RomDataValues(std::string file, std::int32_t offset, std::int32_t size)
            : File(std::move(file)), Offset(offset), Size(size)
        {
        }
    };

    struct RomData final
    {
        std::shared_ptr<RomDataValues> FontModel;
        std::shared_ptr<RomDataValues> FontWidths;
        std::shared_ptr<RomDataValues> FontOffsets;
        std::shared_ptr<RomDataValues> FontCharData;
        std::shared_ptr<RomDataValues> TerrianSfx;
        std::shared_ptr<RomDataValues> BeamSfx;
        std::shared_ptr<RomDataValues> HunterSfx;
        std::shared_ptr<RomDataValues> EnemyDamageSfx;
        std::shared_ptr<RomDataValues> EnemyDeathSfx;
        std::shared_ptr<RomDataValues> PlatformSfx;
    };

    [[nodiscard]] std::shared_ptr<RomDataValues> DataValue(
        const char* file, std::int32_t offset, std::int32_t size)
    {
        return std::make_shared<RomDataValues>(file, offset, size);
    }

    [[nodiscard]] const std::unordered_map<std::string, RomData>& RomDataTable()
    {
        static const std::unordered_map<std::string, RomData> table = {
            {
                "A76E0",
                RomData{
                    DataValue("arm9.bin", 0x9D528, 0x8284),
                    DataValue("arm9.bin", 0x95C68, 480),
                    DataValue("arm9.bin", 0x95A88, 480),
                    DataValue("arm9.bin", 0x96348, 0x4000),
                    DataValue("overlay9_2", 0x1D828, 144),
                    DataValue("overlay9_2", 0x1D8B8, 180),
                    DataValue("overlay9_2", 0x1D96C, 272),
                    DataValue("arm9.bin", 0x9B574, 208),
                    DataValue("arm9.bin", 0x9B644, 208),
                    DataValue("overlay9_12", 0x81E4, 360)
                }
            },
            {
                "AMHE0",
                RomData{
                    DataValue("arm9.bin", 0xC76D4, 0x8284),
                    DataValue("arm9.bin", 0xBF9B0, 480),
                    DataValue("arm9.bin", 0xBFB90, 480),
                    DataValue("arm9.bin", 0xC0270, 0x4000),
                    DataValue("overlay9_2", 0x1DA08, 144),
                    DataValue("overlay9_2", 0x1DA98, 180),
                    DataValue("overlay9_2", 0x1DB4C, 272),
                    DataValue("arm9.bin", 0xC54A8, 208),
                    DataValue("arm9.bin", 0xC5578, 208),
                    DataValue("overlay9_15", 0x8284, 360)
                }
            },
            {
                "AMHE1",
                RomData{
                    DataValue("arm9.bin", 0xC7F5C, 0x8284),
                    DataValue("arm9.bin", 0xC020C, 480),
                    DataValue("arm9.bin", 0xC03EC, 480),
                    DataValue("arm9.bin", 0xC0ACC, 0x4000),
                    DataValue("overlay9_2", 0x1DA68, 144),
                    DataValue("overlay9_2", 0x1DAF8, 180),
                    DataValue("overlay9_2", 0x1DBAC, 272),
                    DataValue("arm9.bin", 0xC5D30, 208),
                    DataValue("arm9.bin", 0xC5E00, 208),
                    DataValue("overlay9_15", 0x8284, 360)
                }
            },
            {
                "AMHJ0",
                RomData{
                    DataValue("arm9.bin", 0xC9510, 0x8284),
                    DataValue("arm9.bin", 0xC1754, 480),
                    DataValue("arm9.bin", 0xC1934, 480),
                    DataValue("arm9.bin", 0xC2014, 0x4000),
                    DataValue("overlay9_2", 0x1DA68, 144),
                    DataValue("overlay9_2", 0x1DAF8, 180),
                    DataValue("overlay9_2", 0x1DBAC, 272),
                    DataValue("arm9.bin", 0xC7278, 208),
                    DataValue("arm9.bin", 0xC7348, 208),
                    DataValue("overlay9_15", 0x8284, 360)
                }
            },
            {
                "AMHJ1",
                RomData{
                    DataValue("arm9.bin", 0xC94D0, 0x8284),
                    DataValue("arm9.bin", 0xC1714, 480),
                    DataValue("arm9.bin", 0xC18F4, 480),
                    DataValue("arm9.bin", 0xC1FD4, 0x4000),
                    DataValue("overlay9_2", 0x1DA68, 144),
                    DataValue("overlay9_2", 0x1DAF8, 180),
                    DataValue("overlay9_2", 0x1DBAC, 272),
                    DataValue("arm9.bin", 0xC7238, 208),
                    DataValue("arm9.bin", 0xC7308, 208),
                    DataValue("overlay9_15", 0x8284, 360)
                }
            },
            {
                "AMHP0",
                RomData{
                    DataValue("arm9.bin", 0xC7F7C, 0x8284),
                    DataValue("arm9.bin", 0xC022C, 480),
                    DataValue("arm9.bin", 0xC040C, 480),
                    DataValue("arm9.bin", 0xC0AEC, 0x4000),
                    DataValue("overlay9_2", 0x1DA08, 144),
                    DataValue("overlay9_2", 0x1DA98, 180),
                    DataValue("overlay9_2", 0x1DB4C, 272),
                    DataValue("arm9.bin", 0xC5D50, 208),
                    DataValue("arm9.bin", 0xC5E20, 208),
                    DataValue("overlay9_15", 0x8284, 360)
                }
            },
            {
                "AMHP1",
                RomData{
                    DataValue("arm9.bin", 0xC7FFC, 0x8284),
                    DataValue("arm9.bin", 0xC02AC, 480),
                    DataValue("arm9.bin", 0xC048C, 480),
                    DataValue("arm9.bin", 0xC0B6C, 0x4000),
                    DataValue("overlay9_2", 0x1DA68, 144),
                    DataValue("overlay9_2", 0x1DAF8, 180),
                    DataValue("overlay9_2", 0x1DBAC, 272),
                    DataValue("arm9.bin", 0xC5DD0, 208),
                    DataValue("arm9.bin", 0xC5EA0, 208),
                    DataValue("overlay9_15", 0x8284, 360)
                }
            },
            {
                "AMHK0",
                RomData{
                    DataValue("arm9.bin", 0xC0D40, 0x8284),
                    DataValue("arm9.bin", 0xBD580, 480),
                    DataValue("arm9.bin", 0xBD760, 480),
                    DataValue("arm9.bin", 0xB9560, 0x4000),
                    DataValue("overlay9_2", 0x1BDBA, 144),
                    DataValue("overlay9_2", 0x1BE4A, 180),
                    DataValue("overlay9_2", 0x1BEFE, 272),
                    DataValue("arm9.bin", 0xBE4DC, 208),
                    DataValue("arm9.bin", 0xBE5AC, 208),
                    DataValue("overlay9_15", 0x7CC0, 360)
                }
            },
            {
                "NTRJ0",
                RomData{
                    DataValue("arm9.bin", 0xED610, 0x8284),
                    DataValue("arm9.bin", 0x1FC07C, 480),
                    DataValue("arm9.bin", 0x1FC25C, 480),
                    DataValue("arm9.bin", 0x1FC93C, 0x4000),
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr
                }
            }
        };
        return table;
    }

    [[nodiscard]] const RomData* FindRomData(const std::string& key) noexcept
    {
        const auto& table = RomDataTable();
        const auto iterator = table.find(key);
        return iterator == table.end() ? nullptr : std::addressof(iterator->second);
    }

    [[nodiscard]] const RomDataValues& Require(const std::shared_ptr<RomDataValues>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
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

    [[nodiscard]] std::vector<std::uint8_t> FileReadAllBytes(const std::string& path)
    {
        std::ifstream stream(PathFromUtf8(path), std::ios::binary | std::ios::ate);
        if (!stream)
        {
            throw std::ios_base::failure("Could not open file: " + path);
        }
        const std::streampos end = stream.tellg();
        if (end < 0)
        {
            throw std::ios_base::failure("Could not determine file length: " + path);
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
        stream.seekg(0, std::ios::beg);
        if (!bytes.empty())
        {
            stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (!stream)
            {
                throw std::ios_base::failure("Could not read file: " + path);
            }
        }
        return bytes;
    }

    void FileWriteAllBytes(const std::string& path, std::span<const std::uint8_t> bytes)
    {
        std::ofstream stream(PathFromUtf8(path), std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::ios_base::failure("Could not open file for writing: " + path);
        }
        if (!bytes.empty())
        {
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        if (!stream)
        {
            throw std::ios_base::failure("Could not write file: " + path);
        }
    }

    void FileWriteAllText(const std::string& path, std::string_view text)
    {
        std::ofstream stream(PathFromUtf8(path), std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::ios_base::failure("Could not open file for writing: " + path);
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw std::ios_base::failure("Could not write file: " + path);
        }
    }

    [[nodiscard]] bool FileExists(const std::string& path) noexcept
    {
        try
        {
            std::error_code error;
            const bool exists = std::filesystem::is_regular_file(PathFromUtf8(path), error);
            return !error && exists;
        }
        catch (...)
        {
            return false;
        }
    }

    void CreateDirectory(const std::string& path)
    {
        std::filesystem::create_directories(PathFromUtf8(path));
    }

    [[nodiscard]] std::string GetFullPath(const std::string& path)
    {
        return PathToUtf8(std::filesystem::absolute(PathFromUtf8(path)).lexically_normal());
    }

    [[nodiscard]] std::string GetFileName(const std::string& path)
    {
        return PathToUtf8(PathFromUtf8(path).filename());
    }

    [[nodiscard]] std::string GetExtension(const std::string& path)
    {
        return PathToUtf8(PathFromUtf8(path).extension());
    }

    [[nodiscard]] std::uint32_t DecodeUtf8(std::string_view text, std::size_t& index) noexcept
    {
        const auto first = static_cast<unsigned char>(text[index]);
        if (first <= 0x7FU)
        {
            ++index;
            return first;
        }

        std::uint32_t value = 0;
        std::size_t length = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xE0U) == 0xC0U)
        {
            value = first & 0x1FU;
            length = 2;
            minimum = 0x80U;
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            value = first & 0x0FU;
            length = 3;
            minimum = 0x800U;
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            value = first & 0x07U;
            length = 4;
            minimum = 0x10000U;
        }
        else
        {
            ++index;
            return 0xFFFDU;
        }

        if (index + length > text.size())
        {
            ++index;
            return 0xFFFDU;
        }

        for (std::size_t i = 1; i < length; ++i)
        {
            const auto next = static_cast<unsigned char>(text[index + i]);
            if ((next & 0xC0U) != 0x80U)
            {
                ++index;
                return 0xFFFDU;
            }
            value = (value << 6) | (next & 0x3FU);
        }
        if (value < minimum || value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU))
        {
            ++index;
            return 0xFFFDU;
        }
        index += length;
        return value;
    }

    [[nodiscard]] bool IsDotNetWhitespace(std::uint32_t codePoint) noexcept
    {
        if (codePoint >= 0x0009U && codePoint <= 0x000DU)
        {
            return true;
        }
        switch (codePoint)
        {
        case 0x0020U:
        case 0x0085U:
        case 0x00A0U:
        case 0x1680U:
        case 0x2000U:
        case 0x2001U:
        case 0x2002U:
        case 0x2003U:
        case 0x2004U:
        case 0x2005U:
        case 0x2006U:
        case 0x2007U:
        case 0x2008U:
        case 0x2009U:
        case 0x200AU:
        case 0x2028U:
        case 0x2029U:
        case 0x202FU:
        case 0x205FU:
        case 0x3000U:
            return true;
        default:
            return false;
        }
    }

    struct Utf8Position final
    {
        std::uint32_t CodePoint;
        std::size_t Start;
        std::size_t End;
    };

    [[nodiscard]] std::vector<Utf8Position> DecodeUtf8Positions(std::string_view text)
    {
        std::vector<Utf8Position> result;
        std::size_t index = 0;
        while (index < text.size())
        {
            const std::size_t start = index;
            const std::uint32_t value = DecodeUtf8(text, index);
            result.push_back(Utf8Position{value, start, index});
        }
        return result;
    }

    [[nodiscard]] bool IsNullOrWhiteSpace(const std::string& value)
    {
        if (value.empty())
        {
            return true;
        }
        for (const Utf8Position& position : DecodeUtf8Positions(value))
        {
            if (!IsDotNetWhitespace(position.CodePoint))
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::string TrimDotNetWhitespace(std::string value)
    {
        const auto positions = DecodeUtf8Positions(value);
        std::size_t first = 0;
        std::size_t last = positions.size();
        while (first < last && IsDotNetWhitespace(positions[first].CodePoint))
        {
            ++first;
        }
        while (last > first && IsDotNetWhitespace(positions[last - 1].CodePoint))
        {
            --last;
        }
        if (first == last)
        {
            return {};
        }
        return value.substr(positions[first].Start, positions[last - 1].End - positions[first].Start);
    }

    [[nodiscard]] std::string ToLowerForPrompt(std::string value)
    {
        for (char& ch : value)
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                ch = static_cast<char>(ch - 'A' + 'a');
            }
        }
        return value;
    }

    [[nodiscard]] std::string ReadLineOrEmpty()
    {
        std::string input;
        if (!std::getline(std::cin, input))
        {
            return {};
        }
        return input;
    }

    void ReadKeyWithEcho()
    {
#if defined(_WIN32)
        (void)_getche();
#else
        if (::isatty(STDIN_FILENO) != 0)
        {
            termios original{};
            if (::tcgetattr(STDIN_FILENO, &original) == 0)
            {
                termios raw = original;
                raw.c_lflag &= static_cast<tcflag_t>(~ICANON);
                raw.c_cc[VMIN] = 1;
                raw.c_cc[VTIME] = 0;
                if (::tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0)
                {
                    unsigned char byte = 0;
                    (void)::read(STDIN_FILENO, &byte, 1);
                    (void)::tcsetattr(STDIN_FILENO, TCSANOW, &original);
                    return;
                }
            }
        }
        (void)std::cin.get();
#endif
    }

    [[nodiscard]] std::int32_t ManagedAdd(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] std::vector<std::uint8_t> Slice(
        const std::vector<std::uint8_t>& bytes, std::int32_t start, std::int32_t end)
    {
        if (start < 0 || end < 0 || end < start
            || static_cast<std::uint64_t>(end) > bytes.size())
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return std::vector<std::uint8_t>(
            bytes.begin() + static_cast<std::ptrdiff_t>(start),
            bytes.begin() + static_cast<std::ptrdiff_t>(end));
    }

    [[nodiscard]] std::uint8_t AtByte(
        const std::vector<std::uint8_t>& bytes, std::uint32_t index)
    {
        if (index >= bytes.size())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        return bytes[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] const T& AtManagedIndex(const std::vector<T>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw std::out_of_range(
                "Index was out of range. Must be non-negative and less than the size of the collection. (Parameter 'index')");
        }
        return values[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] std::int32_t BitConverterToInt32(std::span<const std::uint8_t, 4> bytes) noexcept
    {
        std::int32_t result = 0;
        std::memcpy(static_cast<void*>(std::addressof(result)), bytes.data(), sizeof(result));
        return result;
    }

    [[nodiscard]] std::array<std::uint8_t, 4> BitConverterGetBytes(std::uint32_t value) noexcept
    {
        std::array<std::uint8_t, 4> result{};
        std::memcpy(result.data(), std::addressof(value), sizeof(value));
        return result;
    }

    [[nodiscard]] std::string ToLowerAscii(std::string value)
    {
        for (char& ch : value)
        {
            const unsigned char byte = static_cast<unsigned char>(ch);
            if (byte >= static_cast<unsigned char>('A') && byte <= static_cast<unsigned char>('Z'))
            {
                ch = static_cast<char>(byte - static_cast<unsigned char>('A') + static_cast<unsigned char>('a'));
            }
        }
        return value;
    }

    [[nodiscard]] bool StartsWith(std::string_view value, std::string_view prefix) noexcept
    {
        return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
    }

    [[nodiscard]] std::string HexUpper4(std::uint32_t value)
    {
        std::ostringstream stream;
        stream << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << value;
        return stream.str();
    }

    [[nodiscard]] std::string ProgramVersionToString()
    {
        static_assert(std::is_trivially_copyable_v<System::Version>);
        static_assert(sizeof(System::Version) == sizeof(std::int32_t) * 4);
        std::array<std::int32_t, 4> values{};
        std::memcpy(values.data(), std::addressof(Program::Version), sizeof(Program::Version));

        std::string result = std::to_string(values[0]) + "." + std::to_string(values[1]);
        if (values[2] >= 0)
        {
            result += "." + std::to_string(values[2]);
            if (values[3] >= 0)
            {
                result += "." + std::to_string(values[3]);
            }
        }
        return result;
    }

    [[nodiscard]] const char* EnvironmentNewLine() noexcept
    {
#if defined(_WIN32)
        return "\r\n";
#else
        return "\n";
#endif
    }

    void AddOrReplaceTag(
        std::vector<ExtractDependency::NcsfTag>& tags,
        ExtractDependency::NcsfTag item)
    {
        auto equalsInvariantIgnoreCaseAscii = [](std::string_view left, std::string_view right)
        {
            if (left.size() != right.size())
            {
                return false;
            }
            for (std::size_t i = 0; i < left.size(); ++i)
            {
                unsigned char l = static_cast<unsigned char>(left[i]);
                unsigned char r = static_cast<unsigned char>(right[i]);
                if (l >= 'A' && l <= 'Z')
                {
                    l = static_cast<unsigned char>(l - 'A' + 'a');
                }
                if (r >= 'A' && r <= 'Z')
                {
                    r = static_cast<unsigned char>(r - 'A' + 'a');
                }
                if (l != r)
                {
                    return false;
                }
            }
            return true;
        };

        for (std::size_t i = 0; i < tags.size(); ++i)
        {
            if (equalsInvariantIgnoreCaseAscii(tags[i].Name, item.Name))
            {
                tags[i] = std::move(item);
                return;
            }
        }
        tags.push_back(std::move(item));
    }

    [[nodiscard]] bool ContainsVersion(
        const std::vector<std::uint8_t>& versions, std::uint8_t version)
    {
        return std::find(versions.begin(), versions.end(), version) != versions.end();
    }

    void PrintExit(const std::string& message)
    {
        std::cout << message << '\n';
        std::cout << "Press any key to exit..." << '\n';
        std::cout.flush();
        ReadKeyWithEcho();
    }

    void Nop()
    {
    }

    void ConvertSdat(const std::string& inputPath, const std::string& outputDir)
    {
        const std::vector<std::uint8_t> sdatStorage = FileReadAllBytes(inputPath);
        const std::span<const std::uint8_t> sdatBytes(sdatStorage);

        std::unique_ptr<ExtractDependency::NcsfSdat> finalSdat
            = ExtractDependency::CreateNcsfSdat();
        const std::int32_t sdatNumber = 1;
        std::unique_ptr<ExtractDependency::NcsfSdat> sdat
            = ExtractDependency::CreateNcsfSdat();
        sdat->Read(std::to_string(sdatNumber), sdatBytes);
        finalSdat = finalSdat->Add(*sdat);
        finalSdat->FixOffsetsAndSizes();

        const std::int32_t managedSize = std::bit_cast<std::int32_t>(finalSdat->Size());
        if (managedSize < 0)
        {
            throw std::out_of_range("Non-negative number required. (Parameter 'length')");
        }
        std::unique_ptr<std::uint8_t[]> memoryOwner(new std::uint8_t[
            static_cast<std::size_t>(managedSize)]);
        std::span<std::uint8_t> memorySpan(
            memoryOwner.get(), static_cast<std::size_t>(managedSize));
        finalSdat->Write(memorySpan);

        const std::size_t seqEntryCount = finalSdat->SequenceCount();
        const std::string ncsflibFilename = "mph.ncsflib";
        ExtractDependency::MakeNcsf(
            Paths::Combine(outputDir, ncsflibFilename),
            std::span<const std::uint8_t>(),
            std::span<const std::uint8_t>(memorySpan));

        const std::vector<ExtractDependency::NcsfTag> tags = {
            {"_lib", ncsflibFilename},
            {"utf8", "1"},
            {"ncsfby", "MphRead"}
        };
        std::unique_ptr<ExtractDependency::AlbumGain> albumGain
            = ExtractDependency::CreateAlbumGain();
        (void)albumGain;

        std::unordered_map<std::uint32_t, std::vector<ExtractDependency::NcsfTag>> fileTags;
        fileTags.reserve(seqEntryCount);

        for (std::uint32_t i = 0, count = static_cast<std::uint32_t>(seqEntryCount);
            i < count; ++i)
        {
            const std::size_t index = static_cast<std::size_t>(i);
            const std::uint32_t offset = finalSdat->SequenceOffset(index);
            if (offset != 0 && finalSdat->SequencePresent(index))
            {
                std::string filename = finalSdat->SequenceFilename(index);
                if (StartsWith(filename, "SSEQ"))
                {
                    filename = HexUpper4(i) + " - " + filename;
                    finalSdat->SetSequenceFilename(index, filename);
                }

                const std::string minincsfFilename
                    = finalSdat->SequenceFilename(index) + ".minincsf";
                (void)minincsfFilename;

                std::vector<ExtractDependency::NcsfTag> thisTags = tags;
                const std::string fullFilename
                    = finalSdat->SequenceFullFilename(index, sdatNumber > 1);
                (void)fullFilename;

                AddOrReplaceTag(
                    thisTags,
                    {"origFilename", finalSdat->SequenceSseqOriginalFilename(index)});
                if (sdatNumber > 1)
                {
                    AddOrReplaceTag(
                        thisTags,
                        {"origSDAT", finalSdat->SequenceSdatNumber(index)});
                }
                fileTags[i] = std::move(thisTags);
            }
        }

        for (std::uint32_t i = 0, count = static_cast<std::uint32_t>(seqEntryCount);
            i < count; ++i)
        {
            const std::size_t index = static_cast<std::size_t>(i);
            const std::uint32_t offset = finalSdat->SequenceOffset(index);
            if (offset != 0 && finalSdat->SequencePresent(index))
            {
                const std::string minincsfFilename
                    = finalSdat->SequenceFilename(index) + ".minincsf";
                const auto& thisTags = fileTags.at(i);
                const std::array<std::uint8_t, 4> reserved = BitConverterGetBytes(i);
                ExtractDependency::MakeNcsf(
                    Paths::Combine(outputDir, minincsfFilename),
                    std::span<const std::uint8_t>(reserved),
                    std::span<const std::uint8_t>(),
                    thisTags);
            }
        }
    }

    void ExtractRomData(const std::string& rootName)
    {
        const RomData* data = FindRomData(rootName);
        if (data == nullptr)
        {
            return;
        }

        const RomDataValues& fontModel = Require(data->FontModel);
        const std::vector<std::uint8_t> bytes = FileReadAllBytes(
            Paths::Combine("files", rootName, "_bin", fontModel.File));
        const std::int32_t end = ManagedAdd(fontModel.Offset, fontModel.Size);
        const std::vector<std::uint8_t> fontBytes = Slice(bytes, fontModel.Offset, end);
        FileWriteAllBytes(
            Paths::Combine("files", rootName, "models\\hudfont_Model.bin"),
            fontBytes);
    }

    void ExtractRomFs(
        const Extract::RomHeader& header,
        const std::vector<std::uint8_t>& bytes,
        const std::string& rootName,
        bool hasArchives)
    {
        assert(header.FntOffset > 0 && header.FatSize > 0);
        assert(header.FatOffset > 0 && header.FatSize > 0 && header.FatSize % 8 == 0);

        const std::span<const std::uint8_t> byteSpan(bytes);
        const Extract::DirTableEntry dirStart
            = Read::DoOffset<Extract::DirTableEntry>(byteSpan, header.FntOffset);
        const auto entries = Read::DoOffsets<Extract::DirTableEntry>(
            byteSpan, header.FntOffset, static_cast<std::int32_t>(dirStart.DirNum));

        std::vector<std::pair<std::int32_t, std::int32_t>> fileOffsets;
        const auto addresses = Read::DoOffsets<std::uint32_t>(
            byteSpan, header.FatOffset, header.FatSize / 4);
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(addresses->size()); i += 2)
        {
            const std::uint32_t start = addresses->at(static_cast<std::size_t>(i));
            const std::uint32_t end = addresses->at(static_cast<std::size_t>(i + 1));
            fileOffsets.emplace_back(
                std::bit_cast<std::int32_t>(start),
                std::bit_cast<std::int32_t>(end));
        }

        std::function<void(const std::shared_ptr<Extract::DirInfo>&)> populateDir;
        populateDir = [&](const std::shared_ptr<Extract::DirInfo>& dir)
        {
            const std::int32_t dirIndex = std::bit_cast<std::int32_t>(dir->Index);
            const Extract::DirTableEntry& entry = AtManagedIndex(*entries, dirIndex);
            std::uint32_t offset = header.FntOffset + entry.Offset;
            std::uint16_t fileIndex = entry.FirstFileIndex;
            std::uint8_t type = 1;
            while (type != 0)
            {
                type = AtByte(bytes, offset);
                ++offset;
                if (type >= 1 && type <= 127)
                {
                    const std::int32_t length = type;
                    const std::string name = Read::ReadString(byteSpan, offset, length);
                    offset += static_cast<std::uint32_t>(length);
                    dir->Files->push_back(
                        std::make_shared<Extract::FileInfo>(name, fileIndex));
                    fileIndex = static_cast<std::uint16_t>(fileIndex + 1U);
                }
                else if (type >= 129)
                {
                    const std::int32_t length = static_cast<std::int32_t>(type) - 128;
                    const std::string name = Read::ReadString(byteSpan, offset, length);
                    offset += static_cast<std::uint32_t>(length);
                    const std::uint16_t id = Read::SpanReadUshort(byteSpan, offset);
                    offset += sizeof(std::uint16_t);
                    dir->Subdirectories->push_back(std::make_shared<Extract::DirInfo>(
                        name, static_cast<std::uint32_t>(id) - 0xF000U));
                }
            }

            for (const std::shared_ptr<Extract::DirInfo>& subdir : *dir->Subdirectories)
            {
                populateDir(subdir);
            }
        };

        std::function<void(const std::shared_ptr<Extract::DirInfo>&, const std::string&)> writeFiles;
        writeFiles = [&](const std::shared_ptr<Extract::DirInfo>& dir, const std::string& path)
        {
            std::cout << "Writing " << path << "..." << '\n';
            CreateDirectory(path);
            for (const std::shared_ptr<Extract::FileInfo>& file : *dir->Files)
            {
                const std::int32_t fileIndex = std::bit_cast<std::int32_t>(file->Index);
                const auto& [start, end] = AtManagedIndex(fileOffsets, fileIndex);
                assert(start > 0 && end > start);
                const std::vector<std::uint8_t> fileBytes = Slice(bytes, start, end);
                FileWriteAllBytes(Paths::Combine(path, file->Name), fileBytes);
            }
            for (const std::shared_ptr<Extract::DirInfo>& subdir : *dir->Subdirectories)
            {
                writeFiles(subdir, Paths::Combine(path, subdir->Name));
            }
        };

        const auto root = std::make_shared<Extract::DirInfo>(rootName, 0);
        populateDir(root);
        writeFiles(root, Paths::Combine("files", root->Name));

        if (hasArchives)
        {
            const std::string archivesPath = Paths::Combine("files", root->Name, "archives");
            for (const std::filesystem::directory_entry& entry
                : std::filesystem::directory_iterator(PathFromUtf8(archivesPath)))
            {
                if (entry.is_regular_file())
                {
                    const std::string path = PathToUtf8(entry.path());
                    if (ToLowerAscii(GetExtension(path)) == ".arc")
                    {
                        Read::ExtractArchive(path);
                    }
                }
            }

            std::cout << "Converting sound_data.sdat..." << '\n';
            const std::string sdatDest = Paths::Combine("files", root->Name, "_seq");
            CreateDirectory(sdatDest);
            ConvertSdat(
                Paths::Combine(
                    Paths::Combine("files", root->Name, "data", "sound"),
                    "sound_data.sdat"),
                sdatDest);
        }

        const std::string ftcDir = Paths::Combine("files", root->Name, "ftc");
        CreateDirectory(ftcDir);

        auto writeFile = [&](const std::string& name, std::int32_t offset, std::int32_t size)
        {
            const std::int32_t end = ManagedAdd(offset, size);
            std::vector<std::uint8_t> fileBytes = Slice(bytes, offset, end);
            FileWriteAllBytes(Paths::Combine(ftcDir, name), fileBytes);
            return fileBytes;
        };

        (void)writeFile("arm9.bin", header.ARM9Offset, header.ARM9Size);
        (void)writeFile("arm7.bin", header.ARM7Offset, header.ARM7Size);
        (void)writeFile(
            "fat.bin",
            std::bit_cast<std::int32_t>(header.FatOffset),
            std::bit_cast<std::int32_t>(header.FatSize));
        (void)writeFile(
            "fnt.bin",
            std::bit_cast<std::int32_t>(header.FntOffset),
            std::bit_cast<std::int32_t>(header.FntSize));
        (void)writeFile("banner.bin", header.BannerOffset, 0x840);
        const std::vector<std::uint8_t> overlayInfo
            = writeFile("y9.bin", header.Overlay9Offset, header.Overlay9Size);

        assert(overlayInfo.size() % 32 == 0);
        for (std::int32_t i = 0;
            i < static_cast<std::int32_t>(overlayInfo.size() / 32);
            ++i)
        {
            std::vector<std::int32_t> items;
            for (std::int32_t j = 0; j < 8; ++j)
            {
                const std::int32_t start = i * 32 + j * 4;
                const std::vector<std::uint8_t> value
                    = Slice(overlayInfo, start, ManagedAdd(start, 4));
                std::array<std::uint8_t, 4> raw{};
                std::copy(value.begin(), value.end(), raw.begin());
                items.push_back(BitConverterToInt32(std::span<const std::uint8_t, 4>(raw)));
            }

            const std::int32_t overlayId = items.at(0);
            const std::int32_t fileId = items.at(6);
            const auto& [overlayStart, overlayEnd] = AtManagedIndex(fileOffsets, fileId);
            assert(overlayStart > 0 && overlayEnd > overlayStart);
            const std::vector<std::uint8_t> overlayBytes
                = Slice(bytes, overlayStart, overlayEnd);
            FileWriteAllBytes(
                Paths::Combine(ftcDir, "overlay9_" + std::to_string(overlayId)),
                overlayBytes);
        }

        const std::string ftcDest = Paths::Combine("files", root->Name, "_bin");
        CreateDirectory(ftcDest);
        for (const std::filesystem::directory_entry& entry
            : std::filesystem::directory_iterator(PathFromUtf8(ftcDir)))
        {
            if (entry.is_regular_file())
            {
                const std::string path = PathToUtf8(entry.path());
                const std::string filename = GetFileName(path);
                if (filename == "arm9.bin" || StartsWith(filename, "overlay9_"))
                {
                    std::cout << "Decompressing " << filename << "..." << '\n';
                    ExtractDependency::LzBackwardDecompress(
                        path, Paths::Combine(ftcDest, filename));
                }
            }
        }

        Nop();
    }

    [[nodiscard]] std::vector<std::uint8_t> RuntimeSlice(
        const std::vector<std::uint8_t>& bytes,
        const std::shared_ptr<RomDataValues>& data)
    {
        const RomDataValues& value = Require(data);
        return Slice(bytes, value.Offset, ManagedAdd(value.Offset, value.Size));
    }
}

namespace MphRead
{
    void Extract::Setup(const std::string& path)
    {
        const std::vector<std::uint8_t> bytes = FileReadAllBytes(path);
        const RomHeader header = Read::ReadStruct<RomHeader>(
            std::span<const std::uint8_t>(bytes));

        const std::unordered_map<std::string, std::vector<std::uint8_t>> mphCodes = {
            {"AMHE", {0, 1}},
            {"AMHP", {0, 1}},
            {"AMHJ", {0, 1}},
            {"AMHK", {0}},
            {"A76E", {0}}
        };

        bool isFh = false;
        const std::string gameCode = header.GameCode.MarshalString();
        const auto mphIterator = mphCodes.find(gameCode);
        if (mphIterator == mphCodes.end())
        {
            const std::unordered_map<std::string, std::vector<std::uint8_t>> fhCodes = {
                {"AMFE", {0}},
                {"AMFP", {0}}
            };
            const auto fhIterator = fhCodes.find(gameCode);
            if (fhIterator == fhCodes.end())
            {
                PrintExit("The specified ROM file has invalid game code " + gameCode + ".");
                return;
            }
            if (!ContainsVersion(fhIterator->second, header.Version))
            {
                PrintExit(
                    "The specified " + gameCode + " ROM has unexpected version "
                    + std::to_string(static_cast<std::uint32_t>(header.Version)) + ".");
                return;
            }
            isFh = true;
        }
        else if (!ContainsVersion(mphIterator->second, header.Version))
        {
            PrintExit(
                "The specified " + gameCode + " ROM has unexpected version "
                + std::to_string(static_cast<std::uint32_t>(header.Version)) + ".");
            return;
        }

        ExtractDependency::PathsUpdatePaths();
        if (FileExists("paths.txt"))
        {
            if ((!isFh && !IsNullOrWhiteSpace(Paths::FileSystem()))
                || (isFh && !IsNullOrWhiteSpace(Paths::FhFileSystem())))
            {
                std::cout
                    << "A path has already been specified for "
                    << (isFh ? "FH" : "MPH")
                    << " files. Do you want to update it? (y/n) ";
                std::cout.flush();

                std::string input = ReadLineOrEmpty();
                input = ToLowerForPrompt(TrimDotNetWhitespace(std::move(input)));
                if (input != "y" && input != "yes")
                {
                    return;
                }
            }
        }

        const std::string rootName
            = header.GameCode.MarshalString()
            + std::to_string(static_cast<std::uint32_t>(header.Version));
        ExtractRomFs(header, bytes, rootName, !isFh);
        ExtractRomData(rootName);

        const std::string newPath = isFh
            ? GetFullPath(Paths::Combine("files", rootName, "data"))
            : GetFullPath(Paths::Combine("files", rootName));
        ExtractDependency::PathsSetPath(rootName, newPath);

        static constexpr std::array<std::string_view, 10> versionKeys = {
            "AMFE0",
            "AMFP0",
            "A76E0",
            "AMHE0",
            "AMHE1",
            "AMHP0",
            "AMHP1",
            "AMHJ0",
            "AMHJ1",
            "AMHK0"
        };

        std::vector<std::string> lines;
        lines.push_back(ProgramVersionToString());
        for (std::string_view key : versionKeys)
        {
            const std::string keyString(key);
            lines.push_back(
                keyString + "=" + ExtractDependency::PathsValue(keyString));
        }
        lines.push_back("Export=" + ExtractDependency::PathsValue("Export"));

        std::string contents;
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (i != 0)
            {
                contents += EnvironmentNewLine();
            }
            contents += lines[i];
        }
        FileWriteAllText("paths.txt", contents);
        Nop();
    }

    void Extract::LoadRuntimeData()
    {
        const RomData* data = FindRomData(ExtractDependency::PathsMphKey());
        if (data == nullptr)
        {
            return;
        }

        const RomDataValues& fontWidthsValue = Require(data->FontWidths);
        std::vector<std::uint8_t> bytes = FileReadAllBytes(
            Paths::Combine(
                Paths::FileSystem(),
                "_bin",
                fontWidthsValue.File));

        const std::vector<std::uint8_t> widths = RuntimeSlice(bytes, data->FontWidths);
        const std::vector<std::uint8_t> offsets = RuntimeSlice(bytes, data->FontOffsets);
        const std::vector<std::uint8_t> characters = RuntimeSlice(bytes, data->FontCharData);
        const std::vector<std::uint8_t> enemyDamageSfx
            = RuntimeSlice(bytes, data->EnemyDamageSfx);
        const std::vector<std::uint8_t> enemyDeathSfx
            = RuntimeSlice(bytes, data->EnemyDeathSfx);

        ExtractDependency::SetNormalFontData(widths, offsets, characters, 32);

        const RomDataValues& beamSfxValue = Require(data->BeamSfx);
        bytes = FileReadAllBytes(
            Paths::Combine(
                Paths::FileSystem(),
                "_bin",
                beamSfxValue.File));

        const std::vector<std::uint8_t> terrainSfx
            = RuntimeSlice(bytes, data->TerrianSfx);
        const std::vector<std::uint8_t> beamSfx
            = RuntimeSlice(bytes, data->BeamSfx);
        const std::vector<std::uint8_t> hunterSfx
            = RuntimeSlice(bytes, data->HunterSfx);

        Metadata::SetTerrainSfxData(terrainSfx);
        Metadata::SetBeamSfxData(beamSfx);
        Metadata::SetHunterSfxData(hunterSfx);
        Metadata::SetEnemyDamageSfxData(enemyDamageSfx);
        Metadata::SetEnemyDeathSfxData(enemyDeathSfx);

        const RomDataValues& platformSfxValue = Require(data->PlatformSfx);
        bytes = FileReadAllBytes(
            Paths::Combine(
                Paths::FileSystem(),
                "_bin",
                platformSfxValue.File));
        const std::vector<std::uint8_t> platformSfx
            = RuntimeSlice(bytes, data->PlatformSfx);
        Metadata::SetPlatformSfxData(platformSfx);
    }
}
