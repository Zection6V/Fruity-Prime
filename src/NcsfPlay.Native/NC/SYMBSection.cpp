#include "SYMBSection.hpp"

#include "../Common.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace
{
    [[noreturn]] void ThrowOutOfRange()
    {
        throw std::out_of_range("Specified argument was out of the range of valid values.");
    }

    [[nodiscard]] std::int32_t ToInt32Unchecked(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
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

    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::size_t offset, std::size_t count)
    {
        if (offset > span.size() || count > span.size() - offset)
        {
            ThrowOutOfRange();
        }
        return span.subspan(offset, count);
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
        std::span<std::uint8_t> span, std::int32_t offset, std::int32_t count)
    {
        if (offset < 0 || count < 0)
        {
            ThrowOutOfRange();
        }
        const std::size_t start = static_cast<std::size_t>(offset);
        const std::size_t length = static_cast<std::size_t>(count);
        if (start > span.size() || length > span.size() - start)
        {
            ThrowOutOfRange();
        }
        return span.subspan(start, length);
    }

    [[nodiscard]] std::span<const std::uint8_t> SliceFromInt32(
        std::span<const std::uint8_t> span, std::uint32_t offset)
    {
        const std::int32_t signedOffset = ToInt32Unchecked(offset);
        if (signedOffset < 0)
        {
            ThrowOutOfRange();
        }
        return Slice(span, static_cast<std::size_t>(signedOffset));
    }

    [[nodiscard]] std::span<std::uint8_t> SliceFromInt32(
        std::span<std::uint8_t> span, std::uint32_t offset)
    {
        const std::int32_t signedOffset = ToInt32Unchecked(offset);
        if (signedOffset < 0)
        {
            ThrowOutOfRange();
        }
        return Slice(span, static_cast<std::size_t>(signedOffset));
    }

    [[nodiscard]] std::uint32_t ReadUInt32LittleEndian(std::span<const std::uint8_t> span)
    {
        if (span.size() < sizeof(std::uint32_t))
        {
            ThrowOutOfRange();
        }
        return static_cast<std::uint32_t>(span[0])
            | (static_cast<std::uint32_t>(span[1]) << 8U)
            | (static_cast<std::uint32_t>(span[2]) << 16U)
            | (static_cast<std::uint32_t>(span[3]) << 24U);
    }

    void WriteUInt32LittleEndian(std::span<std::uint8_t> span, std::uint32_t value)
    {
        if (span.size() < sizeof(std::uint32_t))
        {
            ThrowOutOfRange();
        }
        span[0] = static_cast<std::uint8_t>(value);
        span[1] = static_cast<std::uint8_t>(value >> 8U);
        span[2] = static_cast<std::uint8_t>(value >> 16U);
        span[3] = static_cast<std::uint8_t>(value >> 24U);
    }

    void CopyTo(std::span<const std::uint8_t> source, std::span<std::uint8_t> destination)
    {
        if (source.size() > destination.size())
        {
            throw std::invalid_argument("Destination is too short.");
        }
        std::copy(source.begin(), source.end(), destination.begin());
    }

    [[nodiscard]] std::size_t RecordIndex(NCSFCommon::Common::SDATRecordType type)
    {
        return static_cast<std::size_t>(NCSFCommon::Common::ToByte(type));
    }

    [[nodiscard]] std::uint32_t EntryCount(std::size_t count) noexcept
    {
        return static_cast<std::uint32_t>(count);
    }
}

namespace NCSFCommon::NC
{
    const std::array<std::uint8_t, 4> SYMBSection::Header{
        static_cast<std::uint8_t>('S'),
        static_cast<std::uint8_t>('Y'),
        static_cast<std::uint8_t>('M'),
        static_cast<std::uint8_t>('B')
    };

    std::uint32_t SYMBSection::Size() const
    {
        return 0x50U + _seqRecord.Size() + _bankRecord.Size() + _wavearcRecord.Size() + _playerRecord.Size();
    }

    std::span<const std::uint32_t> SYMBSection::RecordOffsets() const noexcept
    {
        return std::span<const std::uint32_t>(_recordOffsets.data(), _recordOffsets.size());
    }

