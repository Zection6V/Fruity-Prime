#include "PlayerEntityIconBounds.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
    std::int32_t FromWrappedUnsigned(std::uint32_t value)
    {
        if (value <= 0x7FFFFFFFU)
        {
            return static_cast<std::int32_t>(value);
        }
        return -1 - static_cast<std::int32_t>(0xFFFFFFFFU - value);
    }

    std::int32_t WrapAdd(std::int32_t left, std::int32_t right)
    {
        return FromWrappedUnsigned(static_cast<std::uint32_t>(left)
            + static_cast<std::uint32_t>(right));
    }

    std::int32_t WrapSubtract(std::int32_t left, std::int32_t right)
    {
        return FromWrappedUnsigned(static_cast<std::uint32_t>(left)
            - static_cast<std::uint32_t>(right));
    }

    std::int32_t WrapMultiply(std::int32_t left, std::int32_t right)
    {
        return FromWrappedUnsigned(static_cast<std::uint32_t>(left)
            * static_cast<std::uint32_t>(right));
    }

    float Int32ToSingle(std::int32_t value)
    {
        static_assert(sizeof(float) == sizeof(std::uint32_t));
        static_assert(std::numeric_limits<float>::is_iec559);
        static_assert(std::numeric_limits<float>::digits == 24);

        if (value == 0)
        {
            return 0.0F;
        }

        const std::uint32_t sign = value < 0 ? 0x80000000U : 0U;
        const std::uint32_t unsignedValue = static_cast<std::uint32_t>(value);
        const std::uint32_t magnitude = value < 0 ? 0U - unsignedValue : unsignedValue;
        std::int32_t exponent = static_cast<std::int32_t>(std::bit_width(magnitude)) - 1;

        std::uint32_t significand;
        if (exponent <= 23)
        {
            significand = magnitude << (23 - exponent);
        }
        else
        {
            const std::int32_t shift = exponent - 23;
            significand = magnitude >> shift;
            const std::uint32_t remainderMask = (std::uint32_t{1} << shift) - 1U;
            const std::uint32_t remainder = magnitude & remainderMask;
            const std::uint32_t halfway = std::uint32_t{1} << (shift - 1);
            if (remainder > halfway || (remainder == halfway && (significand & 1U) != 0))
            {
                significand++;
                if (significand == 0x01000000U)
                {
                    significand >>= 1;
                    exponent++;
                }
            }
        }

        const std::uint32_t bits = sign
            | (static_cast<std::uint32_t>(exponent + 127) << 23)
            | (significand & 0x007FFFFFU);
        return std::bit_cast<float>(bits);
    }
}

namespace MphRead::Entities
{
    IconBounds::IconBounds()
        : MinX(0), MinY(0), MaxX(0), MaxY(0)
    {
    }

    IconBounds::IconBounds(std::int32_t minX, std::int32_t minY, std::int32_t maxX, std::int32_t maxY)
        : MinX(minX), MinY(minY), MaxX(maxX), MaxY(maxY)
    {
    }

    std::int32_t IconBounds::Width() const
    {
        return WrapAdd(WrapSubtract(MaxX, MinX), 1);
    }

    std::int32_t IconBounds::Height() const
    {
        return WrapAdd(WrapSubtract(MaxY, MinY), 1);
    }

    float IconBounds::CentreX() const
    {
        const std::int32_t sum = WrapAdd(WrapAdd(MinX, MaxX), 1);
        return Int32ToSingle(sum) / 2.0F;
    }

    float IconBounds::CentreY() const
    {
        const std::int32_t sum = WrapAdd(WrapAdd(MinY, MaxY), 1);
        return Int32ToSingle(sum) / 2.0F;
    }

    IconBounds PlayerEntity::ModIconBounds(std::span<const std::uint8_t> data,
        std::int32_t frame, std::int32_t width, std::int32_t height)
    {
        const std::int32_t tilesX = width / 8;
        const std::int32_t image = WrapMultiply(WrapMultiply(frame, width), height);
        std::int32_t minX = width;
        std::int32_t minY = height;
        std::int32_t maxX = -1;
        std::int32_t maxY = -1;
        for (std::int32_t y = 0; y < height; y++)
        {
            const std::int32_t ty = y / 8;
            const std::int32_t py = y % 8;
            for (std::int32_t x = 0; x < width; x++)
            {
                std::int32_t index = image;
                index = WrapAdd(index, WrapMultiply(WrapMultiply(ty, tilesX), 64));
                index = WrapAdd(index, WrapMultiply(x / 8, 64));
                index = WrapAdd(index, WrapMultiply(py, 8));
                index = WrapAdd(index, x % 8);
                if (index < 0 || static_cast<std::size_t>(index) >= data.size()
                    || data[static_cast<std::size_t>(index)] == 0)
                {
                    continue;
                }
                if (x < minX)
                {
                    minX = x;
                }
                if (x > maxX)
                {
                    maxX = x;
                }
                if (y < minY)
                {
                    minY = y;
                }
                if (y > maxY)
                {
                    maxY = y;
                }
            }
        }
        if (maxX < minX || maxY < minY)
        {
            return IconBounds(0, 0, WrapSubtract(width, 1), WrapSubtract(height, 1));
        }
        return IconBounds(minX, minY, maxX, maxY);
    }
}
