#pragma once

#include "Metadata.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace MphRead::Entities
{
    struct PlayerValues;
}

namespace MphRead::Metadata
{
    extern const std::array<OpenTK::Mathematics::Vector3, 16> SpireAltVectors;
    extern const std::array<float, 4> SlipSpeedFactors;
    extern const std::array<float, 4> TractionFactors;
    extern const std::array<OpenTK::Mathematics::Vector3, 8> MuzzleOffests;
    extern const std::array<std::int32_t, 9> MuzzleEffectIds;
    extern const std::array<std::int32_t, 9> ChargeEffectIds;
    extern const std::array<std::int32_t, 9> ChargeLoopEffectIds;
    extern const std::array<std::array<std::array<std::int32_t, 10>, 13>, 8> GunAnimationIds;
    extern const std::vector<Entities::PlayerValues> PlayerValues;
}
