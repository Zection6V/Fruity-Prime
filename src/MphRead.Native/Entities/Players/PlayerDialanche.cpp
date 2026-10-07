#include "PlayerEntity.hpp"
#include "HalfturretEntity.hpp"

#include "../../GameState.hpp"
#include "../../Scene.hpp"
#include "../../Mods/Combat/DialancheHitTest.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <cmath>

namespace MphRead::Entities
{
    using NativeRuntime::ManagedAt;
    using NativeRuntime::RequireReference;
    using OpenTK::Mathematics::Vector3;

    bool PlayerEntity::DialancheHitsVolume(const CollisionVolume& volume) const
    {
        const auto frame = RequireReference(_scene).FrameCount();
        if (!DialancheNativeCollision::IsNativeCollisionStep(frame))
        {
            return false;
        }
        // The current tick's visual sample is hidden for every target type.
        const auto pose = _dialancheNativeCollision.PoseForHit(
            DialancheNativeCollision::NativeTick(frame));
        return Mods::Combat::DialancheHitTest::Overlaps(pose, volume);
    }

    std::uint16_t PlayerEntity::DialanchePlayerDamage() const
    {
        if (!IsBot() || !GameState::SinglePlayer())
        {
            return Values().AltAttackDamage;
        }
        const auto encounter = ManagedAt(GameState::EncounterState(), SlotIndex());
        if (encounter == 1 || encounter == 3 || encounter == 4
            || (encounter == 0 && BotLevel() == 0))
        {
            return 2;
        }
        return encounter != 0 || BotLevel() < 2 ? 3 : 5;
    }

    void PlayerEntity::CheckDialanchePlayerHit(PlayerEntity& victim, bool halfturret)
    {
        const CollisionVolume targetVolume = halfturret
            ? CollisionVolume(static_cast<Vector3>(RequireReference(victim.Halfturret()).Position), 0.45F)
            : victim.Volume();
        if (!DialancheHitsVolume(targetVolume))
        {
            return;
        }
        Vector3 direction = Vector3::Zero;
        if (!halfturret)
        {
            const float x = static_cast<Vector3>(victim.Position).X - static_cast<Vector3>(Position).X;
            const float z = static_cast<Vector3>(victim.Position).Z - static_cast<Vector3>(Position).Z;
            const float factor = std::sqrt(x * x + z * z) * 4.0F;
            direction.X = x / factor;
            direction.Z = z / factor;
        }
        auto flags = DamageFlags::NoSfx | DamageFlags::NoDmgInvuln;
        if (halfturret)
        {
            flags |= DamageFlags::Halfturret;
        }
        // Keep damage multipliers, invulnerability, teams and network events
        // in the existing TakeDamage path.
        victim.TakeDamage(DialanchePlayerDamage(), flags, direction, this);
        _soundSource.PlaySfx(SfxId::SPIRE_ALT_ATTACK_HIT);
    }
}
