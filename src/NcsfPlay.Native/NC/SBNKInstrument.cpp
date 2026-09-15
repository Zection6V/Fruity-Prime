#include "SBNKInstrument.hpp"

#include <bit>
#include <cstddef>
#include <random>
#include <stdexcept>

namespace
{
    constexpr std::uint32_t Prime1 = 2654435761U;
    constexpr std::uint32_t Prime2 = 2246822519U;
    constexpr std::uint32_t Prime3 = 3266489917U;

    [[noreturn]] void ThrowIndexOutOfRange()
    {
        throw std::out_of_range("Index was outside the bounds of the array.");
    }

    [[noreturn]] void ThrowArgumentOutOfRange()
    {
        throw std::out_of_range("Specified argument was out of the range of valid values.");
    }

    [[nodiscard]] const std::uint8_t& At(std::span<const std::uint8_t> span, std::size_t index)
    {
        if (index >= span.size())
        {
            ThrowIndexOutOfRange();
        }
        return span[index];
    }

    [[nodiscard]] std::uint8_t& At(std::span<std::uint8_t> span, std::size_t index)
    {
        if (index >= span.size())
        {
            ThrowIndexOutOfRange();
        }
        return span[index];
    }

    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            ThrowArgumentOutOfRange();
        }
        return span.subspan(offset);
    }

    [[nodiscard]] std::span<std::uint8_t> Slice(
        std::span<std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            ThrowArgumentOutOfRange();
        }
        return span.subspan(offset);
    }

    [[nodiscard]] std::uint16_t ReadUInt16LittleEndian(std::span<const std::uint8_t> span)
    {
        if (span.size() < sizeof(std::uint16_t))
        {
            ThrowArgumentOutOfRange();
        }
        return static_cast<std::uint16_t>(static_cast<std::uint16_t>(span[0])
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(span[1]) << 8U));
    }

    void WriteUInt16LittleEndian(std::span<std::uint8_t> span, std::uint16_t value)
    {
        if (span.size() < sizeof(std::uint16_t))
        {
            ThrowArgumentOutOfRange();
        }
        span[0] = static_cast<std::uint8_t>(value);
        span[1] = static_cast<std::uint8_t>(value >> 8U);
    }

    // System.HashCode in .NET 9 uses a process-global randomized seed.
    [[nodiscard]] std::uint32_t GlobalHashSeed()
    {
        static const std::uint32_t seed = []
        {
            std::random_device device;
            std::uniform_int_distribution<std::uint32_t> distribution;
            return distribution(device);
        }();
        return seed;
    }

    [[nodiscard]] std::uint32_t Round(std::uint32_t hash, std::uint32_t input)
    {
        return std::rotl(hash + input * Prime2, 13) * Prime1;
    }

    [[nodiscard]] std::uint32_t MixState(
        std::uint32_t v1, std::uint32_t v2, std::uint32_t v3, std::uint32_t v4)
    {
        return std::rotl(v1, 1) + std::rotl(v2, 7) + std::rotl(v3, 12) + std::rotl(v4, 18);
    }

    [[nodiscard]] std::uint32_t MixFinal(std::uint32_t hash)
    {
        hash ^= hash >> 15U;
        hash *= Prime2;
        hash ^= hash >> 13U;
        hash *= Prime3;
        hash ^= hash >> 16U;
        return hash;
    }

    [[nodiscard]] std::int32_t CombineHashCodes(
        std::int32_t value1,
        std::int32_t value2,
        std::int32_t value3,
        std::int32_t value4,
        std::int32_t value5,
        std::int32_t value6,
        std::int32_t value7,
        std::int32_t value8)
    {
        const std::uint32_t seed = GlobalHashSeed();
        std::uint32_t v1 = seed + Prime1 + Prime2;
        std::uint32_t v2 = seed + Prime2;
        std::uint32_t v3 = seed;
        std::uint32_t v4 = seed - Prime1;

        v1 = Round(v1, static_cast<std::uint32_t>(value1));
        v2 = Round(v2, static_cast<std::uint32_t>(value2));
        v3 = Round(v3, static_cast<std::uint32_t>(value3));
        v4 = Round(v4, static_cast<std::uint32_t>(value4));
        v1 = Round(v1, static_cast<std::uint32_t>(value5));
        v2 = Round(v2, static_cast<std::uint32_t>(value6));
        v3 = Round(v3, static_cast<std::uint32_t>(value7));
        v4 = Round(v4, static_cast<std::uint32_t>(value8));

        std::uint32_t hash = MixState(v1, v2, v3, v4);
        hash += 32U;
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }
}

namespace NCSFCommon::NC
{
    SBNKInstrument::SBNKInstrument(
        std::uint8_t lowNote, std::uint8_t highNote, std::uint8_t record) noexcept
        : _lowNote(lowNote), _highNote(highNote), _record(record)
    {
    }

    std::uint8_t SBNKInstrument::LowNote() const noexcept
    {
        return _lowNote;
    }

    void SBNKInstrument::LowNote(std::uint8_t value) noexcept
    {
        _lowNote = value;
    }

    std::uint8_t SBNKInstrument::HighNote() const noexcept
    {
        return _highNote;
    }

