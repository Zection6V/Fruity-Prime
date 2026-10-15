#pragma once

#include <cstdint>

namespace MphRead
{
    class NodeAnimationGroup;
}

namespace MphRead::Mods::Render
{
    class AnimationClock;

    // What the newest animation tick of one model instance looked like, kept
    // on its AnimationInfo (ModTick). Written only by AnimationBlend::NoteTick
    // and read only by a draw; the simulation never reads it.
    struct AnimationTick final
    {
        const AnimationClock* Clock = nullptr;
        double Time = 0;
        // Clock time between the last two ticks; 0 while unknown.
        double Interval = 0;
        // The node slot as the tick left it, and the frame the next tick will
        // show. A draw blends only while the slot still matches.
        const NodeAnimationGroup* Group = nullptr;
        std::int32_t Frame = 0;
        std::int32_t Next = 0;
        std::uint16_t Flags = 0;
    };
}
