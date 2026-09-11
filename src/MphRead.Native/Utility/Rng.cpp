#include "Rng.hpp"

#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace MphRead
{
    std::uint32_t Rng::rng1_ = Rng1StartValue;
    std::uint32_t Rng::rng2_ = Rng2StartValue;

    std::uint32_t Rng::Rng1()
    {
        return rng1_;
    }

    std::uint32_t Rng::Rng2()
    {
        return rng2_;
    }

    std::uint32_t Rng::CallRng(std::uint32_t& rng, std::uint32_t value)
    {
        rng *= 0x7FF8A3EDU;
        rng += 0x2AA01D31U;
        return static_cast<std::uint32_t>(
            (static_cast<std::int64_t>(rng >> 16) * static_cast<std::int64_t>(value)) / 0x10000LL);
    }

    std::uint32_t Rng::GetRandomInt1(std::int32_t value)
    {
        return GetRandomInt1(static_cast<std::uint32_t>(value));
    }

    std::uint32_t Rng::GetRandomInt2(std::int32_t value)
    {
        return GetRandomInt2(static_cast<std::uint32_t>(value));
    }

    std::uint32_t Rng::GetRandomInt1(std::uint32_t value)
    {
        rng1_ *= 0x7FF8A3EDU;
        rng1_ += 0x2AA01D31U;
        return static_cast<std::uint32_t>(
            (static_cast<std::int64_t>(rng1_ >> 16) * static_cast<std::int64_t>(value)) / 0x10000LL);
    }

    std::uint32_t Rng::GetRandomInt2(std::uint32_t value)
    {
        rng2_ *= 0x7FF8A3EDU;
        rng2_ += 0x2AA01D31U;
        return static_cast<std::uint32_t>(
            (static_cast<std::int64_t>(rng2_ >> 16) * static_cast<std::int64_t>(value)) / 0x10000LL);
    }

    void Rng::SetRng1(std::uint32_t value)
    {
        rng1_ = value;
    }

    void Rng::SetRng2(std::uint32_t value)
    {
        rng2_ = value;
    }

    void Rng::DoDamageShake(std::int32_t damage)
    {
        const float scaled = static_cast<float>(damage) * 40.96F;
        std::int32_t shake;
        if (scaled < -2147483648.0F || scaled >= 2147483648.0F)
        {
            shake = std::numeric_limits<std::int32_t>::min();
        }
        else
        {
            shake = static_cast<std::int32_t>(scaled);
        }
        if (shake < 204)
        {
            shake = 204;
        }
        DoCameraShake(shake);
    }

    void Rng::DoCameraShake(std::int32_t shake)
    {
        std::cout << "shake " << std::to_string(shake) << '\n';
        std::uint32_t rng = rng2_;
        std::int32_t frames = 0;
        while (shake > 0)
        {
            frames++;
            GetRandomInt2(std::int32_t{1});
            GetRandomInt2(std::int32_t{1});
            GetRandomInt2(std::int32_t{1});
            shake = static_cast<std::int32_t>((3481LL * shake + 2048) >> 12);
            if (shake < 41)
            {
                shake = 0;
            }
        }
        std::int32_t calls = frames * 3;
        std::cout << std::to_string(frames) << " frame" << (frames == 1 ? "" : "s")
                  << ", " << std::to_string(calls) << " calls\n";

        std::ostringstream oldRng;
        oldRng << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << rng;
        std::ostringstream newRng;
        newRng << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << rng2_;
        std::cout << "rng " << oldRng.str() << " --> " << newRng.str() << '\n';
    }
}
