#include "45_SlenchTurret.hpp"

#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Messaging.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"

#include <any>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Vector3;

        [[nodiscard]] EnemySpawnEntity* CastSpawner(EntityBase* spawner) noexcept
        {
            EnemySpawnEntity* typedSpawner = dynamic_cast<EnemySpawnEntity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
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

        [[nodiscard]] Enemy45Entity& RequireEnemy(Enemy45Entity* enemy)
        {
            return RequireReference(enemy);
        }

        template <typename T>
        [[nodiscard]] T& ManagedArrayAt(
            const std::shared_ptr<ManagedArray<T>>& values, std::int32_t index)
        {
            ManagedArray<T>& array = RequireReference(values);
            if (index < 0 || static_cast<std::size_t>(index) >= array.Length())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return array[static_cast<std::size_t>(index)];
        }

        template <typename T>
        [[nodiscard]] const T& VectorAt(
            const std::vector<T>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }

        [[nodiscard]] std::int32_t UInt32ToInt32(std::uint32_t value) noexcept
        {
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] std::int32_t AddInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t result
                = std::bit_cast<std::uint32_t>(left)
                + std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(result);
        }

        [[nodiscard]] std::int32_t SubInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t result
                = std::bit_cast<std::uint32_t>(left)
                - std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(result);
        }

        [[nodiscard]] std::int32_t MulInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t result
                = std::bit_cast<std::uint32_t>(left)
                * std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(result);
        }

        [[nodiscard]] Vector3 AddY(Vector3 value, float amount) noexcept
        {
            value.Y += amount;
            return value;
        }

        [[nodiscard]] std::shared_ptr<EntityBase> SharedEntity(
            Scene* scene, EntityBase* entity)
        {
            Scene& sceneRef = RequireReference(scene);
            auto enumerator = sceneRef.Entities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                std::shared_ptr<EntityBase> current = enumerator.Current();
                if (!current)
                {
                    throw System::NullReferenceException();
                }
                if (current.get() == entity)
                {
                    return current;
                }
            }
            throw SceneDetail::InvalidOperationException();
        }

        [[nodiscard]] std::int32_t UnboxInt32(const MessageObject& value)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            try
            {
                return std::any_cast<std::int32_t>(*value);
            }
            catch (const std::bad_any_cast&)
            {
                throw SceneDetail::InvalidCastException();
            }
        }
    }

    Enemy45Entity::Enemy45Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(4);
        (*processes)[0] = [this]() { State0(); };
        (*processes)[1] = [this]() { State0(); };
        (*processes)[2] = [this]() { State2(); };
        (*processes)[3] = [this]() { State0(); };
        _stateProcesses = std::move(processes);
    }

    std::int32_t Enemy45Entity::Index() const noexcept
    {
        return _index;
    }

    void Enemy45Entity::EnemyInitialize()
    {
        _state2 = 3;
        _state1 = _state2;

        EnemySpawnEntity& spawner = RequireReference(_spawner);
        const Vector3 facing = spawner.FacingVector();
        const Vector3 up = spawner.UpVector();
        const Vector3 position = spawner.Position;
        SetTransform(facing, up, position);

        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::NoMaxDistance;
        SetHealthbarMessageId(2);
        _boundingRadius = 1.0F;

        _hurtVolumeInit = CollisionVolume(spawner.Data.Fields.S10().Volume1);
        const auto volume0 = spawner.Data.Fields.S10().Volume0;
        const Vector3 currentPosition = Position;
        _volume = CollisionVolume::Move(volume0, currentPosition);

        const std::int32_t subtype
            = UInt32ToInt32(spawner.Data.Fields.S10().EnemySubtype);
        _values = VectorAt(Metadata::Enemy45Values, subtype);
        _healthMax = _values.Health;
        _health = _healthMax;
        Metadata::LoadEffectiveness(_values.Effectiveness, BeamEffectiveness);
        _scanId = _values.ScanId;
        _ammo = 1000;
        UpdateShotCount();

        const std::int32_t version
            = UInt32ToInt32(spawner.Data.Fields.S10().EnemyVersion);
        const std::shared_ptr<WeaponInfo> weapon
            = VectorAt(RequireReference(Weapons::EnemyWeapons), version);
        _equipInfo = std::make_shared<EquipInfo>();
        _equipInfo->SetWeapon(weapon);
        _equipInfo->SetBeams(_beams);
        _equipInfo->SetGetAmmo([this]() { return _ammo; });
        _equipInfo->SetSetAmmo(
            [this](std::int32_t newAmmo) { _ammo = newAmmo; });
        _equipInfo->UnchargedDamage(_values.Damage);

        _index = static_cast<std::int32_t>(spawner.Data.Fields.S10().Index);
        _subId = _state1;
        _model = &SetUpModel(
            "BigEyeTurret", 0, AnimFlags::NoLoop | AnimFlags::Paused);

        ModelInstance& model = RequireReference(_model);
        AnimationInfo& animInfo = RequireReference(model.AnimInfo);
        const std::int32_t frameCount = ManagedArrayAt(animInfo.FrameCount, 0);
        _animFrameCount = SubInt32Unchecked(frameCount, 1);
        _animDelayTimer = _animInterval;
    }

    void Enemy45Entity::UpdateShotCount()
    {
        const std::int32_t min = static_cast<std::int32_t>(_values.MinShots);
        const std::int32_t max = static_cast<std::int32_t>(_values.MaxShots);
        const std::int32_t range
            = SubInt32Unchecked(AddInt32Unchecked(max, 1), min);
        const std::uint32_t random = Rng::GetRandomInt2(range);
        _shotsRemaining = AddInt32Unchecked(
            min, std::bit_cast<std::int32_t>(random));
    }

    void Enemy45Entity::EnemyProcess()
    {
        if (_state1 != 3)
        {
            UpdateAnimationFrame();
            (void)ContactDamagePlayer(_values.ContactDamage, true);
            CallStateProcess();
        }
    }

    void Enemy45Entity::UpdateAnimationFrame()
    {
        if (!_animating
            || RequireReference(_scene).FrameCount() == 0
            || RequireReference(_scene).FrameCount() % 2 != 0)
        {
            return;
        }

        ModelInstance& model = RequireReference(_model);
        AnimationInfo& animInfo = RequireReference(model.AnimInfo);

        if (!_animReverse)
        {
            const std::int32_t frame = ManagedArrayAt(animInfo.Frame, 0);
            if (frame >= _animFrameCount)
            {
                if (frame > _animFrameCount)
                {
                    ManagedArrayAt(animInfo.Frame, 0) = _animFrameCount;
                }
                _animating = false;
            }
            else if (_animDelayTimer != 0)
            {
                _animDelayTimer = SubInt32Unchecked(_animDelayTimer, 1);
            }
            else
            {
                ManagedArrayAt(animInfo.Frame, 0)
                    = AddInt32Unchecked(frame, 1);
                _animDelayTimer = _animInterval;
            }
        }
        else if (ManagedArrayAt(animInfo.Frame, 0) != 0)
        {
            if (_animDelayTimer != 0)
            {
                _animDelayTimer = SubInt32Unchecked(_animDelayTimer, 1);
            }
            else
            {
                std::int32_t& frame = ManagedArrayAt(animInfo.Frame, 0);
                frame = SubInt32Unchecked(frame, 1);
                _animDelayTimer = _animInterval;
            }
        }
        else
        {
            SetAnimation();
        }
    }

    std::int32_t Enemy45Entity::GetMaxFrameCount()
    {
        ModelInstance& model = RequireReference(_model);
        AnimationInfo& animInfo = RequireReference(model.AnimInfo);
        const std::int32_t frameCount = ManagedArrayAt(animInfo.FrameCount, 0);
        return SubInt32Unchecked(frameCount, 1);
    }

    void Enemy45Entity::SetAnimation()
    {
        _animInterval = 1;
        _animReverse = false;
        _animating = true;
    }

    void Enemy45Entity::SetAnimationReverse()
    {
        _animInterval = 1;
        _animReverse = true;
        _animating = true;
    }

    void Enemy45Entity::State0()
    {
        (void)CallSubroutine<Enemy45Entity>(
            Metadata::Enemy45Subroutines, this);
    }

    void Enemy45Entity::State2()
    {
        if (_shotsRemaining != 0 && _shotCooldown != 0)
        {
            _shotCooldown = SubInt32Unchecked(_shotCooldown, 1);
        }
        else
        {
            PlayerEntity& main = RequireReference(PlayerEntity::Main());
            const Vector3 target
                = AddY(static_cast<Vector3>(main.Position), 0.5F);
            const Vector3 spawnDir
                = (target - static_cast<Vector3>(Position)).Normalized();
            _soundSource.PlaySfx(SfxId::TURRET_ATTACK);

            EquipInfo& equip = RequireReference(_equipInfo);
            equip.UnchargedDamage(_values.Damage);
            equip.HeadshotDamage(_values.Damage);
            SetAnimationReverse();

            const std::shared_ptr<EntityBase> owner
                = SharedEntity(_scene, this);
            const std::shared_ptr<EquipInfo> equipPtr = _equipInfo;
            const Vector3 spawnPosition = Position;
            const BeamSpawnFlags spawnFlags = BeamSpawnFlags::None;
            const Formats::Culling::NodeRef nodeRef = NodeRef;
            Scene* const scene = _scene;
            (void)BeamProjectileEntity::Spawn(
                owner, equipPtr, spawnPosition, spawnDir,
                spawnFlags, nodeRef, scene);

            _shotsRemaining = SubInt32Unchecked(_shotsRemaining, 1);
            _shotCooldown = MulInt32Unchecked(
                static_cast<std::int32_t>(_values.ShotCooldown), 2);
        }
        (void)CallSubroutine<Enemy45Entity>(
            Metadata::Enemy45Subroutines, this);
    }

    void Enemy45Entity::SpawnChargeEffect()
    {
        Scene& scene = RequireReference(_scene);
        const Vector3 facing(1.0F, 0.0F, 0.0F);
        const Vector3 up(0.0F, 1.0F, 0.0F);
        const Vector3 position = Position;
        scene.SpawnEffect(109, facing, up, position);
    }

    bool Enemy45Entity::Behavior00()
    {
        if (RequireReference(PlayerEntity::Main()).Health() == 0
            || !_volume.TestPoint(
                static_cast<Vector3>(RequireReference(PlayerEntity::Main()).Position)))
        {
            return false;
        }
        _soundSource.PlaySfx(SfxId::TURRET_LOCK_ON);
        SpawnChargeEffect();
        return true;
    }

    bool Enemy45Entity::Behavior01()
    {
        return false;
    }

    bool Enemy45Entity::Behavior02()
    {
        return !_volume.TestPoint(
            static_cast<Vector3>(RequireReference(PlayerEntity::Main()).Position));
    }

    bool Enemy45Entity::Behavior03()
    {
        if (_shotsRemaining != 0)
        {
            return false;
        }
        SpawnChargeEffect();
        UpdateShotCount();
        return true;
    }

    bool Enemy45Entity::Behavior04()
    {
        if (_salvoCooldown != 0)
        {
            _salvoCooldown = SubInt32Unchecked(_salvoCooldown, 1);
            return false;
        }
        _salvoCooldown = MulInt32Unchecked(
            static_cast<std::int32_t>(_values.SalvoCooldown), 2);
        return true;
    }

    void Enemy45Entity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::ActivateTurret)
        {
            _state2 = 0;
            _subId = _state2;
            SetAnimation();
        }
        else if (info.Message == Message::DeactivateTurret)
        {
            _state2 = 3;
            _subId = _state2;
        }
        else if (info.Message == Message::DecreaseTurretLights)
        {
            if (_animFrameCount != 0)
            {
                const std::int32_t amount = UnboxInt32(info.Param1);
                _animFrameCount
                    = SubInt32Unchecked(_animFrameCount, amount);
            }
            if (!_animating)
            {
                ModelInstance& model = RequireReference(_model);
                AnimationInfo& animInfo = RequireReference(model.AnimInfo);
                ManagedArrayAt(animInfo.Frame, 0) = _animFrameCount;
            }
        }
        else if (info.Message == Message::IncreaseTurretLights)
        {
            ModelInstance& model = RequireReference(_model);
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            const std::int32_t frameCount
                = ManagedArrayAt(animInfo.FrameCount, 0);
            const std::int32_t maxFrame
                = SubInt32Unchecked(frameCount, 1);
            if (_animFrameCount < maxFrame)
            {
                const std::int32_t amount = UnboxInt32(info.Param1);
                _animFrameCount
                    = AddInt32Unchecked(_animFrameCount, amount);
            }
            if (_animFrameCount > maxFrame)
            {
                _animFrameCount = maxFrame;
            }
            if (!_animating)
            {
                ManagedArrayAt(animInfo.Frame, 0) = _animFrameCount;
            }
        }
    }

    bool Enemy45Entity::Behavior00(Enemy45Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy45Entity::Behavior01(Enemy45Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy45Entity::Behavior02(Enemy45Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy45Entity::Behavior03(Enemy45Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }

    bool Enemy45Entity::Behavior04(Enemy45Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior04();
    }
}
