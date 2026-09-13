#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace NCSFCommon::NC
{
    class SBNKInstrument;

    class SBNKInstrumentEntry
    {
    public:
        SBNKInstrumentEntry() noexcept = default;

        SBNKInstrumentEntry(const SBNKInstrumentEntry&) = delete;
        SBNKInstrumentEntry(SBNKInstrumentEntry&&) = delete;
        SBNKInstrumentEntry& operator=(const SBNKInstrumentEntry&) = delete;
        SBNKInstrumentEntry& operator=(SBNKInstrumentEntry&&) = delete;

        [[nodiscard]] std::uint8_t Record() const noexcept;
        void Record(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint16_t Offset() const noexcept;
        void Offset(std::uint16_t value) noexcept;

        [[nodiscard]] std::span<const std::shared_ptr<SBNKInstrument>> Instruments() const noexcept;

        [[nodiscard]] std::uint32_t Size() const noexcept;

        static constexpr std::uint32_t HeaderSize = 0x04U;

        [[nodiscard]] std::uint32_t DataSize() const noexcept;

        SBNKInstrumentEntry& ReadHeader(std::span<const std::uint8_t> span);
        void ReadInstruments(std::span<const std::uint8_t> span);
        [[nodiscard]] std::uint16_t FixOffset(std::uint16_t newOffset) noexcept;
        void WriteHeader(std::span<std::uint8_t> span);
        void WriteData(std::span<std::uint8_t> span);

        [[nodiscard]] bool Equals(const SBNKInstrumentEntry* other) const noexcept;
        [[nodiscard]] std::int32_t GetHashCode() const;
        [[nodiscard]] static bool OpEquality(
            const SBNKInstrumentEntry* left, const SBNKInstrumentEntry* right) noexcept;
        [[nodiscard]] static bool OpInequality(
            const SBNKInstrumentEntry* left, const SBNKInstrumentEntry* right) noexcept;

        friend bool operator==(const SBNKInstrumentEntry& left, const SBNKInstrumentEntry& right) noexcept;
        friend bool operator!=(const SBNKInstrumentEntry& left, const SBNKInstrumentEntry& right) noexcept;

    private:
        std::uint8_t _record = 0;
        std::uint16_t _offset = 0;
        std::vector<std::shared_ptr<SBNKInstrument>> _instruments;
    };
}
