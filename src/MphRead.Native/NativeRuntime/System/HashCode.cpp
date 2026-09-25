#include "HashCode.hpp"

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
