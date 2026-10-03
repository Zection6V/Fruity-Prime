#pragma once

#include "../../Formats/Types.hpp"
#include "GoldenCaptureValidation.hpp"

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
    void ModGoldenResetFinalStageState() noexcept; \
    void ModGoldenSetHudPassEnabled(bool enabled) noexcept; \
    [[nodiscard]] bool ModGoldenFinalStageReady() const noexcept; \
    [[nodiscard]] bool ModGoldenFadeStateMatches(float color, float percent) const noexcept; \
    void ModGoldenSetFadeState(float color, float percent) noexcept; \
    void ModGoldenArmFadeObservation() noexcept; \
    void ModGoldenObserveFadeUpdate(float percent) noexcept; \
    void ModGoldenObserveFadeDraw( \
        std::int32_t fadeType, \
        float color, \
        float percent) noexcept; \
    [[nodiscard]] ::MphRead::Mods::Render::GoldenCaptureValidation::FadeObservation \
        ModGoldenFadeObservation() const noexcept; \
    void ModGoldenSetElapsedTime(float elapsedTime) noexcept; \
private: \
    bool _modGoldenFadeObservationArmed = false; \
    ::MphRead::Mods::Render::GoldenCaptureValidation::FadeObservation \
        _modGoldenFadeObservation{};

#define MPHREAD_PLAYER_GOLDEN_CAPTURE_MEMBERS \
public: \
    void ModGoldenSetHudShift( \
        std::uint8_t disruptionState, \
        float disruptionFactor, \
        std::int32_t whiteoutState, \
        float whiteoutFactor, \
        float whiteoutAmount) noexcept; \
    [[nodiscard]] bool ModGoldenHudShiftStateMatches( \
        std::uint8_t disruptionState, \
        float disruptionFactor, \
        std::int32_t whiteoutState, \
        float whiteoutFactor, \
        float whiteoutAmount) const noexcept;
