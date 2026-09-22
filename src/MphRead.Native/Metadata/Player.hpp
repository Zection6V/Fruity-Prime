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
    extern std::array<OpenTK::Mathematics::Vector3, 16> SpireAltVectors;
    extern std::array<float, 4> SlipSpeedFactors;
    extern std::array<float, 4> TractionFactors;
    extern std::array<OpenTK::Mathematics::Vector3, 8> MuzzleOffests;
    extern std::array<std::int32_t, 9> MuzzleEffectIds;
    extern std::array<std::int32_t, 9> ChargeEffectIds;
    extern std::array<std::int32_t, 9> ChargeLoopEffectIds;
    extern std::array<std::array<std::array<std::int32_t, 10>, 13>, 8> GunAnimationIds;
    extern const std::vector<Entities::PlayerValues> PlayerValues;
}
