#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace NCSFCommon::NC
{
    class SWAV
    {
    public:
        SWAV() noexcept = default;

        SWAV(const SWAV&) = delete;
        SWAV(SWAV&&) = delete;
        SWAV& operator=(const SWAV&) = delete;
        SWAV& operator=(SWAV&&) = delete;

        [[nodiscard]] std::uint8_t WaveType() const noexcept;
        void WaveType(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t Loop() const noexcept;
        void Loop(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint16_t SampleRate() const noexcept;
        void SampleRate(std::uint16_t value) noexcept;

        [[nodiscard]] std::uint16_t Time() const noexcept;
        void Time(std::uint16_t value) noexcept;

        [[nodiscard]] std::uint16_t OriginalLoopOffset() const noexcept;
        void OriginalLoopOffset(std::uint16_t value) noexcept;

        [[nodiscard]] std::uint32_t LoopOffset() const noexcept;
        void LoopOffset(std::uint32_t value) noexcept;

        [[nodiscard]] std::uint32_t OriginalLoopLength() const noexcept;
        void OriginalLoopLength(std::uint32_t value) noexcept;

        [[nodiscard]] std::uint32_t LoopLength() const noexcept;
        void LoopLength(std::uint32_t value) noexcept;

        [[nodiscard]] std::span<const std::uint8_t> OriginalData() const noexcept;
        [[nodiscard]] std::span<const float> Data() const noexcept;

        [[nodiscard]] std::uint32_t Size() const noexcept;

        void DecodeADPCM(std::uint32_t len);
        SWAV& Read(std::span<const std::uint8_t> span);
        void Write(std::span<std::uint8_t> span);

        [[nodiscard]] bool Equals(const SWAV* other) const noexcept;
        [[nodiscard]] std::int32_t GetHashCode() const;
        [[nodiscard]] static bool OpEquality(const SWAV* left, const SWAV* right) noexcept;
        [[nodiscard]] static bool OpInequality(const SWAV* left, const SWAV* right) noexcept;

        friend bool operator==(const SWAV& left, const SWAV& right) noexcept;
        friend bool operator!=(const SWAV& left, const SWAV& right) noexcept;

    private:
        static constexpr std::array<std::int32_t, 16> IMAIndexTable =
        {
            -1, -1, -1, -1, 2, 4, 6, 8,
            -1, -1, -1, -1, 2, 4, 6, 8
        };

        static constexpr std::array<std::int32_t, 89> IMAStepTable =
        {
            0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x0010, 0x0011,
            0x0013, 0x0015, 0x0017, 0x0019, 0x001C, 0x001F, 0x0022, 0x0025, 0x0029, 0x002D,
            0x0032, 0x0037, 0x003C, 0x0042, 0x0049, 0x0050, 0x0058, 0x0061, 0x006B, 0x0076,
            0x0082, 0x008F, 0x009D, 0x00AD, 0x00BE, 0x00D1, 0x00E6, 0x00FD, 0x0117, 0x0133,
            0x0151, 0x0173, 0x0198, 0x01C1, 0x01EE, 0x0220, 0x0256, 0x0292, 0x02D4, 0x031C,
            0x036C, 0x03C3, 0x0424, 0x048E, 0x0502, 0x0583, 0x0610, 0x06AB, 0x0756, 0x0812,
            0x08E0, 0x09C3, 0x0ABD, 0x0BD0, 0x0CFF, 0x0E4C, 0x0FBA, 0x114C, 0x1307, 0x14EE,
            0x1706, 0x1954, 0x1BDC, 0x1EA5, 0x21B6, 0x2515, 0x28CA, 0x2CDF, 0x315B, 0x364B,
            0x3BB9, 0x41B2, 0x4844, 0x4F7E, 0x5771, 0x602F, 0x69CE, 0x7462, 0x7FFF
        };

        static void DecodeADPCMNibble(
            std::int32_t nibble, std::int32_t& stepIndex, std::int32_t& predictedValue);

        void SetOriginalDataCount(std::int32_t count);
        void EnsureDataCapacity(std::int32_t capacity);
        void ClearData() noexcept;
        void AddData(float value);

        std::uint8_t _waveType = 0;
        std::uint8_t _loop = 0;
        std::uint16_t _sampleRate = 0;
        std::uint16_t _time = 0;
        std::uint16_t _originalLoopOffset = 0;
        std::uint32_t _loopOffset = 0;
        std::uint32_t _originalLoopLength = 0;
        std::uint32_t _loopLength = 0;

        // _originalDataStorage.size() mirrors managed List<byte>.Capacity; the
        // separate count is required to preserve CollectionsMarshal.SetCount's
        // stale value-type slots when increasing Count without a reallocation.
        std::vector<std::uint8_t> _originalDataStorage;
        std::int32_t _originalDataCount = 0;

        // Likewise, keep managed List<float> Count separate from backing-array
        // capacity so Clear/EnsureCapacity/Add retain the C# allocation semantics.
        std::vector<float> _dataStorage;
        std::int32_t _dataCount = 0;
    };
}
