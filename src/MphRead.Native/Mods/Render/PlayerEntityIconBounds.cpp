#include "PlayerEntityIconBounds.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace MphRead::Entities {
namespace {

[[nodiscard]] constexpr std::uint32_t ToWrappedUnsigned(std::int32_t value) noexcept
{
    return value >= 0
        ? static_cast<std::uint32_t>(value)
        : static_cast<std::uint32_t>(
            static_cast<std::int64_t>(value) + (std::int64_t{1} << 32)
        );
}

[[nodiscard]] constexpr std::int32_t FromWrappedUnsigned(std::uint32_t value) noexcept
{
    if (value <= static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)())) {
        return static_cast<std::int32_t>(value);
    }
    return static_cast<std::int32_t>(
        static_cast<std::int64_t>(value) - (std::int64_t{1} << 32)
    );
}

[[nodiscard]] constexpr std::int32_t WrapAdd(
    std::int32_t left,
    std::int32_t right
) noexcept
{
    return FromWrappedUnsigned(ToWrappedUnsigned(left) + ToWrappedUnsigned(right));
}

[[nodiscard]] constexpr std::int32_t WrapSubtract(
    std::int32_t left,
    std::int32_t right
) noexcept
{
    return FromWrappedUnsigned(ToWrappedUnsigned(left) - ToWrappedUnsigned(right));
}

[[nodiscard]] constexpr std::int32_t WrapMultiply(
    std::int32_t left,
    std::int32_t right
) noexcept
{
    return FromWrappedUnsigned(ToWrappedUnsigned(left) * ToWrappedUnsigned(right));
}

[[nodiscard]] float Int32ToSingle(std::int32_t value) noexcept
{
    if (value == 0) {
        return 0.0F;
    }

    const bool negative = value < 0;
    const std::uint32_t magnitude = negative
        ? std::uint32_t{0} - ToWrappedUnsigned(value)
        : ToWrappedUnsigned(value);

    std::uint32_t bitLength = 0;
    for (std::uint32_t scan = magnitude; scan != 0; scan >>= 1) {
        ++bitLength;
    }

    constexpr std::uint32_t precision = std::numeric_limits<float>::digits;
    std::uint32_t rounded = magnitude;
    if (bitLength > precision) {
        const std::uint32_t shift = bitLength - precision;
        const std::uint32_t quotient = magnitude >> shift;
        const std::uint32_t remainderMask = (std::uint32_t{1} << shift) - 1U;
        const std::uint32_t remainder = magnitude & remainderMask;
        const std::uint32_t halfway = std::uint32_t{1} << (shift - 1U);

        rounded = quotient;
        if (remainder > halfway || (remainder == halfway && (quotient & 1U) != 0U)) {
            ++rounded;
        }

        rounded <<= shift;
    }

    const float converted = static_cast<float>(rounded);
    return negative ? -converted : converted;
}

[[noreturn]] void ThrowNullReference()
{
    throw std::runtime_error("Object reference not set to an instance of an object.");
}

[[nodiscard]] constexpr std::int32_t CSharpMin(
    std::int32_t left,
    std::int32_t right
) noexcept
{
    return left < right ? left : right;
}

[[nodiscard]] constexpr std::int32_t CSharpMax(
    std::int32_t left,
    std::int32_t right
) noexcept
{
    return left > right ? left : right;
}

static_assert(
    std::numeric_limits<float>::radix == 2
        && std::numeric_limits<float>::digits == 24
        && std::numeric_limits<float>::max_exponent == 128
        && std::numeric_limits<float>::min_exponent == -125,
    "PlayerEntityIconBounds requires IEEE-754 binary32 float semantics."
);

} // namespace

IconBounds::IconBounds() noexcept
    : MinX(0), MinY(0), MaxX(0), MaxY(0)
{
}

IconBounds::IconBounds(
    std::int32_t minX,
    std::int32_t minY,
    std::int32_t maxX,
    std::int32_t maxY
) noexcept
    : MinX(minX), MinY(minY), MaxX(maxX), MaxY(maxY)
{
}

IconBounds& IconBounds::operator=(const IconBounds& other) noexcept
{
    if (this != std::addressof(other)) {
        const std::int32_t minX = other.MinX;
        const std::int32_t minY = other.MinY;
        const std::int32_t maxX = other.MaxX;
        const std::int32_t maxY = other.MaxY;
        std::destroy_at(this);
        std::construct_at(this, minX, minY, maxX, maxY);
    }
    return *this;
}

std::int32_t IconBounds::Width() const noexcept
{
    return WrapAdd(WrapSubtract(MaxX, MinX), 1);
}

std::int32_t IconBounds::Height() const noexcept
{
    return WrapAdd(WrapSubtract(MaxY, MinY), 1);
}

float IconBounds::CentreX() const noexcept
{
    const std::int32_t sum = WrapAdd(WrapAdd(MinX, MaxX), 1);
    return Int32ToSingle(sum) / 2.0F;
}

float IconBounds::CentreY() const noexcept
{
    const std::int32_t sum = WrapAdd(WrapAdd(MinY, MaxY), 1);
    return Int32ToSingle(sum) / 2.0F;
}

IconBounds PlayerEntity::ModIconBounds(
    std::span<const std::uint8_t> data,
    std::int32_t frame,
    std::int32_t width,
    std::int32_t height
)
{
    return ModIconBounds<std::span<const std::uint8_t>>(data, frame, width, height);
}

IconBounds PlayerEntity::ModIconBounds(
    std::nullptr_t,
    std::int32_t frame,
    std::int32_t width,
    std::int32_t height
)
{
    return ModIconBoundsCore(nullptr, nullptr, nullptr, frame, width, height);
}

IconBounds PlayerEntity::ModIconBoundsCore(
    const void* data,
    CountCallback count,
    ReadCallback read,
    std::int32_t frame,
    std::int32_t width,
    std::int32_t height
)
{
    const std::int32_t tilesX = width / 8;
    const std::int32_t image = WrapMultiply(WrapMultiply(frame, width), height);
    std::int32_t minX = width;
    std::int32_t minY = height;
    std::int32_t maxX = -1;
    std::int32_t maxY = -1;

    for (std::int32_t y = 0; y < height; ++y) {
        const std::int32_t ty = y / 8;
        const std::int32_t py = y % 8;
        for (std::int32_t x = 0; x < width; ++x) {
            std::int32_t index = image;
            index = WrapAdd(index, WrapMultiply(WrapMultiply(ty, tilesX), 64));
            index = WrapAdd(index, WrapMultiply(x / 8, 64));
            index = WrapAdd(index, WrapMultiply(py, 8));
            index = WrapAdd(index, x % 8);

            if (index < 0) {
                continue;
            }

            if (data == nullptr) {
                ThrowNullReference();
            }

            const std::int32_t dataCount = count(data);
            if (index >= dataCount || read(data, index) == 0) {
                continue;
            }

            minX = CSharpMin(minX, x);
            minY = CSharpMin(minY, y);
            maxX = CSharpMax(maxX, x);
            maxY = CSharpMax(maxY, y);
        }
    }

    if (maxX < minX || maxY < minY) {
        return IconBounds(
            0,
            0,
            WrapSubtract(width, 1),
            WrapSubtract(height, 1)
        );
    }
    return IconBounds(minX, minY, maxX, maxY);
}

} // namespace MphRead::Entities
