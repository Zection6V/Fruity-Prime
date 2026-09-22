#pragma once

#include "../Formats/Types.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace System
{
    class IndexOutOfRangeException final : public std::out_of_range
    {
    public:
        IndexOutOfRangeException()
            : std::out_of_range("Index was outside the bounds of the array.")
        {
        }
    };

    class InvalidCastException final : public std::runtime_error
    {
    public:
        InvalidCastException()
            : std::runtime_error("Specified cast is not valid.")
        {
        }
    };
}

namespace MphRead::Testing
{
    class TestMisc final
    {
    public:
        static void TestAllFV();
        static void VerifyFV();
        static void TestFV(std::optional<std::string> path = std::nullopt, bool verify = false);
        static void TestCameraSequences();
        static void TestCameraSequenceFiles();
        static void TestAllCollision();
        static void TestAllFhCollision();
        static void ConvertRoomToMph(
            const std::string& room, std::optional<std::string> over = std::nullopt);
        static void ConvertRoomToFh(
            const std::string& room, std::optional<std::string> over = std::nullopt);
        static void TestCameraShake();

    private:
        using byte = std::uint8_t;
        using ushort = std::uint16_t;
        using uint = std::uint32_t;
        using ulong = std::uint64_t;

        template <typename T>
        class Span final
        {
        public:
            Span() noexcept = default;
            Span(T* data, std::size_t length) noexcept : _data(data), _length(length) {}

            template <std::size_t N>
            explicit Span(std::array<T, N>& values) noexcept
                : _data(values.data()), _length(values.size())
            {
            }

            explicit Span(std::vector<T>& values) noexcept
                : _data(values.data()), _length(values.size())
            {
            }

            [[nodiscard]] std::size_t Length() const noexcept { return _length; }

            [[nodiscard]] T& operator[](std::int64_t index) const
            {
                const std::int32_t narrowed = Narrow(index);
                if (narrowed < 0 || static_cast<std::size_t>(narrowed) >= _length)
                {
                    throw System::IndexOutOfRangeException();
                }
                return _data[static_cast<std::size_t>(narrowed)];
            }

            [[nodiscard]] Span Slice(std::int64_t start) const
            {
                const std::int32_t narrowed = Narrow(start);
                if (narrowed < 0 || static_cast<std::size_t>(narrowed) > _length)
                {
                    throw System::ArgumentOutOfRangeException("start");
                }
                T* const slicedData = narrowed == 0 ? _data : _data + narrowed;
                return Span(slicedData, _length - static_cast<std::size_t>(narrowed));
            }

            [[nodiscard]] Span Slice(std::int64_t start, std::int64_t length) const
            {
                const std::int32_t narrowedStart = Narrow(start);
                const std::int32_t narrowedLength = Narrow(length);
                if (narrowedStart < 0 || static_cast<std::size_t>(narrowedStart) > _length)
                {
                    throw System::ArgumentOutOfRangeException("start");
                }
                if (narrowedLength < 0
                    || static_cast<std::size_t>(narrowedLength)
                        > _length - static_cast<std::size_t>(narrowedStart))
                {
                    throw System::ArgumentOutOfRangeException("length");
                }
                T* const slicedData = narrowedStart == 0 ? _data : _data + narrowedStart;
                return Span(slicedData, static_cast<std::size_t>(narrowedLength));
            }

            template <typename U>
            [[nodiscard]] Span<U> Cast() const noexcept
            {
                return Span<U>(
                    reinterpret_cast<U*>(_data),
                    (_length * sizeof(T)) / sizeof(U));
            }

            T Consume()
            {
                T value = (*this)[0];
                *this = Slice(1);
                return value;
            }

            void Clear() const
            {
                for (std::size_t i = 0; i < _length; ++i)
                {
                    _data[i] = T{};
                }
            }

        private:
            T* _data = nullptr;
            std::size_t _length = 0;

            [[nodiscard]] static std::int32_t Narrow(std::int64_t value) noexcept
            {
                return std::bit_cast<std::int32_t>(
                    static_cast<std::uint32_t>(static_cast<std::uint64_t>(value)));
            }
        };

        template <typename TFrom, typename TTo>
        [[nodiscard]] static Span<TTo> MemoryCast(Span<TFrom> span) noexcept
        {
            return span.template Cast<TTo>();
        }

        TestMisc() = delete;
        TestMisc(const TestMisc&) = delete;
        TestMisc& operator=(const TestMisc&) = delete;
        TestMisc(TestMisc&&) = delete;
        TestMisc& operator=(TestMisc&&) = delete;

        static const std::vector<int> _dword206B720;
        static const std::vector<int> _dword206B820;
        static const std::vector<int> _dword206B8A0;
        static const std::vector<int> _dword206B920;
        static const std::vector<int> _dword206B960;
        static const std::vector<int> _dword206B980;
        static const std::vector<int> _dword206BEBC;
        static const std::vector<short> _dword206C268;
        static const std::vector<int> _dword206B2A0;
        static const std::vector<byte> _byte2067320;

        static std::vector<double> _allDecodeTimes;
        static std::vector<double> _individualDecodeAvgs;
        static std::vector<double> _currentDecodeTimes;
        static double _maxDecodeTime;
        static double _totalDecodeTime;

        static int _readBit;
        alignas(4) static std::array<byte, 256 * 192 * 2> _outputBuf1;
        alignas(4) static std::array<byte, 256 * 192 * 2> _outputBuf2;
        static bool _outputBufferSwap;

        static void TestVideo(int width, int height, Span<byte> videoData, int frameIndex);

        static void Sub20679B0(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub2068488(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub206A0B8(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub206935C(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub2069D44(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub2068B24(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub206A5A4(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub20697D0(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub2069EC0(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub206AD40(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub206AE88(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub206AC14(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub206B0C4(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub206B1B8(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void Sub2067388(Span<ushort>& wordBuf, Span<ushort> outputSpan2Slice);
        static void Sub20674E4(Span<ushort>& wordBuf, Span<byte> outputSpan2, uint outputPos);
        static void Sub206A8C0(uint& outputPos, uint& value, Span<byte>& outputSpan1,
            Span<byte>& outputSpan2, Span<uint>& intBuf, Span<ushort>& wordBuf, Span<byte>& byteBuf);
        static void ReadNextBit(uint& value, Span<uint>& span);
        [[nodiscard]] static uint NextBit(uint value);
        [[nodiscard]] static uint NextValueCarry(Span<uint>& span);
        static void Nop() noexcept;
    };
}
