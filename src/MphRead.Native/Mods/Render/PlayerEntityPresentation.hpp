#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <array>
#include <cstdint>
#include <optional>

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
        // The camera the step built: where it was and what it looked at.
        OpenTK::Mathematics::Vector3 PreviousEye{}, CurrentEye{}, PreviousTarget{}, CurrentTarget{};
    };
}

// A player drawn at the picture's instant (Mods/Render/ScenePresentation).
#define MPHREAD_PLAYER_ENTITY_PRESENTATION_MEMBERS                                             \
public:                                                                                        \
    /* Hook: ScenePresentation::EndStep, a step that ran the world: this step's pose. */      \
    void ModNotePresentationStep(std::uint64_t step);                                          \
    /* Where this player's drawn items are carried to by the picture's instant, if moving. */ \
    [[nodiscard]] std::optional<::OpenTK::Mathematics::Matrix4> ModPresentationCarry() const;  \
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