    void SBNKInstrument::HighNote(std::uint8_t value) noexcept
    {
        _highNote = value;
    }

    std::uint8_t SBNKInstrument::Record() const noexcept
    {
        return _record;
    }

    void SBNKInstrument::Record(std::uint8_t value) noexcept
    {
        _record = value;
    }

    std::uint16_t SBNKInstrument::SWAV() const noexcept
    {
        return _swav;
    }

    void SBNKInstrument::SWAV(std::uint16_t value) noexcept
    {
        _swav = value;
    }

    std::uint16_t SBNKInstrument::SWAR() const noexcept
    {
        return _swar;
    }

    void SBNKInstrument::SWAR(std::uint16_t value) noexcept
    {
        _swar = value;
    }

    std::uint8_t SBNKInstrument::NoteNumber() const noexcept
    {
        return _noteNumber;
    }

    void SBNKInstrument::NoteNumber(std::uint8_t value) noexcept
    {
        _noteNumber = value;
    }

    std::uint8_t SBNKInstrument::AttackRate() const noexcept
    {
        return _attackRate;
    }

    void SBNKInstrument::AttackRate(std::uint8_t value) noexcept
    {
        _attackRate = value;
    }

    std::uint8_t SBNKInstrument::DecayRate() const noexcept
    {
        return _decayRate;
    }

    void SBNKInstrument::DecayRate(std::uint8_t value) noexcept
    {
        _decayRate = value;
    }

    std::uint8_t SBNKInstrument::SustainLevel() const noexcept
    {
        return _sustainLevel;
    }

    void SBNKInstrument::SustainLevel(std::uint8_t value) noexcept
    {
        _sustainLevel = value;
    }

    std::uint8_t SBNKInstrument::ReleaseRate() const noexcept
    {
        return _releaseRate;
    }

    void SBNKInstrument::ReleaseRate(std::uint8_t value) noexcept
    {
        _releaseRate = value;
    }

    std::uint8_t SBNKInstrument::Pan() const noexcept
    {
        return _pan;
    }

    void SBNKInstrument::Pan(std::uint8_t value) noexcept
    {
        _pan = value;
    }

    SBNKInstrument& SBNKInstrument::Read(std::span<const std::uint8_t> span)
    {
        _swav = ReadUInt16LittleEndian(span);
        _swar = ReadUInt16LittleEndian(Slice(span, 0x02U));
        _noteNumber = At(span, 0x04U);
        _attackRate = At(span, 0x05U);
        _decayRate = At(span, 0x06U);
        _sustainLevel = At(span, 0x07U);
        _releaseRate = At(span, 0x08U);
        _pan = At(span, 0x09U);
        return *this;
    }

    void SBNKInstrument::Write(std::span<std::uint8_t> span)
    {
        WriteUInt16LittleEndian(span, _swav);
        WriteUInt16LittleEndian(Slice(span, 0x02U), _swar);
        At(span, 0x04U) = _noteNumber;
        At(span, 0x05U) = _attackRate;
        At(span, 0x06U) = _decayRate;
        At(span, 0x07U) = _sustainLevel;
        At(span, 0x08U) = _releaseRate;
        At(span, 0x09U) = _pan;
    }

    bool SBNKInstrument::Equals(const SBNKInstrument* other) const noexcept
    {
        return other != nullptr
            && _swav == other->_swav
            && _swar == other->_swar
            && _noteNumber == other->_noteNumber
            && _attackRate == other->_attackRate
            && _decayRate == other->_decayRate
            && _sustainLevel == other->_sustainLevel
            && _releaseRate == other->_releaseRate
            && _pan == other->_pan;
    }

    bool SBNKInstrument::Equals(const std::any& obj) const noexcept
    {
        if (const auto* other = std::any_cast<SBNKInstrument*>(&obj))
        {
            return Equals(*other);
        }
        if (const auto* other = std::any_cast<const SBNKInstrument*>(&obj))
        {
            return Equals(*other);
        }
        return false;
    }

    std::int32_t SBNKInstrument::GetHashCode() const
    {
        return CombineHashCodes(
            static_cast<std::int32_t>(_swav),
            static_cast<std::int32_t>(_swar),
            static_cast<std::int32_t>(_noteNumber),
            static_cast<std::int32_t>(_attackRate),
            static_cast<std::int32_t>(_decayRate),
            static_cast<std::int32_t>(_sustainLevel),
            static_cast<std::int32_t>(_releaseRate),
            static_cast<std::int32_t>(_pan));
    }

    // Preserve left?.Equals(right) ?? false, including null == null being false.
    bool SBNKInstrument::OpEquality(
        const SBNKInstrument* left, const SBNKInstrument* right) noexcept
    {
        return left != nullptr && left->Equals(right);
    }

    bool SBNKInstrument::OpInequality(
        const SBNKInstrument* left, const SBNKInstrument* right) noexcept
    {
        return !OpEquality(left, right);
    }

    bool operator==(const SBNKInstrument& left, const SBNKInstrument& right) noexcept
    {
        return left.Equals(&right);
    }

    bool operator!=(const SBNKInstrument& left, const SBNKInstrument& right) noexcept
    {
        return !(left == right);
    }
}
