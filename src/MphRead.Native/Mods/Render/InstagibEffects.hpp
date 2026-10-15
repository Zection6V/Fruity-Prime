#pragma once

#include "../../Formats/Enums.hpp"
#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace MphRead { class Scene; class ModelInstance; }
namespace MphRead::Entities { class EntityBase; }
namespace MphRead::Effects { class EffectEntry; }

namespace MphRead::Mods::Render
{
    // Instagib presentation policy; callers only supply their existing state.
    class InstagibEffects final
    {
    public:
        InstagibEffects() = delete;
        [[nodiscard]] static std::optional<OpenTK::Mathematics::Vector3> Tint(
            BeamType beam, const Entities::EntityBase* owner);
        [[nodiscard]] static bool TryAddRail(Scene& scene, BeamType beam,
            const Entities::EntityBase* owner, OpenTK::Mathematics::Vector3 start,
            OpenTK::Mathematics::Vector3 end);
        static void TintMuzzle(const std::shared_ptr<Effects::EffectEntry>& effect,
            BeamType beam, std::int32_t slot);
        static void TintGun(ModelInstance& gun, BeamType beam, std::int32_t slot);

    private:
        [[nodiscard]] static std::optional<OpenTK::Mathematics::Vector3> PlayerTint(
            BeamType beam, std::int32_t slot);
    };
}
