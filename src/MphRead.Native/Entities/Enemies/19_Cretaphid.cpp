#include "19_Cretaphid.hpp"

#include "../../GameState.hpp"
#include "../../MemoryArrays.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "20_CretaphidEye.hpp"
#include "21_CretaphidCrystal.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <any>
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

using ::MphRead::NativeRuntime::RequireReference;
using ::OpenTK::Mathematics::CreateTranslation;
using ::OpenTK::Mathematics::Equal;
using ::OpenTK::Mathematics::IdentityMatrix;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;
using ::OpenTK::Mathematics::Multiply;
using ::OpenTK::Mathematics::WithY;

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

        [[nodiscard]] Enemy19Entity& RequireEnemy(Enemy19Entity* enemy)
        {
            return RequireReference(enemy);
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

        [[nodiscard]] float DistanceSquared(Vector3 left, Vector3 right) noexcept
        {
            const float x = left.X - right.X;
            const float y = left.Y - right.Y;
            const float z = left.Z - right.Z;
            return x * x + y * y + z * z;
        }

        [[nodiscard]] Matrix4 CreateRotationX(float angle) noexcept
        {
            const float cosine = std::cos(angle);
            const float sine = std::sin(angle);
            return Matrix4(
                Vector4(1.0F, 0.0F, 0.0F, 0.0F),
                Vector4(0.0F, cosine, sine, 0.0F),
                Vector4(0.0F, -sine, cosine, 0.0F),
                Vector4(0.0F, 0.0F, 0.0F, 1.0F));
        }

        void SetRow3Xyz(Matrix4& matrix, Vector3 value) noexcept
        {
            matrix.M41 = value.X;
            matrix.M42 = value.Y;
            matrix.M43 = value.Z;
        }

        [[nodiscard]] std::int32_t UInt32ToInt32(std::uint32_t value) noexcept
        {
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] std::int32_t UnboxInt32(const MessageObject& value)
        {
            if (!value || !value->has_value())
            {
                throw Memory::Detail::NullReferenceException();
            }
            try
            {
                return std::any_cast<std::int32_t>(*value);
            }
            catch (const std::bad_any_cast&)
            {
                throw Memory::Detail::InvalidCastException();
            }
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
        [[nodiscard]] const T& VectorAt(
            const std::vector<T>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }

        template <typename T>
        [[nodiscard]] T ArrayAt(
            const std::shared_ptr<ManagedArray<T>>& values, std::int32_t index)
        {
            if (!values)
            {
                throw System::NullReferenceException();
            }
            if (index < 0 || static_cast<std::size_t>(index) >= values->Length())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return (*values)[static_cast<std::size_t>(index)];
        }

        [[nodiscard]] bool AnimationEnded(ModelInstance& model)
        {
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            ManagedArray<AnimFlags>& flags = RequireReference(animInfo.Flags);
            if (flags.Length() == 0)
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return (flags[0] & AnimFlags::Ended) != AnimFlags::None;
        }

        [[nodiscard]] std::shared_ptr<Model> RequireModel(ModelInstance& instance)
        {
            std::shared_ptr<Model> model = instance.Model();
            if (!model)
            {
                throw System::NullReferenceException();
            }
            return model;
        }
    }

    const std::array<const char*, Enemy19Entity::_eyeCount> Enemy19Entity::_eyeNodes{
        "torret_bone_2", "torret_bone_3", "torret_bone_4", "torret_bone_5",
        "torret_bone_6", "torret_bone_7", "torret_bone_8", "torret_bone_9",
        "torret_bone_10", "torret_bone_11", "torret_bone_12", "torret_bone_13"
    };

    const std::array<Movie, 4> Enemy19Entity::_deathMovieIds{
        Movie::CretaphidCA1Defeat,
        Movie::CretaphidVDO1Defeat,
        Movie::CretaphidAlinos2Defeat,
        Movie::CretaphidArcterra2Defeat
    };

    Enemy19Entity::Enemy19Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene)
    {
        EnemySpawnEntity* spawner = CastSpawner(data.Spawner);
        _spawner = spawner;
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(27);
        for (std::size_t i = 0; i < 27; ++i)
        {
            (*processes)[i] = [this]() { State0(); };
        }
        _stateProcesses = std::move(processes);
    }

    Enemy19Values Enemy19Entity::Values() const
    {
        if (_subtype < 0
            || static_cast<std::size_t>(_subtype) >= Metadata::Enemy19Values.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        return Metadata::Enemy19Values[static_cast<std::size_t>(_subtype)];
    }

    std::int32_t Enemy19Entity::PhaseIndex() const noexcept
    {
        return _phaseIndex;
    }

    ModelInstance* Enemy19Entity::BeamModel() const noexcept
    {
        return _beamModel;
    }

    ModelInstance* Enemy19Entity::BeamColModel() const noexcept
    {
        return _beamColModel;
    }

    Sound::SoundSource& Enemy19Entity::SoundSource() noexcept
    {
        return _soundSource;
    }

    void Enemy19Entity::EnemyInitialize()
    {
        const Vector3 position = RequireReference(_data.Spawner).Position;
        Vector3 facing(0.0F, 0.0F, 1.0F);
        if (!Equal(position, MainPlayer().Position))
        {
            facing = WithY(MainPlayer().Position - position, 0.0F).Normalized();
        }
        Matrix4 transform = GetTransformMatrix(facing, Vector3(0.0F, 1.0F, 0.0F));
        SetRow3Xyz(transform, position);
        Transform = transform;

        EnemySpawnEntity* spawner = dynamic_cast<EnemySpawnEntity*>(_data.Spawner);
        if (spawner != nullptr && spawner->ParentEntCol)
        {
            _parentEntCol = spawner->ParentEntCol;
            _invTransform = Multiply(_transform, spawner->ParentEntCol->Inverse2);
        }

        _health = _healthMax = 100;
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::OnRadar;
        Flags |= EnemyFlags::NoMaxDistance;
        SetHealthbarMessageId(1);
        _boundingRadius = 1.0F;
        _hurtVolumeInit = CollisionVolume(_spawner->Data.Fields.S05().Volume0);
        _subtype = UInt32ToInt32(_spawner->Data.Fields.S05().EnemySubtype);
        _scanId = Values().ScanId;
        _crystalDownTimer = static_cast<std::int32_t>(Values().PhaseFlashTime) * 2;
        _flashTimer = _flashPeriod;

        _model = &SetUpModel("CylinderBoss", 2);
        _model->NodeAnimIgnoreRoot = true;
        RequireModel(*_model)->ComputeNodeMatrices(0);

        if (_subtype == 0)
        {
            _beamModel = &SetUpModel("cylBossLaser");
        }
        else if (_subtype == 2)
        {
            _beamModel = &SetUpModel("cylBossLaserY");
        }
        else if (_subtype == 3)
        {
            _beamModel = &SetUpModel("cylBossLaserG");
        }
        _beamColModel = &SetUpModel("cylBossLaserColl");

        Segments[0] = std::make_shared<SegmentInfo>();
        Segments[1] = std::make_shared<SegmentInfo>();
        Segments[2] = std::make_shared<SegmentInfo>();

        Segments[0]->JointNode
            = RequireModel(RequireReference(_model))->GetNodeByName("Upper_joint");
        Segments[1]->JointNode
            = RequireModel(RequireReference(_model))->GetNodeByName("Mid_joint");
        Segments[2]->JointNode
            = RequireModel(RequireReference(_model))->GetNodeByName("Lower_joint");

        Segments[0]->AngleStep = Fixed::ToFloat(Values().Seg0AngleStep) / 2.0F;
        Segments[0]->BeamAngle = Fixed::ToFloat(Values().Seg0BeamStartAngle);
        Segments[0]->BeamAngleMax = Fixed::ToFloat(Values().Seg0BeamAngleMax);
        Segments[0]->BeamAngleMin = Fixed::ToFloat(Values().Seg0BeamAngleMin);
        Segments[0]->BeamAngleStep = Fixed::ToFloat(Values().Seg0BeamAngleStep) / 2.0F;
        Segments[0]->SpinDirection = 1;

        Segments[1]->AngleStep = Fixed::ToFloat(Values().Seg1AngleStep) / 2.0F;
        Segments[1]->BeamAngle = Fixed::ToFloat(Values().Seg1BeamStartAngle);
        Segments[1]->BeamAngleMax = Fixed::ToFloat(Values().Seg1BeamAngleMax);
        Segments[1]->BeamAngleMin = Fixed::ToFloat(Values().Seg1BeamAngleMin);
        Segments[1]->BeamAngleStep = Fixed::ToFloat(Values().Seg1BeamAngleStep) / 2.0F;
        Segments[1]->SpinDirection = -1;

        Segments[2]->AngleStep = Fixed::ToFloat(Values().Seg2AngleStep) / 2.0F;
        Segments[2]->BeamAngle = Fixed::ToFloat(Values().Seg2BeamStartAngle);
        Segments[2]->BeamAngleMax = Fixed::ToFloat(Values().Seg2BeamAngleMax);
        Segments[2]->BeamAngleMin = Fixed::ToFloat(Values().Seg2BeamAngleMin);
        Segments[2]->BeamAngleStep = Fixed::ToFloat(Values().Seg2BeamAngleStep) / 2.0F;
        Segments[2]->SpinDirection = 1;

        _phaseValues[0][0] = static_cast<std::int32_t>(Values().Phase0CrystalShotDelay) * 2;
        _phaseValues[0][1] = static_cast<std::int32_t>(Values().Phase0CrystalShotTime) * 2;
        _phaseValues[0][2] = static_cast<std::int32_t>(Values().Phase0CrystalUpTime) * 2;
        _phaseValues[0][3] = Values().Phase0CrystalHealth;
        _phaseValues[1][0] = static_cast<std::int32_t>(Values().Phase1CrystalShotDelay) * 2;
        _phaseValues[1][1] = static_cast<std::int32_t>(Values().Phase1CrystalShotTime) * 2;
        _phaseValues[1][2] = static_cast<std::int32_t>(Values().Phase1CrystalUpTime) * 2;
        _phaseValues[1][3] = Values().Phase1CrystalHealth;
        _phaseValues[2][0] = static_cast<std::int32_t>(Values().Phase2CrystalShotDelay) * 2;
        _phaseValues[2][1] = static_cast<std::int32_t>(Values().Phase2CrystalShotTime) * 2;
        _phaseValues[2][2] = static_cast<std::int32_t>(Values().Phase2CrystalUpTime) * 2;
        _phaseValues[2][3] = Values().Phase2CrystalHealth;

        _eyeStartIndex = 3;
        _eyeEndIndex = 6;
        _eyeBurnIndex = _eyeStartIndex;
        _eyeBurnUpdateTimer = 1.0F / 30.0F;

        SpawnEyes();
        SpawnCrystal();

        const Weapons::WeaponList& bossWeapons
            = RequireReference(Weapons::BossWeapons);
        const std::shared_ptr<WeaponInfo> laserWeapon
            = VectorAt(bossWeapons, 1);
        const std::shared_ptr<WeaponInfo> plasmaWeapon
            = VectorAt(bossWeapons, 2);
        EquipInfo[0] = std::make_shared<::MphRead::EquipInfo>(laserWeapon, _beams);
        EquipInfo[1] = std::make_shared<::MphRead::EquipInfo>(plasmaWeapon, _beams);
        EquipInfo[0]->GetAmmo = [this]() { return _ammo0; };
        EquipInfo[0]->SetAmmo = [this](std::int32_t newAmmo) { _ammo0 = newAmmo; };
        EquipInfo[1]->GetAmmo = [this]() { return _ammo1; };
        EquipInfo[1]->SetAmmo = [this](std::int32_t newAmmo) { _ammo1 = newAmmo; };
        EquipInfo[0]->ChargeLevel = RequireReference(laserWeapon).FullCharge;
        EquipInfo[1]->ChargeLevel = RequireReference(plasmaWeapon).FullCharge;

        SetPhase0();
        _crystalShotTimer = GetPhaseValue(PhaseValue::CrystalShotTime);
        _crystalShotDelay = GetPhaseValue(PhaseValue::CrystalShotDelay);
        _crystalUpTimer = GetPhaseValue(PhaseValue::CrystalUpTime);
        Sub2135F54();
    }

    std::int32_t Enemy19Entity::GetPhaseValue(PhaseValue value) const
    {
        if (_phaseIndex < 0
            || static_cast<std::size_t>(_phaseIndex) >= _phaseValues.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        const std::int32_t index = static_cast<std::int32_t>(value);
        if (index < 0 || static_cast<std::size_t>(index) >= _phaseValues[0].size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        return _phaseValues[static_cast<std::size_t>(_phaseIndex)]
            [static_cast<std::size_t>(index)];
    }

    void Enemy19Entity::SpawnEyes()
    {
        for (std::int32_t i = 0; i < _eyeCount; ++i)
        {
            std::shared_ptr<EnemyInstanceEntity> spawned = EnemySpawnEntity::SpawnEnemy(
                this, EnemyType::CretaphidEye, NodeRef, _scene);
            std::shared_ptr<Enemy20Entity> eye
                = std::dynamic_pointer_cast<Enemy20Entity>(spawned);
            if (!eye)
            {
                break;
            }
            RequireReference(_scene).AddEntity(eye);
            _eyes[static_cast<std::size_t>(i)] = eye;
            eye->EyeIndex = i;
            eye->BeamColliding = true;
            const std::shared_ptr<Node> node
                = RequireModel(RequireReference(_model))->GetNodeByName(
                    _eyeNodes[static_cast<std::size_t>(i)]);
            const std::int32_t eyeScanId = Values().EyeScanId;
            const std::uint32_t eyeEffectiveness = Values().EyeEffectiveness;
            const std::uint16_t eyeHealth = Values().EyeHealth;
            const Vector3 eyePosition = Position;
            eye->SetUp(node, eyeScanId, eyeEffectiveness, eyeHealth, eyePosition, 1.0F);
        }
    }

    void Enemy19Entity::RespawnEyes()
    {
        for (std::int32_t i = 0; i < _eyeCount; ++i)
        {
            std::shared_ptr<Enemy20Entity> eye = _eyes[static_cast<std::size_t>(i)];
            if (!eye)
            {
                std::shared_ptr<EnemyInstanceEntity> spawned = EnemySpawnEntity::SpawnEnemy(
                    this, EnemyType::CretaphidEye, NodeRef, _scene);
                std::shared_ptr<Enemy20Entity> newEye
                    = std::dynamic_pointer_cast<Enemy20Entity>(spawned);
                if (!newEye)
                {
                    return;
                }
                RequireReference(_scene).AddEntity(newEye);
                eye = newEye;
                _eyes[static_cast<std::size_t>(i)] = eye;
                eye->EyeIndex = i;
                eye->BeamColliding = false;
                const std::shared_ptr<Node> node
                    = RequireModel(RequireReference(_model))->GetNodeByName(
                        _eyeNodes[static_cast<std::size_t>(i)]);
                const std::int32_t eyeScanId = Values().EyeScanId;
                const std::uint32_t eyeEffectiveness = Values().EyeEffectiveness;
                const std::uint16_t eyeHealth = Values().EyeHealth;
                const Vector3 eyePosition = Position;
                eye->SetUp(node, eyeScanId, eyeEffectiveness, eyeHealth, eyePosition, 0.5F);
            }
            else if (_subtype == 0)
            {
                eye->EyeActive = true;
            }
            eye->UpdateState(9);
        }
    }

    void Enemy19Entity::SpawnCrystal()
    {
        std::shared_ptr<EnemyInstanceEntity> spawned = EnemySpawnEntity::SpawnEnemy(
            this, EnemyType::CretaphidCrystal, NodeRef, _scene);
        std::shared_ptr<Enemy21Entity> crystal
            = std::dynamic_pointer_cast<Enemy21Entity>(spawned);
        if (!crystal)
        {
            return;
        }
        RequireReference(_scene).AddEntity(crystal);
        _crystal = crystal;
        const std::shared_ptr<Node> node
            = RequireModel(RequireReference(_model))->GetNodeByName("Crystal_joint");
        const std::uint16_t crystalScanId = Values().CrystalScanId;
        const std::uint32_t crystalEffectiveness = Values().CrystalEffectiveness;
        const std::uint16_t crystalHealth = Values().CrystalHealth;
        const Vector3 crystalPosition = Position;
        crystal->SetUp(
            node, crystalScanId, crystalEffectiveness, crystalHealth, crystalPosition);
    }

    void Enemy19Entity::SetPhase0() noexcept
    {
        _phaseIndex = 0;
        _eyeStartIndex = 0;
        _eyeEndIndex = 2;
    }

    void Enemy19Entity::SetPhase1() noexcept
    {
        _phaseIndex = 1;
        _eyeStartIndex = 3;
        _eyeEndIndex = 6;
    }

    void Enemy19Entity::SetPhase2() noexcept
    {
        _phaseIndex = 2;
        _eyeStartIndex = 7;
        _eyeEndIndex = 11;
    }

    void Enemy19Entity::EnemyProcess()
    {
        if (_parentEntCol)
        {
            Transform = Multiply(_invTransform, _parentEntCol->Transform);
        }

        for (std::int32_t i = 0; i < 3; ++i)
        {
            SegmentInfo& segment = RequireReference(
                Segments[static_cast<std::size_t>(i)]);
            segment.Angle += static_cast<float>(segment.SpinDirection) * segment.AngleStep;
            if (segment.Angle >= 360.0F)
            {
                segment.Angle -= 360.0F;
            }
            else if (segment.Angle < 0.0F)
            {
                segment.Angle += 360.0F;
            }
            if (segment.InvertBeamRotation)
            {
                if (segment.BeamAngle >= segment.BeamAngleMax)
                {
                    segment.InvertBeamRotation = false;
                }
                else
                {
                    segment.BeamAngle += segment.BeamAngleStep;
                }
            }
            else if (segment.BeamAngle <= segment.BeamAngleMin)
            {
                segment.InvertBeamRotation = true;
            }
            else
            {
                segment.BeamAngle -= segment.BeamAngleStep;
            }
        }

        _eyeBurnUpdateTimer -= RequireReference(_scene).FrameTime();
        if (_eyeBurnUpdateTimer <= 0.0F)
        {
            ++_eyeBurnIndex;
            if (_eyeBurnIndex > _eyeEndIndex)
            {
                _eyeBurnIndex = _eyeStartIndex;
            }
            if (_eyeBurnIndex < 0
                || static_cast<std::size_t>(_eyeBurnIndex) >= _eyes.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            const std::shared_ptr<Enemy20Entity>& eye
                = _eyes[static_cast<std::size_t>(_eyeBurnIndex)];
            if (eye)
            {
                eye->SpawnBurn = true;
            }
            _eyeBurnUpdateTimer = 1.0F / 30.0F;
        }

        const std::int32_t slotIndex = MainPlayer().SlotIndex();
        if (slotIndex < 0 || static_cast<std::size_t>(slotIndex) >= HitPlayers.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        if (HitPlayers[static_cast<std::size_t>(slotIndex)])
        {
            MainPlayer().TakeDamage(10, DamageFlags::None, FacingVector(), this);
        }

        CallStateProcess();

        if (_state1 == 3 || _state1 == 16 || _state1 == 26)
        {
            if (_flashTimer > 0)
            {
                --_flashTimer;
                if (_flashTimer == 0)
                {
                    _flashTimer = _flashPeriod;
                }
            }
        }
    }

    void Enemy19Entity::Sub2135F54()
    {
        if (_phaseIndex == 0)
        {
            for (std::int32_t i = 0; i < _eyeCount; ++i)
            {
                Enemy20Entity* eye = _eyes[static_cast<std::size_t>(i)].get();
                assert(eye != nullptr);
                RequireReference(eye).UpdateState(ArrayAt(Values().Phase0EyeState, i));
                eye->BeamType = ArrayAt(Values().Phase0BeamType, i);
                const std::uint8_t max = ArrayAt(Values().Phase0BeamSpawnMax, i);
                const std::uint8_t minForRange
                    = ArrayAt(Values().Phase0BeamSpawnMin, i);
                const std::uint32_t random = Rng::GetRandomInt2(
                    static_cast<std::int32_t>(max) + 1
                    - static_cast<std::int32_t>(minForRange));
                const std::uint8_t minForCount
                    = ArrayAt(Values().Phase0BeamSpawnMin, i);
                eye->BeamSpawnCount = static_cast<std::uint16_t>(
                    static_cast<std::uint32_t>(minForCount) + random);
                eye->BeamSpawnCooldown
                    = static_cast<std::int32_t>(
                        ArrayAt(Values().Phase0BeamCooldown, i)) * 2;
                eye->BeamSpawnTimer = eye->BeamSpawnCooldown;
            }
        }
        else if (_phaseIndex == 1)
        {
            for (std::int32_t i = 0; i < _eyeCount; ++i)
            {
                Enemy20Entity* eye = _eyes[static_cast<std::size_t>(i)].get();
                assert(eye != nullptr);
                RequireReference(eye).UpdateState(ArrayAt(Values().Phase1EyeState, i));
                eye->BeamType = ArrayAt(Values().Phase1BeamType, i);
                const std::uint8_t max = ArrayAt(Values().Phase1BeamSpawnMax, i);
                const std::uint8_t minForRange
                    = ArrayAt(Values().Phase1BeamSpawnMin, i);
                const std::uint32_t random = Rng::GetRandomInt2(
                    static_cast<std::int32_t>(max) + 1
                    - static_cast<std::int32_t>(minForRange));
                const std::uint8_t minForCount
                    = ArrayAt(Values().Phase1BeamSpawnMin, i);
                eye->BeamSpawnCount = static_cast<std::uint16_t>(
                    static_cast<std::uint32_t>(minForCount) + random);
                eye->BeamSpawnCooldown
                    = static_cast<std::int32_t>(
                        ArrayAt(Values().Phase1BeamCooldown, i)) * 2;
                eye->BeamSpawnTimer = eye->BeamSpawnCooldown;
            }
        }
        else if (_phaseIndex == 2)
        {
            for (std::int32_t i = 0; i < _eyeCount; ++i)
            {
                Enemy20Entity* eye = _eyes[static_cast<std::size_t>(i)].get();
                assert(eye != nullptr);
                RequireReference(eye).UpdateState(ArrayAt(Values().Phase2EyeState, i));
                eye->BeamType = ArrayAt(Values().Phase2BeamType, i);
                const std::uint8_t max = ArrayAt(Values().Phase2BeamSpawnMax, i);
                const std::uint8_t minForRange
                    = ArrayAt(Values().Phase2BeamSpawnMin, i);
                const std::uint32_t random = Rng::GetRandomInt2(
                    static_cast<std::int32_t>(max) + 1
                    - static_cast<std::int32_t>(minForRange));
                const std::uint8_t minForCount
                    = ArrayAt(Values().Phase2BeamSpawnMin, i);
                eye->BeamSpawnCount = static_cast<std::uint16_t>(
                    static_cast<std::uint32_t>(minForCount) + random);
                eye->BeamSpawnCooldown
                    = static_cast<std::int32_t>(
                        ArrayAt(Values().Phase2BeamCooldown, i)) * 2;
                eye->BeamSpawnTimer = eye->BeamSpawnCooldown;
            }
        }
    }

    void Enemy19Entity::Sub213619C(Enemy20Entity* eye)
    {
        Enemy20Entity& eyeRef = RequireReference(eye);
        const std::int32_t index = eyeRef.EyeIndex;
        if (_phaseIndex == 0)
        {
            const std::uint8_t max = ArrayAt(Values().Phase0BeamSpawnMax, index);
            const std::uint8_t minForRange
                = ArrayAt(Values().Phase0BeamSpawnMin, index);
            const std::uint32_t random = Rng::GetRandomInt2(
                static_cast<std::int32_t>(max) + 1
                - static_cast<std::int32_t>(minForRange));
            const std::uint8_t minForCount
                = ArrayAt(Values().Phase0BeamSpawnMin, index);
            eyeRef.BeamSpawnCount = static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(minForCount) + random);
        }
        else if (_phaseIndex == 1)
        {
            const std::uint8_t max = ArrayAt(Values().Phase1BeamSpawnMax, index);
            const std::uint8_t minForRange
                = ArrayAt(Values().Phase1BeamSpawnMin, index);
            const std::uint32_t random = Rng::GetRandomInt2(
                static_cast<std::int32_t>(max) + 1
                - static_cast<std::int32_t>(minForRange));
            const std::uint8_t minForCount
                = ArrayAt(Values().Phase1BeamSpawnMin, index);
            eyeRef.BeamSpawnCount = static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(minForCount) + random);
        }
        else if (_phaseIndex == 2)
        {
            const std::uint8_t max = ArrayAt(Values().Phase2BeamSpawnMax, index);
            const std::uint8_t minForRange
                = ArrayAt(Values().Phase2BeamSpawnMin, index);
            const std::uint32_t random = Rng::GetRandomInt2(
                static_cast<std::int32_t>(max) + 1
                - static_cast<std::int32_t>(minForRange));
            const std::uint8_t minForCount
                = ArrayAt(Values().Phase2BeamSpawnMin, index);
            eyeRef.BeamSpawnCount = static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(minForCount) + random);
        }
    }

    void Enemy19Entity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::Destroyed)
        {
            EnemyInstanceEntity* enemy = dynamic_cast<EnemyInstanceEntity*>(info.Sender);
            if (enemy == nullptr)
            {
                return;
            }
            if (enemy->EnemyType() == EnemyType::CretaphidEye)
            {
                Enemy20Entity* eye = dynamic_cast<Enemy20Entity*>(enemy);
                if (eye == nullptr)
                {
                    throw SceneDetail::InvalidCastException();
                }
                const std::int32_t eyeIndex = eye->EyeIndex;
                if (eyeIndex < 0 || static_cast<std::size_t>(eyeIndex) >= _eyes.size())
                {
                    throw SceneDetail::IndexOutOfRangeException();
                }
                _eyes[static_cast<std::size_t>(eyeIndex)].reset();
            }
            else if (enemy->EnemyType() == EnemyType::CretaphidCrystal)
            {
                _crystal.reset();
            }
        }
        else if (info.Message == Message::SetActive)
        {
            assert(UnboxInt32(info.Param1) == 0);
            EnemyInstanceEntity* enemy = dynamic_cast<EnemyInstanceEntity*>(info.Sender);
            if (enemy != nullptr
                && enemy->EnemyType() == EnemyType::CretaphidCrystal
                && _state1 != 26)
            {
                assert(_crystal != nullptr);
                _state2 = 26;
                _subId = _state2;
                _crystalDownTimer = 35 * 2;
                RequireReference(_model).SetAnimation(0);
                RequireReference(_scene).SpawnEffect(
                    74,
                    Vector3(1.0F, 0.0F, 0.0F),
                    Vector3(0.0F, 1.0F, 0.0F),
                    RequireReference(_crystal).Position);
                _soundSource.StopAllSfx();
                _soundSource.PlaySfx(SfxId::CYLINDER_BOSS_DIE);
                _soundSource.PlaySfx(SfxId::CYLINDER_BOSS_CRYSTAL_SCR);
            }
            if (MainPlayer().Health() > 0 && GameState::SinglePlayer())
            {
                if (_subtype < 0
                    || static_cast<std::size_t>(_subtype) >= _deathMovieIds.size())
                {
                    throw SceneDetail::IndexOutOfRangeException();
                }
                RequireReference(_scene).StartMovie(
                    _deathMovieIds[static_cast<std::size_t>(_subtype)],
                    FadeType::FadeOutInWhite,
                    40.0F / 30.0F,
                    FadeType::FadeOutInWhite,
                    5.0F / 30.0F);
            }
        }
    }

    void Enemy19Entity::State0()
    {
        (void)CallSubroutine<Enemy19Entity>(Metadata::Enemy19Subroutines, this);
    }

    bool Enemy19Entity::Behavior00()
    {
        if (!AnimationEnded(RequireReference(_model)))
        {
            return false;
        }
        RequireReference(_model).SetAnimation(2);
        RespawnEyes();
        Enemy21Entity* crystal = _crystal.get();
        assert(crystal != nullptr);
        if (_phaseIndex == 0)
        {
            RequireReference(crystal).SetHealth(static_cast<std::uint16_t>(
                GetPhaseValue(PhaseValue::CrystalHealth)));
            SetPhase1();
        }
        else if (_phaseIndex == 1)
        {
            RequireReference(crystal).SetHealth(static_cast<std::uint16_t>(
                GetPhaseValue(PhaseValue::CrystalHealth)));
            SetPhase2();
        }
        Sub2135F54();
        _crystalShotTimer = GetPhaseValue(PhaseValue::CrystalShotTime);
        _crystalShotDelay = GetPhaseValue(PhaseValue::CrystalShotDelay);
        _crystalUpTimer = GetPhaseValue(PhaseValue::CrystalUpTime);
        return true;
    }

    bool Enemy19Entity::Behavior01()
    {
        if (_crystalDownTimer > 0)
        {
            --_crystalDownTimer;
            return false;
        }
        assert(_crystal != nullptr);
        RequireReference(_crystal).SetHealth(0);
        return true;
    }

    bool Enemy19Entity::Behavior02()
    {
        bool allEyesDestroyed = true;
        if (_subtype == 0)
        {
            for (std::int32_t i = 0; i < _eyeCount; ++i)
            {
                if (_eyes[static_cast<std::size_t>(i)]
                    && (i < _eyeStartIndex || i > _eyeEndIndex))
                {
                    allEyesDestroyed = false;
                    break;
                }
            }
        }
        else
        {
            for (std::int32_t i = 0; i < _eyeCount; ++i)
            {
                if (_eyes[static_cast<std::size_t>(i)])
                {
                    allEyesDestroyed = false;
                    break;
                }
            }
        }
        if (allEyesDestroyed)
        {
            if (_subtype == 0)
            {
                for (std::int32_t i = _eyeStartIndex; i <= _eyeEndIndex; ++i)
                {
                    Enemy20Entity* eye = _eyes[static_cast<std::size_t>(i)].get();
                    assert(eye != nullptr);
                    RequireReference(eye).EyeActive = false;
                    eye->UpdateState(5);
                }
            }
            _soundSource.PlaySfx(SfxId::CYLINDER_BOSS_CRYSTAL_UP);
            RequireReference(_model).SetAnimation(4, AnimFlags::NoLoop);
            assert(_crystal != nullptr);
            RequireReference(_crystal).Flags &= ~EnemyFlags::Invincible;
            return true;
        }
        return false;
    }

    bool Enemy19Entity::Behavior03()
    {
        if (_crystalDownTimer > 0)
        {
            --_crystalDownTimer;
            return false;
        }
        RequireReference(_model).SetAnimation(1, AnimFlags::NoLoop);
        _crystalDownTimer = static_cast<std::int32_t>(Values().PhaseFlashTime) * 2;
        _flashTimer = _flashPeriod;
        return true;
    }

    bool Enemy19Entity::Behavior04()
    {
        if (!AnimationEnded(RequireReference(_model)))
        {
            return false;
        }
        RequireReference(_model).SetAnimation(3);
        if (_crystal)
        {
            RequireReference(_scene).SpawnEffect(
                65,
                Vector3(1.0F, 0.0F, 0.0F),
                Vector3(0.0F, 1.0F, 0.0F),
                _crystal->Position);
        }
        return true;
    }

    bool Enemy19Entity::Behavior05()
    {
        return DistanceSquared(MainPlayer().Position, Position)
            < Fixed::ToFloat(610352);
    }

    bool Enemy19Entity::Behavior06()
    {
        if (!AnimationEnded(RequireReference(_model)))
        {
            return false;
        }
        RequireReference(_model).SetAnimation(2);
        RespawnEyes();
        if (_phaseIndex == 0)
        {
            SetPhase0();
        }
        else if (_phaseIndex == 1)
        {
            SetPhase1();
        }
        else if (_phaseIndex == 2)
        {
            SetPhase2();
        }
        Sub2135F54();
        _crystalShotTimer = GetPhaseValue(PhaseValue::CrystalShotTime);
        _crystalShotDelay = GetPhaseValue(PhaseValue::CrystalShotDelay);
        _crystalUpTimer = GetPhaseValue(PhaseValue::CrystalUpTime);
        return true;
    }

    bool Enemy19Entity::Behavior07()
    {
        return true;
    }

    bool Enemy19Entity::Behavior08()
    {
        return true;
    }

    void Enemy19Entity::KillEyes()
    {
        for (std::int32_t i = 0; i < _eyeCount; ++i)
        {
            const std::shared_ptr<Enemy20Entity>& eye
                = _eyes[static_cast<std::size_t>(i)];
            if (eye)
            {
                eye->SetHealth(0);
            }
        }
    }

    bool Enemy19Entity::Behavior09()
    {
        if (_phaseIndex == 2 && !_crystal)
        {
            Flags &= ~EnemyFlags::Invincible;
            _health = 0;
            KillEyes();
            return true;
        }
        assert(_crystal != nullptr);
        Enemy21Entity& crystal = RequireReference(_crystal);
        if (crystal.Health() > GetPhaseValue(PhaseValue::CrystalHealth))
        {
            return false;
        }
        if (_phaseIndex == 2)
        {
            RequireReference(_scene).SpawnEffect(
                74,
                Vector3(1.0F, 0.0F, 0.0F),
                Vector3(0.0F, 1.0F, 0.0F),
                crystal.Position);
            Flags &= ~EnemyFlags::Invincible;
            _health = 0;
            KillEyes();
        }
        else
        {
            _soundSource.PlaySfx(SfxId::CYLINDER_BOSS_CRYSTAL_DOWN);
            _soundSource.PlaySfx(SfxId::CYLINDER_BOSS_CRYSTAL_SCR);
            RequireReference(_model).SetAnimation(0);
            const std::int32_t effectId = _phaseIndex == 1 ? 73 : 66;
            RequireReference(_scene).SpawnEffect(
                effectId,
                Vector3(1.0F, 0.0F, 0.0F),
                Vector3(0.0F, 1.0F, 0.0F),
                crystal.Position);
            crystal.Flags |= EnemyFlags::Invincible;
        }
        return true;
    }

    bool Enemy19Entity::Behavior10()
    {
        if (_crystalUpTimer > 0)
        {
            --_crystalUpTimer;
            return false;
        }
        _soundSource.PlaySfx(SfxId::CYLINDER_BOSS_CRYSTAL_DOWN);
        RequireReference(_model).SetAnimation(1, AnimFlags::NoLoop);
        assert(_crystal != nullptr);
        RequireReference(_crystal).Flags |= EnemyFlags::Invincible;
        return true;
    }

    bool Enemy19Entity::Behavior11()
    {
        if (_crystalShotDelay > 0)
        {
            --_crystalShotDelay;
            return false;
        }
        _crystalShotDelay = GetPhaseValue(PhaseValue::CrystalShotDelay);
        return true;
    }

    bool Enemy19Entity::Behavior12()
    {
        if (_crystalShotTimer > 0)
        {
            --_crystalShotTimer;
            return false;
        }
        _crystalShotTimer = GetPhaseValue(PhaseValue::CrystalShotTime);
        assert(_crystal != nullptr);
        Enemy21Entity& crystal = RequireReference(_crystal);
        RequireReference(_scene).SpawnEffect(
            67,
            Vector3(1.0F, 0.0F, 0.0F),
            Vector3(0.0F, 1.0F, 0.0F),
            crystal.Position);
        crystal.SpawnBeam(ArrayAt(Values().CrystalBeamDamage, _phaseIndex));
        _soundSource.PlaySfx(SfxId::CYLINDER_BOSS_ATTACK2);
        return true;
    }

    void Enemy19Entity::UpdateTransforms(bool rootPosition)
    {
        for (std::int32_t i = 0; i < 3; ++i)
        {
            SegmentInfo& segment = RequireReference(
                Segments[static_cast<std::size_t>(i)]);
            const float angle = DegreesToRadians(segment.Angle);
            RequireReference(segment.JointNode).AfterTransform = CreateRotationX(angle);
        }
        RequireModel(RequireReference(_model))->AnimateNodes2(
            0, false, IdentityMatrix(), Vector3(1.0F, 1.0F, 1.0F),
            RequireReference(_model).AnimInfo);
        if (rootPosition)
        {
            const Matrix4 transform = CreateTranslation(Position);
            std::shared_ptr<Model> rootModel = RequireModel(RequireReference(_model));
            if (!rootModel->Nodes)
            {
                throw System::NullReferenceException();
            }
            for (std::size_t i = 0; i < rootModel->Nodes->size(); ++i)
            {
                Node& node = RequireReference((*rootModel->Nodes)[i]);
                node.Animation = Multiply(node.Animation, transform);
            }
        }
    }

    void Enemy19Entity::ResetTransforms()
    {
        for (std::int32_t i = 0; i < 3; ++i)
        {
            SegmentInfo& segment = RequireReference(
                Segments[static_cast<std::size_t>(i)]);
            RequireReference(segment.JointNode).AfterTransform.reset();
        }
    }

    bool Enemy19Entity::EnemyGetDrawInfo()
    {
        if (_health == 0
            || (static_cast<EnemyFlags>(Flags) & EnemyFlags::Visible) == EnemyFlags{})
        {
            return true;
        }
        if (_flashTimer < _flashLength
            && (_state1 == 7 || _state1 == 16 || _state1 == 26))
        {
            SetPaletteOverride(Metadata::WhitePalette);
        }
        UpdateTransforms(true);
        RequireModel(RequireReference(_model))->UpdateMatrixStack();
        UpdateMaterials(RequireReference(_model), Recolor());
        GetDrawItems(RequireReference(_model), 0);
        ResetTransforms();
        SetPaletteOverride(std::nullopt);
        return true;
    }

    bool Enemy19Entity::Behavior00(Enemy19Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy19Entity::Behavior01(Enemy19Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy19Entity::Behavior02(Enemy19Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy19Entity::Behavior03(Enemy19Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }

    bool Enemy19Entity::Behavior04(Enemy19Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior04();
    }

    bool Enemy19Entity::Behavior05(Enemy19Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior05();
    }

    bool Enemy19Entity::Behavior06(Enemy19Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior06();
    }

    bool Enemy19Entity::Behavior07(Enemy19Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior07();
    }

    bool Enemy19Entity::Behavior08(Enemy19Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior08();
    }

    bool Enemy19Entity::Behavior09(Enemy19Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior09();
    }

    bool Enemy19Entity::Behavior10(Enemy19Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior10();
    }

    bool Enemy19Entity::Behavior11(Enemy19Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior11();
    }

    bool Enemy19Entity::Behavior12(Enemy19Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior12();
    }
}
