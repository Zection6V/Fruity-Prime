#include "SDAT.hpp"

#include "SBNK.hpp"
#include "SSEQ.hpp"
#include "SWAR.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    [[noreturn]] void ThrowOutOfRange()
    {
        throw std::out_of_range("Specified argument was out of the range of valid values.");
    }

    [[noreturn]] void ThrowNullReference()
    {
        throw std::runtime_error("Object reference not set to an instance of an object.");
    }

    [[nodiscard]] std::int32_t ToInt32Unchecked(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::int32_t AddInt32Unchecked(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            std::bit_cast<std::uint32_t>(left) + std::bit_cast<std::uint32_t>(right));
    }

    [[nodiscard]] std::int32_t CountAsInt32(std::size_t count)
    {
        if (count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::overflow_error("Arithmetic operation resulted in an overflow.");
        }
        return static_cast<std::int32_t>(count);
    }

    [[nodiscard]] std::size_t IndexFromInt32(std::int32_t index, std::size_t count)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= count)
        {
            ThrowOutOfRange();
        }
        return static_cast<std::size_t>(index);
    }

    [[nodiscard]] std::size_t IndexFromUInt32(std::uint32_t index, std::size_t count)
    {
        return IndexFromInt32(ToInt32Unchecked(index), count);
    }

    template <typename T>
    [[nodiscard]] const T& At(std::span<const T> span, std::int32_t index)
    {
        return span[IndexFromInt32(index, span.size())];
    }

    template <typename T>
    [[nodiscard]] const T& At(std::span<const T> span, std::uint32_t index)
    {
        return span[IndexFromUInt32(index, span.size())];
    }

    template <typename T>
    [[nodiscard]] T& Require(const std::shared_ptr<T>& value)
    {
        if (value == nullptr)
        {
            ThrowNullReference();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] T& Require(const std::unique_ptr<T>& value)
    {
        if (value == nullptr)
        {
            ThrowNullReference();
        }
        return *value;
    }

    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            ThrowOutOfRange();
        }
        return span.subspan(offset);
    }

    [[nodiscard]] std::span<std::uint8_t> Slice(
        std::span<std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            ThrowOutOfRange();
        }
        return span.subspan(offset);
    }

    [[nodiscard]] std::span<std::uint8_t> Slice(
        std::span<std::uint8_t> span, std::size_t offset, std::size_t count)
    {
        if (offset > span.size() || count > span.size() - offset)
        {
            ThrowOutOfRange();
        }
        return span.subspan(offset, count);
    }

    [[nodiscard]] std::span<const std::uint8_t> SliceFromUInt32(
        std::span<const std::uint8_t> span, std::uint32_t offset)
    {
        const std::int32_t signedOffset = ToInt32Unchecked(offset);
        if (signedOffset < 0)
        {
            ThrowOutOfRange();
        }
        return Slice(span, static_cast<std::size_t>(signedOffset));
    }

    [[nodiscard]] std::span<std::uint8_t> SliceFromUInt32(
        std::span<std::uint8_t> span, std::uint32_t offset)
    {
        const std::int32_t signedOffset = ToInt32Unchecked(offset);
        if (signedOffset < 0)
        {
            ThrowOutOfRange();
        }
        return Slice(span, static_cast<std::size_t>(signedOffset));
    }

    [[nodiscard]] std::uint32_t ReadUInt32LittleEndianAt(
        std::span<const std::uint8_t> span, std::size_t offset)
    {
        const std::span<const std::uint8_t> tail = Slice(span, offset);
        if (tail.size() < sizeof(std::uint32_t))
        {
            ThrowOutOfRange();
        }
        return static_cast<std::uint32_t>(tail[0])
            | (static_cast<std::uint32_t>(tail[1]) << 8U)
            | (static_cast<std::uint32_t>(tail[2]) << 16U)
            | (static_cast<std::uint32_t>(tail[3]) << 24U);
    }

    [[nodiscard]] std::uint16_t ReadUInt16LittleEndianAt(
        std::span<const std::uint8_t> span, std::size_t offset)
    {
        const std::span<const std::uint8_t> tail = Slice(span, offset);
        if (tail.size() < sizeof(std::uint16_t))
        {
            ThrowOutOfRange();
        }
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(tail[0])
            | (static_cast<std::uint16_t>(tail[1]) << 8U));
    }

    void WriteUInt32LittleEndianAt(
        std::span<std::uint8_t> span, std::size_t offset, std::uint32_t value)
    {
        std::span<std::uint8_t> tail = Slice(span, offset);
        if (tail.size() < sizeof(std::uint32_t))
        {
            ThrowOutOfRange();
        }
        tail[0] = static_cast<std::uint8_t>(value);
        tail[1] = static_cast<std::uint8_t>(value >> 8U);
        tail[2] = static_cast<std::uint8_t>(value >> 16U);
        tail[3] = static_cast<std::uint8_t>(value >> 24U);
    }

    void WriteUInt16LittleEndianAt(
        std::span<std::uint8_t> span, std::size_t offset, std::uint16_t value)
    {
        std::span<std::uint8_t> tail = Slice(span, offset);
        if (tail.size() < sizeof(std::uint16_t))
        {
            ThrowOutOfRange();
        }
        tail[0] = static_cast<std::uint8_t>(value);
        tail[1] = static_cast<std::uint8_t>(value >> 8U);
    }

    void CopyTo(std::span<const std::uint8_t> source, std::span<std::uint8_t> destination)
    {
        if (source.size() > destination.size())
        {
            throw std::invalid_argument("Destination is too short.");
        }
        std::copy(source.begin(), source.end(), destination.begin());
    }

    [[nodiscard]] std::u16string Hex(std::uint64_t value, std::size_t minimumWidth)
    {
        static constexpr char16_t Digits[] = u"0123456789ABCDEF";
        std::u16string reversed;
        do
        {
            reversed.push_back(Digits[value & 0xFU]);
            value >>= 4U;
        } while (value != 0U);
        while (reversed.size() < minimumWidth)
        {
            reversed.push_back(u'0');
        }
        std::reverse(reversed.begin(), reversed.end());
        return reversed;
    }

    [[nodiscard]] std::u16string Interpolated(const std::optional<std::u16string>& value)
    {
        return value.value_or(std::u16string{});
    }

    [[nodiscard]] bool StartsWith(std::u16string_view value, std::u16string_view prefix) noexcept
    {
        return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
    }

    [[nodiscard]] bool IsNullOrEmpty(const std::optional<std::u16string>& value) noexcept
    {
        return !value.has_value() || value->empty();
    }

    [[nodiscard]] std::string Utf16ToUtf8(std::u16string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            std::uint32_t scalar = static_cast<std::uint16_t>(value[i]);
            if (scalar >= 0xD800U && scalar <= 0xDBFFU && i + 1 < value.size())
            {
                const std::uint32_t low = static_cast<std::uint16_t>(value[i + 1]);
                if (low >= 0xDC00U && low <= 0xDFFFU)
                {
                    scalar = 0x10000U + ((scalar - 0xD800U) << 10U) + (low - 0xDC00U);
                    ++i;
                }
            }
            if (scalar <= 0x7FU)
            {
                result.push_back(static_cast<char>(scalar));
            }
            else if (scalar <= 0x7FFU)
            {
                result.push_back(static_cast<char>(0xC0U | (scalar >> 6U)));
                result.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
            }
            else if (scalar <= 0xFFFFU)
            {
                result.push_back(static_cast<char>(0xE0U | (scalar >> 12U)));
                result.push_back(static_cast<char>(0x80U | ((scalar >> 6U) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
            }
            else
            {
                result.push_back(static_cast<char>(0xF0U | (scalar >> 18U)));
                result.push_back(static_cast<char>(0x80U | ((scalar >> 12U) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | ((scalar >> 6U) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
            }
        }
        return result;
    }

    void ConsoleWriteLine(std::u16string_view value)
    {
        std::cout << Utf16ToUtf8(value) << '\n';
    }

    void ConsoleWriteLine()
    {
        std::cout << '\n';
    }

    template <typename Dictionary>
    [[nodiscard]] bool ValuesContain(const Dictionary& dictionary, std::uint32_t value)
    {
        for (const auto& pair : dictionary)
        {
            if (std::find(pair.second.begin(), pair.second.end(), value) != pair.second.end())
            {
                return true;
            }
        }
        return false;
    }

    template <typename Dictionary>
    [[nodiscard]] const std::vector<std::uint32_t>* FindContainingValue(
        const Dictionary& dictionary, std::uint32_t value)
    {
        for (const auto& pair : dictionary)
        {
            if (std::find(pair.second.begin(), pair.second.end(), value) != pair.second.end())
            {
                return &pair.second;
            }
        }
        return nullptr;
    }

    template <typename Dictionary>
    [[nodiscard]] std::vector<std::uint32_t>* FindContainingValue(
        Dictionary& dictionary, std::uint32_t value)
    {
        for (auto& pair : dictionary)
        {
            if (std::find(pair.second.begin(), pair.second.end(), value) != pair.second.end())
            {
                return &pair.second;
            }
        }
        return nullptr;
    }

    template <typename Dictionary>
    void SetDictionaryValue(Dictionary& dictionary, std::uint32_t key, std::vector<std::uint32_t> value)
    {
        for (auto& pair : dictionary)
        {
            if (pair.first == key)
            {
                pair.second = std::move(value);
                return;
            }
        }
        dictionary.emplace_back(key, std::move(value));
    }

    using MoveDictionary = std::vector<std::pair<std::uint32_t, std::uint32_t>>;

    [[nodiscard]] std::uint32_t MoveDictionaryAt(const MoveDictionary& dictionary, std::uint32_t key)
    {
        for (const auto& pair : dictionary)
        {
            if (pair.first == key)
            {
                return pair.second;
            }
        }
        throw std::out_of_range("The given key was not present in the dictionary.");
    }

    [[nodiscard]] bool MoveDictionaryTryGetValue(
        const MoveDictionary& dictionary, std::uint32_t key, std::uint32_t& value) noexcept
    {
        for (const auto& pair : dictionary)
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

    [[noreturn]] void ThrowArgumentNull()
    {
        throw std::invalid_argument("Value cannot be null.");
    }

    [[nodiscard]] NCSFCommon::Common::KeepType IncludeFilenameAdapter(
        const std::optional<std::u16string>& filename,
        const std::optional<std::u16string>& sdatNumber,
        const std::vector<std::shared_ptr<NCSFCommon::Common::KeepInfo>>& includesAndExcludes)
    {
        if (filename.has_value() && sdatNumber.has_value())
        {
            return NCSFCommon::Common::IncludeFilename(*filename, *sdatNumber, includesAndExcludes);
        }

        NCSFCommon::Common::KeepType keep = NCSFCommon::Common::KeepType::Neither;
        for (const auto& info : includesAndExcludes)
        {
            if (info == nullptr)
            {
                ThrowNullReference();
            }
            if (!info->Filename.has_value())
            {
                ThrowNullReference();
            }

            const std::u16string& pattern = *info->Filename;
            std::vector<std::u16string_view> parts;
            std::size_t partStart = 0;
            for (std::size_t i = 0; i <= pattern.size(); ++i)
            {
                if (i == pattern.size() || pattern[i] == u'/')
                {
                    parts.emplace_back(pattern.data() + partStart, i - partStart);
                    partStart = i + 1U;
                }
            }
#ifndef NDEBUG
            assert(parts.size() <= 2U);
#endif
            if (parts.size() == 2U)
            {
                if (!sdatNumber.has_value())
                {
                    ThrowArgumentNull();
                }
                if (NCSFCommon::Common::WildcardStringToRegex(parts[0]).IsMatch(*sdatNumber))
                {
                    if (!filename.has_value())
                    {
                        ThrowArgumentNull();
                    }
                    if (NCSFCommon::Common::WildcardStringToRegex(parts[1]).IsMatch(*filename))
                    {
                        keep = info->Keep;
                    }
                }
            }
            else
            {
                if (!filename.has_value())
                {
                    ThrowArgumentNull();
                }
                if (NCSFCommon::Common::WildcardStringToRegex(pattern).IsMatch(*filename))
                {
                    keep = info->Keep;
                }
            }
        }
        return keep;
    }
}

namespace NCSFCommon::NC
{
    const std::array<std::uint8_t, 8> SDAT::Signature{
        0x53U, 0x44U, 0x41U, 0x54U,
        0xFFU, 0xFEU, 0x00U, 0x01U
    };

    const std::array<std::uint8_t, 4> SDAT::FILEHeader{
        static_cast<std::uint8_t>('F'),
        static_cast<std::uint8_t>('I'),
        static_cast<std::uint8_t>('L'),
        static_cast<std::uint8_t>('E')
    };

    std::uint32_t SDAT::Magic() const noexcept
    {
        return 0x0100FEFFU;
    }

    std::uint32_t SDAT::FileSize() const noexcept
    {
        return _size;
    }

    std::uint16_t SDAT::HeaderSize() const noexcept
    {
        return 0x40U;
    }

    std::uint16_t SDAT::Blocks() const noexcept
    {
        return _actualBlocks;
    }

    const std::optional<std::u16string>& SDAT::Filename() const noexcept
    {
        return _filename;
    }

    void SDAT::Filename(std::optional<std::u16string> value)
    {
        _filename = std::move(value);
    }

    std::uint32_t SDAT::SYMBOffset() const noexcept { return _symbOffset; }
    void SDAT::SYMBOffset(std::uint32_t value) noexcept { _symbOffset = value; }
    std::uint32_t SDAT::SYMBSize() const noexcept { return _symbSize; }
    void SDAT::SYMBSize(std::uint32_t value) noexcept { _symbSize = value; }
    std::uint32_t SDAT::INFOOffset() const noexcept { return _infoOffset; }
    void SDAT::INFOOffset(std::uint32_t value) noexcept { _infoOffset = value; }
    std::uint32_t SDAT::INFOSize() const noexcept { return _infoSize; }
    void SDAT::INFOSize(std::uint32_t value) noexcept { _infoSize = value; }
    std::uint32_t SDAT::FATOffset() const noexcept { return _fatOffset; }
    void SDAT::FATOffset(std::uint32_t value) noexcept { _fatOffset = value; }
    std::uint32_t SDAT::FATSize() const noexcept { return _fatSize; }
    void SDAT::FATSize(std::uint32_t value) noexcept { _fatSize = value; }
    std::uint32_t SDAT::FILEOffset() const noexcept { return _fileOffset; }
    void SDAT::FILEOffset(std::uint32_t value) noexcept { _fileOffset = value; }
    std::uint32_t SDAT::FILESize() const noexcept { return _fileSize; }
    void SDAT::FILESize(std::uint32_t value) noexcept { _fileSize = value; }
    std::uint16_t SDAT::Count() const noexcept { return _count; }
    void SDAT::Count(std::uint16_t value) noexcept { _count = value; }

    NCSFCommon::NC::SYMBSection* SDAT::SYMBSection() noexcept
    {
        return _symbSection.get();
    }

    const NCSFCommon::NC::SYMBSection* SDAT::SYMBSection() const noexcept
    {
        return _symbSection.get();
    }

    NCSFCommon::NC::INFOSection& SDAT::INFOSection() noexcept
    {
        return _infoSection;
    }

    const NCSFCommon::NC::INFOSection& SDAT::INFOSection() const noexcept
    {
        return _infoSection;
    }

    NCSFCommon::NC::FATSection& SDAT::FATSection() noexcept
    {
        return _fatSection;
    }

    const NCSFCommon::NC::FATSection& SDAT::FATSection() const noexcept
    {
        return _fatSection;
    }

    bool SDAT::SYMBSectionNeedsCleanup() const noexcept
    {
        return _symbSectionNeedsCleanup;
    }

    void SDAT::SYMBSectionNeedsCleanup(bool value) noexcept
    {
        _symbSectionNeedsCleanup = value;
    }

    std::span<const std::shared_ptr<SSEQ>> SDAT::SSEQs() const noexcept
    {
        return std::span<const std::shared_ptr<SSEQ>>(_sseqs.data(), _sseqs.size());
    }

    std::span<const std::shared_ptr<SBNK>> SDAT::SBNKs() const noexcept
    {
        return std::span<const std::shared_ptr<SBNK>>(_sbnks.data(), _sbnks.size());
    }

    std::span<const std::shared_ptr<SWAR>> SDAT::SWARs() const noexcept
    {
        return std::span<const std::shared_ptr<SWAR>>(_swars.data(), _swars.size());
    }

    const std::shared_ptr<INFOEntryPLAYER>& SDAT::Player() const noexcept
    {
        return _player;
    }

    std::uint32_t SDAT::Size() const noexcept
    {
        return _size;
    }

    void SDAT::Size(std::uint32_t value) noexcept
    {
        _size = value;
    }

    std::vector<std::uint8_t> SDAT::ExpectedHeader() const
    {
        return {
            static_cast<std::uint8_t>('S'),
            static_cast<std::uint8_t>('D'),
            static_cast<std::uint8_t>('A'),
            static_cast<std::uint8_t>('T')
        };
    }

    void SDAT::BaseRead(const std::u16string& filename, std::span<const std::uint8_t> span)
    {
        _filename = filename;

        NDSStandardHeader::Read(span);
        _symbOffset = ReadUInt32LittleEndianAt(span, 0x10U);
        _symbSize = ReadUInt32LittleEndianAt(span, 0x14U);
        _infoOffset = ReadUInt32LittleEndianAt(span, 0x18U);
        _infoSize = ReadUInt32LittleEndianAt(span, 0x1CU);
        _fatOffset = ReadUInt32LittleEndianAt(span, 0x20U);
        _fatSize = ReadUInt32LittleEndianAt(span, 0x24U);
        _count = ReadUInt16LittleEndianAt(span, 0x30U);

        if (_symbOffset != 0U)
        {
            auto symbSection = std::make_unique<NCSFCommon::NC::SYMBSection>();
            symbSection->Read(SliceFromUInt32(span, _symbOffset));
            _symbSection = std::move(symbSection);
            _actualBlocks = 4U;
        }
        _infoSection.Read(SliceFromUInt32(span, _infoOffset));
        _fatSection.Read(SliceFromUInt32(span, _fatOffset));

#ifndef NDEBUG
        assert(!_infoSection.SEQRecord().Entries().empty());
#endif
    }

    void SDAT::Read(std::u16string filename, std::span<const std::uint8_t> span, bool failOnMissingFiles)
    {
        BaseRead(filename, span);

        const bool hasSYMBSection = _symbOffset != 0U;
        const auto seqINFOEntries = _infoSection.SEQRecord().Entries();

        _sseqs.clear();
        const auto seqSYMBEntries = hasSYMBSection
            ? Require(_symbSection).SEQRecord().Entries()
            : std::span<const SYMBRecordEntry>{};
        const auto fatRecords = _fatSection.Records();
        const std::int32_t seqCount = CountAsInt32(seqINFOEntries.size());
        for (std::int32_t i = 0; i < seqCount; ++i)
        {
            const auto& record = At(seqINFOEntries, i);
            if (record.Offset != 0U && record.Entry != nullptr)
            {
                const std::uint32_t fileID = record.Entry->FileID();
                std::optional<std::u16string> origName = std::u16string(u"SSEQ") + Hex(fileID, 4U);
                std::optional<std::u16string> name = origName;
                if (hasSYMBSection)
                {
                    origName = At(seqSYMBEntries, i).Name;
                    name = Hex(static_cast<std::uint32_t>(i), 4U) + u" - " + Interpolated(origName);
                }
                record.Entry->OriginalFilename(origName);
                record.Entry->SDATNumber(_filename);
                auto sseq = std::make_shared<SSEQ>(name, origName);
                sseq->EntryNumber(i);
                sseq->Info(record.Entry);
                const auto& fatRecord = At(fatRecords, fileID);
                sseq->Read(SliceFromUInt32(span, Require(fatRecord).Offset()), failOnMissingFiles);
                record.Entry->SSEQ(sseq);
                _sseqs.push_back(std::move(sseq));
            }
        }

        _sbnks.clear();
        const auto bankSYMBEntries = hasSYMBSection
            ? Require(_symbSection).BANKRecord().Entries()
            : std::span<const SYMBRecordEntry>{};
        const auto bankINFOEntries = _infoSection.BANKRecord().Entries();
        const std::int32_t bankCount = CountAsInt32(bankINFOEntries.size());
        for (std::int32_t i = 0; i < bankCount; ++i)
        {
            const auto& record = At(bankINFOEntries, i);
            if (record.Offset != 0U && record.Entry != nullptr)
            {
                const std::uint32_t fileID = record.Entry->FileID();
                std::optional<std::u16string> origName = hasSYMBSection
                    ? At(bankSYMBEntries, i).Name
                    : std::optional<std::u16string>(std::u16string(u"SBNK") + Hex(fileID, 4U));
                record.Entry->OriginalFilename(origName);
                record.Entry->SDATNumber(_filename);
                auto sbnk = std::make_shared<SBNK>(origName);
                sbnk->EntryNumber(i);
                const auto& fatRecord = At(fatRecords, fileID);
                sbnk->Read(SliceFromUInt32(span, Require(fatRecord).Offset()), failOnMissingFiles);
                record.Entry->SBNK(sbnk);
                _sbnks.push_back(std::move(sbnk));
            }
        }

        _swars.clear();
        const auto wavearcSYMBEntries = hasSYMBSection
            ? Require(_symbSection).WAVEARCRecord().Entries()
            : std::span<const SYMBRecordEntry>{};
        const auto wavearcINFOEntries = _infoSection.WAVEARCRecord().Entries();
        const std::int32_t wavearcCount = CountAsInt32(wavearcINFOEntries.size());
        for (std::int32_t i = 0; i < wavearcCount; ++i)
        {
            const auto& record = At(wavearcINFOEntries, i);
            if (record.Offset != 0U && record.Entry != nullptr)
            {
                const std::uint32_t fileID = record.Entry->FileID();
                std::optional<std::u16string> origName = hasSYMBSection
                    ? At(wavearcSYMBEntries, i).Name
                    : std::optional<std::u16string>(std::u16string(u"SWAR") + Hex(fileID, 4U));
                record.Entry->OriginalFilename(origName);
                record.Entry->SDATNumber(_filename);
                auto swar = std::make_shared<SWAR>(origName);
                swar->EntryNumber(i);
                const auto& fatRecord = At(fatRecords, fileID);
                swar->Read(SliceFromUInt32(span, Require(fatRecord).Offset()), failOnMissingFiles);
                record.Entry->SWAR(swar);
                _swars.push_back(std::move(swar));
            }
        }

        const auto playerSYMBEntries = hasSYMBSection
            ? Require(_symbSection).PLAYERRecord().Entries()
            : std::span<const SYMBRecordEntry>{};
        const auto playerINFOEntries = _infoSection.PLAYERRecord().Entries();
        const std::int32_t playerCount = CountAsInt32(playerINFOEntries.size());
        for (std::int32_t i = 0; i < playerCount; ++i)
        {
            const auto& record = At(playerINFOEntries, i);
            if (record.Offset != 0U && record.Entry != nullptr)
            {
                record.Entry->OriginalFilename(
                    hasSYMBSection
                        ? At(playerSYMBEntries, i).Name
                        : std::optional<std::u16string>(std::u16string(u"PLAYER") + Hex(static_cast<std::uint32_t>(i), 2U)));
                record.Entry->SDATNumber(_filename);
            }
        }
    }

    void SDAT::Read(
        std::u16string filename,
        std::span<const std::uint8_t> span,
        std::uint32_t sseqToLoad,
        bool failOnMissingFiles)
    {
        BaseRead(filename, span);

        const bool hasSYMBSection = _symbOffset != 0U;
        const auto seqINFOEntries = _infoSection.SEQRecord().Entries();

#ifndef NDEBUG
        assert(static_cast<std::uint64_t>(sseqToLoad) <= static_cast<std::uint64_t>(seqINFOEntries.size()));
#endif

        _sseqs.clear();
        const auto seqSYMBEntries = hasSYMBSection
            ? Require(_symbSection).SEQRecord().Entries()
            : std::span<const SYMBRecordEntry>{};
        const auto fatRecords = _fatSection.Records();
        const auto& seqRecord = At(seqINFOEntries, sseqToLoad);
#ifndef NDEBUG
        assert(seqRecord.Offset != 0U && seqRecord.Entry != nullptr);
#endif
        auto seqEntry = seqRecord.Entry;
        (void)Require(seqEntry);
        std::uint32_t fileID = seqEntry->FileID();
        std::optional<std::u16string> origName = std::u16string(u"SSEQ") + Hex(fileID, 4U);
        std::optional<std::u16string> name = origName;
        if (hasSYMBSection)
        {
            origName = At(seqSYMBEntries, sseqToLoad).Name;
            name = Hex(sseqToLoad, 4U) + u" - " + Interpolated(origName);
        }
        seqEntry->OriginalFilename(origName);
        seqEntry->SDATNumber(_filename);
        auto sseq = std::make_shared<SSEQ>(name, origName);
        sseq->EntryNumber(ToInt32Unchecked(sseqToLoad));
        sseq->Info(seqEntry);
        const auto& seqFatRecord = At(fatRecords, fileID);
        sseq->Read(SliceFromUInt32(span, Require(seqFatRecord).Offset()), failOnMissingFiles);
        seqEntry->SSEQ(sseq);
        _sseqs.push_back(std::move(sseq));

        _sbnks.clear();
        const std::uint16_t bankID = seqEntry->Bank();
        const auto bankINFOEntries = _infoSection.BANKRecord().Entries();
        const auto& bankRecord = At(bankINFOEntries, static_cast<std::int32_t>(bankID));
#ifndef NDEBUG
        assert(bankRecord.Offset != 0U && bankRecord.Entry != nullptr);
#endif
        auto bankEntry = bankRecord.Entry;
        (void)Require(bankEntry);
        fileID = bankEntry->FileID();
        origName = hasSYMBSection
            ? At(Require(_symbSection).BANKRecord().Entries(), static_cast<std::int32_t>(bankID)).Name
            : std::optional<std::u16string>(std::u16string(u"SBNK") + Hex(fileID, 4U));
        bankEntry->OriginalFilename(origName);
        bankEntry->SDATNumber(_filename);
        auto sbnk = std::make_shared<SBNK>(origName);
        sbnk->EntryNumber(static_cast<std::int32_t>(bankID));
        sbnk->Info(bankEntry);
        const auto& bankFatRecord = At(fatRecords, fileID);
        sbnk->Read(SliceFromUInt32(span, Require(bankFatRecord).Offset()), failOnMissingFiles);
        bankEntry->SBNK(sbnk);
        _sbnks.push_back(std::move(sbnk));

        _swars.clear();
        const auto waveArchives = bankEntry->WaveArchives();
        for (std::int32_t i = 0; i < 4; ++i)
        {
            const std::uint16_t waveArcID = waveArchives[static_cast<std::size_t>(i)];
            if (waveArcID != std::numeric_limits<std::uint16_t>::max())
            {
                const auto wavearcINFOEntries = _infoSection.WAVEARCRecord().Entries();
                const auto& wavearcRecord = At(wavearcINFOEntries, static_cast<std::int32_t>(waveArcID));
#ifndef NDEBUG
                assert(wavearcRecord.Offset != 0U && wavearcRecord.Entry != nullptr);
#endif
                auto wavearcEntry = wavearcRecord.Entry;
                (void)Require(wavearcEntry);
                fileID = wavearcEntry->FileID();
                origName = hasSYMBSection
                    ? At(Require(_symbSection).WAVEARCRecord().Entries(), static_cast<std::int32_t>(waveArcID)).Name
                    : std::optional<std::u16string>(std::u16string(u"SWAR") + Hex(fileID, 4U));
                wavearcEntry->OriginalFilename(origName);
                wavearcEntry->SDATNumber(_filename);
                auto swar = std::make_shared<SWAR>(origName);
                swar->EntryNumber(static_cast<std::int32_t>(waveArcID));
                const auto& wavearcFatRecord = At(fatRecords, fileID);
                swar->Read(SliceFromUInt32(span, Require(wavearcFatRecord).Offset()), failOnMissingFiles);
                wavearcEntry->SWAR(swar);
                _swars.push_back(std::move(swar));
            }
        }

        const auto playerEntries = _infoSection.PLAYERRecord().Entries();
        if (!playerEntries.empty() && static_cast<std::size_t>(seqEntry->Player()) < playerEntries.size())
        {
            _player = playerEntries[static_cast<std::size_t>(seqEntry->Player())].Entry;
        }
        if (_player != nullptr && _player->ChannelMask() == 0U)
        {
            _player->ChannelMask(0xFFFFU);
        }
    }

    void SDAT::Write(std::span<std::uint8_t> span)
    {
        NDSStandardHeader::Write(span);
        WriteUInt32LittleEndianAt(span, 0x10U, _symbOffset);
        const std::uint32_t symbSizeMulOf4 = (_symbSize + 3U) & ~std::uint32_t{0x03U};
        WriteUInt32LittleEndianAt(span, 0x14U, symbSizeMulOf4);
        WriteUInt32LittleEndianAt(span, 0x18U, _infoOffset);
        WriteUInt32LittleEndianAt(span, 0x1CU, _infoSize);
        WriteUInt32LittleEndianAt(span, 0x20U, _fatOffset);
        WriteUInt32LittleEndianAt(span, 0x24U, _fatSize);
        WriteUInt32LittleEndianAt(span, 0x28U, _fileOffset);
        WriteUInt32LittleEndianAt(span, 0x2CU, _fileSize);
        WriteUInt16LittleEndianAt(span, 0x30U, _count);
        std::span<std::uint8_t> reserved = Slice(span, 0x32U, 0x0EU);
        std::fill(reserved.begin(), reserved.end(), std::uint8_t{0});

        std::uint32_t pos = 0x40U;
        if (_symbSection != nullptr)
        {
            _symbSection->Write(SliceFromUInt32(span, pos));
        }
        pos += symbSizeMulOf4;
        _infoSection.Write(SliceFromUInt32(span, pos));
        pos += _infoSize;
        _fatSection.Write(SliceFromUInt32(span, pos));
        pos += _fatSize;

        CopyTo(FILEHeader, SliceFromUInt32(span, pos));
        WriteUInt32LittleEndianAt(SliceFromUInt32(span, pos), 0x04U, _fileSize);
        WriteUInt32LittleEndianAt(
            SliceFromUInt32(span, pos), 0x08U, static_cast<std::uint32_t>(_fatSection.Records().size()));
        std::span<std::uint8_t> fileReserved = SliceFromUInt32(span, pos + 0x0CU);
        if (fileReserved.size() < 4U)
        {
            ThrowOutOfRange();
        }
        std::fill(fileReserved.begin(), fileReserved.begin() + 4, std::uint8_t{0});

        pos += 0x10U;
        for (const auto& record : _infoSection.SEQRecord().Entries())
        {
            if (record.Entry != nullptr && record.Entry->SSEQ() != nullptr)
            {
                record.Entry->SSEQ()->Write(SliceFromUInt32(span, pos));
            }
            pos += record.Entry != nullptr && record.Entry->SSEQ() != nullptr
                ? record.Entry->SSEQ()->Size()
                : 0U;
        }
        for (const auto& record : _infoSection.BANKRecord().Entries())
        {
            if (record.Entry != nullptr && record.Entry->SBNK() != nullptr)
            {
                record.Entry->SBNK()->Write(SliceFromUInt32(span, pos));
            }
            pos += record.Entry != nullptr && record.Entry->SBNK() != nullptr
                ? record.Entry->SBNK()->Size()
                : 0U;
        }
        for (const auto& record : _infoSection.WAVEARCRecord().Entries())
        {
            if (record.Entry != nullptr && record.Entry->SWAR() != nullptr)
            {
                record.Entry->SWAR()->Write(SliceFromUInt32(span, pos));
            }
            pos += record.Entry != nullptr && record.Entry->SWAR() != nullptr
                ? record.Entry->SWAR()->Size()
                : 0U;
        }
    }

    std::shared_ptr<SDAT> SDAT::Add(const SDAT& sdat1, const SDAT& sdat2)
    {
        auto newSDAT = std::make_shared<SDAT>();

        const std::uint32_t sdat1SEQCount = static_cast<std::uint32_t>(sdat1._infoSection.SEQRecord().Entries().size());
        const std::uint32_t sdat1BANKCount = static_cast<std::uint32_t>(sdat1._infoSection.BANKRecord().Entries().size());
        const std::uint32_t sdat1WAVEARCCount = static_cast<std::uint32_t>(sdat1._infoSection.WAVEARCRecord().Entries().size());
        const std::uint32_t sdat1PLAYERCount = static_cast<std::uint32_t>(sdat1._infoSection.PLAYERRecord().Entries().size());

        const std::int32_t newSEQcount = AddInt32Unchecked(
            ToInt32Unchecked(sdat1SEQCount), CountAsInt32(sdat2._infoSection.SEQRecord().Entries().size()));
        const std::int32_t newBANKcount = AddInt32Unchecked(
            ToInt32Unchecked(sdat1BANKCount), CountAsInt32(sdat2._infoSection.BANKRecord().Entries().size()));
        const std::int32_t newWAVEARCcount = AddInt32Unchecked(
            ToInt32Unchecked(sdat1WAVEARCCount), CountAsInt32(sdat2._infoSection.WAVEARCRecord().Entries().size()));
        const std::int32_t newPLAYERcount = AddInt32Unchecked(
            ToInt32Unchecked(sdat1PLAYERCount), CountAsInt32(sdat2._infoSection.PLAYERRecord().Entries().size()));

        if (sdat1._symbSection != nullptr || sdat2._symbSection != nullptr)
        {
            auto merged = NCSFCommon::NC::SYMBSection::Add(sdat1._symbSection.get(), sdat2._symbSection.get());
            newSDAT->_symbSection = std::make_unique<NCSFCommon::NC::SYMBSection>(std::move(merged));
            newSDAT->_symbSection->SEQRecord().ExpandNumberOfEntries(static_cast<std::uint32_t>(newSEQcount));
            newSDAT->_symbSection->BANKRecord().ExpandNumberOfEntries(static_cast<std::uint32_t>(newBANKcount));
            newSDAT->_symbSection->WAVEARCRecord().ExpandNumberOfEntries(static_cast<std::uint32_t>(newWAVEARCcount));
            newSDAT->_symbSection->PLAYERRecord().ExpandNumberOfEntries(static_cast<std::uint32_t>(newPLAYERcount));
            newSDAT->_symbSectionNeedsCleanup = true;
            newSDAT->_symbOffset = 0x40U;
        }

        newSDAT->_infoSection = sdat1._infoSection + sdat2._infoSection;

        newSDAT->_infoSection.SEQRecord().ExpandNumberOfEntries(static_cast<std::uint32_t>(newSEQcount));
        const auto seqINFOEntries = newSDAT->_infoSection.SEQRecord().Entries();
        for (std::uint32_t i = sdat1SEQCount; static_cast<std::int64_t>(i) < newSEQcount; ++i)
        {
            const auto& record = At(seqINFOEntries, i);
            if (record.Entry != nullptr)
            {
                record.Entry->Bank(static_cast<std::uint16_t>(
                    static_cast<std::uint32_t>(record.Entry->Bank())
                    + static_cast<std::uint16_t>(sdat1BANKCount)));
                record.Entry->Player(static_cast<std::uint8_t>(
                    static_cast<std::uint32_t>(record.Entry->Player())
                    + static_cast<std::uint8_t>(sdat1PLAYERCount)));
                if (record.Entry->SSEQ() != nullptr)
                {
                    record.Entry->SSEQ()->EntryNumber(ToInt32Unchecked(i));
                }
            }
        }

        newSDAT->_infoSection.BANKRecord().ExpandNumberOfEntries(static_cast<std::uint32_t>(newBANKcount));
        const auto bankINFOEntries = newSDAT->_infoSection.BANKRecord().Entries();
        for (std::uint32_t i = sdat1BANKCount; static_cast<std::int64_t>(i) < newBANKcount; ++i)
        {
            const auto& record = At(bankINFOEntries, i);
            if (record.Entry != nullptr)
            {
                const auto waveArchives = record.Entry->WaveArchives();
                for (std::int32_t j = 0; j < 4; ++j)
                {
                    const std::uint16_t waveArchive = waveArchives[static_cast<std::size_t>(j)];
                    if (waveArchive != std::numeric_limits<std::uint16_t>::max())
                    {
                        record.Entry->ReplaceWaveArchive(
                            j,
                            static_cast<std::uint16_t>(static_cast<std::uint32_t>(waveArchive) + sdat1WAVEARCCount));
                    }
                }
                if (record.Entry->SBNK() != nullptr)
                {
                    record.Entry->SBNK()->EntryNumber(ToInt32Unchecked(i));
                }
            }
        }

        newSDAT->_infoSection.WAVEARCRecord().ExpandNumberOfEntries(static_cast<std::uint32_t>(newWAVEARCcount));
        const auto wavearcINFOEntries = newSDAT->_infoSection.WAVEARCRecord().Entries();
        for (std::uint32_t i = sdat1WAVEARCCount; static_cast<std::int64_t>(i) < newWAVEARCcount; ++i)
        {
            const auto& record = At(wavearcINFOEntries, i);
            if (record.Entry != nullptr && record.Entry->SWAR() != nullptr)
            {
                record.Entry->SWAR()->EntryNumber(ToInt32Unchecked(i));
            }
        }

        newSDAT->_infoSection.PLAYERRecord().ExpandNumberOfEntries(static_cast<std::uint32_t>(newPLAYERcount));

        newSDAT->_fatSection = sdat1._fatSection + sdat2._fatSection;

        newSDAT->_sseqs.insert(newSDAT->_sseqs.end(), sdat1._sseqs.begin(), sdat1._sseqs.end());
        newSDAT->_sseqs.insert(newSDAT->_sseqs.end(), sdat2._sseqs.begin(), sdat2._sseqs.end());
        newSDAT->_sbnks.insert(newSDAT->_sbnks.end(), sdat1._sbnks.begin(), sdat1._sbnks.end());
        newSDAT->_sbnks.insert(newSDAT->_sbnks.end(), sdat2._sbnks.begin(), sdat2._sbnks.end());
        newSDAT->_swars.insert(newSDAT->_swars.end(), sdat1._swars.begin(), sdat1._swars.end());
        newSDAT->_swars.insert(newSDAT->_swars.end(), sdat2._swars.begin(), sdat2._swars.end());

        newSDAT->_count = static_cast<std::uint16_t>(static_cast<std::uint32_t>(sdat1._count) + 1U);

        return newSDAT;
    }

    std::shared_ptr<SDAT> operator+(const SDAT& sdat1, const SDAT& sdat2)
    {
        return SDAT::Add(sdat1, sdat2);
    }

    std::uint32_t SDAT::GetNonDuplicateNumber(
        std::uint32_t orig,
        const DuplicateDictionary& duplicates)
    {
        for (const auto& pair : duplicates)
        {
            if (pair.first == orig)
            {
                return orig;
            }
        }
        for (const auto& pair : duplicates)
        {
            if (std::find(pair.second.begin(), pair.second.end(), orig) != pair.second.end())
            {
                return pair.first;
            }
        }
        return orig;
    }

    template <typename T>
        requires std::derived_from<T, INFOEntry> && std::default_initializable<T>
    void SDAT::OutputList(
        const std::vector<std::uint32_t>& list,
        std::span<const INFORecordEntry<T>> nameSource,
        bool multipleSDATs,
        std::u16string outputPrefix,
        std::int32_t columnWidth)
    {
        std::u16string sb = std::move(outputPrefix);
        for (const std::uint32_t item : list)
        {
            const auto& source = At(nameSource, item);
            const std::u16string keep = Require(source.Entry).FullFilename(multipleSDATs);
            const std::int32_t totalLength = AddInt32Unchecked(CountAsInt32(sb.size()), CountAsInt32(keep.size()));
            if (totalLength > columnWidth)
            {
                ConsoleWriteLine(sb);
                sb.clear();
                sb.append(u"   ");
            }
            sb.push_back(u' ');
            sb.append(keep);
            sb.push_back(u',');
        }
        if (!sb.empty())
        {
            ConsoleWriteLine(std::u16string_view(sb).substr(0, sb.size() - 1U));
        }
    }

    template <typename T>
        requires std::derived_from<T, INFOEntry> && std::default_initializable<T>
    void SDAT::OutputDictionary(
        const DuplicateDictionary& dictionary,
        std::span<const INFORecordEntry<T>> nameSource,
        bool multipleSDATs,
        std::int32_t columnWidth)
    {
        for (const auto& pair : dictionary)
        {
            const auto& source = At(nameSource, pair.first);
            std::u16string prefix = u"  ";
            prefix.append(Require(source.Entry).FullFilename(multipleSDATs));
            prefix.push_back(u':');
            OutputList(pair.second, nameSource, multipleSDATs, std::move(prefix), columnWidth);
        }
    }

    void SDAT::Strip(
        const std::vector<std::shared_ptr<Common::KeepInfo>>& includesAndExcludes,
        bool verbose,
        bool removeExcluded)
    {
        DuplicateDictionary duplicatePLAYERs;

        auto playerINFOEntries = _infoSection.PLAYERRecord().Entries();
        const std::uint32_t playerEntries = static_cast<std::uint32_t>(playerINFOEntries.size());
        for (std::uint32_t i = 0; i < playerEntries; ++i)
        {
            const auto& iRecord = At(playerINFOEntries, i);
            if (iRecord.Offset != 0U && iRecord.Entry != nullptr && !ValuesContain(duplicatePLAYERs, i))
            {
                std::vector<std::uint32_t> duplicates;
                for (std::uint32_t j = i + 1U; j < playerEntries; ++j)
                {
                    const auto& jRecord = At(playerINFOEntries, j);
                    if (jRecord.Offset != 0U && jRecord.Entry != nullptr
                        && INFOEntryPLAYER::EqualityOperator(iRecord.Entry.get(), jRecord.Entry.get()))
                    {
                        duplicates.push_back(j);
                    }
                }
                if (!duplicates.empty())
                {
                    SetDictionaryValue(duplicatePLAYERs, i, std::move(duplicates));
                }
            }
        }

        DuplicateDictionary duplicateSWARs;

        auto wavearcINFOEntries = _infoSection.WAVEARCRecord().Entries();
        const std::uint32_t wavearcEntries = static_cast<std::uint32_t>(wavearcINFOEntries.size());
        for (std::uint32_t i = 0; i < wavearcEntries; ++i)
        {
            const auto& iRecord = At(wavearcINFOEntries, i);
            if (iRecord.Offset != 0U && iRecord.Entry != nullptr && !ValuesContain(duplicateSWARs, i))
            {
                std::vector<std::uint32_t> duplicates;
                for (std::uint32_t j = i + 1U; j < wavearcEntries; ++j)
                {
                    const auto& jRecord = At(wavearcINFOEntries, j);
                    if (jRecord.Offset != 0U && jRecord.Entry != nullptr
                        && INFOEntryWAVEARC::EqualityOperator(iRecord.Entry.get(), jRecord.Entry.get()))
                    {
                        duplicates.push_back(j);
                    }
                }
                if (!duplicates.empty())
                {
                    SetDictionaryValue(duplicateSWARs, i, std::move(duplicates));
                }
            }
        }

        DuplicateDictionary duplicateSBNKs;

        auto bankINFOEntries = _infoSection.BANKRecord().Entries();
        const std::uint32_t bankEntries = static_cast<std::uint32_t>(bankINFOEntries.size());
        for (std::uint32_t i = 0; i < bankEntries; ++i)
        {
            const auto& iRecord = At(bankINFOEntries, i);
            if (iRecord.Offset != 0U && iRecord.Entry != nullptr && !ValuesContain(duplicateSBNKs, i))
            {
                std::array<std::uint16_t, 4> iWaveArchives{
                    std::numeric_limits<std::uint16_t>::max(),
                    std::numeric_limits<std::uint16_t>::max(),
                    std::numeric_limits<std::uint16_t>::max(),
                    std::numeric_limits<std::uint16_t>::max()
                };
                auto waveArchives = iRecord.Entry->WaveArchives();
                for (std::int32_t k = 0; k < 4; ++k)
                {
                    const std::uint16_t waveArchive = waveArchives[static_cast<std::size_t>(k)];
                    if (waveArchive != std::numeric_limits<std::uint16_t>::max())
                    {
                        iWaveArchives[static_cast<std::size_t>(k)] = static_cast<std::uint16_t>(
                            GetNonDuplicateNumber(waveArchive, duplicateSWARs));
                    }
                }
                std::vector<std::uint32_t> duplicates;
                for (std::uint32_t j = i + 1U; j < bankEntries; ++j)
                {
                    const auto& jRecord = At(bankINFOEntries, j);
                    if (jRecord.Offset != 0U && jRecord.Entry != nullptr
                        && iRecord.Entry->FileEquals(jRecord.Entry.get()))
                    {
                        std::array<std::uint16_t, 4> jWaveArchives{
                            std::numeric_limits<std::uint16_t>::max(),
                            std::numeric_limits<std::uint16_t>::max(),
                            std::numeric_limits<std::uint16_t>::max(),
                            std::numeric_limits<std::uint16_t>::max()
                        };
                        waveArchives = jRecord.Entry->WaveArchives();
                        for (std::int32_t k = 0; k < 4; ++k)
                        {
                            const std::uint16_t waveArchive = waveArchives[static_cast<std::size_t>(k)];
                            if (waveArchive != std::numeric_limits<std::uint16_t>::max())
                            {
                                jWaveArchives[static_cast<std::size_t>(k)] = static_cast<std::uint16_t>(
                                    GetNonDuplicateNumber(waveArchive, duplicateSWARs));
                            }
                        }
                        if (iWaveArchives == jWaveArchives)
                        {
                            duplicates.push_back(j);
                        }
                    }
                }
                if (!duplicates.empty())
                {
                    SetDictionaryValue(duplicateSBNKs, i, std::move(duplicates));
                }
            }
        }

        DuplicateDictionary duplicateSSEQs;
        std::vector<std::uint32_t> excludedSSEQs;

        auto seqINFOEntries = _infoSection.SEQRecord().Entries();
        const std::uint32_t seqEntries = static_cast<std::uint32_t>(seqINFOEntries.size());
        for (std::uint32_t i = 0; i < seqEntries; ++i)
        {
            const auto& iRecord = At(seqINFOEntries, i);
            if (iRecord.Offset != 0U && iRecord.Entry != nullptr
                && std::find(excludedSSEQs.begin(), excludedSSEQs.end(), i) == excludedSSEQs.end())
            {
                std::vector<std::uint32_t>* alreadyFound = FindContainingValue(duplicateSSEQs, i);
                if (IncludeFilenameAdapter(
                        iRecord.Entry->OriginalFilename(),
                        iRecord.Entry->SDATNumber(),
                        includesAndExcludes) == Common::KeepType::Exclude)
                {
                    excludedSSEQs.push_back(i);
                    if (alreadyFound != nullptr)
                    {
                        const auto found = std::find(alreadyFound->begin(), alreadyFound->end(), i);
                        if (found != alreadyFound->end())
                        {
                            alreadyFound->erase(found);
                        }
                    }
                    continue;
                }

                if (alreadyFound == nullptr)
                {
                    const std::uint32_t iNonDuplicateBank = GetNonDuplicateNumber(iRecord.Entry->Bank(), duplicateSBNKs);
                    std::vector<std::uint32_t> duplicates;
                    for (std::uint32_t j = i + 1U; j < seqEntries; ++j)
                    {
                        const auto& jRecord = At(seqINFOEntries, j);
                        if (jRecord.Offset != 0U && jRecord.Entry != nullptr
                            && iRecord.Entry->FileEquals(jRecord.Entry.get()))
                        {
                            const std::uint32_t jNonDuplicateBank = GetNonDuplicateNumber(jRecord.Entry->Bank(), duplicateSBNKs);
                            if (iNonDuplicateBank == jNonDuplicateBank)
                            {
                                duplicates.push_back(j);
                            }
                        }
                    }
                    if (!duplicates.empty())
                    {
                        SetDictionaryValue(duplicateSSEQs, i, std::move(duplicates));
                    }
                }
            }
        }

        std::vector<std::uint32_t> SSEQsToKeep;
        for (std::uint32_t i = 0; i < seqEntries; ++i)
        {
            const auto& record = At(seqINFOEntries, i);
            const bool excluded = std::find(excludedSSEQs.begin(), excludedSSEQs.end(), i) != excludedSSEQs.end();
            if (record.Offset != 0U && record.Entry != nullptr && !ValuesContain(duplicateSSEQs, i)
                && (!removeExcluded || !excluded))
            {
                SSEQsToKeep.push_back(i);
            }
        }

        std::vector<std::uint32_t> SBNKsToKeep;
        for (const std::uint32_t SSEQToKeep : SSEQsToKeep)
        {
            const auto& record = At(seqINFOEntries, SSEQToKeep);
            const std::uint32_t nonDuplicateBank = GetNonDuplicateNumber(Require(record.Entry).Bank(), duplicateSBNKs);
            if (std::find(SBNKsToKeep.begin(), SBNKsToKeep.end(), nonDuplicateBank) == SBNKsToKeep.end())
            {
                SBNKsToKeep.push_back(nonDuplicateBank);
            }
        }
        std::sort(SBNKsToKeep.begin(), SBNKsToKeep.end());

        std::vector<std::uint32_t> SWARsToKeep;
        for (const std::uint32_t SBNKToKeep : SBNKsToKeep)
        {
            const auto& record = At(bankINFOEntries, SBNKToKeep);
            for (const std::uint16_t waveArchive : Require(record.Entry).WaveArchives())
            {
                if (waveArchive != std::numeric_limits<std::uint16_t>::max())
                {
                    const std::uint32_t nonDuplicateWaveArchive = GetNonDuplicateNumber(waveArchive, duplicateSWARs);
                    if (std::find(SWARsToKeep.begin(), SWARsToKeep.end(), nonDuplicateWaveArchive) == SWARsToKeep.end())
                    {
                        SWARsToKeep.push_back(nonDuplicateWaveArchive);
                    }
                }
            }
        }
        std::sort(SWARsToKeep.begin(), SWARsToKeep.end());

        std::vector<std::uint32_t> PLAYERsToKeep;
        const std::uint32_t numPlayers = static_cast<std::uint32_t>(playerINFOEntries.size());
        for (const std::uint32_t SSEQToKeep : SSEQsToKeep)
        {
            const auto& record = At(seqINFOEntries, SSEQToKeep);
            const std::uint32_t nonDuplicatePlayer = GetNonDuplicateNumber(Require(record.Entry).Player(), duplicatePLAYERs);
            if (std::find(PLAYERsToKeep.begin(), PLAYERsToKeep.end(), nonDuplicatePlayer) == PLAYERsToKeep.end()
                && nonDuplicatePlayer < numPlayers)
            {
                PLAYERsToKeep.push_back(nonDuplicatePlayer);
            }
        }
        std::sort(PLAYERsToKeep.begin(), PLAYERsToKeep.end());

        if (verbose)
        {
            if (removeExcluded && !excludedSSEQs.empty())
            {
                ConsoleWriteLine(std::u16string(u"The following SSEQ")
                    + (excludedSSEQs.size() == 1U ? u"" : u"s")
                    + u" were excluded by request:");
                OutputList(excludedSSEQs, seqINFOEntries, _count > 1U);
                ConsoleWriteLine();
            }

            if (!duplicateSSEQs.empty())
            {
                ConsoleWriteLine(std::u16string(u"The following SSEQ")
                    + (duplicateSSEQs.size() == 1U ? u"" : u"s")
                    + u" had duplicates, the duplicates will be removed:");
                OutputDictionary(duplicateSSEQs, seqINFOEntries, _count > 1U);
                ConsoleWriteLine();
            }

            if (!duplicateSBNKs.empty())
            {
                ConsoleWriteLine(std::u16string(u"The following SBNK")
                    + (duplicateSBNKs.size() == 1U ? u"" : u"s")
                    + u" had duplicates, the duplicates will be removed:");
                OutputDictionary(duplicateSBNKs, bankINFOEntries, _count > 1U);
                ConsoleWriteLine();
            }

            if (!duplicateSWARs.empty())
            {
                ConsoleWriteLine(std::u16string(u"The following SWAR")
                    + (duplicateSWARs.size() == 1U ? u"" : u"s")
                    + u" had duplicates, the duplicates will be removed:");
                OutputDictionary(duplicateSWARs, wavearcINFOEntries, _count > 1U);
                ConsoleWriteLine();
            }

            if (!duplicatePLAYERs.empty())
            {
                ConsoleWriteLine(std::u16string(u"The following PLAYER")
                    + (duplicatePLAYERs.size() == 1U ? u"" : u"s")
                    + u" had duplicates, the duplicates will be removed:");
                OutputDictionary(duplicatePLAYERs, playerINFOEntries, _count > 1U);
                ConsoleWriteLine();
            }
        }

        MoveDictionary SBNKMove;
        for (std::uint32_t i = 0, num = static_cast<std::uint32_t>(SBNKsToKeep.size()); i < num; ++i)
        {
            SBNKMove.emplace_back(SBNKsToKeep[static_cast<std::size_t>(i)], i);
        }

        MoveDictionary SWARMove;
        for (std::uint32_t i = 0, num = static_cast<std::uint32_t>(SWARsToKeep.size()); i < num; ++i)
        {
            SWARMove.emplace_back(SWARsToKeep[static_cast<std::size_t>(i)], i);
        }

        MoveDictionary PLAYERMove;
        for (std::uint32_t i = 0, num = static_cast<std::uint32_t>(PLAYERsToKeep.size()); i < num; ++i)
        {
            PLAYERMove.emplace_back(PLAYERsToKeep[static_cast<std::size_t>(i)], i);
        }

        const bool hasSYMBSection = _symbSection != nullptr;
        auto newSYMBSection = std::make_unique<NCSFCommon::NC::SYMBSection>();
        if (hasSYMBSection)
        {
            newSYMBSection->SEQRecord().SetNumberOfEntries(static_cast<std::uint32_t>(SSEQsToKeep.size()));
            newSYMBSection->BANKRecord().SetNumberOfEntries(static_cast<std::uint32_t>(SBNKsToKeep.size()));
            newSYMBSection->WAVEARCRecord().SetNumberOfEntries(static_cast<std::uint32_t>(SWARsToKeep.size()));
            newSYMBSection->PLAYERRecord().SetNumberOfEntries(static_cast<std::uint32_t>(PLAYERsToKeep.size()));
        }

        NCSFCommon::NC::INFOSection newINFOSection;
        newINFOSection.SEQRecord().SetNumberOfEntries(static_cast<std::uint32_t>(SSEQsToKeep.size()));
        newINFOSection.BANKRecord().SetNumberOfEntries(static_cast<std::uint32_t>(SBNKsToKeep.size()));
        newINFOSection.WAVEARCRecord().SetNumberOfEntries(static_cast<std::uint32_t>(SWARsToKeep.size()));
        newINFOSection.PLAYERRecord().SetNumberOfEntries(static_cast<std::uint32_t>(PLAYERsToKeep.size()));

        std::vector<std::shared_ptr<SSEQ>> newSSEQs;
        std::uint16_t fileID = 0;
        const auto seqSYMBEntries = hasSYMBSection
            ? Require(_symbSection).SEQRecord().Entries()
            : std::span<const SYMBRecordEntry>{};
        for (std::uint32_t i = 0, num = static_cast<std::uint32_t>(SSEQsToKeep.size()); i < num; ++i)
        {
            if (hasSYMBSection)
            {
                newSYMBSection->SEQRecord().SetEntry(i, At(seqSYMBEntries, SSEQsToKeep[static_cast<std::size_t>(i)]));
            }

            const auto& record = At(seqINFOEntries, SSEQsToKeep[static_cast<std::size_t>(i)]);
            auto newSEQEntry = std::make_shared<INFOEntrySEQ>(record.Entry.get());
            newSEQEntry->FileID(fileID);
            fileID = static_cast<std::uint16_t>(static_cast<std::uint32_t>(fileID) + 1U);
            newSEQEntry->Bank(static_cast<std::uint16_t>(
                MoveDictionaryAt(SBNKMove, GetNonDuplicateNumber(newSEQEntry->Bank(), duplicateSBNKs))));
            std::uint32_t player = 0;
            if (MoveDictionaryTryGetValue(
                    PLAYERMove,
                    GetNonDuplicateNumber(newSEQEntry->Player(), duplicatePLAYERs),
                    player))
            {
                newSEQEntry->Player(static_cast<std::uint8_t>(player));
            }
            Require(newSEQEntry->SSEQ()).EntryNumber(ToInt32Unchecked(i));
            newSSEQs.push_back(newSEQEntry->SSEQ());
            newINFOSection.SEQRecord().SetEntry(i, INFORecordEntry<INFOEntrySEQ>{record.Offset, std::move(newSEQEntry)});
        }

        std::vector<std::shared_ptr<SBNK>> newSBNKs;
        const auto bankSYMBEntries = hasSYMBSection
            ? Require(_symbSection).BANKRecord().Entries()
            : std::span<const SYMBRecordEntry>{};
        for (std::uint32_t i = 0, num = static_cast<std::uint32_t>(SBNKsToKeep.size()); i < num; ++i)
        {
            if (hasSYMBSection)
            {
                newSYMBSection->BANKRecord().SetEntry(i, At(bankSYMBEntries, SBNKsToKeep[static_cast<std::size_t>(i)]));
            }

            const auto& record = At(bankINFOEntries, SBNKsToKeep[static_cast<std::size_t>(i)]);
            auto newBANKEntry = std::make_shared<INFOEntryBANK>(record.Entry.get());
            newBANKEntry->FileID(fileID);
            fileID = static_cast<std::uint16_t>(static_cast<std::uint32_t>(fileID) + 1U);
            const auto waveArchives = newBANKEntry->WaveArchives();
            for (std::int32_t j = 0; j < 4; ++j)
            {
                const std::uint16_t waveArc = waveArchives[static_cast<std::size_t>(j)];
                if (waveArc != std::numeric_limits<std::uint16_t>::max())
                {
                    newBANKEntry->ReplaceWaveArchive(
                        j,
                        static_cast<std::uint16_t>(MoveDictionaryAt(
                            SWARMove, GetNonDuplicateNumber(waveArc, duplicateSWARs))));
                }
            }
            Require(newBANKEntry->SBNK()).EntryNumber(ToInt32Unchecked(i));
            newSBNKs.push_back(newBANKEntry->SBNK());
            newINFOSection.BANKRecord().SetEntry(i, INFORecordEntry<INFOEntryBANK>{record.Offset, std::move(newBANKEntry)});
        }

        std::vector<std::shared_ptr<SWAR>> newSWARs;
        const auto wavearcSYMBEntries = hasSYMBSection
            ? Require(_symbSection).WAVEARCRecord().Entries()
            : std::span<const SYMBRecordEntry>{};
        for (std::uint32_t i = 0, num = static_cast<std::uint32_t>(SWARsToKeep.size()); i < num; ++i)
        {
            if (hasSYMBSection)
            {
                newSYMBSection->WAVEARCRecord().SetEntry(i, At(wavearcSYMBEntries, SWARsToKeep[static_cast<std::size_t>(i)]));
            }

            const auto& record = At(wavearcINFOEntries, SWARsToKeep[static_cast<std::size_t>(i)]);
            auto newWAVEARCEntry = std::make_shared<INFOEntryWAVEARC>(record.Entry.get());
            newWAVEARCEntry->FileID(fileID);
            fileID = static_cast<std::uint16_t>(static_cast<std::uint32_t>(fileID) + 1U);
            Require(newWAVEARCEntry->SWAR()).EntryNumber(ToInt32Unchecked(i));
            newSWARs.push_back(newWAVEARCEntry->SWAR());
            newINFOSection.WAVEARCRecord().SetEntry(
                i, INFORecordEntry<INFOEntryWAVEARC>{record.Offset, std::move(newWAVEARCEntry)});
        }

        const auto playerSYMBEntries = hasSYMBSection
            ? Require(_symbSection).PLAYERRecord().Entries()
            : std::span<const SYMBRecordEntry>{};
        for (std::uint32_t i = 0, num = static_cast<std::uint32_t>(PLAYERsToKeep.size()); i < num; ++i)
        {
            if (hasSYMBSection)
            {
                newSYMBSection->PLAYERRecord().SetEntry(i, At(playerSYMBEntries, PLAYERsToKeep[static_cast<std::size_t>(i)]));
            }

            const auto& record = At(playerINFOEntries, PLAYERsToKeep[static_cast<std::size_t>(i)]);
            newINFOSection.PLAYERRecord().SetEntry(
                i,
                INFORecordEntry<INFOEntryPLAYER>{record.Offset, std::make_shared<INFOEntryPLAYER>(record.Entry.get())});
        }

        if (hasSYMBSection)
        {
            _symbSection = std::move(newSYMBSection);
        }
        _infoSection = std::move(newINFOSection);

        _sseqs.clear();
        _sseqs.insert(_sseqs.end(), newSSEQs.begin(), newSSEQs.end());
        _sbnks.clear();
        _sbnks.insert(_sbnks.end(), newSBNKs.begin(), newSBNKs.end());
        _swars.clear();
        _swars.insert(_swars.end(), newSWARs.begin(), newSWARs.end());

        NCSFCommon::NC::FATSection newFATSection;
        newFATSection.SetNumberOfRecords(fileID);
        _fatSection = std::move(newFATSection);

        if (_symbSectionNeedsCleanup)
        {
            auto& symb = Require(_symbSection);
            auto cleanupSeqSYMBEntries = symb.SEQRecord().Entries();
            auto cleanupSeqINFOEntries = _infoSection.SEQRecord().Entries();
            for (std::uint32_t i = 0, num = static_cast<std::uint32_t>(SSEQsToKeep.size()); i < num; ++i)
            {
                const auto& symbRecord = At(cleanupSeqSYMBEntries, i);
                const auto& infoRecord = At(cleanupSeqINFOEntries, i);
                std::optional<std::u16string> symbEntry = symbRecord.Name;
                if (IsNullOrEmpty(symbEntry))
                {
                    symbEntry = std::u16string(u"SSEQ") + Hex(Require(infoRecord.Entry).FileID(), 4U);
                    symb.SEQRecord().SetEntry(i, SYMBRecordEntry{symbRecord.Offset, symbEntry});
                }
                auto sseqObject = Require(infoRecord.Entry).SSEQ();
                Require(sseqObject).OriginalFilename(symbEntry);
                if (!StartsWith(*symbEntry, u"SSEQ"))
                {
                    sseqObject->Filename(Hex(i, 4U) + u" - " + *symbEntry);
                }
            }

            auto cleanupBankSYMBEntries = symb.BANKRecord().Entries();
            auto cleanupBankINFOEntries = _infoSection.BANKRecord().Entries();
            for (std::uint32_t i = 0, num = static_cast<std::uint32_t>(SBNKsToKeep.size()); i < num; ++i)
            {
                const auto& symbRecord = At(cleanupBankSYMBEntries, i);
                if (IsNullOrEmpty(symbRecord.Name))
                {
                    const auto& infoRecord = At(cleanupBankINFOEntries, i);
                    symb.BANKRecord().SetEntry(
                        i,
                        SYMBRecordEntry{
                            symbRecord.Offset,
                            std::u16string(u"SBNK") + Hex(Require(infoRecord.Entry).FileID(), 4U)});
                }
            }

            auto cleanupWavearcSYMBEntries = symb.WAVEARCRecord().Entries();
            auto cleanupWavearcINFOEntries = _infoSection.WAVEARCRecord().Entries();
            for (std::uint32_t i = 0, num = static_cast<std::uint32_t>(SWARsToKeep.size()); i < num; ++i)
            {
                const auto& symbRecord = At(cleanupWavearcSYMBEntries, i);
                if (IsNullOrEmpty(symbRecord.Name))
                {
                    const auto& infoRecord = At(cleanupWavearcINFOEntries, i);
                    symb.WAVEARCRecord().SetEntry(
                        i,
                        SYMBRecordEntry{
                            symbRecord.Offset,
                            std::u16string(u"SWAR") + Hex(Require(infoRecord.Entry).FileID(), 4U)});
                }
            }

            auto cleanupPlayerSYMBEntries = symb.PLAYERRecord().Entries();
            for (std::uint32_t i = 0, num = static_cast<std::uint32_t>(PLAYERsToKeep.size()); i < num; ++i)
            {
                const auto& symbRecord = At(cleanupPlayerSYMBEntries, i);
                if (IsNullOrEmpty(symbRecord.Name))
                {
                    symb.PLAYERRecord().SetEntry(
                        i,
                        SYMBRecordEntry{symbRecord.Offset, std::u16string(u"PLAYER") + Hex(i, 2U)});
                }
            }

            _symbSectionNeedsCleanup = false;
        }

        FixOffsetsAndSizes();
    }

    void SDAT::FixOffsetsAndSizes()
    {
        _infoOffset = 0x40U;
        if (_symbSection != nullptr)
        {
            _symbOffset = 0x40U;
            _symbSize = _symbSection->Size();
            _symbSection->FixOffsets();
            _infoOffset = _symbOffset + ((_symbSize + 3U) & ~std::uint32_t{0x03U});
        }
        _infoSize = _infoSection.Size();
        _infoSection.FixOffsets();

        const std::uint32_t recordCount = static_cast<std::uint32_t>(
            static_cast<std::uint32_t>(_sseqs.size())
            + static_cast<std::uint32_t>(_sbnks.size())
            + static_cast<std::uint32_t>(_swars.size()));
        _fatSection.ResizeRecords(recordCount);
        _fatOffset = _infoOffset + _infoSize;
        _fatSize = _fatSection.Size();

        _fileOffset = _fatOffset + _fatSize;
        std::uint32_t offset = _fileOffset + 0x10U;
        _fileSize = 0x10U;

        std::uint32_t fileID = 0;
        const auto fatRecords = _fatSection.Records();
        const auto seqEntries = _infoSection.SEQRecord().Entries();
        for (const auto& sseq : _sseqs)
        {
            auto& sseqObject = Require(sseq);
            const auto& infoRecord = At(seqEntries, sseqObject.EntryNumber());
            Require(infoRecord.Entry).FileID(fileID);
            const auto& fatRecord = At(fatRecords, fileID++);
            Require(fatRecord).Offset(offset);
            const std::uint32_t fileSize = sseqObject.Size();
            fatRecord->Size(fileSize);
            offset += fileSize;
            _fileSize += fileSize;
        }

        const auto bankEntries2 = _infoSection.BANKRecord().Entries();
        for (const auto& sbnk : _sbnks)
        {
            auto& sbnkObject = Require(sbnk);
            sbnkObject.FixOffsets();
            const auto& infoRecord = At(bankEntries2, sbnkObject.EntryNumber());
            Require(infoRecord.Entry).FileID(fileID);
            const auto& fatRecord = At(fatRecords, fileID++);
            Require(fatRecord).Offset(offset);
            const std::uint32_t fileSize = sbnkObject.Size();
            fatRecord->Size(fileSize);
            offset += fileSize;
            _fileSize += fileSize;
        }

        const auto wavearcEntries2 = _infoSection.WAVEARCRecord().Entries();
        for (const auto& swar : _swars)
        {
            auto& swarObject = Require(swar);
            const auto& infoRecord = At(wavearcEntries2, swarObject.EntryNumber());
            Require(infoRecord.Entry).FileID(fileID);
            const auto& fatRecord = At(fatRecords, fileID++);
            Require(fatRecord).Offset(offset);
            const std::uint32_t fileSize = swarObject.Size();
            fatRecord->Size(fileSize);
            offset += fileSize;
            _fileSize += fileSize;
        }

        _size = _fileOffset + _fileSize;
        _actualBlocks = static_cast<std::uint16_t>(_symbSection != nullptr ? 4U : 3U);
    }
}
