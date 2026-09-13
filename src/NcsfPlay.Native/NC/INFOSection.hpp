#pragma once

#include "INFOEntryBANK.hpp"
#include "INFOEntryPLAYER.hpp"
#include "INFOEntrySEQ.hpp"
#include "INFOEntryWAVEARC.hpp"
#include "INFORecord.hpp"

#include <array>
#include <cstdint>
#include <cstddef>
#include <span>

namespace NCSFCommon::NC
{
    class INFOSection
    {
    public:
        INFOSection() = default;

        INFOSection(const INFOSection&) = delete;
        INFOSection& operator=(const INFOSection&) = delete;
        INFOSection(INFOSection&&) noexcept = default;
        INFOSection& operator=(INFOSection&&) noexcept = default;

        [[nodiscard]] std::uint32_t Size() const;
        [[nodiscard]] std::span<const std::uint32_t> RecordOffsets() const noexcept;

        [[nodiscard]] INFORecord<INFOEntrySEQ>& SEQRecord() noexcept;
        [[nodiscard]] const INFORecord<INFOEntrySEQ>& SEQRecord() const noexcept;
        [[nodiscard]] INFORecord<INFOEntryBANK>& BANKRecord() noexcept;
        [[nodiscard]] const INFORecord<INFOEntryBANK>& BANKRecord() const noexcept;
        [[nodiscard]] INFORecord<INFOEntryWAVEARC>& WAVEARCRecord() noexcept;
        [[nodiscard]] const INFORecord<INFOEntryWAVEARC>& WAVEARCRecord() const noexcept;
        [[nodiscard]] INFORecord<INFOEntryPLAYER>& PLAYERRecord() noexcept;
        [[nodiscard]] const INFORecord<INFOEntryPLAYER>& PLAYERRecord() const noexcept;

        void Read(std::span<const std::uint8_t> span);
        void FixOffsets();
        void Write(std::span<std::uint8_t> span);

        [[nodiscard]] static INFOSection Add(const INFOSection* infoSection1, const INFOSection* infoSection2);

        friend INFOSection operator+(const INFOSection& infoSection1, const INFOSection& infoSection2);
        friend INFOSection operator+(const INFOSection& infoSection, std::nullptr_t);
        friend INFOSection operator+(std::nullptr_t, const INFOSection& infoSection);

    private:
        static const std::array<std::uint8_t, 4> Header;

        std::array<std::uint32_t, 8> _recordOffsets{};
        INFORecord<INFOEntrySEQ> _seqRecord;
        INFORecord<INFOEntryBANK> _bankRecord;
        INFORecord<INFOEntryWAVEARC> _wavearcRecord;
        INFORecord<INFOEntryPLAYER> _playerRecord;
    };
}
