#include "InstagibEffects.hpp"

#include "EffectTint.hpp"
#include "InstagibRails.hpp"
#include "../Combat/InstagibRules.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Formats/Effects.hpp"
#include "../../Formats/Model.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

namespace MphRead::Mods::Render
{
    using OpenTK::Mathematics::Vector3;
    using NativeRuntime::RequireReference;

    std::optional<Vector3> InstagibEffects::PlayerTint(BeamType beam, std::int32_t slot)
    {
        if (!Combat::InstagibRules::Active() || beam != BeamType::Imperialist)
        {
            return std::nullopt;
        }
        return InstagibRails::ColorForSlot(slot);
    }

    std::optional<Vector3> InstagibEffects::Tint(BeamType beam, const Entities::EntityBase* owner)
    {
        if (owner == nullptr || owner->Type != EntityType::Player)
        {
            return std::nullopt;
        }
        return PlayerTint(beam, static_cast<const Entities::PlayerEntity*>(owner)->SlotIndex());
    }

    bool InstagibEffects::TryAddRail(Scene& scene, BeamType beam,
        const Entities::EntityBase* owner, Vector3 start, Vector3 end)
    {
        if (!Tint(beam, owner))
        {
            return false;
        }
        InstagibRails::Add(scene, start, end,
            static_cast<const Entities::PlayerEntity*>(owner)->SlotIndex());
        return true;
    }

    void InstagibEffects::TintMuzzle(const std::shared_ptr<Effects::EffectEntry>& effect,
        BeamType beam, std::int32_t slot)
    {
        if (effect)
        {
            effect->ModTint = PlayerTint(beam, slot);
        }
    }

    void InstagibEffects::TintGun(ModelInstance& gun, BeamType beam, std::int32_t slot)
    {
        const auto tint = PlayerTint(beam, slot);
        if (!tint || !gun.AnimInfo || !gun.AnimInfo->Material || !gun.AnimInfo->Material->Group)
        {
            return;
        }
        const auto& animated = RequireReference(gun.AnimInfo->Material->Group->Animations);
        for (const auto& value : RequireReference(RequireReference(gun.Model()).Materials))
        {
            Material& material = RequireReference(value);
            if (animated.find(material.Name) != animated.end())
            {
                material.CurrentDiffuse = EffectTint::Color(material.CurrentDiffuse, tint);
                material.CurrentAmbient = EffectTint::Color(material.CurrentAmbient, tint);
            }
        }
    }
}
