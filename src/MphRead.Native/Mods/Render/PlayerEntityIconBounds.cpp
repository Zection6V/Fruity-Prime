#include "PlayerEntityIconBounds.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

using ::MphRead::NativeRuntime::UInt32ToInt32;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;
using ::MphRead::NativeRuntime::UncheckedSubtract;

namespace
{
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

    [[noreturn]] void ThrowNullReference()
    {
        throw std::runtime_error("Object reference not set to an instance of an object.");
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

    IconBounds& IconBounds::operator=(const IconBounds& other)
    {
        if (this != std::addressof(other))
        {
            std::destroy_at(this);
            std::construct_at(this, other);
        }
        return *this;
    }

    std::int32_t IconBounds::Width() const
    {
        return UncheckedAdd(UncheckedSubtract(MaxX, MinX), 1);
    }

    std::int32_t IconBounds::Height() const
    {
        return UncheckedAdd(UncheckedSubtract(MaxY, MinY), 1);
    }

    float IconBounds::CentreX() const
    {
        const std::int32_t sum = UncheckedAdd(UncheckedAdd(MinX, MaxX), 1);
        return Int32ToSingle(sum) / 2.0F;
    }

    float IconBounds::CentreY() const
    {
        const std::int32_t sum = UncheckedAdd(UncheckedAdd(MinY, MaxY), 1);
        return Int32ToSingle(sum) / 2.0F;
    }

    IconBounds PlayerEntity::ModIconBounds(std::span<const std::uint8_t> data,
        std::int32_t frame, std::int32_t width, std::int32_t height)
    {
        return PlayerEntity::ModIconBounds<std::span<const std::uint8_t>>(data, frame, width, height);
    }

    IconBounds PlayerEntity::ModIconBounds(std::nullptr_t,
        std::int32_t frame, std::int32_t width, std::int32_t height)
    {
        return ModIconBoundsCore(nullptr, nullptr, nullptr, frame, width, height);
    }

    IconBounds PlayerEntity::ModIconBoundsCore(const void* data, CountCallback count, ReadCallback read,
        std::int32_t frame, std::int32_t width, std::int32_t height)
    {
        const std::int32_t tilesX = width / 8;
        const std::int32_t image = UncheckedMultiply(UncheckedMultiply(frame, width), height);
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
                index = UncheckedAdd(index, UncheckedMultiply(UncheckedMultiply(ty, tilesX), 64));
                index = UncheckedAdd(index, UncheckedMultiply(x / 8, 64));
                index = UncheckedAdd(index, UncheckedMultiply(py, 8));
                index = UncheckedAdd(index, x % 8);
                if (index < 0)
                {
                    continue;
                }
                if (data == nullptr)
                {
                    ThrowNullReference();
                }
                const std::int32_t dataCount = count(data);
                if (index >= dataCount || read(data, index) == 0)
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
            return IconBounds(0, 0, UncheckedSubtract(width, 1), UncheckedSubtract(height, 1));
        }
        return IconBounds(minX, minY, maxX, maxY);
    }
}
