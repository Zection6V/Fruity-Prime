#include "EffectTint.hpp"

#include "../../Formats/Effects.hpp"
#include "../../Scene.hpp"

#include <algorithm>
#include <utility>

namespace MphRead::Mods::Render
{
    using OpenTK::Mathematics::Vector3;

    Vector3 EffectTint::Color(Vector3 color, std::optional<Vector3> tint) noexcept
    {
        if (!tint)
        {
            return color;
        }
        const float brightness = std::max({color.X, color.Y, color.Z});
        return Vector3(tint->X * brightness, tint->Y * brightness, tint->Z * brightness);
    }

    std::optional<Vector3> EffectTint::ForElement(const Effects::EffectElementEntry& element) noexcept
    {
        if (element.ModTint)
        {
            return element.ModTint;
        }
        return element.EffectEntry ? element.EffectEntry->ModTint : std::nullopt;
    }

    Vector3 EffectTint::ParticleColor(const Effects::EffectParticle& particle, Vector3 color) noexcept
    {
        return Color(color, particle.Owner ? ForElement(*particle.Owner) : std::nullopt);
    }

    void EffectTint::Spawn(Scene& scene, std::int32_t effectId,
        OpenTK::Mathematics::Matrix4 transform, std::optional<Vector3> tint,
        std::shared_ptr<Formats::Collision::EntityCollision> collision)
    {
        if (tint)
        {
            scene.ModSpawnTintedEffect(effectId, transform, *tint, std::move(collision));
        }
        else
        {
            scene.SpawnEffect(effectId, transform, false, std::move(collision));
        }
    }
}
