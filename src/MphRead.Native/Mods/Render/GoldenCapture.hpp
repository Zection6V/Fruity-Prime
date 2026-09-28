#pragma once

#include "../../Formats/Types.hpp"

#include <cstdint>
#include <string>

namespace MphRead::Mods::Render
{
    enum class GoldenFixture : std::int32_t
    {
        TransparentObject,
        Decal,
        Particle,
        Trail
    };

    class GoldenCapture final
    {
    public:
        static std::int32_t Run(
            const std::string& candidate,
            const std::string& directory);

        GoldenCapture() = delete;
        GoldenCapture(const GoldenCapture&) = delete;
        GoldenCapture& operator=(const GoldenCapture&) = delete;
    };
}

#define MPHREAD_SCENE_GOLDEN_CAPTURE_MEMBERS \
public: \
    void ModGoldenInjectFixture( \
        ::MphRead::Mods::Render::GoldenFixture fixture, \
        std::int32_t textureBindingId); \
    void ModGoldenSetPlayerCamera( \
        ::OpenTK::Mathematics::Vector3 position, \
        ::OpenTK::Mathematics::Vector3 target, \
        float fovDegrees); \
    void ModGoldenSetFadeState(float color, float percent) noexcept; \
    void ModGoldenSetElapsedTime(float elapsedTime) noexcept;

#define MPHREAD_PLAYER_GOLDEN_CAPTURE_MEMBERS \
public: \
    void ModGoldenSetHudShift( \
        std::uint8_t disruptionState, \
        float disruptionFactor, \
        std::int32_t whiteoutState, \
        float whiteoutFactor, \
        float whiteoutAmount) noexcept;
