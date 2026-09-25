#include "Extract.hpp"

#include "../Strings.hpp"
#include "Compress.hpp"

#include "../../NcsfPlay.Native/NC/SDAT.hpp"
#include "../../NcsfPlay.Native/NC/SSEQ.hpp"
#include "../../NcsfPlay.Native/NCSF.hpp"
#include "../../NcsfPlay.Native/TagList.hpp"
#include "../../NcsfPlay.Native/ReplayGain/AlbumGain.hpp"

#include "../Metadata/SoundMeta.hpp"
#include "../Program.hpp"
#include "../Read.hpp"
#include "../Formats/Types.hpp"
#include "../NativeRuntime/System/Console.hpp"
#include "../NativeRuntime/System/Encoding.hpp"
#include "../NativeRuntime/System/Globalization.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Managed.hpp"

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

using ::MphRead::NativeRuntime::CharIsWhiteSpace;
using ::MphRead::NativeRuntime::DirectoryCreateDirectory;
using ::MphRead::NativeRuntime::EnvironmentNewLine;
using ::MphRead::NativeRuntime::FileExists;
using ::MphRead::NativeRuntime::FileReadAllBytes;
using ::MphRead::NativeRuntime::FileWriteAllBytes;
using ::MphRead::NativeRuntime::ManagedListAt;
using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::PathGetExtension;
using ::MphRead::NativeRuntime::PathGetFileName;
using ::MphRead::NativeRuntime::PathGetFullPath;
using ::MphRead::NativeRuntime::PathToUtf8;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::StringIsNullOrWhiteSpace;
using ::MphRead::NativeRuntime::StringTrim;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::Utf8ToUtf16;

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

    struct Utf8Position final
    {
        std::uint32_t CodePoint;
        std::size_t Start;
        std::size_t End;
    };

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
        return Program::Version.ToString();
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

        const RomDataValues& fontModel = RequireReference(data->FontModel);
        const std::vector<std::uint8_t> bytes = FileReadAllBytes(
            Paths::Combine("files", rootName, "_bin", fontModel.File));
        const std::int32_t end = UncheckedAdd(fontModel.Offset, fontModel.Size);
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
            const Extract::DirTableEntry& entry = ManagedListAt(*entries, dirIndex);
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
            DirectoryCreateDirectory(path);
            for (const std::shared_ptr<Extract::FileInfo>& file : *dir->Files)
            {
                const std::int32_t fileIndex = std::bit_cast<std::int32_t>(file->Index);
                const auto& [start, end] = ManagedListAt(fileOffsets, fileIndex);
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
                    if (ToLowerAscii(PathGetExtension(path)) == ".arc")
                    {
                        Read::ExtractArchive(path);
                    }
                }
            }

            std::cout << "Converting sound_data.sdat..." << '\n';
            const std::string sdatDest = Paths::Combine("files", root->Name, "_seq");
            DirectoryCreateDirectory(sdatDest);
            ConvertSdat(
                Paths::Combine(
                    Paths::Combine("files", root->Name, "data", "sound"),
                    "sound_data.sdat"),
                sdatDest);
        }

        const std::string ftcDir = Paths::Combine("files", root->Name, "ftc");
        DirectoryCreateDirectory(ftcDir);

        auto writeFile = [&](const std::string& name, std::int32_t offset, std::int32_t size)
        {
            const std::int32_t end = UncheckedAdd(offset, size);
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
                    = Slice(overlayInfo, start, UncheckedAdd(start, 4));
                std::array<std::uint8_t, 4> raw{};
                std::copy(value.begin(), value.end(), raw.begin());
                items.push_back(BitConverterToInt32(std::span<const std::uint8_t, 4>(raw)));
            }

            const std::int32_t overlayId = items.at(0);
            const std::int32_t fileId = items.at(6);
            const auto& [overlayStart, overlayEnd] = ManagedListAt(fileOffsets, fileId);
            assert(overlayStart > 0 && overlayEnd > overlayStart);
            const std::vector<std::uint8_t> overlayBytes
                = Slice(bytes, overlayStart, overlayEnd);
            FileWriteAllBytes(
                Paths::Combine(ftcDir, "overlay9_" + std::to_string(overlayId)),
                overlayBytes);
        }

        const std::string ftcDest = Paths::Combine("files", root->Name, "_bin");
        DirectoryCreateDirectory(ftcDest);
        for (const std::filesystem::directory_entry& entry
            : std::filesystem::directory_iterator(PathFromUtf8(ftcDir)))
        {
            if (entry.is_regular_file())
            {
                const std::string path = PathToUtf8(entry.path());
                const std::string filename = PathGetFileName(path);
                if (filename == "arm9.bin" || StartsWith(filename, "overlay9_"))
                {
                    std::cout << "Decompressing " << filename << "..." << '\n';
                    LZBackward::Decompress(
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
        const RomDataValues& value = RequireReference(data);
        return Slice(bytes, value.Offset, UncheckedAdd(value.Offset, value.Size));
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

        Paths::UpdatePaths();
        if (FileExists("paths.txt"))
        {
            if ((!isFh && !StringIsNullOrWhiteSpace(Paths::FileSystem()))
                || (isFh && !StringIsNullOrWhiteSpace(Paths::FhFileSystem())))
            {
                std::cout
                    << "A path has already been specified for "
                    << (isFh ? "FH" : "MPH")
                    << " files. Do you want to update it? (y/n) ";
                std::cout.flush();

                std::string input = ReadLineOrEmpty();
                input = ToLowerForPrompt(StringTrim(std::move(input)));
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
            ? PathGetFullPath(Paths::Combine("files", rootName, "data"))
            : PathGetFullPath(Paths::Combine("files", rootName));
        Paths::SetPath(rootName, newPath);

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
                keyString + "=" + Paths::AllPaths().at(keyString));
        }
        lines.push_back("Export=" + Paths::AllPaths().at("Export"));

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
        const RomData* data = FindRomData(Paths::MphKey);
        if (data == nullptr)
        {
            return;
        }

        const RomDataValues& fontWidthsValue = RequireReference(data->FontWidths);
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

        Text::Font::Normal()->SetData(
            std::make_shared<std::vector<std::uint8_t>>(widths),
            std::make_shared<std::vector<std::uint8_t>>(offsets),
            std::make_shared<std::vector<std::uint8_t>>(characters), 32);

        const RomDataValues& beamSfxValue = RequireReference(data->BeamSfx);
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

        const RomDataValues& platformSfxValue = RequireReference(data->PlatformSfx);
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

namespace MphRead::ExtractDependency
{
    // The SDAT and NCSF the extractor works with, which are NcsfPlay's own.
    // Strings are UTF-16 there and UTF-8 here, so each one crosses at the
    // boundary rather than inside.
    namespace
    {
        [[nodiscard]] std::string ToUtf8(const std::u16string& value)
        {
            std::string result;
            for (std::size_t i = 0; i < value.size(); ++i)
            {
                char32_t code = value[i];
                if (code >= 0xD800U && code <= 0xDBFFU && i + 1 < value.size())
                {
                    const char32_t low = value[i + 1];
                    if (low >= 0xDC00U && low <= 0xDFFFU)
                    {
                        code = 0x10000U + ((code - 0xD800U) << 10) + (low - 0xDC00U);
                        ++i;
                    }
                }
                if (code < 0x80U)
                {
                    result += static_cast<char>(code);
                }
                else if (code < 0x800U)
                {
                    result += static_cast<char>(0xC0U | (code >> 6));
                    result += static_cast<char>(0x80U | (code & 0x3FU));
                }
                else if (code < 0x10000U)
                {
                    result += static_cast<char>(0xE0U | (code >> 12));
                    result += static_cast<char>(0x80U | ((code >> 6) & 0x3FU));
                    result += static_cast<char>(0x80U | (code & 0x3FU));
                }
                else
                {
                    result += static_cast<char>(0xF0U | (code >> 18));
                    result += static_cast<char>(0x80U | ((code >> 12) & 0x3FU));
                    result += static_cast<char>(0x80U | ((code >> 6) & 0x3FU));
                    result += static_cast<char>(0x80U | (code & 0x3FU));
                }
            }
            return result;
        }

        class SdatAdapter final : public NcsfSdat
        {
        public:
            SdatAdapter()
                : _sdat(std::make_shared<NCSFCommon::NC::SDAT>())
            {
            }

            explicit SdatAdapter(std::shared_ptr<NCSFCommon::NC::SDAT> sdat)
                : _sdat(std::move(sdat))
            {
            }

            void Read(const std::string& filename, std::span<const std::uint8_t> bytes) override
            {
                _sdat->Read(Utf8ToUtf16(filename), bytes);
            }

            [[nodiscard]] std::unique_ptr<NcsfSdat> Add(const NcsfSdat& other) const override
            {
                const auto& value = static_cast<const SdatAdapter&>(other);
                return std::make_unique<SdatAdapter>(*_sdat + *value._sdat);
            }

            void FixOffsetsAndSizes() override { _sdat->FixOffsetsAndSizes(); }

            [[nodiscard]] std::uint32_t Size() const override { return _sdat->Size(); }

            void Write(std::span<std::uint8_t> bytes) const override { _sdat->Write(bytes); }

            [[nodiscard]] std::size_t SequenceCount() const override
            {
                return _sdat->INFOSection().SEQRecord().Entries().size();
            }

            [[nodiscard]] std::uint32_t SequenceOffset(std::size_t index) const override
            {
                return _sdat->INFOSection().SEQRecord().Entries()[index].Offset;
            }

            [[nodiscard]] bool SequencePresent(std::size_t index) const override
            {
                return _sdat->INFOSection().SEQRecord().Entries()[index].Entry != nullptr;
            }

            [[nodiscard]] std::string SequenceFilename(std::size_t index) const override
            {
                const auto sseq = Sequence(index);
                if (sseq == nullptr || !sseq->Filename().has_value())
                {
                    return std::string();
                }
                return ToUtf8(*sseq->Filename());
            }

            void SetSequenceFilename(std::size_t index, const std::string& filename) override
            {
                const auto sseq = Sequence(index);
                if (sseq != nullptr)
                {
                    sseq->Filename(Utf8ToUtf16(filename));
                }
            }

            [[nodiscard]] std::string SequenceSseqOriginalFilename(
                std::size_t index) const override
            {
                const auto sseq = Sequence(index);
                if (sseq == nullptr || !sseq->OriginalFilename().has_value())
                {
                    return std::string();
                }
                return ToUtf8(*sseq->OriginalFilename());
            }

            [[nodiscard]] std::string SequenceSdatNumber(std::size_t index) const override
            {
                const auto& entry = _sdat->INFOSection().SEQRecord().Entries()[index].Entry;
                if (entry == nullptr || !entry->SDATNumber().has_value())
                {
                    return std::string();
                }
                return ToUtf8(*entry->SDATNumber());
            }

            [[nodiscard]] std::string SequenceFullFilename(
                std::size_t index, bool multipleSdats) const override
            {
                const auto& entry = _sdat->INFOSection().SEQRecord().Entries()[index].Entry;
                if (entry == nullptr)
                {
                    return std::string();
                }
                return ToUtf8(entry->FullFilename(multipleSdats));
            }

        private:
            [[nodiscard]] std::shared_ptr<NCSFCommon::NC::SSEQ> Sequence(std::size_t index) const
            {
                const auto& entry = _sdat->INFOSection().SEQRecord().Entries()[index].Entry;
                return entry == nullptr ? nullptr : entry->SSEQ();
            }

            std::shared_ptr<NCSFCommon::NC::SDAT> _sdat;
        };

        class AlbumGainAdapter final : public AlbumGain
        {
        public:
            NCSFCommon::ReplayGain::AlbumGain Value;
        };
    }

    std::unique_ptr<NcsfSdat> CreateNcsfSdat()
    {
        return std::make_unique<SdatAdapter>();
    }

    std::unique_ptr<AlbumGain> CreateAlbumGain()
    {
        return std::make_unique<AlbumGainAdapter>();
    }

    void MakeNcsf(const std::string& filename,
        std::span<const std::uint8_t> reservedSection,
        std::span<const std::uint8_t> programSection)
    {
        NCSFCommon::NCSF::MakeNCSF(
            Utf8ToUtf16(filename), reservedSection, programSection, nullptr);
    }

    void MakeNcsf(const std::string& filename,
        std::span<const std::uint8_t> reservedSection,
        std::span<const std::uint8_t> programSection,
        const std::vector<NcsfTag>& tags)
    {
        NCSFCommon::TagList list;
        for (const NcsfTag& tag : tags)
        {
            list.AddOrReplace(NCSFCommon::TagList::Item{Utf8ToUtf16(tag.Name), Utf8ToUtf16(tag.Value)});
        }
        NCSFCommon::NCSF::MakeNCSF(
            Utf8ToUtf16(filename), reservedSection, programSection, &list);
    }
}
