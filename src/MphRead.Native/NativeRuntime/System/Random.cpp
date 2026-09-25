#include "Random.hpp"

#include "Exceptions.hpp"

#include <bit>
#include <cerrno>
#include <limits>
#include <new>
#include <random>
#include <stdexcept>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace MphRead::NativeRuntime
{
    void RandomNumberGeneratorFill(void* buffer, std::size_t size)
    {
#if defined(_WIN32)
        const NTSTATUS status = BCryptGenRandom(
            nullptr, static_cast<PUCHAR>(buffer), static_cast<ULONG>(size),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (status != 0)
        {
            if (status == static_cast<NTSTATUS>(0xC0000017L))
            {
                throw std::bad_alloc();
            }
            throw std::runtime_error("BCryptGenRandom failed");
        }
#else
        int descriptor;
        do
        {
            descriptor = ::open("/dev/urandom", O_RDONLY
#ifdef O_CLOEXEC
                | O_CLOEXEC
#endif
            );
        }
        while (descriptor == -1 && errno == EINTR);
        if (descriptor == -1)
        {
            throw std::runtime_error("/dev/urandom unavailable");
        }

        auto* output = static_cast<unsigned char*>(buffer);
        std::size_t offset = 0;
        while (offset < size)
        {
            const ssize_t count = ::read(descriptor, output + offset, size - offset);
            if (count < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                const int savedErrno = errno;
                ::close(descriptor);
                throw std::system_error(savedErrno, std::generic_category(), "read /dev/urandom");
            }
            if (count == 0)
            {
                ::close(descriptor);
                throw std::runtime_error("short read from /dev/urandom");
            }
            offset += static_cast<std::size_t>(count);
        }
        ::close(descriptor);
#endif
    }

    Random::Random()
    {
        if constexpr (sizeof(void*) == 8)
        {
            do
            {
                RandomNumberGeneratorFill(_state64.data(), sizeof(_state64));
            }
            while ((_state64[0] | _state64[1] | _state64[2] | _state64[3]) == 0);
        }
        else
        {
            do
            {
                RandomNumberGeneratorFill(_state32.data(), sizeof(_state32));
            }
            while ((_state32[0] | _state32[1] | _state32[2] | _state32[3]) == 0);
        }
    }

    Random::Random(std::int32_t seed) noexcept
        : _seeded(true)
    {
        constexpr std::int32_t Max = std::numeric_limits<std::int32_t>::max();
        const std::int32_t subtraction = seed == std::numeric_limits<std::int32_t>::min()
            ? Max
            : (seed < 0 ? -seed : seed);
        std::int32_t mj = 161803398 - subtraction;
        _seedArray[55] = mj;
        std::int32_t mk = 1;

        std::int32_t ii = 0;
        for (std::int32_t i = 1; i < 55; i++)
        {
            if ((ii += 21) >= 55)
            {
                ii -= 55;
            }
            _seedArray[static_cast<std::size_t>(ii)] = mk;
            mk = mj - mk;
            if (mk < 0)
            {
                mk += Max;
            }
            mj = _seedArray[static_cast<std::size_t>(ii)];
        }

        for (std::int32_t k = 1; k < 5; k++)
        {
            for (std::int32_t i = 1; i < 56; i++)
            {
                std::int32_t n = i + 30;
                if (n >= 55)
                {
                    n -= 55;
                }
                _seedArray[static_cast<std::size_t>(i)] -= _seedArray[static_cast<std::size_t>(1 + n)];
                if (_seedArray[static_cast<std::size_t>(i)] < 0)
                {
                    _seedArray[static_cast<std::size_t>(i)] += Max;
                }
            }
        }

        _inext = 0;
        _inextp = 21;
    }

    double Random::NextDouble() noexcept
    {
        if (_seeded)
        {
            return InternalSample() * (1.0 / static_cast<double>(std::numeric_limits<std::int32_t>::max()));
        }
        std::uint64_t value;
        if constexpr (sizeof(void*) == 8)
        {
            value = NextUInt64();
        }
        else
        {
            const std::uint32_t high = NextUInt32();
            const std::uint32_t low = NextUInt32();
            value = (static_cast<std::uint64_t>(high) << 32) | low;
        }
        return static_cast<double>(value >> 11) * (1.0 / static_cast<double>(std::uint64_t{1} << 53));
    }

    std::int32_t Random::Next(std::int32_t maxValue)
    {
        if (maxValue < 0)
        {
            throw System::ArgumentOutOfRangeException("maxValue");
        }
        if (_seeded)
        {
            return static_cast<std::int32_t>(
                InternalSample() * (1.0 / static_cast<double>(std::numeric_limits<std::int32_t>::max()))
                * maxValue);
        }
        if (maxValue <= 1)
        {
            return 0;
        }
        // Lemire's multiply-shift with rejection, as the xoshiro impls do.
        const auto next = [this]() noexcept -> std::uint32_t
        {
            if constexpr (sizeof(void*) == 8)
            {
                return static_cast<std::uint32_t>(NextUInt64() >> 32);
            }
            else
            {
                return NextUInt32();
            }
        };
        const std::uint32_t max = static_cast<std::uint32_t>(maxValue);
        std::uint64_t product = static_cast<std::uint64_t>(max) * next();
        std::uint32_t low = static_cast<std::uint32_t>(product);
        if (low < max)
        {
            const std::uint32_t remainder = (0U - max) % max;
            while (low < remainder)
            {
                product = static_cast<std::uint64_t>(max) * next();
                low = static_cast<std::uint32_t>(product);
            }
        }
        return static_cast<std::int32_t>(product >> 32);
    }

    std::int32_t Random::InternalSample() noexcept
    {
        std::int32_t locINext = _inext;
        if (++locINext >= 56)
        {
            locINext = 1;
        }
        std::int32_t locINextp = _inextp;
        if (++locINextp >= 56)
        {
            locINextp = 1;
        }

        std::int32_t retVal = _seedArray[static_cast<std::size_t>(locINext)]
            - _seedArray[static_cast<std::size_t>(locINextp)];
        if (retVal == std::numeric_limits<std::int32_t>::max())
        {
            retVal--;
        }
        if (retVal < 0)
        {
            retVal += std::numeric_limits<std::int32_t>::max();
        }

        _seedArray[static_cast<std::size_t>(locINext)] = retVal;
        _inext = locINext;
        _inextp = locINextp;
        return retVal;
    }

    // Xoshiro256** (64-bit processes) and Xoshiro128** (32-bit), as .NET.
    std::uint64_t Random::NextUInt64() noexcept
    {
        std::uint64_t s0 = _state64[0];
        std::uint64_t s1 = _state64[1];
        std::uint64_t s2 = _state64[2];
        std::uint64_t s3 = _state64[3];

        const std::uint64_t result = std::rotl(s1 * 5U, 7) * 9U;
        const std::uint64_t t = s1 << 17;
        s2 ^= s0;
        s3 ^= s1;
        s1 ^= s2;
        s0 ^= s3;
        s2 ^= t;
        s3 = std::rotl(s3, 45);

        _state64 = {s0, s1, s2, s3};
        return result;
    }

    std::uint32_t Random::NextUInt32() noexcept
    {
        std::uint32_t s0 = _state32[0];
        std::uint32_t s1 = _state32[1];
        std::uint32_t s2 = _state32[2];
        std::uint32_t s3 = _state32[3];

        const std::uint32_t result = std::rotl(s1 * 5U, 7) * 9U;
        const std::uint32_t t = s1 << 9;
        s2 ^= s0;
        s3 ^= s1;
        s1 ^= s2;
        s0 ^= s3;
        s2 ^= t;
        s3 = std::rotl(s3, 11);

        _state32 = {s0, s1, s2, s3};
        return result;
    }

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
        std::uniform_int_distribution<std::int32_t> distribution(0, maxValue - 1);
        return distribution(SharedEngine());
    }
}