    SYMBRecord& SYMBSection::SEQRecord() noexcept
    {
        return _seqRecord;
    }

    const SYMBRecord& SYMBSection::SEQRecord() const noexcept
    {
        return _seqRecord;
    }

    SYMBRecord& SYMBSection::BANKRecord() noexcept
    {
        return _bankRecord;
    }

    const SYMBRecord& SYMBSection::BANKRecord() const noexcept
    {
        return _bankRecord;
    }

    SYMBRecord& SYMBSection::WAVEARCRecord() noexcept
    {
        return _wavearcRecord;
    }

    const SYMBRecord& SYMBSection::WAVEARCRecord() const noexcept
    {
        return _wavearcRecord;
    }

    SYMBRecord& SYMBSection::PLAYERRecord() noexcept
    {
        return _playerRecord;
    }

    const SYMBRecord& SYMBSection::PLAYERRecord() const noexcept
    {
        return _playerRecord;
    }

    SYMBSection& SYMBSection::Read(std::span<const std::uint8_t> span)
    {
#ifndef NDEBUG
        assert(Common::VerifyHeader(Slice(span, 0, Header.size()), Header));
#endif

        const std::span<const std::uint8_t> offsetBytes = Slice(span, 0x08, 0x20);
        for (std::size_t i = 0; i < _recordOffsets.size(); ++i)
        {
            _recordOffsets[i] = ReadUInt32LittleEndian(offsetBytes.subspan(i * sizeof(std::uint32_t)));
        }

        const std::uint32_t sequenceOffset = _recordOffsets[RecordIndex(Common::SDATRecordType::Sequence)];
        const std::uint32_t bankOffset = _recordOffsets[RecordIndex(Common::SDATRecordType::Bank)];
        const std::uint32_t waveArchiveOffset = _recordOffsets[RecordIndex(Common::SDATRecordType::WaveArchive)];
        const std::uint32_t playerOffset = _recordOffsets[RecordIndex(Common::SDATRecordType::Player)];

        if (sequenceOffset != 0)
        {
            _seqRecord.Read(SliceFromInt32(span, sequenceOffset), sequenceOffset);
        }
        if (bankOffset != 0)
        {
            _bankRecord.Read(SliceFromInt32(span, bankOffset), bankOffset);
        }
        if (waveArchiveOffset != 0)
        {
            _wavearcRecord.Read(SliceFromInt32(span, waveArchiveOffset), waveArchiveOffset);
        }
        if (playerOffset != 0)
        {
            _playerRecord.Read(SliceFromInt32(span, playerOffset), playerOffset);
        }
        return *this;
    }

    void SYMBSection::FixOffsets()
    {
        const std::size_t sequence = RecordIndex(Common::SDATRecordType::Sequence);
        const std::size_t sequenceArchive = RecordIndex(Common::SDATRecordType::SequenceArchive);
        const std::size_t bank = RecordIndex(Common::SDATRecordType::Bank);
        const std::size_t waveArchive = RecordIndex(Common::SDATRecordType::WaveArchive);
        const std::size_t player = RecordIndex(Common::SDATRecordType::Player);
        const std::size_t group = RecordIndex(Common::SDATRecordType::Group);
        const std::size_t player2 = RecordIndex(Common::SDATRecordType::Player2);
        const std::size_t stream = RecordIndex(Common::SDATRecordType::Stream);

        _recordOffsets[sequence] = 0x40U;
        _recordOffsets[sequenceArchive] =
            _recordOffsets[sequence] + 4U + 4U * EntryCount(_seqRecord.Entries().size());
        _recordOffsets[bank] = _recordOffsets[sequenceArchive] + 4U;
        _recordOffsets[waveArchive] =
            _recordOffsets[bank] + 4U + 4U * EntryCount(_bankRecord.Entries().size());
        _recordOffsets[player] =
            _recordOffsets[waveArchive] + 4U + 4U * EntryCount(_wavearcRecord.Entries().size());
        _recordOffsets[group] =
            _recordOffsets[player] + 4U + 4U * EntryCount(_playerRecord.Entries().size());
        _recordOffsets[player2] = _recordOffsets[group] + 4U;
        _recordOffsets[stream] = _recordOffsets[player2] + 4U;

        std::uint32_t offset = _recordOffsets[stream] + 4U;
        _seqRecord.FixOffsets(offset);
        offset += _seqRecord.SizeOfNames();
        _bankRecord.FixOffsets(offset);
        offset += _bankRecord.SizeOfNames();
        _wavearcRecord.FixOffsets(offset);
        offset += _wavearcRecord.SizeOfNames();
        _playerRecord.FixOffsets(offset);
    }

