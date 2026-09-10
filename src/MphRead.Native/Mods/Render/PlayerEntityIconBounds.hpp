#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace MphRead::Entities
{
    struct IconBounds
    {
        const std::int32_t MinX;
        const std::int32_t MinY;
        const std::int32_t MaxX;
        const std::int32_t MaxY;

        IconBounds();
        IconBounds(std::int32_t minX, std::int32_t minY, std::int32_t maxX, std::int32_t maxY);

        [[nodiscard]] std::int32_t Width() const;
        [[nodiscard]] std::int32_t Height() const;
        [[nodiscard]] float CentreX() const;
        [[nodiscard]] float CentreY() const;
    };

    namespace Detail
    {
        template <typename T>
        concept ByteReadOnlyList = requires(const T& data, std::size_t index)
        {
            { std::ranges::size(data) };
            requires std::integral<std::remove_cvref_t<decltype(std::ranges::size(data))>>;
            requires std::same_as<std::remove_cvref_t<decltype(data[index])>, std::uint8_t>;
        };
    }

    class PlayerEntity
    {
    public:
        static IconBounds ModIconBounds(std::span<const std::uint8_t> data,
            std::int32_t frame, std::int32_t width, std::int32_t height);

        template <Detail::ByteReadOnlyList T>
        static IconBounds ModIconBounds(const T& data,
            std::int32_t frame, std::int32_t width, std::int32_t height)
        {
            return ModIconBoundsCore(std::addressof(data), &CountAdapter<T>, &ReadAdapter<T>,
                frame, width, height);
        }

        template <Detail::ByteReadOnlyList T>
        static IconBounds ModIconBounds(const T* data,
            std::int32_t frame, std::int32_t width, std::int32_t height)
        {
            return ModIconBoundsCore(data, &CountAdapter<T>, &ReadAdapter<T>, frame, width, height);
        }

        static IconBounds ModIconBounds(std::nullptr_t,
            std::int32_t frame, std::int32_t width, std::int32_t height);

    private:
        using CountCallback = std::int32_t (*)(const void* data);
        using ReadCallback = std::uint8_t (*)(const void* data, std::size_t index);

        template <Detail::ByteReadOnlyList T>
        static std::int32_t CountAdapter(const void* data)
        {
            const auto count = std::ranges::size(*static_cast<const T*>(data));
            using CountType = std::remove_cvref_t<decltype(count)>;
            if constexpr (std::signed_integral<CountType>)
            {
                const std::intmax_t wide = static_cast<std::intmax_t>(count);
                if (wide < static_cast<std::intmax_t>(INT32_MIN)
                    || wide > static_cast<std::intmax_t>(INT32_MAX))
                {
                    throw std::length_error("IReadOnlyList<byte>.Count is outside System.Int32 range");
                }
                return static_cast<std::int32_t>(wide);
            }
            else
            {
                const std::uintmax_t wide = static_cast<std::uintmax_t>(count);
                if (wide > static_cast<std::uintmax_t>(INT32_MAX))
                {
                    throw std::length_error("IReadOnlyList<byte>.Count is outside System.Int32 range");
                }
                return static_cast<std::int32_t>(wide);
            }
        }

        template <Detail::ByteReadOnlyList T>
        static std::uint8_t ReadAdapter(const void* data, std::size_t index)
        {
            return (*static_cast<const T*>(data))[index];
        }

        static IconBounds ModIconBoundsCore(const void* data, CountCallback count, ReadCallback read,
            std::int32_t frame, std::int32_t width, std::int32_t height);
    };
}
