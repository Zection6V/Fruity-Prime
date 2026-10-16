#pragma once

#include <array>
#include <cstdint>

namespace MphRead::Mods::Render
{
    class AnimationClock;

    // The curves a draw may blend. Texture swaps are discrete and are not one.
    enum class AnimationChannel : std::uint8_t
    {
        Node,
        Material,
        Texcoord
    };

    // What the newest animation tick of one model instance looked like, kept
    // on its AnimationInfo (ModTick). Written only by AnimationBlend::NoteTick
    // and read only by a draw; the simulation never reads it.
    struct AnimationTick final
    {
        const AnimationClock* Clock = nullptr;
        double Time = 0;
        // Clock time between the last two ticks; 0 while unknown.
        double Interval = 0;
        // Each slot as the tick left it, and the frame the next tick will show.
        // A channel blends only while its slot and group still match.
        std::array<std::int32_t, 2> Frame{};
        std::array<std::int32_t, 2> Next{};
        std::array<std::uint16_t, 2> Flags{};
        // Per channel, the animation group the tick saw.
        std::array<const void*, 3> Group{};
        // Scoped draw-only override; restored before simulation can run.
        float DrawFrame = -1.0F;
    };
}
