#pragma once

#include "../Read.hpp"

#include <cassert>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace MphRead::Utility
{
    class Parser final
    {
    public:
        template <typename T>
            requires (std::is_object_v<T> && !std::is_array_v<T> && !std::is_pointer_v<T>)
        [[nodiscard]] static std::shared_ptr<const std::vector<T>> ParseBytes(
            std::int32_t size, std::int32_t count, const std::vector<std::uint8_t>& array)
        {
            static_assert(sizeof(T) <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()));
            assert(size == static_cast<std::int32_t>(sizeof(T)));

            auto results = std::make_shared<std::vector<T>>();
            const std::span<const std::uint8_t> bytes(array.data(), array.size());
            assert(bytes.size() <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())
                && static_cast<std::int32_t>(bytes.size()) == ManagedMultiply(count, size));

            for (std::int32_t i = 0; i < count; i = ManagedAdd(i, 1))
            {
                const std::int32_t start = ManagedMultiply(i, size);
                const std::int32_t end = ManagedAdd(start, size);
                results->push_back(Read::ReadStruct<T>(Slice(bytes, start, end)));
            }
            return results;
        }

        static void ParseFloat(std::uint64_t value);
        static void MainLoop();

        Parser() = delete;
        Parser(const Parser&) = delete;
        Parser& operator=(const Parser&) = delete;

    private:
        enum class ThingType : std::int32_t
        {
            General,
            Boolean,
            Enum
        };

        struct Thing
        {
            const std::string Name;
            const ThingType Type;
            const std::type_info* const EnumType;
            const std::int32_t BitFrom;
            const std::int32_t BitTo;

            Thing(std::int32_t bit, std::string name, ThingType type);
            Thing(std::int32_t bit, std::string name, const std::type_info& enumType);
            Thing(std::int32_t bitFrom, std::int32_t bitTo, std::string name, ThingType type);
            Thing(std::int32_t bitFrom, std::int32_t bitTo, std::string name, const std::type_info& enumType);

            [[nodiscard]] std::string Get(std::int32_t value) const;
        };

        static const std::unordered_map<std::string, std::vector<Thing>> _things;

        [[nodiscard]] static constexpr std::int32_t ManagedAdd(std::int32_t left, std::int32_t right) noexcept
        {
            return std::bit_cast<std::int32_t>(
                std::bit_cast<std::uint32_t>(left) + std::bit_cast<std::uint32_t>(right));
        }

        [[nodiscard]] static constexpr std::int32_t ManagedSubtract(std::int32_t left, std::int32_t right) noexcept
        {
            return std::bit_cast<std::int32_t>(
                std::bit_cast<std::uint32_t>(left) - std::bit_cast<std::uint32_t>(right));
        }

        [[nodiscard]] static constexpr std::int32_t ManagedMultiply(std::int32_t left, std::int32_t right) noexcept
        {
            return std::bit_cast<std::int32_t>(
                std::bit_cast<std::uint32_t>(left) * std::bit_cast<std::uint32_t>(right));
        }

        [[nodiscard]] static constexpr std::int32_t ManagedLeftShift(std::int32_t value, std::int32_t count) noexcept
        {
            const std::uint32_t shift = static_cast<std::uint32_t>(count) & 31U;
            return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value) << shift);
        }

        [[nodiscard]] static constexpr std::int32_t ManagedArithmeticRightShift(
            std::int32_t value, std::int32_t count) noexcept
        {
            const std::uint32_t shift = static_cast<std::uint32_t>(count) & 31U;
            if (shift == 0)
            {
                return value;
            }
            const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
            std::uint32_t shifted = bits >> shift;
            if ((bits & 0x80000000U) != 0)
            {
                shifted |= 0xFFFFFFFFU << (32U - shift);
            }
            return std::bit_cast<std::int32_t>(shifted);
        }

        [[nodiscard]] static constexpr std::int32_t ManagedNot(std::int32_t value) noexcept
        {
            return std::bit_cast<std::int32_t>(~std::bit_cast<std::uint32_t>(value));
        }

        [[nodiscard]] static constexpr std::int32_t ManagedAnd(std::int32_t left, std::int32_t right) noexcept
        {
            return std::bit_cast<std::int32_t>(
                std::bit_cast<std::uint32_t>(left) & std::bit_cast<std::uint32_t>(right));
        }

        [[nodiscard]] static std::span<const std::uint8_t> Slice(
            std::span<const std::uint8_t> bytes, std::int32_t start, std::int32_t end)
        {
            if (start < 0 || end < 0 || end < start || static_cast<std::size_t>(end) > bytes.size())
            {
                ReadDetail::ThrowRange();
            }
            return bytes.subspan(
                static_cast<std::size_t>(start), static_cast<std::size_t>(end - start));
        }
    };
}
