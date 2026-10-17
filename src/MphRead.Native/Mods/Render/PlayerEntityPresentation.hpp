#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace MphRead::Mods::Render
{
    // A player's drawn pose at the last two steps that ran the world.
    struct PlayerPoseHistory final
    {
        std::uint64_t Step = 0;
        // The step before this one was recorded too.
        bool HasPrevious = false;
        // And in the same form: a morph is not the body's motion (the camera
        // still moves through one).
        bool SameForm = false;
        bool AltForm = false;
        OpenTK::Mathematics::Matrix4 Previous{}, Current{};
        std::array<OpenTK::Mathematics::Matrix4, 5> PreviousSegments{}, CurrentSegments{};
        // Kanden's segments as the game's own update makes them one step on.
        std::array<OpenTK::Mathematics::Matrix4, 5> NextSegments{};
        bool HasNextSegments = false;
        // The camera the step built: where it was and what it looked at, the
        // target without its hit shake (CurrentShake), which is drawn apart.
        OpenTK::Mathematics::Vector3 PreviousEye{}, CurrentEye{}, PreviousTarget{}, CurrentTarget{};
        // The eye against the body over the last steps, oldest first, and how
        // many of them in a row were recorded (MotionCarry::Trail).
        std::array<OpenTK::Mathematics::Vector3, 4> TrailEye{};
        std::int32_t TrailSteps = 0;
        // The camera type the trail was recorded under.
        std::int32_t TrailCamera = -1;
        OpenTK::Mathematics::Vector3 CurrentShake{};
        // The shake's last two samples, one every StepsPerDsFrame steps, and
        // the step of the newer: a picture eases from one to the other.
        OpenTK::Mathematics::Vector3 ShakeFrom{}, ShakeTo{};
        std::uint64_t ShakeStep = 0;
    };
}

// A player drawn at the picture's instant (Mods/Render/ScenePresentation).
#define MPHREAD_PLAYER_ENTITY_PRESENTATION_MEMBERS                                             \
public:                                                                                        \
    /* Hook: ScenePresentation::EndStep, a step that ran the world: this step's pose. */      \
    void ModNotePresentationStep(std::uint64_t step);                                          \
    /* Where this player's drawn items are carried to by the picture's instant, if moving. */ \
    /* A trailing camera's shift against its body at this picture's instant, if it trails. */ \
    [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector3> ModTrailShift() const;         \
    /* Hook: DrawMorphBallTrail. Segment `index` as this picture draws it. */                  \
    [[nodiscard]] ::OpenTK::Mathematics::Matrix4 ModTrailSegment(std::int32_t index,           \
        ::OpenTK::Mathematics::Matrix4 segment) const;                                         \
    [[nodiscard]] std::optional<::OpenTK::Mathematics::Matrix4> ModPresentationCarry() const;  \
    /* Where this picture has this player's muzzle and the way it points, if moving. */      \
    [[nodiscard]] std::optional<std::pair<::OpenTK::Mathematics::Vector3,                      \
        ::OpenTK::Mathematics::Vector3>> ModPresentedMuzzle() const;                           \
    /* Where a point on this player's body is in this picture. */                             \
    [[nodiscard]] ::OpenTK::Mathematics::Vector3 ModPresentedPoint(::OpenTK::Mathematics::Vector3 point) const; \
    /* Hook: PlayerEntity::Draw. Your gun's effects are drawn while your gun is. */          \
    void ModGunEffectsDrawn(bool gunDrawn);                                                    \
    /* Hook: PlayerEntity::Draw. Body effects on the body this picture draws. */            \
    void ModPlaceBodyEffectsDrawn();                                                           \
    /* Hook: DrawKandenAlt, segment `index` as drawn, each on along its own last step. */     \
    [[nodiscard]] ::OpenTK::Mathematics::Matrix4 ModKandenSegmentDrawn(std::int32_t index) const; \
    /* Hook: DrawKandenAlt, after drawing: the step's own segment matrices back. */           \
    void ModRestoreKandenSegments();                                                           \
private:                                                                                       \
    [[nodiscard]] bool ModPresentationMoving() const;                                          \
    [[nodiscard]] bool ModCarriesKandenSegments() const;                                       \
    void ModLookAheadKandenSegments();                                                         \
    /* What UpdateStinglarvaSegments adds to the frame it reads: 1 for a look-ahead. */      \
    std::uint64_t _modSegmentLookAhead = 0;                                                    \
    ::MphRead::Mods::Render::PlayerPoseHistory _modPose{};
