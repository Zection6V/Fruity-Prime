#pragma once

#include "FrameAim.hpp"

#include <optional>
#include <utility>

// The local player's mouse aim turned at the display's rate (Mods/Input/FrameAim).
#define MPHREAD_PLAYER_ENTITY_FRAME_AIM_MEMBERS                                                \
public:                                                                                        \
    /* Once a frame, before its steps: turn the aim by mouse motion not yet applied. */       \
    void ModApplyFrameAim(const ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState& live); \
    /* What the camera should turn by since the last step built it, if anything. */           \
    [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector3> ModFrameFacingShift() const;   \
private:                                                                                       \
    /* The step's own share of its mouse delta: what the frames have not applied. */           \
    [[nodiscard]] std::pair<float, float> ModStepMouseAim(float deltaX, float deltaY);         \
    [[nodiscard]] bool ModFrameAimAllowed() const;                                             \
    ::MphRead::Mods::Input::FrameAimState _modFrameAim{};
