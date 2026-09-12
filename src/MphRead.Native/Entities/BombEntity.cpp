#include "BombEntity.hpp"

#include "../Formats/CollisionDetection.hpp"
#include "../Formats/Effects.hpp"
#include "../GameState.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Messaging.hpp"
#include "../Read.hpp"
#include "../Renderer.hpp"
#include "../Scene.hpp"
#include "../Utility/Rng.hpp"
#include "../Mods/Network/NetDamage.hpp"
#include "DoorEntity.hpp"
#include "Enemies/Enemy02Entity.hpp"
#include "Enemies/EnemyInstanceEntity.hpp"
#include "HalfturretEntity.hpp"
#include "Players/PlayerEntity.hpp"

#include <algorithm>
#include <any>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace MphRead::Entities
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;
        using OpenTK::Mathematics::Vector4;

        template <typename TEnum>
        [[nodiscard]] constexpr bool TestFlag(TEnum value, TEnum flag) noexcept
        {
            using Underlying = std::underlying_type_t<TEnum>;
            return (static_cast<Underlying>(value) & static_cast<Underlying>(flag))
                == static_cast<Underlying>(flag);
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

        template <typename T>
        [[nodiscard]] T* RawPointer(T* value) noexcept
        {
            return value;
        }

        template <typename T>
        [[nodiscard]] T* RawPointer(const std::shared_ptr<T>& value) noexcept
        {
            return value.get();
        }

        template <typename T>
        [[nodiscard]] const T* RawPointer(const std::shared_ptr<const T>& value) noexcept
        {
            return value.get();
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
                throw SceneDetail::IndexOutOfRangeException();
            }
            return storage[static_cast<std::size_t>(index)];
        }

        template <typename T>
        [[nodiscard]] T& VectorAt(const std::shared_ptr<const std::vector<std::shared_ptr<T>>>& values,
            std::int32_t index)
        {
            if (!values)
            {
                throw System::NullReferenceException();
            }
            if (index < 0 || static_cast<std::size_t>(index) >= values->size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return RequireReference((*values)[static_cast<std::size_t>(index)]);
        }

        template <typename T>
        [[nodiscard]] const T& VectorAt(
            const std::shared_ptr<const std::vector<T>>& values, std::int32_t index)
        {
            if (!values)
            {
                throw System::NullReferenceException();
            }
            if (index < 0 || static_cast<std::size_t>(index) >= values->size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return (*values)[static_cast<std::size_t>(index)];
        }

        [[nodiscard]] constexpr Vector3 Scale(Vector3 value, float scalar) noexcept
        {
            return Vector3(value.X * scalar, value.Y * scalar, value.Z * scalar);
        }

        [[nodiscard]] constexpr Vector3 Divide(Vector3 value, float scalar) noexcept
        {
            return Vector3(value.X / scalar, value.Y / scalar, value.Z / scalar);
        }

        [[nodiscard]] constexpr float LengthSquared(Vector3 value) noexcept
        {
            return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
        }

        [[nodiscard]] float Length(Vector3 value)
        {
            return std::sqrt(LengthSquared(value));
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

        [[nodiscard]] Matrix4 CreateTranslation(Vector3 position) noexcept
        {
            return Matrix4(
                Vector4(1.0F, 0.0F, 0.0F, 0.0F),
                Vector4(0.0F, 1.0F, 0.0F, 0.0F),
                Vector4(0.0F, 0.0F, 1.0F, 0.0F),
                Vector4(position, 1.0F));
        }

        [[nodiscard]] MessageObject BoxInt32(std::int32_t value)
        {
            return std::make_shared<const std::any>(value);
        }

        template <typename TPlayer>
        [[nodiscard]] decltype(auto) SyluxBombArray(TPlayer& owner)
        {
            return owner.SyluxBombs();
        }

        [[nodiscard]] BombEntity* BombAt(PlayerEntity& owner, std::int32_t index)
        {
            auto&& bombs = SyluxBombArray(owner);
            return RawPointer(ManagedAt(bombs, index));
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

        [[nodiscard]] HalfturretEntity* GetHalfturret(PlayerEntity& player)
        {
            return ObjectPointer(player.Halfturret());
        }
    }

    BombEntity::BombEntity(Scene* scene)
        : EntityBase(EntityType::Bomb, scene)
    {
    }

    BombFlags BombEntity::Flags() const noexcept
    {
        return _flags;
    }

    PlayerEntity* BombEntity::Owner() const noexcept
    {
        return _owner;
    }

    MphRead::BombType BombEntity::BombType() const noexcept
    {
        return _bombType;
    }

    std::int32_t BombEntity::BombIndex() const noexcept
    {
        return _bombIndex;
    }

    void BombEntity::SetBombIndex(std::int32_t value) noexcept
    {
        _bombIndex = value;
    }

    std::int32_t BombEntity::Countdown() const noexcept
    {
        return _countdown;
    }

    void BombEntity::SetCountdown(std::int32_t value) noexcept
    {
        _countdown = value;
    }

    float BombEntity::Radius() const noexcept
    {
        return _radius;
    }

    void BombEntity::SetRadius(float value) noexcept
    {
        _radius = value;
    }

    float BombEntity::SelfRadius() const noexcept
    {
        return _selfRadius;
    }

    void BombEntity::SetSelfRadius(float value) noexcept
    {
        _selfRadius = value;
    }

    std::uint16_t BombEntity::Damage() const noexcept
    {
        return _damage;
    }

    void BombEntity::SetDamage(std::uint16_t value) noexcept
    {
        _damage = value;
    }

    std::uint16_t BombEntity::EnemyDamage() const noexcept
    {
        return _enemyDamage;
    }

    void BombEntity::SetEnemyDamage(std::uint16_t value) noexcept
    {
        _enemyDamage = value;
    }

    std::shared_ptr<Effects::EffectEntry> BombEntity::Effect() const noexcept
    {
        return _effect;
    }

    void BombEntity::Initialize()
    {
        EntityBase::Initialize();
        std::int32_t effectId = 0;
        if (_bombType == MphRead::BombType::Stinglarva)
        {
            SetUpModel("KandenAlt_TailBomb");
            _flags |= BombFlags::HasModel;
            _countdown = 43 * 2;
        }
        else if (_bombType == MphRead::BombType::Lockjaw)
        {
            if (Recolor() == 0)
            {
                _trailModel = Read::GetModelInstance("arcWelder");
            }
            else
            {
                _trailModel = Read::GetModelInstance("arcWelder1");
            }
            _countdown = 900 * 2;
            const std::int32_t recolor = Recolor();
            if (recolor < 0
                || static_cast<std::size_t>(recolor) >= Metadata::SyluxBombEffects.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            effectId = Metadata::SyluxBombEffects[static_cast<std::size_t>(recolor)];
            PlayerEntity& owner = RequireReference(_owner);
            if (owner.SyluxBombCount() == 1)
            {
                Formats::CollisionResult colRes{};
                BombEntity& firstBomb = RequireReference(BombAt(owner, 0));
                Vector3 between = static_cast<Vector3>(firstBomb.Position)
                    - static_cast<Vector3>(Position);
                if (LengthSquared(between) >= 100.0F
                    || Formats::CollisionDetection::CheckBetweenPoints(
                        static_cast<Vector3>(firstBomb.Position),
                        static_cast<Vector3>(Position),
                        Formats::TestFlags::Players,
                        _scene,
                        colRes))
                {
                    _countdown = 1;
                    firstBomb._countdown = 1;
                }
            }
        }
        else if (_bombType == MphRead::BombType::MorphBall)
        {
            _countdown = 43 * 2;
            effectId = GameState::Multiplayer() && PlayerEntity::PlayerCount() > 2 ? 119 : 9;
        }
        if (effectId != 0)
        {
            _effect = RequireReference(_scene).SpawnEffectGetEntry(
                effectId, static_cast<Matrix4>(Transform));
            if (_effect)
            {
                _effect->SetElementExtension(true);
            }
        }
        if (_trailModel)
        {
            std::int32_t recolor = Recolor();
            if (Recolor() > 0)
            {
                --recolor;
            }
            std::shared_ptr<Model> model = _trailModel->Model();
            Model& modelRef = RequireReference(model);
            Material& material = VectorAt(modelRef.Materials, 0);
            _bindingId = RequireReference(_scene).BindGetTexture(
                model, material.TextureId, material.PaletteId, recolor);
        }
    }

    void BombEntity::Reposition(Vector3 offset)
    {
        Position = static_cast<Vector3>(Position) + offset;
        _target = nullptr;
    }

    bool BombEntity::Process()
    {
        EntityBase* hitEntity = nullptr;
        _soundSource.Update(static_cast<Vector3>(Position), 5);
        UpdateNodeRefVolume();
        if (_countdown > 0)
        {
            --_countdown;
        }
        if (_countdown == 0)
        {
            _flags |= BombFlags::Exploding;
        }
        if (!TestFlag(_flags, BombFlags::Exploded))
        {
            auto playerEnumerator = RequireReference(_scene).GetPlayerEntities().GetEnumerator();
            while (playerEnumerator.MoveNext())
            {
                PlayerEntity& player = RequireReference(playerEnumerator.Current());
                if (&player == _owner
                    || player.Health() == 0
                    || player.TeamIndex() == RequireReference(_owner).TeamIndex())
                {
                    if (&player != _owner && player.Health() > 0)
                    {
                        ++Mods::Network::NetDamage::BombTeamSkips;
                    }
                    continue;
                }
                ++Mods::Network::NetDamage::BombPlayerChecks;
                const Vector3 gapVector = player.Volume().SpherePosition
                    - static_cast<Vector3>(Position);
                const float gap = Length(gapVector);
                if (gap < Mods::Network::NetDamage::BombNearest)
                {
                    Mods::Network::NetDamage::BombNearest = gap;
                }
                if (_radius > Mods::Network::NetDamage::BombRadiusSeen)
                {
                    Mods::Network::NetDamage::BombRadiusSeen = _radius;
                }
                if (player.CheckHitByBomb(this, false))
                {
                    ++Mods::Network::NetDamage::BombHits;
                    hitEntity = &player;
                    _flags |= BombFlags::Exploding;
                }
                if (TestFlag(player.Flags2(), PlayerFlags2::Halfturret)
                    && player.CheckHitByBomb(this, true))
                {
                    hitEntity = &player;
                    _flags |= BombFlags::Exploding;
                }
                if (_target != nullptr)
                {
                    continue;
                }
                if (_bombType == MphRead::BombType::Lockjaw)
                {
                    LockjawCheckTargeting(player, hitEntity);
                }
                else if (_bombType == MphRead::BombType::Stinglarva)
                {
                    Vector3 between = static_cast<Vector3>(player.Position)
                        - static_cast<Vector3>(Position);
                    if (LengthSquared(between) < 5.0F * 5.0F)
                    {
                        _target = &player;
                        _speed = Scale(FacingVector(), 0.3F);
                    }
                }
            }
            if (_bombType == MphRead::BombType::Stinglarva && _target == nullptr)
            {
                auto halfturretEnumerator
                    = RequireReference(_scene).GetHalfturretEntities().GetEnumerator();
                while (halfturretEnumerator.MoveNext())
                {
                    HalfturretEntity& halfturret
                        = RequireReference(halfturretEnumerator.Current());
                    Vector3 between = static_cast<Vector3>(halfturret.Position)
                        - static_cast<Vector3>(Position);
                    if (LengthSquared(between) < 5.0F * 5.0F)
                    {
                        _target = &halfturret;
                        _speed = Scale(FacingVector(), 0.3F);
                    }
                }
            }
            {
                auto enemyEnumerator
                    = RequireReference(_scene).GetEnemyInstanceEntities().GetEnumerator();
                while (enemyEnumerator.MoveNext())
                {
                    EnemyInstanceEntity& enemy
                        = RequireReference(enemyEnumerator.Current());
                    if (TestFlag(enemy.Flags(), EnemyFlags::CollideBeam)
                        && (enemy.EnemyType() != MphRead::EnemyType::Temroid
                            || enemy.StateA() != 8)
                        && enemy.CheckHitByBomb(this))
                    {
                        hitEntity = &enemy;
                        _flags |= BombFlags::Exploding;
                    }
                }
            }
            {
                auto enemyEnumerator
                    = RequireReference(_scene).GetEnemyInstanceEntities().GetEnumerator();
                while (enemyEnumerator.MoveNext())
                {
                    EnemyInstanceEntity& enemy
                        = RequireReference(enemyEnumerator.Current());
                    if (TestFlag(enemy.Flags(), EnemyFlags::CollideBeam)
                        && enemy.EnemyType() == MphRead::EnemyType::Temroid
                        && enemy.StateA() == 8)
                    {
                        auto* temroid = dynamic_cast<Enemy02Entity*>(&enemy);
                        if (temroid == nullptr)
                        {
                            throw SceneDetail::InvalidCastException();
                        }
                        if (temroid->CheckTemroidHitByBomb(this))
                        {
                            hitEntity = &enemy;
                        }
                    }
                }
            }
            if (RequireReference(_owner).IsAltForm())
            {
                RequireReference(_owner).CheckHitByBomb(this, false);
            }
            if (TestFlag(_flags, BombFlags::Exploding))
            {
                auto doorEnumerator = RequireReference(_scene).GetDoorEntities().GetEnumerator();
                while (doorEnumerator.MoveNext())
                {
                    DoorEntity& door = RequireReference(doorEnumerator.Current());
                    const Vector3 doorFacing = door.FacingVector();
                    Vector3 between = static_cast<Vector3>(Position) - door.LockPosition();
                    const float dot = Vector3::Dot(doorFacing, between);
                    const float radius = _selfRadius + 0.4F;
                    if (dot < radius && dot > -radius)
                    {
                        between = between - Scale(doorFacing, dot);
                        if (LengthSquared(between) <= door.RadiusSquared())
                        {
                            if (TestFlag(door.Flags(), DoorFlags::Locked)
                                && door.Data().PaletteId == 8)
                            {
                                door.Unlock(true, true);
                            }
                            door.SetFlags(door.Flags() | DoorFlags::ShotOpen);
                        }
                    }
                }
            }
            if (_bombType == MphRead::BombType::Lockjaw
                && _bombIndex == 0
                && RequireReference(_owner).SyluxBombCount() == 3
                && _target == nullptr
                && hitEntity == nullptr)
            {
                PlayerEntity& owner = RequireReference(_owner);
                for (std::int32_t i = 0; i < 3; ++i)
                {
                    BombEntity* bomb = BombAt(owner, i);
                    assert(bomb != nullptr);
                    BombEntity& bombRef = RequireReference(bomb);
                    bombRef._countdown = 1;
                    bombRef._target = _owner;
                }
            }
        }
        if (_target != nullptr)
        {
            if (_target->GetTargetable())
            {
                ProcessTargeting();
            }
            else
            {
                _target = nullptr;
            }
        }
        if (_bombType == MphRead::BombType::Lockjaw)
        {
            PlayerEntity& owner = RequireReference(_owner);
            if (owner.Health() == 0)
            {
                _flags |= BombFlags::Exploding;
                _countdown = 0;
            }
            if (hitEntity != nullptr)
            {
                for (std::int32_t i = 0; i < owner.SyluxBombCount(); ++i)
                {
                    BombEntity* bomb = BombAt(owner, i);
                    assert(bomb != nullptr);
                    BombEntity& bombRef = RequireReference(bomb);
                    bombRef._target = hitEntity;
                    if (bombRef._countdown > 22 * 2)
                    {
                        bombRef._countdown = 22 * 2;
                    }
                }
            }
        }
        if (TestFlag(_flags, BombFlags::Exploding))
        {
            _flags &= ~BombFlags::Exploding;
            if (TestFlag(_flags, BombFlags::Exploded))
            {
                return false;
            }
            _flags |= BombFlags::Exploded;
            _target = nullptr;
            _models = ModelList{};
            if (_effect)
            {
                RequireReference(_scene).UnlinkEffectEntry(_effect);
                _effect.reset();
            }
            if (_bombType == MphRead::BombType::Stinglarva)
            {
                RequireReference(_scene).SpawnEffect(128, static_cast<Matrix4>(Transform));
            }
            else if (_bombType == MphRead::BombType::Lockjaw)
            {
                RequireReference(_scene).SpawnEffect(146, static_cast<Matrix4>(Transform));
            }
            else if (_bombType == MphRead::BombType::MorphBall)
            {
                RequireReference(_scene).SpawnEffect(145, static_cast<Matrix4>(Transform));
            }
            _countdown = 0;
            _soundSource.StopSfx(SfxId::KANDEN_ALT_ATTACK);
            _soundSource.StopSfx(SfxId::MORPH_BALL_BOMB_PLACE);
            _soundSource.PlaySfx(SfxId::MORPH_BALL_BOMB);
            if (hitEntity == nullptr)
            {
                RequireReference(_scene).SendMessage(
                    Message::Impact, this, _owner, BoxInt32(0), BoxInt32(0));
            }
        }
        if (_effect)
        {
            _effect->Transform(
                static_cast<Vector3>(Position), static_cast<Matrix4>(Transform));
        }
        return EntityBase::Process();
    }

    void BombEntity::LockjawCheckTargeting(PlayerEntity& player, EntityBase*& hitEntity)
    {
        Vector3 targetPos{};
        if (player.IsAltForm())
        {
            targetPos = player.Volume().SpherePosition;
        }
        else
        {
            targetPos = AddY(
                static_cast<Vector3>(player.Position),
                Fixed::ToFloat(player.Values().MinPickupHeight));
        }
        const float cylHeight
            = Fixed::ToFloat(player.Values().MaxPickupHeight)
            - Fixed::ToFloat(player.Values().MinPickupHeight);
        Formats::CollisionResult discard{};
        bool lineHitPlayer = false;
        bool lineHitHalfturret = false;
        if (_bombIndex == 1)
        {
            PlayerEntity& owner = RequireReference(_owner);
            BombEntity* bombZero = BombAt(owner, 0);
            assert(bombZero != nullptr);
            BombEntity& zero = RequireReference(bombZero);
            if (player.IsAltForm()
                && Formats::CollisionDetection::CheckCylinderOverlapSphere(
                    static_cast<Vector3>(Position),
                    static_cast<Vector3>(zero.Position),
                    targetPos,
                    player.Volume().SphereRadius,
                    discard))
            {
                lineHitPlayer = true;
            }
            else if (!player.IsAltForm()
                && Formats::CollisionDetection::CheckCylindersOverlap(
                    static_cast<Vector3>(Position),
                    static_cast<Vector3>(zero.Position),
                    targetPos,
                    Vector3(0.0F, 1.0F, 0.0F),
                    cylHeight,
                    player.Volume().SphereRadius,
                    discard))
            {
                lineHitPlayer = true;
            }
            else if (TestFlag(player.Flags2(), PlayerFlags2::Halfturret))
            {
                HalfturretEntity& halfturret = RequireReference(GetHalfturret(player));
                if (Formats::CollisionDetection::CheckCylinderOverlapSphere(
                    static_cast<Vector3>(Position),
                    static_cast<Vector3>(zero.Position),
                    static_cast<Vector3>(halfturret.Position),
                    0.45F,
                    discard))
                {
                    lineHitHalfturret = true;
                }
            }
        }
        else if (_bombIndex == 2)
        {
            PlayerEntity& owner = RequireReference(_owner);
            BombEntity* bombZero = BombAt(owner, 0);
            BombEntity* bombOne = BombAt(owner, 1);
            assert(bombZero != nullptr);
            assert(bombOne != nullptr);
            BombEntity& zero = RequireReference(bombZero);
            BombEntity& one = RequireReference(bombOne);
            if (player.IsAltForm()
                && Formats::CollisionDetection::CheckCylinderOverlapSphere(
                    static_cast<Vector3>(Position),
                    static_cast<Vector3>(zero.Position),
                    targetPos,
                    player.Volume().SphereRadius,
                    discard))
            {
                lineHitPlayer = true;
            }
            else if (!player.IsAltForm()
                && Formats::CollisionDetection::CheckCylindersOverlap(
                    static_cast<Vector3>(Position),
                    static_cast<Vector3>(zero.Position),
                    targetPos,
                    Vector3(0.0F, 1.0F, 0.0F),
                    cylHeight,
                    player.Volume().SphereRadius,
                    discard))
            {
                lineHitPlayer = true;
            }
            if (player.IsAltForm()
                && Formats::CollisionDetection::CheckCylinderOverlapSphere(
                    static_cast<Vector3>(Position),
                    static_cast<Vector3>(one.Position),
                    targetPos,
                    player.Volume().SphereRadius,
                    discard))
            {
                lineHitPlayer = true;
            }
            else if (!player.IsAltForm()
                && Formats::CollisionDetection::CheckCylindersOverlap(
                    static_cast<Vector3>(Position),
                    static_cast<Vector3>(one.Position),
                    targetPos,
                    Vector3(0.0F, 1.0F, 0.0F),
                    cylHeight,
                    player.Volume().SphereRadius,
                    discard))
            {
                lineHitPlayer = true;
            }
            else if (TestFlag(player.Flags2(), PlayerFlags2::Halfturret))
            {
                HalfturretEntity& halfturret = RequireReference(GetHalfturret(player));
                if (Formats::CollisionDetection::CheckCylinderOverlapSphere(
                    static_cast<Vector3>(Position),
                    static_cast<Vector3>(zero.Position),
                    static_cast<Vector3>(halfturret.Position),
                    0.45F,
                    discard))
                {
                    lineHitHalfturret = true;
                }
                else if (Formats::CollisionDetection::CheckCylinderOverlapSphere(
                    static_cast<Vector3>(Position),
                    static_cast<Vector3>(one.Position),
                    static_cast<Vector3>(halfturret.Position),
                    0.45F,
                    discard))
                {
                    lineHitHalfturret = true;
                }
            }
        }
        else if (RequireReference(_owner).SyluxBombCount() == 3)
        {
            PlayerEntity& owner = RequireReference(_owner);
            assert(_bombIndex == 0);
            if (LockjawCheckSnare(static_cast<Vector3>(player.Position)))
            {
                hitEntity = &player;
                for (std::int32_t i = 0; i < owner.SyluxBombCount(); ++i)
                {
                    BombEntity* bomb = BombAt(owner, i);
                    assert(bomb != nullptr);
                    BombEntity& bombRef = RequireReference(bomb);
                    bombRef._damage = 60;
                    bombRef._enemyDamage = 60;
                    if (owner.IsBot() && GameState::SinglePlayer())
                    {
                        const std::int32_t encounter
                            = ManagedAt(GameState::EncounterState, owner.SlotIndex());
                        if (encounter == 1
                            || encounter == 3
                            || encounter == 4
                            || (encounter == 0 && owner.BotLevel() == 0))
                        {
                            bombRef._enemyDamage = 4;
                            bombRef._damage = bombRef._enemyDamage;
                        }
                        else if (encounter != 0 || owner.BotLevel() < 2)
                        {
                            bombRef._enemyDamage = 7;
                            bombRef._damage = bombRef._enemyDamage;
                        }
                        else
                        {
                            bombRef._enemyDamage = 10;
                            bombRef._damage = bombRef._enemyDamage;
                        }
                    }
                }
            }
            else if (TestFlag(player.Flags2(), PlayerFlags2::Halfturret))
            {
                HalfturretEntity& halfturret = RequireReference(GetHalfturret(player));
                if (LockjawCheckSnare(static_cast<Vector3>(halfturret.Position)))
                {
                    hitEntity = &halfturret;
                    for (std::int32_t i = 0; i < owner.SyluxBombCount(); ++i)
                    {
                        BombEntity* bomb = BombAt(owner, i);
                        assert(bomb != nullptr);
                        BombEntity& bombRef = RequireReference(bomb);
                        bombRef._damage = 60;
                        bombRef._enemyDamage = 60;
                    }
                }
            }
            return;
        }
        if (lineHitPlayer)
        {
            assert(!lineHitHalfturret);
            hitEntity = &player;
            std::uint32_t damage = 20;
            PlayerEntity& owner = RequireReference(_owner);
            if (owner.IsBot() && GameState::SinglePlayer())
            {
                const std::int32_t encounter
                    = ManagedAt(GameState::EncounterState, owner.SlotIndex());
                if (encounter == 1
                    || encounter == 3
                    || encounter == 4
                    || (encounter == 0 && owner.BotLevel() == 0))
                {
                    damage = 1;
                }
                else
                {
                    damage = 3;
                }
            }
            player.TakeDamage(
                damage, DamageFlags::NoDmgInvuln, std::nullopt, this);
        }
        else if (lineHitHalfturret)
        {
            HalfturretEntity& halfturret = RequireReference(GetHalfturret(player));
            hitEntity = &halfturret;
            player.TakeDamage(
                20,
                DamageFlags::NoDmgInvuln | DamageFlags::Halfturret,
                std::nullopt,
                this);
        }
    }

    bool BombEntity::LockjawCheckSnare(Vector3 position)
    {
        PlayerEntity& owner = RequireReference(_owner);
        BombEntity* bombZero = BombAt(owner, 0);
        BombEntity* bombOne = BombAt(owner, 1);
        BombEntity* bombTwo = BombAt(owner, 2);
        assert(bombZero != nullptr);
        assert(bombOne != nullptr);
        assert(bombTwo != nullptr);
        BombEntity& zero = RequireReference(bombZero);
        BombEntity& one = RequireReference(bombOne);
        BombEntity& two = RequireReference(bombTwo);
        const Vector3 zeroToOne
            = static_cast<Vector3>(one.Position) - static_cast<Vector3>(zero.Position);
        const Vector3 oneToTwo
            = static_cast<Vector3>(two.Position) - static_cast<Vector3>(one.Position);
        const Vector3 cross1 = Vector3::Cross(oneToTwo, zeroToOne).Normalized();
        const Vector3 zeroToPosition = position - static_cast<Vector3>(zero.Position);
        const float dot = Vector3::Dot(cross1, zeroToPosition);
        if (dot > -0.75F && dot < 0.75F)
        {
            const Vector3 cross2 = Vector3::Cross(zeroToPosition, zeroToOne);
            if (Vector3::Dot(cross2, cross1) > 0.0F)
            {
                const Vector3 oneToPosition = position - static_cast<Vector3>(one.Position);
                const Vector3 cross3 = Vector3::Cross(oneToPosition, oneToTwo);
                if (Vector3::Dot(cross3, cross1) > 0.0F)
                {
                    const Vector3 twoToPosition
                        = position - static_cast<Vector3>(two.Position);
                    const Vector3 twoToZero
                        = static_cast<Vector3>(zero.Position)
                        - static_cast<Vector3>(two.Position);
                    const Vector3 cross4 = Vector3::Cross(twoToPosition, twoToZero);
                    if (Vector3::Dot(cross4, cross1) > 0.0F)
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void BombEntity::ProcessTargeting()
    {
        assert(_target != nullptr);
        EntityBase& target = RequireReference(_target);
        Vector3 targetPos{};
        target.GetPosition(targetPos);
        const Vector3 prevPos = static_cast<Vector3>(Position);
        Vector3 newSpeed{};
        if (_bombType == MphRead::BombType::Lockjaw)
        {
            Vector3 between = targetPos - static_cast<Vector3>(Position);
            const float magSqr = LengthSquared(between);
            if (magSqr > 0.0F)
            {
                between = Divide(between, std::sqrt(magSqr));
            }
            newSpeed = _speed + Scale(between - _speed, 0.15F);
        }
        else
        {
            assert(_bombType == MphRead::BombType::Stinglarva);
            Vector3 between = WithY(
                targetPos - static_cast<Vector3>(Position), 0.0F);
            const float hMagSqr = between.X * between.X + between.Z * between.Z;
            if (hMagSqr > 0.0F)
            {
                between = Divide(between, std::sqrt(hMagSqr));
            }
            const float deltaX = (between.X - _speed.X) * 0.05F;
            const float deltaZ = (between.Z - _speed.Z) * 0.05F;
            newSpeed = Vector3(
                _speed.X + deltaX,
                _speed.Y - 0.05F,
                _speed.Z + deltaZ);
        }
        _speed = _speed + Divide(newSpeed - _speed, 2.0F);
        Position = static_cast<Vector3>(Position) + Divide(_speed, 2.0F);
        ManagedArray<Formats::CollisionResult> results(8);
        const std::int32_t count
            = Formats::CollisionDetection::CheckSphereBetweenPoints(
                prevPos,
                static_cast<Vector3>(Position),
                0.4F,
                8,
                false,
                Formats::TestFlags::None,
                _scene,
                &results);
        for (std::int32_t i = 0; i < count; ++i)
        {
            const Formats::CollisionResult result
                = results[static_cast<std::size_t>(i)];
            assert(result.Field0 == 0);
            const Vector3 normal = result.Plane.Xyz();
            const float dotw = result.Plane.W
                - Vector3::Dot(static_cast<Vector3>(Position), normal)
                + 0.4F;
            if (dotw > 0.0F)
            {
                Position = static_cast<Vector3>(Position) + Scale(normal, dotw);
                const float dot = Vector3::Dot(Divide(_speed, 2.0F), normal);
                if (dot < 0.0F)
                {
                    _speed = _speed + Scale(normal, -dot);
                }
            }
        }
        if (_bombType != MphRead::BombType::Lockjaw
            && (_speed.X != 0.0F || _speed.Z != 0.0F))
        {
            SetTransform(
                _speed.Normalized(),
                UpVector(),
                static_cast<Vector3>(Position));
        }
    }

    void BombEntity::GetDrawInfo()
    {
        if (_bombType == MphRead::BombType::Lockjaw)
        {
            if (_bombIndex == 1)
            {
                PlayerEntity& owner = RequireReference(_owner);
                BombEntity& bombZero = RequireReference(BombAt(owner, 0));
                DrawLockjawTrail(
                    static_cast<Vector3>(Position),
                    static_cast<Vector3>(bombZero.Position),
                    Fixed::ToFloat(614),
                    10);
            }
            else if (_bombIndex == 2)
            {
                PlayerEntity& owner = RequireReference(_owner);
                BombEntity& bombOne = RequireReference(BombAt(owner, 1));
                DrawLockjawTrail(
                    static_cast<Vector3>(Position),
                    static_cast<Vector3>(bombOne.Position),
                    Fixed::ToFloat(614),
                    10);
                BombEntity& bombZero = RequireReference(BombAt(owner, 0));
                DrawLockjawTrail(
                    static_cast<Vector3>(Position),
                    static_cast<Vector3>(bombZero.Position),
                    Fixed::ToFloat(614),
                    10);
            }
        }
        EntityBase::GetDrawInfo();
    }

    void BombEntity::DrawLockjawTrail(
        Vector3 point1,
        Vector3 point2,
        float height,
        std::int32_t segments)
    {
        assert(_trailModel != nullptr);
        if (segments < 2)
        {
            return;
        }
        const std::int32_t count = 4 * segments;
        std::int32_t recolor = Recolor();
        if (Recolor() > 0)
        {
            --recolor;
        }
        const Vector3 vec = point2 - point1;
        ModelInstance& trailModel = RequireReference(_trailModel);
        Model& model = RequireReference(trailModel.Model());
        MphRead::Recolor& recolorData = VectorAt(model.Recolors, recolor);
        const Texture& texture = VectorAt(recolorData.Textures, 0);
        const float uvT = (texture.Height - (1.0F / 16.0F)) / texture.Height;
        auto uvsAndVerts = std::make_shared<ManagedArray<Vector3>>(
            static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < segments; ++i)
        {
            float uvS = 0.0F;
            if (i > 0)
            {
                uvS = (texture.Width / static_cast<float>(segments - 1) * i
                    - (1.0F / 16.0F)) / texture.Width;
            }
            const float pct = i * (1.0F / (segments - 1));
            float x = vec.X * pct;
            float y = vec.Y * pct;
            float z = vec.Z * pct;
            if (i > 0 && i < segments - 1)
            {
                x += Rng::GetRandomInt1(0x800) / 4096.0F - 0.25F;
                y += Rng::GetRandomInt1(0x800) / 4096.0F - 0.25F;
                z += Rng::GetRandomInt1(0x800) / 4096.0F - 0.25F;
            }
            (*uvsAndVerts)[static_cast<std::size_t>(4 * i)]
                = Vector3(uvS, 0.0F, 0.0F);
            (*uvsAndVerts)[static_cast<std::size_t>(4 * i + 1)]
                = Vector3(x, y - height, z);
            (*uvsAndVerts)[static_cast<std::size_t>(4 * i + 2)]
                = Vector3(uvS, uvT, 0.0F);
            (*uvsAndVerts)[static_cast<std::size_t>(4 * i + 3)]
                = Vector3(x, y + height, z);
        }
        Material& material = VectorAt(model.Materials, 0);
        RequireReference(_scene).AddRenderItem(
            RenderItemType::TrailMulti,
            1.0F,
            RequireReference(_scene).GetNextPolygonId(),
            Vector3(1.0F, 1.0F, 1.0F),
            material.XRepeat,
            material.YRepeat,
            material.ScaleS,
            material.ScaleT,
            CreateTranslation(point1),
            uvsAndVerts,
            _bindingId,
            count);
    }

    void BombEntity::Destroy()
    {
        _soundSource.StopAllSfx();
        std::int32_t owned = 0;
        if (_owner != nullptr)
        {
            auto&& bombs = SyluxBombArray(*_owner);
            owned = std::min<std::int32_t>(
                _owner->SyluxBombCount(), ManagedLength(bombs));
        }
        if (_bombType == MphRead::BombType::Lockjaw
            && _owner != nullptr
            && _bombIndex >= 0
            && _bombIndex < owned)
        {
            auto&& bombs = SyluxBombArray(*_owner);
            if (RawPointer(ManagedAt(bombs, _bombIndex)) == this)
            {
                for (std::int32_t i = _bombIndex; i < owned - 1; ++i)
                {
                    auto bomb = ManagedAt(bombs, i + 1);
                    ManagedAt(bombs, i) = bomb;
                    BombEntity* bombPtr = RawPointer(bomb);
                    if (bombPtr != nullptr)
                    {
                        bombPtr->_bombIndex = i;
                    }
                }
                ManagedAt(bombs, owned - 1) = nullptr;
                _owner->SetSyluxBombCount(
                    static_cast<std::uint8_t>(_owner->SyluxBombCount() - 1));
            }
        }
        _models = ModelList{};
        _trailModel.reset();
        if (_effect)
        {
            RequireReference(_scene).UnlinkEffectEntry(_effect);
        }
        _effect.reset();
        _owner = nullptr;
        RequireReference(_scene).UnlinkBomb(this);
        EntityBase::Destroy();
    }

    void BombEntity::PlaySpawnSfx()
    {
        _soundSource.Update(static_cast<Vector3>(Position), 5);
        UpdateNodeRefVolume();
        const SfxId sfx = _bombType == MphRead::BombType::Stinglarva
            ? SfxId::KANDEN_ALT_ATTACK
            : SfxId::MORPH_BALL_BOMB_PLACE;
        _soundSource.PlaySfx(sfx);
    }

    std::shared_ptr<BombEntity> BombEntity::Spawn(
        PlayerEntity* owner,
        Matrix4 transform,
        Scene* scene)
    {
        PlayerEntity& ownerRef = RequireReference(owner);
        MphRead::BombType type = MphRead::BombType::MorphBall;
        if (ownerRef.Hunter() == Hunter::Kanden)
        {
            type = MphRead::BombType::Stinglarva;
        }
        else if (ownerRef.Hunter() == Hunter::Sylux)
        {
            type = MphRead::BombType::Lockjaw;
        }
        std::shared_ptr<BombEntity> bomb = RequireReference(scene).InitBomb();
        if (!bomb)
        {
            assert(false && "Failed to spawn bomb");
            return nullptr;
        }
        bomb->_owner = owner;
        bomb->_bombType = type;
        bomb->Transform = transform;
        bomb->SetRecolor(ownerRef.Recolor());
        bomb->_flags = BombFlags::None;
        bomb->NodeRef = Formats::Culling::NodeRef::None;
        RequireReference(scene).AddEntity(bomb);
        return bomb;
    }
}
