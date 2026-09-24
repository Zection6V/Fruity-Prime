#include "18_AlimbicTurret.hpp"

#include "../../GameState.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;
        using OpenTK::Mathematics::Vector4;

        EnemySpawnEntity* CastSpawner(EntityBase* spawner) noexcept
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

        [[nodiscard]] Enemy18Entity& RequireEnemy(Enemy18Entity* enemy)
        {
            return RequireReference(enemy);
        }

        [[nodiscard]] float Length(Vector3 value)
        {
            return std::sqrt(
                value.X * value.X + value.Y * value.Y + value.Z * value.Z);
        }

        [[nodiscard]] Vector3 ScaleVector(Vector3 value, float scale) noexcept
        {
            return Vector3(value.X * scale, value.Y * scale, value.Z * scale);
        }

        [[nodiscard]] Vector3 AddY(Vector3 value, float amount) noexcept
        {
            value.Y += amount;
            return value;
        }

        [[nodiscard]] float DegreesToRadians(float degrees) noexcept
        {
            return degrees * (3.14159265358979323846F / 180.0F);
        }

        [[nodiscard]] float RadiansToDegrees(float radians) noexcept
        {
            return radians * (180.0F / 3.14159265358979323846F);
        }

        [[nodiscard]] std::int32_t UInt32ToInt32(std::uint32_t value) noexcept
        {
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] std::uint16_t TwiceToUInt16(std::uint16_t value) noexcept
        {
            return static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(value) * 2U);
        }

        [[nodiscard]] std::uint16_t GetShotCount(const Enemy18Values& values)
        {
            const std::int32_t range
                = static_cast<std::int32_t>(values.MaxShots)
                + 1
                - static_cast<std::int32_t>(values.MinShots);
            const std::uint32_t random = Rng::GetRandomInt2(range);
            const std::int64_t count
                = static_cast<std::int64_t>(values.MinShots)
                + static_cast<std::int64_t>(random);
            return static_cast<std::uint16_t>(count);
        }

        template <typename T>
        [[nodiscard]] T& VectorAt(std::vector<T>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }

        template <typename T>
        [[nodiscard]] const T& VectorAt(const std::vector<T>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }

        [[nodiscard]] Matrix4 IdentityMatrix() noexcept
        {
            return Matrix4(
                Vector4(1.0F, 0.0F, 0.0F, 0.0F),
                Vector4(0.0F, 1.0F, 0.0F, 0.0F),
                Vector4(0.0F, 0.0F, 1.0F, 0.0F),
                Vector4(0.0F, 0.0F, 0.0F, 1.0F));
        }

        [[nodiscard]] Matrix4 CreateRotationX(float angle) noexcept
        {
            const float cos = std::cos(angle);
            const float sin = std::sin(angle);
            return Matrix4(
                Vector4(1.0F, 0.0F, 0.0F, 0.0F),
                Vector4(0.0F, cos, sin, 0.0F),
                Vector4(0.0F, -sin, cos, 0.0F),
                Vector4(0.0F, 0.0F, 0.0F, 1.0F));
        }

        [[nodiscard]] Matrix4 CreateRotationY(float angle) noexcept
        {
            const float cos = std::cos(angle);
            const float sin = std::sin(angle);
            return Matrix4(
                Vector4(cos, 0.0F, -sin, 0.0F),
                Vector4(0.0F, 1.0F, 0.0F, 0.0F),
                Vector4(sin, 0.0F, cos, 0.0F),
                Vector4(0.0F, 0.0F, 0.0F, 1.0F));
        }

        [[nodiscard]] Matrix4 Multiply(Matrix4 left, Matrix4 right) noexcept
        {
            Matrix4 result{};
            result.M11 = left.M11 * right.M11 + left.M12 * right.M21
                + left.M13 * right.M31 + left.M14 * right.M41;
            result.M12 = left.M11 * right.M12 + left.M12 * right.M22
                + left.M13 * right.M32 + left.M14 * right.M42;
            result.M13 = left.M11 * right.M13 + left.M12 * right.M23
                + left.M13 * right.M33 + left.M14 * right.M43;
            result.M14 = left.M11 * right.M14 + left.M12 * right.M24
                + left.M13 * right.M34 + left.M14 * right.M44;

            result.M21 = left.M21 * right.M11 + left.M22 * right.M21
                + left.M23 * right.M31 + left.M24 * right.M41;
            result.M22 = left.M21 * right.M12 + left.M22 * right.M22
                + left.M23 * right.M32 + left.M24 * right.M42;
            result.M23 = left.M21 * right.M13 + left.M22 * right.M23
                + left.M23 * right.M33 + left.M24 * right.M43;
            result.M24 = left.M21 * right.M14 + left.M22 * right.M24
                + left.M23 * right.M34 + left.M24 * right.M44;

            result.M31 = left.M31 * right.M11 + left.M32 * right.M21
                + left.M33 * right.M31 + left.M34 * right.M41;
            result.M32 = left.M31 * right.M12 + left.M32 * right.M22
                + left.M33 * right.M32 + left.M34 * right.M42;
            result.M33 = left.M31 * right.M13 + left.M32 * right.M23
                + left.M33 * right.M33 + left.M34 * right.M43;
            result.M34 = left.M31 * right.M14 + left.M32 * right.M24
                + left.M33 * right.M34 + left.M34 * right.M44;

            result.M41 = left.M41 * right.M11 + left.M42 * right.M21
                + left.M43 * right.M31 + left.M44 * right.M41;
            result.M42 = left.M41 * right.M12 + left.M42 * right.M22
                + left.M43 * right.M32 + left.M44 * right.M42;
            result.M43 = left.M41 * right.M13 + left.M42 * right.M23
                + left.M43 * right.M33 + left.M44 * right.M43;
            result.M44 = left.M41 * right.M14 + left.M42 * right.M24
                + left.M43 * right.M34 + left.M44 * right.M44;
            return result;
        }

        [[nodiscard]] Matrix4 Transpose(Matrix4 value) noexcept
        {
            Matrix4 result{};
            result.M11 = value.M11;
            result.M12 = value.M21;
            result.M13 = value.M31;
            result.M14 = value.M41;
            result.M21 = value.M12;
            result.M22 = value.M22;
            result.M23 = value.M32;
            result.M24 = value.M42;
            result.M31 = value.M13;
            result.M32 = value.M23;
            result.M33 = value.M33;
            result.M34 = value.M43;
            result.M41 = value.M14;
            result.M42 = value.M24;
            result.M43 = value.M34;
            result.M44 = value.M44;
            return result;
        }

        [[nodiscard]] Matrix4 ClearTranslation(Matrix4 value) noexcept
        {
            value.M41 = 0.0F;
            value.M42 = 0.0F;
            value.M43 = 0.0F;
            return value;
        }
    }

    const std::array<std::int32_t, 11> Enemy18Entity::_recolors{
        0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0
    };
}

