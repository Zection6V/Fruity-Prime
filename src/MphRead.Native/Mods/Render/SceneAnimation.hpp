#pragma once

#include "AnimationBlend.hpp"
#include "AnimationClock.hpp"

#include <cstdint>
#include <optional>

namespace MphRead
{
    class ModelInstance;
}

namespace MphRead::Mods::Render
{
    // Where a scene meets the animation time line: each simulation step is a
    // step of the simulation's AnimationClock, and each draw sits FrameTiming's
    // step phase past the newest one and may blend toward the next 30 Hz frame.
    class SceneAnimation final
    {
    public:
        SceneAnimation() = delete;

        // Hook: Scene::OnSimulationFrame. A process that never draws (a
        // dedicated server) keeps no time line: nothing would read it.
        class Step final
        {
        public:
            Step() noexcept;
            Step(const Step&) = delete;
            Step& operator=(const Step&) = delete;

        private:
            std::optional<AnimationClock::TickScope> _ticks{};
        };

        // Hook: an animation the game advances from a draw function, behind the
        // usual even-frame check. That ran once per step when the draw did; at
        // the display's rate it ran on every draw of an even step, so these
        // played faster the faster the screen. At most once per step, and on
        // the simulation clock so it is blended like every other.
        static void TickFromDraw(ModelInstance& instance);

        // Hook: a time-based animation a draw advances by one simulation frame
        // per call (a HUD object's ProcessAnimation). The simulation steps
        // since `key` last advanced, so it keeps the game's pace at any
        // display rate: 0 on a draw that no step preceded.
        [[nodiscard]] static std::int32_t StepsForDraw(const void* key);

        // Hook: Scene::OnDrawFrame. One picture of blended animation.
        class Draw final
        {
        public:
            Draw() noexcept;
            Draw(const Draw&) = delete;
            Draw& operator=(const Draw&) = delete;

        private:
            AnimationBlend::DrawScope _blend;
        };
    };
}
