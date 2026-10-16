#pragma once

#include "FrameAim.hpp"

#include <optional>
#include <utility>

// The local player's mouse aim turned at the display's rate (Mods/Input/FrameAim).
#define MPHREAD_PLAYER_ENTITY_FRAME_AIM_MEMBERS                                                \
public:                                                                                        \
    /* Once a frame, before its steps: turn the aim by mouse motion not yet applied. */       \
    void ModApplyFrameAim(const ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState& live); \
    /* Once a frame, after its steps: the local player's picture -- the eye carried on   */  \
    /* along its step's motion, and the DS-style aim at this frame's mouse if any.      */     \
    [[nodiscard]] bool ModPresentOwnView(                                                      \
        const ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState* live);                \
    /* The DS-style aim at this frame's mouse, after ModRestoreFrameAim. */                   \
    [[nodiscard]] bool ModPresentFrameAim(                                                     \
        const ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState& live);                \
    /* Once the picture's camera is set: the reticle where the aim shown is. */               \
    void ModPresentFrameReticle();                                                             \
    /* A watched replica, as the snapshot moves it: what its step's camera was built from. */ \
    void ModNoteCameraSource();                                                                \
    /* Once a frame, after its steps: show this watched player's camera moved by the       */  \
    /* playout since its step built it, and turned to the facing applied since.           */   \
    [[nodiscard]] bool ModPresentWatchedView(::OpenTK::Mathematics::Vector3 playoutShift);     \
    /* Once the picture is drawn: the step's own aim back (FrameAimPresentation). */          \
    void ModRestoreFrameAim();                                                                 \
    /* The hit shake this picture applies to its finished view, if any. */                   \
    [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector3> ModFrameShake() const;         \
    /* What the camera should turn by since the last step built it, if anything. */           \
    [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector3> ModFrameFacingShift() const;   \
private:                                                                                       \
    /* The step's own share of its mouse delta: what the frames have not applied. */           \
    [[nodiscard]] std::pair<float, float> ModStepMouseAim(float deltaX, float deltaY);         \
    [[nodiscard]] bool ModFrameAimAllowed(bool fixedCrosshair) const;                          \
    /* The gun, the muzzle and their effects, as a step builds them from the facing. */       \
    void ModRebuildAimFromFacing();                                                            \
    /* What a picture changes, kept so that ModRestoreFrameAim can put it back. */            \
    void ModSaveFrameView();                                                                   \
    /* The camera moved by `eye` and its sight line by `turn`, the gun riding with it. */     \
    void ModCarryCamera(::OpenTK::Mathematics::Vector3 eye, ::OpenTK::Mathematics::Vector3 turn); \
    /* The muzzle, charge and double damage effects where the gun is, drawn there at once. */ \
    void ModPlaceGunEffects();                                                                 \
    /* The idle sway's step, shared curve, and draw-only camera presentation. */              \
    void ModUpdateIdleSway();                                                                  \
    [[nodiscard]] bool ModIdleSwayEnabled() const;                                             \
    [[nodiscard]] std::int32_t ModIdleSwayStartTime() const;                                   \
    [[nodiscard]] ::OpenTK::Mathematics::Vector3 ModIdleSwayFacing(float phase) const;          \
    [[nodiscard]] bool ModIdleSwayPresentable(                                                 \
        const ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState* live) const;          \
    void ModPresentIdleSway();                                                                 \
    ::MphRead::Mods::Input::FrameAimState _modFrameAim{};                                      \
    ::MphRead::Mods::Input::FrameAimView _modFrameView{};                                      \
    ::MphRead::Mods::Input::CameraSource _modCameraSource{};
