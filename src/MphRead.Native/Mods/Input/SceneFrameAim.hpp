#pragma once

#include "FrameAim.hpp"

#include <memory>

// The scene's side of display-rate camera input (Mods/Input/FrameAim).
#define MPHREAD_SCENE_FRAME_AIM_MEMBERS                                                        \
public:                                                                                        \
    /* Hook: the render loop, once per frame after the input pump and before the steps:  */   \
    /* the free camera moved by the frame's time, the local aim by its mouse motion.       */   \
    void ModApplyFrameInput();                                                                 \
    /* Hook: the render loop, after the steps: this picture's DS-style aim, shown until    */   \
    /* the returned scope ends, which is once the picture is drawn.                        */   \
    [[nodiscard]] ::MphRead::Mods::Input::FrameAimPresentation ModPresentFrameInput();          \
    /* Hook: OnDrawFrame, once the picture's camera is set: the reticle on the aim shown. */  \
    void ModPresentFrameReticle();                                                             \
    /* A menu, chat or lost focus owns the input: the step suspends it, frames do not aim. */  \
    [[nodiscard]] bool ModPlayerInputBlocked() const;                                          \
private:                                                                                       \
    /* The local player whose aim this frame's mouse turns, if anything does. */              \
    [[nodiscard]] std::shared_ptr<::MphRead::Entities::PlayerEntity> ModFrameAimPlayer() const;
