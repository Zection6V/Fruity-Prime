#include "Random.hpp"

#include "Exceptions.hpp"

#include <random>

namespace MphRead::NativeRuntime
{
    namespace
    {
        // Random.Shared hands every thread its own generator, which is what
        // makes it lock-free; the seed is not reproducible there either.
        std::mt19937_64& SharedEngine()
        {
            thread_local std::mt19937_64 engine{std::random_device{}()};
            return engine;
        }
    }

    std::int32_t RandomSharedNext(std::int32_t maxValue)
    {
        if (maxValue < 0)
        {
            throw System::ArgumentOutOfRangeException("maxValue");
        }
        if (maxValue <= 1)
        {
            return 0;
        }
        // Next(maxValue) is uniform over [0, maxValue).
        std::uniform_int_distribution<std::int32_t> distribution(0, maxValue - 1);
        return distribution(SharedEngine());
    }
}
