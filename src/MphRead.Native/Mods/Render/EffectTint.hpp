#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace MphRead { class Scene; }
namespace MphRead::Effects { class EffectElementEntry; class EffectParticle; }
namespace MphRead::Formats::Collision { class EntityCollision; }

namespace MphRead::Mods::Render
{
    // Optional presentation data carried by the effect pool. Its interpretation
    // lives here so the cartridge's effect parser/particle bodies stay thin.
    class EffectTint final
    {
    public:
        EffectTint() = delete;
        [[nodiscard]] static OpenTK::Mathematics::Vector3 Color(
            OpenTK::Mathematics::Vector3 color,
            std::optional<OpenTK::Mathematics::Vector3> tint) noexcept;
        [[nodiscard]] static std::optional<OpenTK::Mathematics::Vector3> ForElement(
            const Effects::EffectElementEntry& element) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector3 ParticleColor(
            const Effects::EffectParticle& particle,
            OpenTK::Mathematics::Vector3 color) noexcept;
        static void Spawn(Scene& scene, std::int32_t effectId,
            OpenTK::Mathematics::Matrix4 transform,
            std::optional<OpenTK::Mathematics::Vector3> tint,
            std::shared_ptr<Formats::Collision::EntityCollision> collision = nullptr);
    };
}