    void SYMBSection::Write(std::span<std::uint8_t> span)
    {
        const std::uint32_t size = Size();
        const std::uint32_t sizeMulOf4 = (size + 3U) & ~std::uint32_t{0x03U};
        const std::int32_t clearLength = ToInt32Unchecked(sizeMulOf4);
        std::span<std::uint8_t> clearRange = Slice(span, 0, clearLength);
        std::fill(clearRange.begin(), clearRange.end(), std::uint8_t{0});

        CopyTo(Header, span);
        WriteUInt32LittleEndian(Slice(span, 0x04), sizeMulOf4);
        std::array<std::uint8_t, 0x20> offsetBytes{};
        for (std::size_t i = 0; i < _recordOffsets.size(); ++i)
        {
            WriteUInt32LittleEndian(
                std::span<std::uint8_t>(offsetBytes).subspan(i * sizeof(std::uint32_t)),
                _recordOffsets[i]);
        }
        CopyTo(offsetBytes, Slice(span, 0x08));

        _seqRecord.WriteHeader(Slice(span, 0x40));
        std::uint32_t pos = 0x40U + _seqRecord.HeaderSize();
        pos += 0x04U;
        _bankRecord.WriteHeader(SliceFromInt32(span, pos));
        pos += _bankRecord.HeaderSize();
        _wavearcRecord.WriteHeader(SliceFromInt32(span, pos));
        pos += _wavearcRecord.HeaderSize();
        _playerRecord.WriteHeader(SliceFromInt32(span, pos));
        pos += _playerRecord.HeaderSize();
        pos += 0x0CU;

        _seqRecord.WriteData(SliceFromInt32(span, pos));
        pos += _seqRecord.SizeOfNames();
        _bankRecord.WriteData(SliceFromInt32(span, pos));
        pos += _bankRecord.SizeOfNames();
        _wavearcRecord.WriteData(SliceFromInt32(span, pos));
        pos += _wavearcRecord.SizeOfNames();
        _playerRecord.WriteData(SliceFromInt32(span, pos));
    }

    SYMBSection SYMBSection::Add(const SYMBSection* symbSection1, const SYMBSection* symbSection2)
    {
        assert(symbSection1 != nullptr || symbSection2 != nullptr);

        std::optional<SYMBSection> empty1;
        std::optional<SYMBSection> empty2;
        if (symbSection1 == nullptr)
        {
            empty1.emplace();
            symbSection1 = &*empty1;
        }
        if (symbSection2 == nullptr)
        {
            empty2.emplace();
            symbSection2 = &*empty2;
        }

        SYMBSection result;
        result._seqRecord = symbSection1->_seqRecord + symbSection2->_seqRecord;
        result._bankRecord = symbSection1->_bankRecord + symbSection2->_bankRecord;
        result._wavearcRecord = symbSection1->_wavearcRecord + symbSection2->_wavearcRecord;
        result._playerRecord = symbSection1->_playerRecord + symbSection2->_playerRecord;
        return result;
    }

    SYMBSection operator+(const SYMBSection& symbSection1, const SYMBSection& symbSection2)
    {
        return SYMBSection::Add(&symbSection1, &symbSection2);
    }

    SYMBSection operator+(const SYMBSection& symbSection, std::nullptr_t)
    {
        return SYMBSection::Add(&symbSection, nullptr);
    }

    SYMBSection operator+(std::nullptr_t, const SYMBSection& symbSection)
    {
        return SYMBSection::Add(nullptr, &symbSection);
    }
}
