#include "HashCode.hpp"

#include "Encoding.hpp"

#include <bit>
#include <random>

namespace MphRead::NativeRuntime::HashCodeDetail
{
    std::uint32_t Seed() noexcept
    {
        static const std::uint32_t seed = []() noexcept
        {
            try
            {
                std::random_device device;
                return std::uniform_int_distribution<std::uint32_t>()(device);
            }
            catch (...)
            {
                // No entropy source: a fixed seed still hashes consistently.
                return 0U;
            }
        }();
        return seed;
    }
}

namespace MphRead::NativeRuntime
{
    std::int32_t StringGetHashCode(std::string_view value) noexcept
    {
        using namespace HashCodeDetail;
        std::uint32_t hash = Seed() + Prime5 + 0x6D2B79F5U;
        std::uint32_t length = 0;
        for (std::size_t offset = 0; offset < value.size();)
        {
            const Utf8Scalar scalar = DecodeUtf8Scalar(value, offset);
            char32_t code = scalar.Value;
            if (code >= 0x10000U)
            {
                code -= 0x10000U;
                hash = QueueRound(hash, 0xD800U + (code >> 10));
                hash = QueueRound(hash, 0xDC00U + (code & 0x3FFU));
                length += 2;
            }
            else
            {
                hash = QueueRound(hash, code);
                ++length;
            }
            offset += scalar.Length;
        }
        hash += length * 2U;
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }

    std::int32_t ReferenceGetHashCode(const void* value) noexcept
    {
        if (value == nullptr)
        {
            return 0;
        }
        const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(value));
        return HashCodeCombine(static_cast<std::int32_t>(address), static_cast<std::int32_t>(address >> 32));
    }
}
