#pragma once

#include <cstdint>

namespace MphRead
{
    class Rng
    {
    public:
        static constexpr std::uint32_t Rng1StartValue = 0x3DE9179BU;
        static constexpr std::uint32_t Rng2StartValue = 0U;

        [[nodiscard]] static std::uint32_t Rng1();
        [[nodiscard]] static std::uint32_t Rng2();

        static std::uint32_t CallRng(std::uint32_t& rng, std::uint32_t value);
        static std::uint32_t GetRandomInt1(std::int32_t value);
        static std::uint32_t GetRandomInt2(std::int32_t value);
        static std::uint32_t GetRandomInt1(std::uint32_t value);
        static std::uint32_t GetRandomInt2(std::uint32_t value);
        static void SetRng1(std::uint32_t value);
        static void SetRng2(std::uint32_t value);
        static void DoDamageShake(std::int32_t damage);
        static void DoCameraShake(std::int32_t shake);

    private:
        static std::uint32_t rng1_;
        static std::uint32_t rng2_;
    };
}
