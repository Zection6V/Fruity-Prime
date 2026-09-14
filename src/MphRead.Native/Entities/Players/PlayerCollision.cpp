#include "PlayerCollision.hpp"

#include "../../Features.hpp"
#include "../../GameState.hpp"
#include "../../Scene.hpp"
#include "../CamSeq/CameraSequence.hpp"
#include "../DoorEntity.hpp"
#include "../EnemyInstanceEntity.hpp"
#include "../ForceFieldEntity.hpp"
#include "../PlatformEntity.hpp"
#include "HalfturretEntity.hpp"
#include "PlayerEntity.hpp"

#include <algorithm>
#include <any>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace
{
    using MphRead::Formats::CollisionCandidate;
    using MphRead::Formats::CollisionDetection;
    using MphRead::Formats::CollisionResult;
    using MphRead::Formats::TestFlags;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    template <typename TEnum>
    [[nodiscard]] constexpr bool TestFlag(TEnum value, TEnum flag) noexcept
    {
        using U = std::underlying_type_t<TEnum>;
        return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
    }

    template <typename TEnum>
    [[nodiscard]] constexpr bool TestAny(TEnum value, TEnum flags) noexcept
    {
        using U = std::underlying_type_t<TEnum>;
        return (static_cast<U>(value) & static_cast<U>(flags)) != 0;
    }

    template <typename T>
    [[nodiscard]] T& RequireReference(T* value)
    {
        if (value == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename TValue>
    [[nodiscard]] auto ObjectPointer(TValue&& value) noexcept
    {
        using Value = std::remove_cvref_t<TValue>;
        if constexpr (std::is_pointer_v<Value>)
        {
            return value;
        }
        else if constexpr (requires { value.get(); })
        {
            return value.get();
        }
        else
        {
            return std::addressof(value);
        }
    }

    template <typename TValue>
    [[nodiscard]] decltype(auto) ManagedStorage(TValue& value)
    {
        using Value = std::remove_cvref_t<TValue>;
        if constexpr (std::is_pointer_v<Value>)
        {
            return RequireReference(value);
        }
        else if constexpr (requires { value.get(); })
        {
            return RequireReference(value);
        }
        else
        {
            return (value);
        }
    }

    template <typename TContainer>
    [[nodiscard]] std::int32_t ManagedLength(TContainer& values)
    {
        auto&& storage = ManagedStorage(values);
        if constexpr (requires { storage.Length(); })
        {
            return static_cast<std::int32_t>(storage.Length());
        }
        else
        {
            return static_cast<std::int32_t>(std::size(storage));
        }
    }

    template <typename TContainer>
    [[nodiscard]] decltype(auto) ManagedAt(TContainer& values, std::int32_t index)
    {
        auto&& storage = ManagedStorage(values);
        const std::int32_t length = ManagedLength(storage);
        if (index < 0 || index >= length)
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return storage[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] float MathFMin(float x, float y) noexcept
    {
        if (std::isnan(x))
        {
            return x;
        }
        if (std::isnan(y))
        {
            return y;
        }
        if (x == y && x == 0.0F)
        {
            return std::signbit(x) || std::signbit(y) ? -0.0F : 0.0F;
        }
        return x < y ? x : y;
    }

    [[nodiscard]] float MathFMax(float x, float y) noexcept
    {
        if (std::isnan(x))
        {
            return x;
        }
        if (std::isnan(y))
        {
            return y;
        }
        if (x == y && x == 0.0F)
        {
            return !std::signbit(x) || !std::signbit(y) ? 0.0F : -0.0F;
        }
        return x > y ? x : y;
    }

    [[nodiscard]] constexpr float LengthSquared(Vector3 value) noexcept
    {
        return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
    }

    [[nodiscard]] constexpr bool IsZero(Vector3 value) noexcept
    {
        return value.X == 0.0F && value.Y == 0.0F && value.Z == 0.0F;
    }

    [[nodiscard]] constexpr Vector3 Scale(Vector3 value, float scale) noexcept
    {
        return Vector3(value.X * scale, value.Y * scale, value.Z * scale);
    }

    [[nodiscard]] constexpr Vector4 Scale(Vector4 value, float scale) noexcept
    {
        return Vector4(value.X * scale, value.Y * scale, value.Z * scale, value.W * scale);
    }

    [[nodiscard]] constexpr Vector3 Divide(Vector3 value, float divisor) noexcept
    {
        return Vector3(value.X / divisor, value.Y / divisor, value.Z / divisor);
    }

    [[nodiscard]] constexpr Vector3 AddY(Vector3 value, float amount) noexcept
    {
        value.Y += amount;
        return value;
    }

    [[nodiscard]] constexpr Vector3 WithY(Vector3 value, float y) noexcept
    {
        value.Y = y;
        return value;
    }

    [[nodiscard]] std::int32_t ConvertToInt32Net9(float value) noexcept
    {
        if (std::isnan(value))
        {
            return 0;
        }
        const double wide = static_cast<double>(value);
        if (wide < static_cast<double>(std::numeric_limits<std::int32_t>::min()))
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        if (wide > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        {
            return std::numeric_limits<std::int32_t>::max();
        }
        return static_cast<std::int32_t>(std::trunc(wide));
    }

    [[nodiscard]] MphRead::MessageObject BoxInt32(std::int32_t value)
    {
        return std::make_shared<const std::any>(value);
    }

    [[nodiscard]] Matrix4 CreateRotationY(float angle)
    {
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        return Matrix4(
            Vector4(c, 0.0F, -s, 0.0F),
            Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            Vector4(s, 0.0F, c, 0.0F),
            Vector4(0.0F, 0.0F, 0.0F, 1.0F));
    }

    [[nodiscard]] constexpr float DegreesToRadians(float degrees) noexcept
    {
        return degrees * (3.14159265358979323846F / 180.0F);
    }
}

namespace MphRead::Entities
{
    using Formats::CollisionDetection;
    using Formats::CollisionResult;
    using Formats::TestFlags;

    void PlayerEntity::CheckPlayerCollision()
    {
        if (TestFlag(Flags2(), PlayerFlags2::Spectating))
        {
            return;
        }
        auto enumerator = RequireReference(_scene).GetPlayerEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            PlayerEntity& other = RequireReference(enumerator.Current());
            if (!TestFlag(other.LoadFlags(), LoadFlags::Active) || other.Health() == 0
                || TestFlag(other.Flags2(), PlayerFlags2::Spectating))
            {
                continue;
            }
            if (TestFlag(Flags2(), PlayerFlags2::Halfturret))
            {
                HalfturretEntity& halfturret = RequireReference(_halfturret);
                Vector3 toTurret = other.Volume().SpherePosition
                    - static_cast<Vector3>(halfturret.Position);
                float radius = other.Volume().SphereRadius + 0.45F + 0.1F;
                if (LengthSquared(toTurret) <= radius * radius)
                {
                    CollisionResult turretRes{};
                    if (toTurret.Y < 0.0F)
                    {
                        toTurret.Y = 0.0F;
                    }
                    assert(!IsZero(toTurret));
                    toTurret = toTurret.Normalized();
                    turretRes.Field0 = 0;
                    turretRes.Plane = Vector4(toTurret);
                    toTurret = Scale(toTurret, 0.45F);
                    toTurret = toTurret + static_cast<Vector3>(halfturret.Position);
                    turretRes.Plane.W = Vector3::Dot(toTurret, turretRes.Plane.Xyz());
                    other.HandleCollision(turretRes);
                    if (&other != this)
                    {
                        if (TestFlag(other.Flags1(), PlayerFlags1::Boosting))
                        {
                            TakeDamage(other._boostDamage,
                                DamageFlags::NoDmgInvuln | DamageFlags::Halfturret,
                                other.Speed(), &other);
                            other.EndAltAttack();
                        }
                        if (other._deathaltTimer > 0)
                        {
                            TakeDamage(200,
                                DamageFlags::Deathalt | DamageFlags::NoDmgInvuln
                                    | DamageFlags::Halfturret,
                                other.Speed(), &other);
                        }
                        CheckAltAttackHit2(&other, this, true);
                    }
                }
                CheckAltAttackHit1(&other, this, true);
            }
            if (&other == this)
            {
                continue;
            }
            Vector3 between = _volume.SpherePosition - other.Volume().SpherePosition;
            float distSqr = LengthSquared(between);
            if (distSqr == 0.0F)
            {
                between = Vector3(1.0F, 0.0F, 0.0F);
                distSqr = 1.0F;
            }
            float radii = _volume.SphereRadius + other.Volume().SphereRadius;
            if (distSqr < radii * radii)
            {
                float dist = std::sqrt(distSqr);
                between = Divide(between, dist);
                float dot = Vector3::Dot(Speed(), between);
                SetSpeed(Speed() - Scale(between, dot));
                Vector3 posAdd = Scale(between, radii - dist);
                Position = static_cast<Vector3>(Position) + posAdd;
                _volume = CollisionVolume::Move(
                    _volume, _volume.SpherePosition + posAdd);
                float kbAccel = Fixed::ToFloat(Values().AltAttackKnockbackAccel);
                if (Hunter() == MphRead::Hunter::Noxus && IsAltForm())
                {
                    other.SetAcceleration(
                        Vector3(between.X * -kbAccel, 0.0F, between.Z * -kbAccel));
                    other._accelerationTimer = static_cast<std::uint16_t>(
                        Values().AltAttackKnockbackTime * 2);
                }
                if (other.Hunter() == MphRead::Hunter::Noxus && other.IsAltForm())
                {
                    SetAcceleration(
                        Vector3(between.X * kbAccel, 0.0F, between.Z * kbAccel));
                    _accelerationTimer = static_cast<std::uint16_t>(
                        Values().AltAttackKnockbackTime * 2);
                }
                if (TestFlag(Flags1(), PlayerFlags1::Boosting))
                {
                    other.TakeDamage(_boostDamage, DamageFlags::NoDmgInvuln,
                        Speed(), this);
                    EndAltAttack();
                }
                if (TestFlag(other.Flags1(), PlayerFlags1::Boosting))
                {
                    TakeDamage(other._boostDamage, DamageFlags::NoDmgInvuln,
                        other.Speed(), &other);
                    other.EndAltAttack();
                }
                if (_deathaltTimer > 0)
                {
                    other.TakeDamage(200,
                        DamageFlags::Deathalt | DamageFlags::NoDmgInvuln,
                        Speed(), this);
                }
                if (other._deathaltTimer > 0)
                {
                    TakeDamage(200,
                        DamageFlags::Deathalt | DamageFlags::NoDmgInvuln,
                        other.Speed(), &other);
                }
                CheckAltAttackHit2(this, &other, false);
                CheckAltAttackHit2(&other, this, false);
            }
            CheckAltAttackHit1(this, &other, false);
        }
    }

    void PlayerEntity::CheckAltAttackHit1(
        PlayerEntity* attacker, PlayerEntity* target, bool halfturret)
    {
        PlayerEntity& source = RequireReference(attacker);
        if (source.Hunter() == MphRead::Hunter::Spire
            && TestFlag(source.Flags2(), PlayerFlags2::AltAttack))
        {
            PlayerEntity& victim = RequireReference(target);
            CollisionResult unused{};
            bool hit = false;
            if (halfturret)
            {
                auto&& halfturretValue = victim.Halfturret();
                auto* turret = ObjectPointer(halfturretValue);
                CollisionVolume otherVolume(
                    static_cast<Vector3>(RequireReference(turret).Position), 0.45F);
                hit = CollisionDetection::CheckSphereOverlapVolume(
                          &otherVolume, source._spireRockPosL, 0.5F, unused)
                    || CollisionDetection::CheckSphereOverlapVolume(
                          &otherVolume, source._spireRockPosR, 0.5F, unused);
            }
            else
            {
                CollisionVolume targetVolume = victim.Volume();
                hit = CollisionDetection::CheckSphereOverlapVolume(
                          &targetVolume, source._spireRockPosL, 0.5F, unused)
                    || CollisionDetection::CheckSphereOverlapVolume(
                          &targetVolume, source._spireRockPosR, 0.5F, unused);
            }
            if (hit)
            {
                Vector3 dir = Vector3::Zero;
                if (!halfturret)
                {
                    float x = static_cast<Vector3>(victim.Position).X
                        - static_cast<Vector3>(source.Position).X;
                    float z = static_cast<Vector3>(victim.Position).Z
                        - static_cast<Vector3>(source.Position).Z;
                    float factor = std::sqrt(x * x + z * z) * 4.0F;
                    dir.X = x / factor;
                    dir.Z = z / factor;
                }
                std::uint16_t damage = source.Values().AltAttackDamage;
                if (source.IsBot() && GameState::SinglePlayer)
                {
                    std::int32_t encounter
                        = ManagedAt(GameState::EncounterState, source.SlotIndex());
                    if (encounter == 1 || encounter == 3 || encounter == 4
                        || (encounter == 0 && source.BotLevel() == 0))
                    {
                        damage = 2;
                    }
                    else if (encounter != 0 || source.BotLevel() < 2)
                    {
                        damage = 3;
                    }
                    else
                    {
                        damage = 5;
                    }
                }
                DamageFlags flags
                    = DamageFlags::NoSfx | DamageFlags::NoDmgInvuln;
                if (halfturret)
                {
                    flags |= DamageFlags::Halfturret;
                }
                victim.TakeDamage(damage, flags, dir, &source);
                source._soundSource.PlaySfx(SfxId::SPIRE_ALT_ATTACK_HIT);
            }
        }
        else if (source.Hunter() == MphRead::Hunter::Noxus
            && source._altAttackTime >= source.Values().AltAttackStartup * 2)
        {
            PlayerEntity& victim = RequireReference(target);
            Vector3 between;
            if (halfturret)
            {
                auto&& halfturretValue = victim.Halfturret();
                auto* turret = ObjectPointer(halfturretValue);
                between = static_cast<Vector3>(RequireReference(turret).Position)
                    - source.Volume().SpherePosition;
            }
            else
            {
                between = victim.Volume().SpherePosition
                    - source.Volume().SpherePosition;
            }
            float radius = victim.Volume().SphereRadius;
            if (between.Y > -radius && between.Y < radius)
            {
                float hMagSqr = between.X * between.X + between.Z * between.Z;
                float radAddSqr = radius + 1.8F;
                radAddSqr *= radAddSqr;
                if (hMagSqr < radAddSqr)
                {
                    float factor = std::sqrt(hMagSqr) * 8.0F;
                    Vector3 dir(between.X / factor, 0.0F, between.Z / factor);
                    victim.SetAcceleration(dir);
                    victim._accelerationTimer = 8 * 2;
                    std::uint16_t damage = source.Values().AltAttackDamage;
                    if (source.IsBot() && GameState::SinglePlayer)
                    {
                        std::int32_t encounter
                            = ManagedAt(GameState::EncounterState, source.SlotIndex());
                        if (encounter == 1 || encounter == 3 || encounter == 4
                            || (encounter == 0 && source.BotLevel() == 0))
                        {
                            damage = 10;
                        }
                        else if (encounter != 0 || source.BotLevel() < 2)
                        {
                            damage = 12;
                        }
                        else
                        {
                            damage = 15;
                        }
                    }
                    DamageFlags flags
                        = DamageFlags::NoSfx | DamageFlags::NoDmgInvuln;
                    if (halfturret)
                    {
                        flags |= DamageFlags::Halfturret;
                    }
                    victim.TakeDamage(damage, flags, dir, &source);
                    source._soundSource.PlaySfx(SfxId::NOX_ALT_ATTACK_HIT);
                    RequireReference(victim._scene).SpawnEffect(
                        235, Vector3(1.0F, 0.0F, 0.0F),
                        Vector3(0.0F, 1.0F, 0.0F),
                        static_cast<Vector3>(victim.Position));
                    source.EndAltAttack();
                }
            }
        }
    }

    void PlayerEntity::CheckAltAttackHit2(
        PlayerEntity* attacker, PlayerEntity* target, bool halfturret)
    {
        PlayerEntity& source = RequireReference(attacker);
        if ((source.Hunter() != MphRead::Hunter::Trace
                && source.Hunter() != MphRead::Hunter::Weavel)
            || !TestFlag(source.Flags2(), PlayerFlags2::AltAttack))
        {
            return;
        }
        PlayerEntity& victim = RequireReference(target);
        Vector3 dir = Vector3::Zero;
        if (source.Hunter() == MphRead::Hunter::Trace)
        {
            dir.X = source.Speed().X;
            dir.Z = source.Speed().Z;
        }
        else
        {
            dir.X = source._field70;
            dir.Z = source._field74;
        }
        if (!halfturret)
        {
            float kbAccel = Fixed::ToFloat(source.Values().AltAttackKnockbackAccel);
            victim.SetAcceleration(
                Vector3(dir.X * kbAccel, -0.1F, dir.Z * kbAccel));
            victim._accelerationTimer = static_cast<std::uint16_t>(
                source.Values().AltAttackKnockbackTime * 2);
        }
        std::uint16_t damage = source.Values().AltAttackDamage;
        if (source.IsBot() && GameState::SinglePlayer)
        {
            std::int32_t encounter
                = ManagedAt(GameState::EncounterState, source.SlotIndex());
            if (encounter == 1 || encounter == 3 || encounter == 4)
            {
                damage = static_cast<std::uint16_t>(
                    source.Hunter() == MphRead::Hunter::Trace ? 15 : 10);
            }
            else if (encounter == 0 && source.BotLevel() == 0)
            {
                damage = 10;
            }
            else if (encounter != 0 || source.BotLevel() < 2)
            {
                damage = 15;
            }
            else
            {
                damage = static_cast<std::uint16_t>(
                    source.Hunter() == MphRead::Hunter::Trace ? 20 : 18);
            }
        }
        DamageFlags flags = DamageFlags::NoSfx | DamageFlags::NoDmgInvuln;
        if (halfturret)
        {
            flags |= DamageFlags::Halfturret;
        }
        victim.TakeDamage(damage, flags, dir, &source);
        SfxId sfx = source.Hunter() == MphRead::Hunter::Weavel
            ? SfxId::WEAVEL_ALT_ATTACK_HIT
            : SfxId::TRACE_ALT_ATTACK_HIT;
        source._soundSource.PlaySfx(sfx);
        source.EndAltAttack();
    }

    bool PlayerEntity::CheckAltAttackHitEnemy1(EnemyInstanceEntity* target)
    {
        EnemyInstanceEntity& enemy = RequireReference(target);
        if (TestAny(enemy.Flags(),
            EnemyFlags::Invincible | EnemyFlags::NoBombDamage))
        {
            return false;
        }
        if (Hunter() == MphRead::Hunter::Spire
            && TestFlag(Flags2(), PlayerFlags2::AltAttack))
        {
            CollisionResult unused{};
            CollisionVolume hurtVolume = enemy.HurtVolume();
            if (CollisionDetection::CheckSphereOverlapVolume(
                    &hurtVolume, _spireRockPosL, 0.5F, unused)
                || CollisionDetection::CheckSphereOverlapVolume(
                    &hurtVolume, _spireRockPosR, 0.5F, unused))
            {
                enemy.TakeDamage(Values().AltAttackDamage, this);
                _soundSource.PlaySfx(SfxId::SPIRE_ALT_ATTACK_HIT);
                return true;
            }
        }
        else if (Hunter() == MphRead::Hunter::Noxus
            && _altAttackTime >= Values().AltAttackStartup * 2)
        {
            CollisionVolume hurtVolume = enemy.HurtVolume();
            Vector3 between = hurtVolume.SpherePosition - Volume().SpherePosition;
            float radius = hurtVolume.SphereRadius;
            if (between.Y > -radius && between.Y < radius)
            {
                float hMagSqr = between.X * between.X + between.Z * between.Z;
                float radAddSqr = radius + 1.8F;
                radAddSqr *= radAddSqr;
                if (hMagSqr < radAddSqr)
                {
                    enemy.TakeDamage(Values().AltAttackDamage, this);
                    _soundSource.PlaySfx(SfxId::NOX_ALT_ATTACK_HIT);
                    RequireReference(_scene).SpawnEffect(
                        235, Vector3(1.0F, 0.0F, 0.0F),
                        Vector3(0.0F, 1.0F, 0.0F),
                        static_cast<Vector3>(enemy.Position));
                    EndAltAttack();
                    return true;
                }
            }
        }
        return false;
    }

    bool PlayerEntity::CheckAltAttackHitEnemy2(EnemyInstanceEntity* target)
    {
        EnemyInstanceEntity& enemy = RequireReference(target);
        if (TestAny(enemy.Flags(),
            EnemyFlags::Invincible | EnemyFlags::NoBombDamage))
        {
            return false;
        }
        if ((Hunter() == MphRead::Hunter::Trace
                || Hunter() == MphRead::Hunter::Weavel)
            && TestFlag(Flags2(), PlayerFlags2::AltAttack))
        {
            CollisionVolume hurtVolume = enemy.HurtVolume();
            Vector3 between = _volume.SpherePosition - hurtVolume.SpherePosition;
            float distSqr = LengthSquared(between);
            if (distSqr == 0.0F)
            {
                between = Vector3(1.0F, 0.0F, 0.0F);
                distSqr = 1.0F;
            }
            float radii = _volume.SphereRadius + hurtVolume.SphereRadius;
            if (distSqr < radii * radii)
            {
                enemy.TakeDamage(Values().AltAttackDamage, this);
                SfxId sfx = Hunter() == MphRead::Hunter::Weavel
                    ? SfxId::WEAVEL_ALT_ATTACK_HIT
                    : SfxId::TRACE_ALT_ATTACK_HIT;
                _soundSource.PlaySfx(sfx);
                EndAltAttack();
                return true;
            }
        }
        return false;
    }

    void PlayerEntity::AltAttackHitDoor(DoorEntity* door)
    {
        if (((Hunter() == MphRead::Hunter::Spire
                    || Hunter() == MphRead::Hunter::Trace
                    || Hunter() == MphRead::Hunter::Weavel)
                && TestFlag(Flags2(), PlayerFlags2::AltAttack))
            || (Hunter() == MphRead::Hunter::Noxus
                && _altAttackTime >= Values().AltAttackStartup * 2)
            || (Features::BoostOpensDoors
                && Hunter() == MphRead::Hunter::Samus
                && TestFlag(Flags1(), PlayerFlags1::Boosting)))
        {
            DoorEntity& doorRef = RequireReference(door);
            if (TestFlag(doorRef.Flags(), DoorFlags::Locked)
                && doorRef.Data().PaletteId == 8)
            {
                doorRef.Unlock(true, true);
            }
            doorRef.SetFlags(doorRef.Flags() | DoorFlags::ShotOpen);
            if (Hunter() == MphRead::Hunter::Spire)
            {
                _soundSource.PlaySfx(
                    SfxId::SPIRE_ALT_ATTACK_HIT, false, false,
                    std::numeric_limits<float>::max());
            }
            else if (Hunter() == MphRead::Hunter::Trace
                || Hunter() == MphRead::Hunter::Weavel)
            {
                SfxId sfx = Hunter() == MphRead::Hunter::Weavel
                    ? SfxId::WEAVEL_ALT_ATTACK_HIT
                    : SfxId::TRACE_ALT_ATTACK_HIT;
                _soundSource.PlaySfx(sfx);
                EndAltAttack();
            }
            else if (Hunter() == MphRead::Hunter::Noxus)
            {
                _soundSource.PlaySfx(SfxId::NOX_ALT_ATTACK_HIT);
                RequireReference(_scene).SpawnEffect(
                    235, Vector3(1.0F, 0.0F, 0.0F),
                    Vector3(0.0F, 1.0F, 0.0F),
                    static_cast<Vector3>(doorRef.Position));
                EndAltAttack();
            }
        }
    }

    void PlayerEntity::CheckCollision()
    {
        _standingEntCol.reset();
        _collidedEntCol.reset();
        _terrainDamage = false;
        ManagedArray<CollisionResult> results(40);
        CollisionVolume altVolume = ManagedAt(
            ManagedAt(PlayerVolumes, static_cast<std::int32_t>(Hunter())), 2);
        Vector3 point1;
        Vector3 point2;
        Vector3 limitMin;
        Vector3 limitMax;
        float margin;
        if (IsAltForm())
        {
            point1 = PrevPosition() + altVolume.SpherePosition;
            point2 = static_cast<Vector3>(Position) + altVolume.SpherePosition;
            margin = altVolume.SphereRadius + 0.4F;
            limitMin = Vector3(
                MathFMin(MathFMin(std::numeric_limits<float>::max(), point1.X), point2.X) - margin,
                MathFMin(MathFMin(std::numeric_limits<float>::max(), point1.Y), point2.Y) - margin,
                MathFMin(MathFMin(std::numeric_limits<float>::max(), point1.Z), point2.Z) - margin);
            limitMax = Vector3(
                MathFMax(MathFMax(std::numeric_limits<float>::lowest(), point1.X), point2.X) + margin,
                MathFMax(MathFMax(std::numeric_limits<float>::lowest(), point1.Y), point2.Y) + margin,
                MathFMax(MathFMax(std::numeric_limits<float>::lowest(), point1.Z), point2.Z) + margin);
            limitMin.Y = MathFMin(
                limitMin.Y,
                static_cast<Vector3>(Position).Y
                    + Fixed::ToFloat(Values().MaxPickupHeight));
        }
        else
        {
            float midpoint = (Fixed::ToFloat(Values().MaxPickupHeight)
                    + Fixed::ToFloat(Values().MinPickupHeight))
                / 2.0F;
            point1 = AddY(PrevPosition(), midpoint);
            point2 = AddY(static_cast<Vector3>(Position), midpoint);
            margin = (Fixed::ToFloat(Values().MaxPickupHeight)
                    - Fixed::ToFloat(Values().MinPickupHeight))
                / 2.0F + 0.4F;
            limitMin = Vector3(
                MathFMin(MathFMin(std::numeric_limits<float>::max(), point1.X), point2.X) - margin,
                MathFMin(MathFMin(std::numeric_limits<float>::max(), point1.Y), point2.Y) - margin,
                MathFMin(MathFMin(std::numeric_limits<float>::max(), point1.Z), point2.Z) - margin);
            limitMax = Vector3(
                MathFMax(MathFMax(std::numeric_limits<float>::lowest(), point1.X), point2.X) + margin,
                MathFMax(MathFMax(std::numeric_limits<float>::lowest(), point1.Y), point2.Y) + margin,
                MathFMax(MathFMax(std::numeric_limits<float>::lowest(), point1.Z), point2.Z) + margin);
        }
        bool includeEntities
            = GameState::TransitionState == MphRead::TransitionState::None;
        const auto& candidates = CollisionDetection::GetCandidatesForLimits(
            point1, point2, margin, limitMin, limitMax, includeEntities, _scene);
        if (IsAltForm())
        {
            float radius = altVolume.SphereRadius
                + (Hunter() == MphRead::Hunter::Spire
                        || Hunter() == MphRead::Hunter::Sylux
                    ? 0.5F
                    : 0.35F);
            std::int32_t count = CollisionDetection::CheckSphereBetweenPoints(
                &candidates, point1, point2, radius, 40, true,
                TestFlags::Players, _scene, &results);
            for (std::int32_t i = 0; i < count; ++i)
            {
                HandleCollision(results[static_cast<std::size_t>(i)]);
            }
            radius = Fixed::ToFloat(Values().BipedColRadius) - 0.15F;
            point1 = AddY(static_cast<Vector3>(Position), radius);
            float yOffset = Fixed::ToFloat(Values().AltColYPos)
                + Fixed::ToFloat(Values().MaxPickupHeight)
                - Fixed::ToFloat(Values().MinPickupHeight)
                - altVolume.SphereRadius - radius;
            point2 = AddY(static_cast<Vector3>(Position), yOffset);
            count = CollisionDetection::CheckSphereBetweenPoints(
                &candidates, point1, point2, radius, 1, false,
                TestFlags::Players, _scene, &results);
            if (count > 0)
            {
                SetFlags1(Flags1() | PlayerFlags1::NoUnmorph);
            }
            if (Hunter() == MphRead::Hunter::Kanden)
            {
                float altRadius = Fixed::ToFloat(Values().AltColRadius);
                for (std::int32_t i = 1; i < ManagedLength(_kandenSegPos); ++i)
                {
                    point2 = AddY(
                        ManagedAt(_kandenSegPos, i),
                        Fixed::ToFloat(Values().AltColYPos));
                    count = CollisionDetection::CheckSphereBetweenPoints(
                        &candidates, point2, point2, altRadius, 40, true,
                        TestFlags::Players, _scene, &results);
                    for (std::int32_t j = 0; j < count; ++j)
                    {
                        CollisionResult result
                            = results[static_cast<std::size_t>(j)];
                        if (result.Field0 == 1)
                        {
                            Vector3 edge = result.EdgePoint2 - result.EdgePoint1;
                            assert(!IsZero(edge));
                            Vector3 between = point2 - result.EdgePoint1;
                            float dot = Vector3::Dot(between, edge);
                            float div = std::clamp(
                                dot / LengthSquared(edge), 0.0F, 1.0F);
                            between = result.EdgePoint1 + Scale(edge, div);
                            between = point2 - between;
                            float magSqr = LengthSquared(between);
                            if (magSqr > 0.0F
                                && magSqr < altRadius * altRadius)
                            {
                                float mag = std::sqrt(magSqr);
                                if (altRadius - mag > 0.0F)
                                {
                                    float yInc
                                        = between.Y / mag * (altRadius - mag);
                                    ManagedAt(_kandenSegPos, i).Y += yInc;
                                }
                            }
                        }
                        else if (result.Field0 == 0)
                        {
                            float dot = altRadius + result.Plane.W
                                - Vector3::Dot(point2, result.Plane.Xyz());
                            if (dot > 0.0F)
                            {
                                ManagedAt(_kandenSegPos, i)
                                    = ManagedAt(_kandenSegPos, i)
                                    + Scale(result.Plane.Xyz(), dot);
                            }
                        }
                    }
                }
            }
        }
        else
        {
            float radius = (Fixed::ToFloat(Values().MaxPickupHeight)
                    - Fixed::ToFloat(Values().MinPickupHeight))
                / 2.0F;
            std::int32_t count = CollisionDetection::CheckSphereBetweenPoints(
                &candidates, point1, point2, radius, 40, true,
                TestFlags::Players, _scene, &results);
            for (std::int32_t i = 0; i < count; ++i)
            {
                HandleCollision(results[static_cast<std::size_t>(i)]);
            }
        }

        auto doorEnumerator
            = RequireReference(_scene).GetDoorEntities().GetEnumerator();
        while (doorEnumerator.MoveNext())
        {
            DoorEntity& door = RequireReference(doorEnumerator.Current());
            if (TestFlag(door.Flags(), DoorFlags::Open)
                || door.ConnectorInactive())
            {
                continue;
            }
            Vector3 lockPos = door.LockPosition();
            Vector3 doorFacing = door.FacingVector();
            Vector3 between = static_cast<Vector3>(Position) - lockPos;
            float dot = Vector3::Dot(between, doorFacing);
            if (dot <= 1.25F && dot >= -1.25F)
            {
                between = between - Scale(doorFacing, dot);
                if (LengthSquared(between) < door.RadiusSquared())
                {
                    CollisionResult doorResult{};
                    doorResult.Field0 = 0;
                    doorResult.Plane = Vector4(doorFacing);
                    doorResult.EntityCollision.reset();
                    doorResult.Flags = Formats::Collision::CollisionFlags::None;
                    if (Vector3::Dot(PrevPosition() - lockPos, doorFacing) < 0.0F)
                    {
                        doorResult.Plane.X *= -1.0F;
                        doorResult.Plane.Y *= -1.0F;
                        doorResult.Plane.Z *= -1.0F;
                    }
                    doorResult.Plane.W
                        = doorResult.Plane.X
                            * (lockPos.X + 0.4F * doorResult.Plane.X)
                        + doorResult.Plane.Y
                            * (lockPos.Y + 0.4F * doorResult.Plane.Y)
                        + doorResult.Plane.Z
                            * (lockPos.Z + 0.4F * doorResult.Plane.Z);
                    HandleCollision(doorResult);
                    AltAttackHitDoor(&door);
                }
            }
        }

        auto forceFieldEnumerator
            = RequireReference(_scene).GetForceFieldEntities().GetEnumerator();
        while (forceFieldEnumerator.MoveNext())
        {
            ForceFieldEntity& forceField
                = RequireReference(forceFieldEnumerator.Current());
            if (!forceField.Active())
            {
                continue;
            }
            Vector4 forcePlane = forceField.Plane();
            float dot1 = Vector3::Dot(
                    static_cast<Vector3>(Position), forcePlane.Xyz())
                - forcePlane.W;
            float dot2 = Vector3::Dot(PrevPosition(), forcePlane.Xyz())
                - forcePlane.W;
            if ((dot1 < 1.0F && dot1 > -1.0F)
                || (dot2 < 1.0F && dot2 > -1.0F))
            {
                Vector3 between
                    = Volume().SpherePosition
                    - static_cast<Vector3>(forceField.Position);
                float dotH = Vector3::Dot(between, forceField.UpVector());
                float dotW = Vector3::Dot(between, forceField.RightVector());
                if (dotH <= forceField.Height() && dotH >= -forceField.Height()
                    && dotW <= forceField.Width() && dotW >= -forceField.Width())
                {
                    CollisionResult ffResult{};
                    ffResult.Field0 = 0;
                    ffResult.EntityCollision.reset();
                    ffResult.Flags = Formats::Collision::CollisionFlags::None;
                    ffResult.Plane = forcePlane;
                    if (dot2 < 0.0F)
                    {
                        ffResult.Plane = Scale(ffResult.Plane, -1.0F);
                    }
                    HandleCollision(ffResult);
                }
            }
        }

        DamageResult dmgRes{};
        dmgRes.Damage = 1;
        dmgRes.TakeDamage = _terrainDamage;
        if (_collidedEntCol != nullptr)
        {
            RequireReference(_collidedEntCol->Entity).CheckContactDamage(dmgRes);
        }
        if (dmgRes.TakeDamage)
        {
            TakeDamage(dmgRes.Damage, DamageFlags::IgnoreInvuln,
                std::nullopt, nullptr);
        }
        _volume = CollisionVolume::Move(
            _volumeUnxf, static_cast<Vector3>(Position));
    }

    void PlayerEntity::HandleCollision(CollisionResult result)
    {
        bool v163 = false;
        bool v164 = false;
        bool v165 = false;
        bool v166 = false;
        bool isPlatform = false;
        bool isCrusher = false;
        PlatformEntity* platform = nullptr;
        if (result.EntityCollision != nullptr
            && result.EntityCollision->Entity != nullptr
            && result.EntityCollision->Entity->Type == EntityType::Platform)
        {
            isPlatform = true;
            platform = dynamic_cast<PlatformEntity*>(
                result.EntityCollision->Entity.get());
            if (platform == nullptr)
            {
                throw SceneDetail::InvalidCastException();
            }
            if (TestFlag(platform->Flags(), PlatformFlags::Hazard))
            {
                isCrusher = true;
            }
        }
        if (isCrusher && _health == 0)
        {
            return;
        }

        float v2;
        if (IsAltForm())
        {
            float altRad = Fixed::ToFloat(Values().AltColRadius);
            Vector3 altPos = AddY(
                static_cast<Vector3>(Position),
                Fixed::ToFloat(Values().AltColYPos));
            if (result.Field0 == 0)
            {
                v2 = altRad + result.Plane.W
                    - Vector3::Dot(altPos, result.Plane.Xyz());
            }
            else
            {
                if (result.Field0 != 1)
                {
                    return;
                }
                Vector3 edge = result.EdgePoint2 - result.EdgePoint1;
                assert(!IsZero(edge));
                Vector3 between = altPos - result.EdgePoint1;
                float dot = Vector3::Dot(between, edge);
                float div = std::clamp(
                    dot / LengthSquared(edge), 0.0F, 1.0F);
                between = altPos
                    - (result.EdgePoint1 + Scale(edge, div));
                float magSqr = LengthSquared(between);
                if (magSqr >= altRad * altRad || magSqr <= 0.0F)
                {
                    return;
                }
                float mag = std::sqrt(magSqr);
                between = Divide(between, mag);
                dot = Vector3::Dot(between, result.Plane.Xyz())
                    * (altRad - mag);
                v2 = dot * dot;
            }
        }
        else if (result.Field0 == 0)
        {
            float radius = Fixed::ToFloat(Values().BipedColRadius);
            Vector3 vec1 = AddY(
                static_cast<Vector3>(Position),
                Fixed::ToFloat(Values().MaxPickupHeight) - radius);
            Vector3 vec2 = AddY(
                static_cast<Vector3>(Position),
                Fixed::ToFloat(Values().MinPickupHeight) + radius);
            float dot1 = radius + result.Plane.W
                - Vector3::Dot(vec1, result.Plane.Xyz());
            float dot2 = radius + result.Plane.W
                - Vector3::Dot(vec2, result.Plane.Xyz());
            if (dot2 <= dot1)
            {
                v2 = dot1;
                v163 = true;
            }
            else
            {
                v2 = dot2;
            }
        }
        else
        {
            if (result.Field0 != 1)
            {
                return;
            }
            float v11 = 0.0F;
            float v162 = 1.0F;
            bool v169 = false;
            Vector3 edge = result.EdgePoint2 - result.EdgePoint1;
            assert(!IsZero(edge));
            float yTop = static_cast<Vector3>(Position).Y
                + Fixed::ToFloat(Values().MaxPickupHeight);
            float yBot = static_cast<Vector3>(Position).Y
                + Fixed::ToFloat(Values().MinPickupHeight);
            float yBotAdd = yBot + 0.5F;
            if (result.EdgePoint1.Y >= yBot)
            {
                if (result.EdgePoint1.Y > yTop)
                {
                    if (edge.Y == 0.0F)
                    {
                        return;
                    }
                    v11 = (yTop - result.EdgePoint1.Y) / edge.Y;
                }
                v2 = 0.0F;
            }
            else if (edge.Y != 0.0F)
            {
                v11 = (yBot - result.EdgePoint1.Y) / edge.Y;
                v2 = 0.0F;
            }
            else
            {
                if (result.EdgePoint1.Y <= yBotAdd)
                {
                    return;
                }
                float betweenX
                    = static_cast<Vector3>(Position).X - result.EdgePoint1.X;
                float betweenZ
                    = static_cast<Vector3>(Position).Z - result.EdgePoint1.Z;
                float div = (betweenX * edge.X + betweenZ * edge.Z)
                    / (edge.X * edge.X + edge.Z * edge.Z);
                div = std::clamp(div, 0.0F, 1.0F);
                Vector3 between(
                    static_cast<Vector3>(Position).X - result.EdgePoint1.X
                        + edge.X * div,
                    yBotAdd - result.EdgePoint1.Y,
                    static_cast<Vector3>(Position).Z - result.EdgePoint1.Z
                        + edge.Z * div);
                float magSqr = LengthSquared(between);
                if (magSqr >= 0.25F)
                {
                    return;
                }
                float mag = std::sqrt(magSqr);
                Vector3 normal = Divide(between, mag);
                result.Plane.X = normal.X;
                result.Plane.Y = normal.Y;
                result.Plane.Z = normal.Z;
                v2 = 0.5F - mag;
                v169 = true;
            }

            if (!v169)
            {
                if (result.EdgePoint2.Y >= yBot)
                {
                    if (result.EdgePoint2.Y > yTop)
                    {
                        v162 = (yTop - result.EdgePoint1.Y) / edge.Y;
                    }
                }
                else
                {
                    v162 = (yBot - result.EdgePoint1.Y) / edge.Y;
                }
                if (std::abs(v11 - v162) < 1.0F / 4096.0F)
                {
                    return;
                }
                Vector3 between(
                    static_cast<Vector3>(Position).X - result.EdgePoint1.X,
                    yTop - result.EdgePoint1.Y,
                    static_cast<Vector3>(Position).Z - result.EdgePoint1.Z);
                float dot1 = Vector3::Dot(between, edge);
                float dot2 = Vector3::Dot(edge, edge);
                float div = dot1 / dot2;
                if (div >= v11)
                {
                    if (div <= v162)
                    {
                        v11 = div;
                    }
                    else
                    {
                        v11 = v162;
                    }
                }
                float betweenY = result.EdgePoint1.Y + edge.Y * v11;
                if (betweenY > yTop + Fixed::ToFloat(2)
                    || betweenY < yBot - Fixed::ToFloat(2))
                {
                    return;
                }
                if (betweenY <= yBotAdd)
                {
                    between = Vector3(
                        static_cast<Vector3>(Position).X
                            - (result.EdgePoint1.X + edge.X * v11),
                        yBotAdd - betweenY,
                        static_cast<Vector3>(Position).Z
                            - (result.EdgePoint1.Z + edge.Z * v11));
                    float magSqr = LengthSquared(between);
                    if (magSqr >= 0.25F)
                    {
                        return;
                    }
                    float mag = std::sqrt(magSqr);
                    Vector3 normal = Divide(between, mag);
                    result.Plane.X = normal.X;
                    result.Plane.Y = normal.Y;
                    result.Plane.Z = normal.Z;
                    v2 = 0.5F - mag;
                }
                else
                {
                    float radius = Fixed::ToFloat(Values().BipedColRadius);
                    float betweenX = static_cast<Vector3>(Position).X
                        - (result.EdgePoint1.X + edge.X * v11);
                    float betweenZ = static_cast<Vector3>(Position).Z
                        - (result.EdgePoint1.Z + edge.Z * v11);
                    float v31 = betweenX * betweenX + betweenZ * betweenZ;
                    if (v31 >= radius * radius)
                    {
                        return;
                    }
                    float v32 = std::sqrt(v31);
                    result.Plane.X = betweenX / v32;
                    result.Plane.Y = 0.0F;
                    result.Plane.Z = betweenZ / v32;
                    v2 = radius - v32;
                    if (betweenY > static_cast<Vector3>(Position).Y)
                    {
                        v163 = true;
                    }
                }
            }
        }

        Vector3 position = static_cast<Vector3>(Position);
        if (v2 >= Fixed::ToFloat(-5))
        {
            v164 = true;
        }
        if (v2 > 0.0F)
        {
            if (result.Plane.Y < 0.1F && result.Plane.Y > -0.1F)
            {
                if (Cheats::WalkThroughWalls && !IsAltForm())
                {
                    return;
                }
                v165 = true;
                SetFlags1(Flags1() | PlayerFlags1::CollidingLateral);
            }
            position.X += result.Plane.X * v2;
            position.Z += result.Plane.Z * v2;
            if (!v163 || !TestFlag(Flags1(), PlayerFlags1::StandingPrevious))
            {
                float factor = 1.0F;
                if (result.Plane.Y > 0.0F && result.Plane.Y < 0.9F)
                {
                    factor = 0.5F / 2.0F;
                }
                else if (result.Plane.Y < 0.0F)
                {
                    factor = 2.0F * 2.0F;
                }
                position.Y += result.Plane.Y * v2 * factor;
            }
            float dot = Vector3::Dot(Speed(), result.Plane.Xyz());
            if (dot < 0.0F)
            {
                float damageSpeed = Fixed::ToFloat(Values().FallDamageSpeed);
                if (Speed().Y <= -damageSpeed
                    && !v163 && !v165 && !IsAltForm() && !IsMorphing()
                    && !IsUnmorphing()
                    && !TestFlag(Flags1(), PlayerFlags1::Standing)
                    && _timeSinceJumpPad > 5 * 2)
                {
                    std::int32_t damage = ConvertToInt32Net9(
                        Fixed::ToFloat(Values().FallDamageMax)
                        * -(Speed().Y + damageSpeed) / 0.8F);
                    assert(damage >= 0);
                    if (damage == 0)
                    {
                        damage = 1;
                    }
                    TakeDamage(static_cast<std::uint32_t>(damage),
                        DamageFlags::NoDmgInvuln, std::nullopt, nullptr);
                }
                if (Hunter() == MphRead::Hunter::Noxus && IsAltForm() && v165)
                {
                    float magSqr = PrevSpeed().X * result.Plane.X
                        + PrevSpeed().Z * result.Plane.Z;
                    if (magSqr < 0.0F)
                    {
                        float tilt = Fixed::ToFloat(Values().AltBounceTilt)
                            * -magSqr;
                        _altTiltX += result.Plane.X * tilt;
                        _altTiltZ += result.Plane.Z * tilt;
                        _altWobble += Fixed::ToFloat(Values().AltBounceWobble)
                            * -magSqr;
                        _altSpinSpeed -= Fixed::ToFloat(Values().AltBounceSpin)
                            * -magSqr;
                        Matrix4 rotMtx = CreateRotationY(DegreesToRadians(40.0F));
                        Vector3 axis = Matrix::Vec3MultMtx3(
                            result.Plane.Xyz(), rotMtx);
                        Vector3 speed = Speed();
                        speed.X += axis.X * (-magSqr / 2.0F);
                        speed.Z += axis.Z * (-magSqr / 2.0F);
                        SetSpeed(speed);
                    }
                }
                SetSpeed(Speed() + Scale(result.Plane.Xyz(), -dot));
                if (!v163 && !IsAltForm() && result.Field0 != 1)
                {
                    float hMagSqr
                        = Speed().X * Speed().X + Speed().Z * Speed().Z;
                    if (hMagSqr > 0.0F)
                    {
                        float div = (result.Plane.X * Speed().X
                                + result.Plane.Z * Speed().Z)
                            / std::sqrt(hMagSqr);
                        if (div < 0.0F)
                        {
                            SetSpeed(Scale(Speed(), div + 1.0F));
                        }
                    }
                }
            }
        }

        if (IsAltForm())
        {
            bool climbing
                = Hunter() == MphRead::Hunter::Spire && result.Field0 == 0;
            if (climbing)
            {
                if ((RequireReference(_scene).RoomId == 30
                        || RequireReference(_scene).RoomId == 67)
                    && result.Plane.Y == 0.0F && result.Plane.Z == 0.0F)
                {
                    climbing = false;
                }
                else if (RequireReference(_scene).RoomId == 80
                    && Formats::CameraSequence::Current() != nullptr)
                {
                    climbing = false;
                }
            }
            if (climbing)
            {
                for (std::int32_t i = 0; i < ManagedLength(_spireAltVecs); ++i)
                {
                    Vector3 vec = static_cast<Vector3>(Position)
                        + ManagedAt(_spireAltVecs, i);
                    float dot = result.Plane.W
                        - Vector3::Dot(vec, result.Plane.Xyz());
                    if (dot >= 0.0F)
                    {
                        assert(!IsZero(vec - static_cast<Vector3>(Position)));
                        v164 = true;
                        Position = static_cast<Vector3>(Position)
                            + Scale(result.Plane.Xyz(), dot);
                        vec = Vector3(
                            static_cast<Vector3>(Position).X - vec.X,
                            0.0F,
                            static_cast<Vector3>(Position).Z - vec.Z).Normalized();
                        vec.X *= dot / 4.0F;
                        vec.Z *= dot / 4.0F;
                        vec.X *= _hSpeedMag + 0.1F;
                        vec.Z *= _hSpeedMag + 0.1F;
                        SetSpeed(Speed() + Divide(vec, 2.0F));
                        if (result.Plane.Y > Fixed::ToFloat(-357)
                            && Speed().Y < 0.15F)
                        {
                            v166 = true;
                            float yFactor = _hSpeedMag / 2.0F;
                            if (Speed().Y < 0.01F)
                            {
                                yFactor += 0.3F;
                            }
                            Vector3 speed = Speed();
                            speed.Y += 4.0F * dot * yFactor / 2.0F;
                            SetSpeed(speed);
                            SetSpeed(WithY(Speed(), MathFMin(Speed().Y, 0.15F)));
                        }
                    }
                }
            }
            else if (Hunter() == MphRead::Hunter::Sylux
                && result.Field0 == 0
                && result.Plane.Y > Fixed::ToFloat(3138)
                && !TestFlag(Flags1(), PlayerFlags1::NoUnmorphPrevious))
            {
                Vector3 pos = AddY(
                    static_cast<Vector3>(Position),
                    Fixed::ToFloat(Values().AltColYPos)
                        - Fixed::ToFloat(Values().AltColRadius) - 0.3F);
                float dot = result.Plane.W
                    - Vector3::Dot(pos, result.Plane.Xyz());
                if (dot >= 0.0F)
                {
                    v164 = true;
                    Vector3 speed = Speed();
                    speed.Y += Fixed::ToFloat(Values().AltAirGravity);
                    SetSpeed(speed);
                    if (Speed().Y < 0.25F)
                    {
                        if (Speed().Y < 0.0F)
                        {
                            SetSpeed(WithY(
                                Speed(),
                                Speed().Y * Fixed::ToFloat(4034)));
                        }
                        speed = Speed();
                        speed.Y += dot * 0.2F;
                        SetSpeed(speed);
                        SetSpeed(WithY(Speed(), MathFMin(Speed().Y, 0.25F)));
                    }
                }
            }
        }

        if (result.EntityCollision != nullptr)
        {
            if (result.Plane.Y > 0.25F)
            {
                _standingEntCol = result.EntityCollision;
            }
            SetFlags1(Flags1() | PlayerFlags1::CollidingEntity);
            RequireReference(_scene).SendMessage(
                Message::PlayerCollideWith, this,
                result.EntityCollision->Entity.get(),
                BoxInt32(0), BoxInt32(_standingEntCol == nullptr ? 0 : 1));
            _collidedEntCol = result.EntityCollision;
            if (isCrusher)
            {
                assert(platform != nullptr);
                if (result.Field0 == 0
                    && Vector3::Dot(result.Plane.Xyz(), platform->Velocity())
                        >= 0.0F)
                {
                    float value = Fixed::ToFloat(4080);
                    if (result.Plane.X <= value)
                    {
                        if (result.Plane.X >= -value)
                        {
                            if (result.Plane.Y <= value)
                            {
                                if (result.Plane.Y >= -value)
                                {
                                    if (result.Plane.Z <= value)
                                    {
                                        if (result.Plane.Z < -value)
                                        {
                                            _crushBits |= 0x20;
                                        }
                                    }
                                    else
                                    {
                                        _crushBits |= 0x10;
                                    }
                                }
                                else
                                {
                                    _crushBits |= 8;
                                }
                            }
                            else
                            {
                                _crushBits |= 4;
                            }
                        }
                        else
                        {
                            _crushBits |= 2;
                        }
                    }
                    else
                    {
                        _crushBits |= 1;
                    }
                    if ((_crushBits & 3) == 3
                        || (_crushBits & 0xC) == 0xC
                        || (_crushBits & 0x30) == 0x30)
                    {
                        TakeDamage(static_cast<std::uint32_t>(_health),
                            DamageFlags::Death | DamageFlags::IgnoreInvuln
                                | DamageFlags::NoDmgInvuln,
                            std::nullopt, nullptr);
                    }
                }
            }
            else if (isPlatform)
            {
                assert(platform != nullptr);
                if (platform->Velocity().Y <= 0.0F
                    && (TestFlag(Flags1(), PlayerFlags1::Standing)
                        || TestFlag(Flags1(), PlayerFlags1::StandingPrevious))
                    && (v163
                        || (result.Plane.Y < Fixed::ToFloat(-3849)
                            && IsAltForm())))
                {
                    platform->Recoil();
                }
            }
        }

        _terrainDamage = TestFlag(
            result.Flags, Formats::Collision::CollisionFlags::Damaging);
        if (v164)
        {
            _field449 = 0;
            if (result.Plane.Y >= Fixed::ToFloat(1401))
            {
                _fieldC0 = _fieldC0 + result.Plane.Xyz();
            }
            if (!v163)
            {
                _slipperiness = result.Slipperiness();
                _standTerrain = result.Terrain();
                if (_terrainDamage)
                {
                    SetFlags1(Flags1() | PlayerFlags1::OnAcid);
                }
                if (_standTerrain == Terrain::Lava)
                {
                    SetFlags1(Flags1() | PlayerFlags1::OnLava);
                }
                if (result.Field0 == 0 && result.Plane.Y > 0.5F)
                {
                    SetFlags1(Flags1() | PlayerFlags1::Standing);
                    _timeSinceStanding = 0;
                    if (!TestFlag(Flags1(), PlayerFlags1::StandingPrevious)
                        || TestFlag(Flags1(), PlayerFlags1::AltFormPrevious))
                    {
                        SetFlags1(Flags1() & ~PlayerFlags1::UsedJump);
                    }
                    if (_health > 0 && !IsAltForm()
                        && !TestFlag(Flags1(), PlayerFlags1::Grounded))
                    {
                        if (Biped1Anim() == PlayerAnimation::JumpLeft)
                        {
                            SetBiped1Animation(
                                PlayerAnimation::LandLeft, AnimFlags::NoLoop);
                        }
                        else if (Biped1Anim() == PlayerAnimation::JumpRight)
                        {
                            SetBiped1Animation(
                                PlayerAnimation::LandRight, AnimFlags::NoLoop);
                        }
                        else
                        {
                            SetBiped1Animation(
                                PlayerAnimation::LandNeutral, AnimFlags::NoLoop);
                        }
                    }
                }
            }
        }
        if (v166 && !TestFlag(Flags1(), PlayerFlags1::Standing))
        {
            SetFlags2(Flags2() | PlayerFlags2::SpireClimbing);
        }
        Position = position;
        _volume = CollisionVolume::Move(
            _volumeUnxf, static_cast<Vector3>(Position));
    }
}
