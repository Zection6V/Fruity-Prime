#pragma once

// The scene's side of the display-rate mouse look (Mods/Input/FrameAim).
#define MPHREAD_SCENE_FRAME_AIM_MEMBERS                                                        \
public:                                                                                        \
    /* Hook: the render loop, once per frame after the input pump and before the steps. */    \
    void ModApplyFrameAim();                                                                   \
    /* A menu, chat or lost focus owns the input: the step suspends it, frames do not aim. */  \
    [[nodiscard]] bool ModPlayerInputBlocked() const;
