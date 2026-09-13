#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <memory>
#include <regex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace NCSFCommon
{
    class Common final
    {
    private:
        Common() = delete;

    public:
        template <typename T>
        class ListMemory final
        {
        private:
            ListMemory() = delete;

        public:
            using AsMemoryFunction = std::function<std::span<T>(std::vector<T>&)>;

            [[nodiscard]] static const AsMemoryFunction& AsMemory()
            {
                static const AsMemoryFunction asMemory = [](std::vector<T>& list)
                {
                    return std::span<T>(list.data(), list.size());
                };
                return asMemory;
            }
        };

        template <typename T>
        [[nodiscard]] static std::span<T> AsMemory(std::vector<T>& list)
        {
            return ListMemory<T>::AsMemory()(list);
        }

        template <typename T>
        [[nodiscard]] static std::uint8_t ToByte(T value)
        {
            static_assert(std::is_enum_v<T>, "T must be an enum type.");
            if constexpr (sizeof(T) == sizeof(std::uint8_t))
            {
                return std::bit_cast<std::uint8_t>(value);
            }
            else
            {
                throw std::runtime_error("Specified method is not supported.");
            }
        }

        template <typename T>
        [[nodiscard]] static T ToEnum(std::uint8_t value)
        {
            static_assert(std::is_enum_v<T>, "T must be an enum type.");
            if constexpr (sizeof(T) == sizeof(std::uint8_t))
            {
                return std::bit_cast<T>(value);
            }
            else
            {
                throw std::runtime_error("Specified method is not supported.");
            }
        }

        static const std::array<std::uint8_t, 4> DataBytes;

        [[nodiscard]] static std::u16string ReadNullTerminatedString(std::span<const std::uint8_t> span);
        static void WriteNullTerminatedString(std::span<std::uint8_t> span, std::u16string_view str);

        enum class SDATRecordType : std::uint8_t
        {
            Sequence,
            SequenceArchive,
            Bank,
            WaveArchive,
            Player,
            Group,
            Player2,
            Stream
        };

        [[nodiscard]] static bool VerifyHeader(
            std::span<const std::uint8_t> actual, std::span<const std::uint8_t> expected);

        [[nodiscard]] static std::wregex WildcardStringToRegex(std::u16string_view wildcard);

        enum class KeepType : std::uint8_t
        {
            Exclude,
            Include,
            Neither
        };

        class KeepInfo
        {
        public:
            const std::u16string Filename;
            const KeepType Keep;

            KeepInfo(std::u16string filename, KeepType keep);

            [[nodiscard]] bool operator==(const KeepInfo& other) const = default;
        };

        [[nodiscard]] static KeepType IncludeFilename(
            std::u16string_view filename,
            std::u16string_view sdatNumber,
            const std::vector<std::shared_ptr<KeepInfo>>& includesAndExcludes);

        [[nodiscard]] static std::u16string SecondsToString(float seconds);
        [[nodiscard]] static std::int32_t StringToMS(std::u16string_view time);
        [[nodiscard]] static std::int32_t VLVLength(std::int32_t value) noexcept;

    };
}
