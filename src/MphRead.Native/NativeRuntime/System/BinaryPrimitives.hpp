#pragma once

// System.Buffers.Binary.BinaryPrimitives over spans, and the span slicing
// (span[a..], span.Slice(a, n)) that goes with it. Every access is checked
// the way a .NET span is: out of range throws ArgumentOutOfRangeException.

#include "Exceptions.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace MphRead::NativeRuntime
{
    namespace BinaryPrimitivesDetail
    {
        template <typename T>
        void Require(std::span<T> span, std::size_t count)
        {
            if (span.size() < count)
            {
                throw System::ArgumentOutOfRangeException("length");
            }
        }

        template <typename U, typename T>
        [[nodiscard]] U ReadLittle(std::span<T> source)
        {
            Require(source, sizeof(U));
            U value = 0;
            for (std::size_t i = 0; i < sizeof(U); ++i)
            {
                value = static_cast<U>(value | (static_cast<U>(source[i]) << (8 * i)));
            }
            return value;
        }

        template <typename U>
        void WriteLittle(std::span<std::uint8_t> destination, U value)
        {
            Require(destination, sizeof(U));
            for (std::size_t i = 0; i < sizeof(U); ++i)
            {
                destination[i] = static_cast<std::uint8_t>(value >> (8 * i));
            }
        }
    }

    // span[start..] / span.Slice(start).
    template <typename T>
    [[nodiscard]] std::span<T> SpanSlice(std::span<T> span, std::size_t start)
    {
        if (start > span.size())
        {
            throw System::ArgumentOutOfRangeException("start");
        }
        return span.subspan(start);
    }

    // span.Slice(start, length) / span[start..(start + length)].
    template <typename T>
    [[nodiscard]] std::span<T> SpanSlice(std::span<T> span, std::size_t start, std::size_t length)
    {
        if (start > span.size() || length > span.size() - start)
        {
            throw System::ArgumentOutOfRangeException("start");
        }
        return span.subspan(start, length);
    }

    [[nodiscard]] inline std::uint16_t ReadUInt16LittleEndian(std::span<const std::uint8_t> source)
    {
        return BinaryPrimitivesDetail::ReadLittle<std::uint16_t>(source);
    }
    [[nodiscard]] inline std::int16_t ReadInt16LittleEndian(std::span<const std::uint8_t> source)
    {
        return static_cast<std::int16_t>(ReadUInt16LittleEndian(source));
    }
    [[nodiscard]] inline std::uint32_t ReadUInt32LittleEndian(std::span<const std::uint8_t> source)
    {
        return BinaryPrimitivesDetail::ReadLittle<std::uint32_t>(source);
    }
    [[nodiscard]] inline std::int32_t ReadInt32LittleEndian(std::span<const std::uint8_t> source)
    {
        return static_cast<std::int32_t>(ReadUInt32LittleEndian(source));
    }
    [[nodiscard]] inline std::uint64_t ReadUInt64LittleEndian(std::span<const std::uint8_t> source)
    {
        return BinaryPrimitivesDetail::ReadLittle<std::uint64_t>(source);
    }
    [[nodiscard]] inline std::int64_t ReadInt64LittleEndian(std::span<const std::uint8_t> source)
    {
        return static_cast<std::int64_t>(ReadUInt64LittleEndian(source));
    }
    [[nodiscard]] inline float ReadSingleLittleEndian(std::span<const std::uint8_t> source)
    {
        return std::bit_cast<float>(ReadUInt32LittleEndian(source));
    }
    [[nodiscard]] inline std::uint32_t ReadUInt32BigEndian(std::span<const std::uint8_t> source)
    {
        BinaryPrimitivesDetail::Require(source, 4);
        return (static_cast<std::uint32_t>(source[0]) << 24) | (static_cast<std::uint32_t>(source[1]) << 16)
            | (static_cast<std::uint32_t>(source[2]) << 8) | static_cast<std::uint32_t>(source[3]);
    }

    inline void WriteUInt16LittleEndian(std::span<std::uint8_t> destination, std::uint16_t value)
    {
        BinaryPrimitivesDetail::WriteLittle(destination, value);
    }
    inline void WriteInt16LittleEndian(std::span<std::uint8_t> destination, std::int16_t value)
    {
        WriteUInt16LittleEndian(destination, static_cast<std::uint16_t>(value));
    }
    inline void WriteUInt32LittleEndian(std::span<std::uint8_t> destination, std::uint32_t value)
    {
        BinaryPrimitivesDetail::WriteLittle(destination, value);
    }
    inline void WriteInt32LittleEndian(std::span<std::uint8_t> destination, std::int32_t value)
    {
        WriteUInt32LittleEndian(destination, static_cast<std::uint32_t>(value));
    }
    inline void WriteUInt64LittleEndian(std::span<std::uint8_t> destination, std::uint64_t value)
    {
        BinaryPrimitivesDetail::WriteLittle(destination, value);
    }
    inline void WriteInt64LittleEndian(std::span<std::uint8_t> destination, std::int64_t value)
    {
        WriteUInt64LittleEndian(destination, static_cast<std::uint64_t>(value));
    }
    inline void WriteSingleLittleEndian(std::span<std::uint8_t> destination, float value)
    {
        WriteUInt32LittleEndian(destination, std::bit_cast<std::uint32_t>(value));
    }
    inline void WriteUInt32BigEndian(std::span<std::uint8_t> destination, std::uint32_t value)
    {
        BinaryPrimitivesDetail::Require(destination, 4);
        destination[0] = static_cast<std::uint8_t>(value >> 24);
        destination[1] = static_cast<std::uint8_t>(value >> 16);
        destination[2] = static_cast<std::uint8_t>(value >> 8);
        destination[3] = static_cast<std::uint8_t>(value);
    }
}
