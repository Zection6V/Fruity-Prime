#pragma once

#include "NativeRuntime/OpenTK/Mathematics.hpp"

#include <memory>

namespace MphRead::Effects
{
    class EffectEntry;
}

namespace MphRead::Mods::Render
{
    // Places an attached effect outside its owner's own process step.
    //
    // EffectEntry::Transform sets the elements' own transform; the one their
    // attached particles are drawn with is copied from it by
    // Scene::ProcessEffects, once a step. An effect placed anywhere else -- a
    // picture between steps, or a remote replica moved to its display pose
    // after the step -- is copied here too, or it is drawn where it was.
    void PlaceEffect(const std::shared_ptr<Effects::EffectEntry>& effect,
        OpenTK::Mathematics::Vector3 facing, OpenTK::Mathematics::Vector3 up,
        OpenTK::Mathematics::Vector3 position);

    // How far body effects (boost trail, furl, Death Alt, burn) were moved
    // from their step's pose onto the body as a picture drew it.
    class BodyEffectDraws final
    {
    public:
        BodyEffectDraws() = delete;
        static void Note(float offset) noexcept
        {
            ++_samples;
            if (offset > _worst) _worst = offset;
        }
        [[nodiscard]] static float WorstOffset() noexcept { return _worst; }
        [[nodiscard]] static long long Samples() noexcept { return _samples; }
    private:
        inline static float _worst = 0.0F;
        inline static long long _samples = 0;
    };
}
