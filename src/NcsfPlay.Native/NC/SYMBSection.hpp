#pragma once

#include "SYMBRecord.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace NCSFCommon::NC
{
    class SYMBSection
    {
    public:
        SYMBSection() = default;

        SYMBSection(const SYMBSection&) = delete;
        SYMBSection& operator=(const SYMBSection&) = delete;
        SYMBSection(SYMBSection&&) noexcept = default;
        SYMBSection& operator=(SYMBSection&&) noexcept = default;

        [[nodiscard]] std::uint32_t Size() const;
        [[nodiscard]] std::span<const std::uint32_t> RecordOffsets() const noexcept;

        [[nodiscard]] SYMBRecord& SEQRecord() noexcept;
        [[nodiscard]] const SYMBRecord& SEQRecord() const noexcept;
        [[nodiscard]] SYMBRecord& BANKRecord() noexcept;
        [[nodiscard]] const SYMBRecord& BANKRecord() const noexcept;
        [[nodiscard]] SYMBRecord& WAVEARCRecord() noexcept;
        [[nodiscard]] const SYMBRecord& WAVEARCRecord() const noexcept;
        [[nodiscard]] SYMBRecord& PLAYERRecord() noexcept;
        [[nodiscard]] const SYMBRecord& PLAYERRecord() const noexcept;

        SYMBSection& Read(std::span<const std::uint8_t> span);
        void FixOffsets();
        void Write(std::span<std::uint8_t> span);

        [[nodiscard]] static SYMBSection Add(const SYMBSection* symbSection1, const SYMBSection* symbSection2);

        friend SYMBSection operator+(const SYMBSection& symbSection1, const SYMBSection& symbSection2);
        friend SYMBSection operator+(const SYMBSection& symbSection, std::nullptr_t);
        friend SYMBSection operator+(std::nullptr_t, const SYMBSection& symbSection);

    private:
        static const std::array<std::uint8_t, 4> Header;

        std::array<std::uint32_t, 8> _recordOffsets{};
        SYMBRecord _seqRecord;
        SYMBRecord _bankRecord;
        SYMBRecord _wavearcRecord;
        SYMBRecord _playerRecord;
    };
}
