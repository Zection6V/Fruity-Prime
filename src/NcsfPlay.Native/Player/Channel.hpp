#pragma once

#include "../Channel.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace NCSFPlayer
{
    enum class Interpolation : std::int32_t;
    class Player;
    class SWAVWrapper;

    class Channel : public NCSFCommon::Channel
    {
    public:
        static constexpr std::int32_t SincWidth = 8;

        Channel();
        ~Channel() override = default;

        [[nodiscard]] float Interpolate();
        [[nodiscard]] float GenerateSample() override;
        void IncrementSample() override;

    private:
        static constexpr std::int32_t Alpha = 3;
        static constexpr std::int32_t SincResolution = 8192;
        static constexpr std::int32_t SincSamples = SincResolution * SincWidth;
        static constexpr std::int32_t LanczosSamples = SincResolution * Alpha;

        static std::array<float, static_cast<std::size_t>(SincSamples) + 1U> SincLut;
        static std::array<float, static_cast<std::size_t>(SincSamples) + 1U> WindowLut;
        static std::array<float, static_cast<std::size_t>(SincSamples) + 1U> FlatTopWindowSincLut;
        static std::array<float, static_cast<std::size_t>(LanczosSamples) + 1U> LanczosLut;

        std::shared_ptr<SWAVWrapper> swavWrapper;

        [[nodiscard]] static float Sinc(float x);

        [[nodiscard]] float LanczosInterpolate(double ratio);
        [[nodiscard]] float SimpleSincInterpolate(double ratio);
        [[nodiscard]] float OldSincInterpolate(double ratio);
        [[nodiscard]] float SixPointLagrangeInterpolate(double ratio);

        static const std::array<float, 4> fourPtLagrange_c0multipliers;
        static const std::array<float, 4> fourPtLagrange_c1multipliers;
        static const std::array<float, 4> fourPtLagrange_c2multipliers;
        static const std::array<float, 4> fourPtLagrange_c3multipliers;

        [[nodiscard]] float FourPointInterpolate(double ratio);
        [[nodiscard]] float LinearInterpolate(double ratio);
    };
}
