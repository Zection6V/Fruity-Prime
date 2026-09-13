#pragma once

#include <cstdint>
#include <span>

namespace NCSFCommon::NC
{
    class SBNKInstrument
    {
    public:
        SBNKInstrument(std::uint8_t lowNote, std::uint8_t highNote, std::uint8_t record) noexcept;

        SBNKInstrument(const SBNKInstrument&) = delete;
        SBNKInstrument(SBNKInstrument&&) = delete;
        SBNKInstrument& operator=(const SBNKInstrument&) = delete;
        SBNKInstrument& operator=(SBNKInstrument&&) = delete;

        [[nodiscard]] std::uint8_t LowNote() const noexcept;
        void LowNote(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t HighNote() const noexcept;
        void HighNote(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t Record() const noexcept;
        void Record(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint16_t SWAV() const noexcept;
        void SWAV(std::uint16_t value) noexcept;

        [[nodiscard]] std::uint16_t SWAR() const noexcept;
        void SWAR(std::uint16_t value) noexcept;

        [[nodiscard]] std::uint8_t NoteNumber() const noexcept;
        void NoteNumber(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t AttackRate() const noexcept;
        void AttackRate(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t DecayRate() const noexcept;
        void DecayRate(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t SustainLevel() const noexcept;
        void SustainLevel(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t ReleaseRate() const noexcept;
        void ReleaseRate(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t Pan() const noexcept;
        void Pan(std::uint8_t value) noexcept;

        static constexpr std::uint32_t Size = 0x0AU;

        SBNKInstrument& Read(std::span<const std::uint8_t> span);
        void Write(std::span<std::uint8_t> span);

        [[nodiscard]] bool Equals(const SBNKInstrument* other) const noexcept;
        [[nodiscard]] std::int32_t GetHashCode() const;
        [[nodiscard]] static bool OpEquality(const SBNKInstrument* left, const SBNKInstrument* right) noexcept;
        [[nodiscard]] static bool OpInequality(const SBNKInstrument* left, const SBNKInstrument* right) noexcept;

        friend bool operator==(const SBNKInstrument& left, const SBNKInstrument& right) noexcept;
        friend bool operator!=(const SBNKInstrument& left, const SBNKInstrument& right) noexcept;

    private:
        std::uint8_t _lowNote;
        std::uint8_t _highNote;
        std::uint8_t _record;
        std::uint16_t _swav = 0;
        std::uint16_t _swar = 0;
        std::uint8_t _noteNumber = 0;
        std::uint8_t _attackRate = 0;
        std::uint8_t _decayRate = 0;
        std::uint8_t _sustainLevel = 0;
        std::uint8_t _releaseRate = 0;
        std::uint8_t _pan = 0;
    };
}