namespace MphRead::Entities::Enemies
{
    Enemy18Entity::Enemy18Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(5);
        (*processes)[0] = [this]() { State0(); };
        (*processes)[1] = [this]() { State1(); };
        (*processes)[2] = [this]() { State2(); };
        (*processes)[3] = [this]() { State3(); };
        (*processes)[4] = [this]() { State4(); };
        _stateProcesses = std::move(processes);
    }

    void Enemy18Entity::EnemyInitialize()
    {
        EnemySpawnEntity& spawner = RequireReference(_spawner);

        const std::int32_t version = UInt32ToInt32(spawner.Data.Fields.S06().EnemyVersion);
        if (version < 0 || static_cast<std::size_t>(version) >= _recolors.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        SetRecolor(_recolors[static_cast<std::size_t>(version)]);

        const Vector3 facing = spawner.FacingVector();
        SetTransform(facing, spawner.UpVector(), spawner.Position);

        if (18 >= static_cast<std::int32_t>(Metadata::EnemyModelNames.size()))
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        ModelInstance& inst = SetUpModel(Metadata::EnemyModelNames[18]);

        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::OnRadar;
        _boundingRadius = 1.0F;
        _hurtVolumeInit = CollisionVolume(spawner.Data.Fields.S06().Volume0);

        const std::int32_t subtype = UInt32ToInt32(spawner.Data.Fields.S06().EnemySubtype);
        _values = VectorAt(Metadata::Enemy18Values, subtype);
        _health = _healthMax = _values.HealthMax;
        Metadata::LoadEffectiveness(_values.Effectiveness, BeamEffectiveness);
        _scanId = _values.ScanId;
        _rangeVolume = CollisionVolume::Move(
            spawner.Data.Fields.S06().Volume1, Position);

        _shotCount = GetShotCount(_values);
        _shotTimer = TwiceToUInt16(_values.ShotCooldown);

        std::shared_ptr<Model> model = inst.Model();
        _rotNode = RequireReference(model).GetNodeByName("Door_Rot");
        _rotNodePos = Position;
        _delayTimer = TwiceToUInt16(_values.DelayTime);
        _initialFacing = facing;
        _aimVec = facing;

        const Weapons::WeaponList& enemyWeapons = RequireReference(Weapons::EnemyWeapons);
        if (version < 0
            || static_cast<std::size_t>(version) >= enemyWeapons.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        const std::shared_ptr<WeaponInfo> weapon
            = enemyWeapons[static_cast<std::size_t>(version)];
        _equipInfo = std::make_shared<EquipInfo>(weapon, _beams);
        _equipInfo->GetAmmo = [this]() { return _ammo; };
        _equipInfo->SetAmmo = [this](std::int32_t newAmmo) { _ammo = newAmmo; };
        _equipInfo->UnchargedDamage(_values.BeamDamage);
        _equipInfo->SplashDamage(_values.SplashDamage);
    }

    void Enemy18Entity::EnemyProcess()
    {
        ContactDamagePlayer();
        if (_target)
        {
            for (std::int32_t i = 0; i < _scene->MessageQueue().Count(); ++i)
            {
                const MessageInfo info = _scene->MessageQueue()[i];
                if (info.Message == Message::Destroyed
                    && info.ExecuteFrame == _scene->FrameCount()
                    && info.Sender == _target.get())
                {
                    _target.reset();
                    break;
                }
            }
        }
        CallStateProcess();
    }

    void Enemy18Entity::ContactDamagePlayer()
    {
        if (!_target)
        {
            return;
        }

        const std::int32_t slotIndex = _target->SlotIndex();
        if (slotIndex < 0 || static_cast<std::size_t>(slotIndex) >= HitPlayers.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        if (HitPlayers[static_cast<std::size_t>(slotIndex)])
        {
            const CollisionVolume volume = _target->Volume();
            const Vector3 between = volume.SpherePosition - static_cast<Vector3>(Position);
            const float mag = Length(between) * 5.0F;
            Vector3 speed = _target->Speed();
            speed.X += between.X / mag;
            speed.Z += between.Z / mag;
            _target->SetSpeed(speed);
            _target->TakeDamage(
                _values.ContactDamage, DamageFlags::None, std::nullopt, this);
        }
    }

    void Enemy18Entity::State0()
    {
        if (_angleIncYSign == -1.0F)
        {
            if (_angleY < Fixed::ToFloat(_values.MinAngleY))
            {
                _angleIncYSign = 1.0F;
            }
        }
        else if (_angleY > Fixed::ToFloat(_values.MaxAngleY))
        {
            _angleIncYSign = -1.0F;
        }

        if (_angleIncXSign == -1.0F)
        {
            if (_angleX < Fixed::ToFloat(_values.MinAngleX))
            {
                _angleIncXSign = 1.0F;
            }
        }
        else if (_angleX > Fixed::ToFloat(_values.MaxAngleX))
        {
            _angleIncXSign = -1.0F;
        }

        _angleY += Fixed::ToFloat(_values.AngleIncY) * _angleIncYSign / 2.0F;
        _angleX += Fixed::ToFloat(_values.AngleIncX) * _angleIncXSign / 2.0F;
        (void)CallSubroutine<Enemy18Entity>(Metadata::Enemy18Subroutines, this);
    }

    void Enemy18Entity::State1()
    {
        (void)CallSubroutine<Enemy18Entity>(Metadata::Enemy18Subroutines, this);
    }

    void Enemy18Entity::UpdateAimVec()
    {
        if (_target)
        {
            _aimVec = static_cast<Vector3>(_target->Position) - _rotNodePos;
        }
        _aimVec = _aimVec.Normalized();
    }

    void Enemy18Entity::State2()
    {
        UpdateAimVec();
        (void)CallSubroutine<Enemy18Entity>(Metadata::Enemy18Subroutines, this);
    }

    void Enemy18Entity::State3()
    {
        UpdateAimVec();
        if (_shotCount > 0 && _shotTimer > 0)
        {
            --_shotTimer;
        }
        else if (_target)
        {
            const Vector3 targetPos = AddY(static_cast<Vector3>(_target->Position), 0.5F);
            const Vector3 spawnVec = (targetPos - _rotNodePos).Normalized();
            const Vector3 spawnPos = ScaleVector(spawnVec, Fixed::ToFloat(_values.ShotOffset))
                + _rotNodePos;
            _soundSource.PlaySfx(SfxId::TURRET_ATTACK);

            EquipInfo& equip = RequireReference(_equipInfo);
            equip.UnchargedDamage(_values.BeamDamage);
            equip.SplashDamage(_values.SplashDamage);
            equip.HeadshotDamage(_values.BeamDamage);
            (void)BeamProjectileEntity::Spawn(
                SharedFrom<EntityBase>(this), _equipInfo,
                spawnPos, spawnVec, BeamSpawnFlags::None, NodeRef, _scene);

            _shotCount = static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(_shotCount) - 1U);
            _shotTimer = TwiceToUInt16(_values.ShotCooldown);
        }
        (void)CallSubroutine<Enemy18Entity>(Metadata::Enemy18Subroutines, this);
    }

    void Enemy18Entity::State4()
    {
        (void)CallSubroutine<Enemy18Entity>(Metadata::Enemy18Subroutines, this);
    }

    bool Enemy18Entity::Behavior00()
    {
        if (!_target)
        {
            auto enumerator = _scene->GetPlayerEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                std::shared_ptr<PlayerEntity> player = enumerator.Current();
                PlayerEntity& playerRef = RequireReference(player);
                if (playerRef.IsBot() && GameState::SinglePlayer()
                    || playerRef.Health() == 0
                    || !_rangeVolume.TestPoint(playerRef.Position)
                    || GameState::Mode() == GameMode::BountyTeams && playerRef.TeamIndex() == 0)
                {
                    continue;
                }

                _target = player;
                _targetVec = (static_cast<Vector3>(playerRef.Position)
                    - static_cast<Vector3>(Position)).Normalized();
                _aimSteps = 20 * 2;
                const float angle = RadiansToDegrees(
                    std::acos(Vector3::Dot(_aimVec, _targetVec)));
                _aimAngleStep = angle / static_cast<float>(_aimSteps);
                _crossVec = Vector3::Cross(_aimVec, _targetVec).Normalized();
                _soundSource.PlaySfx(SfxId::TURRET_LOCK_ON);
                return true;
            }
        }
        return false;
    }

    bool Enemy18Entity::Behavior01()
    {
        if (!SeekTargetVector(
            _targetVec, _aimVec, _crossVec, _aimSteps, _aimAngleStep))
        {
            return false;
        }
        _angleX = 0.0F;
        _angleY = 0.0F;
        return true;
    }

    bool Enemy18Entity::Behavior02()
    {
        if (!_target)
        {
            return true;
        }
        if (_rangeVolume.TestPoint(_target->Position))
        {
            return false;
        }

        auto enumerator = _scene->GetPlayerEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<PlayerEntity> player = enumerator.Current();
            PlayerEntity& playerRef = RequireReference(player);
            if (playerRef.Health() == 0
                || !_rangeVolume.TestPoint(playerRef.Position)
                || GameState::Mode() == GameMode::BountyTeams && playerRef.TeamIndex() == 0)
            {
                continue;
            }
            _target = player;
            return false;
        }

        _targetVec = _initialFacing;
        _aimSteps = 20 * 2;
        _shotCount = GetShotCount(_values);
        const float angle = RadiansToDegrees(
            std::acos(Vector3::Dot(_aimVec, _targetVec)));
        _aimAngleStep = angle / static_cast<float>(_aimSteps);
        _crossVec = Vector3::Cross(_aimVec, _targetVec).Normalized();
        _target.reset();
        return true;
    }

    bool Enemy18Entity::Behavior03()
    {
        if (_shotCount > 0)
        {
            return false;
        }
        _shotCount = GetShotCount(_values);
        return true;
    }

    bool Enemy18Entity::Behavior04()
    {
        return SeekTargetVector(
            _targetVec, _aimVec, _crossVec, _aimSteps, _aimAngleStep);
    }

    bool Enemy18Entity::Behavior05()
    {
        if (_delayTimer > 0)
        {
            --_delayTimer;
            return false;
        }
        _delayTimer = TwiceToUInt16(_values.DelayTime);
        return true;
    }

    bool Enemy18Entity::EnemyGetDrawInfo()
    {
        ModelInstance& inst = _models[0];
        std::shared_ptr<Model> model = inst.Model();
        Model& modelRef = RequireReference(model);
        const std::shared_ptr<AnimationInfo> animInfo = inst.AnimInfo;

        if (_timeSinceDamage < 5 * 2)
        {
            SetPaletteOverride(Metadata::RedPalette);
        }

        const Vector3 upVector = UpVector();
        Matrix4 aimTransform{};
        if (_state1 == 0)
        {
            const Matrix4 rotX = CreateRotationX(DegreesToRadians(_angleX));
            const Matrix4 rotY = CreateRotationY(DegreesToRadians(_angleY));
            aimTransform = Multiply(rotX, rotY);
            _aimVec = Matrix::Vec3MultMtx4(_initialFacing, aimTransform);
            const Matrix4 transpose = Transpose(ClearTranslation(Transform));
            aimTransform = Multiply(aimTransform, transpose);
        }
        else
        {
            aimTransform = GetTransformMatrix(_aimVec, upVector);
            const Matrix4 transpose = Transpose(ClearTranslation(Transform));
            aimTransform = Multiply(aimTransform, transpose);
            aimTransform.M41 = 0.0F;
            aimTransform.M42 = 0.0F;
            aimTransform.M43 = 0.0F;
        }

        Node& rotNode = RequireReference(_rotNode);
        rotNode.AfterTransform = aimTransform;
        modelRef.AnimateNodes2(
            0, false, IdentityMatrix(), Vector3(1.0F, 1.0F, 1.0F), animInfo);
        rotNode.AfterTransform = std::nullopt;

        const std::vector<std::shared_ptr<Node>>& nodes = RequireReference(modelRef.Nodes);
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(nodes.size()); ++i)
        {
            const std::shared_ptr<Node>& node = nodes[static_cast<std::size_t>(i)];
            Node& nodeRef = RequireReference(node);
            nodeRef.Animation = Multiply(nodeRef.Animation, Transform);
        }
        modelRef.UpdateMatrixStack();
        UpdateMaterials(inst, Recolor());
        if (IsVisible(NodeRef))
        {
            GetDrawItems(inst, 0);
        }
        SetPaletteOverride(std::nullopt);
        _rotNodePos = Vector3(
            rotNode.Animation.M41,
            rotNode.Animation.M42,
            rotNode.Animation.M43);
        return true;
    }

    bool Enemy18Entity::Behavior00(Enemy18Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy18Entity::Behavior01(Enemy18Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy18Entity::Behavior02(Enemy18Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy18Entity::Behavior03(Enemy18Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }

    bool Enemy18Entity::Behavior04(Enemy18Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior04();
    }

    bool Enemy18Entity::Behavior05(Enemy18Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior05();
    }
}
