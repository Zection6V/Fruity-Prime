#include "24_Gorea1A.hpp"

#include "25_GoreaHead.hpp"
#include "26_GoreaArm.hpp"
#include "27_GoreaLeg.hpp"
#include "28_Gorea1B.hpp"
#include "29_GoreaSealSphere1.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Scene.hpp"
#include "../../Sound/Music.hpp"
#include "../../Utility/Rng.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/HalfturretEntity.hpp"
#include "../ItemInstanceEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::RequireReference;
using ::OpenTK::Mathematics::CreateRotationY;
using ::OpenTK::Mathematics::CreateScale;
using ::OpenTK::Mathematics::Equal;
using ::OpenTK::Mathematics::IdentityMatrix;
using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;
using ::OpenTK::Mathematics::ScaleVector;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;
        using OpenTK::Mathematics::Vector4;

        [[nodiscard]] EnemySpawnEntity* CastSpawner(EntityBase* spawner) noexcept
        {
            EnemySpawnEntity* typedSpawner = dynamic_cast<EnemySpawnEntity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
        }

        [[nodiscard]] Enemy24Entity& RequireEnemy(Enemy24Entity* enemy)
        {
            return RequireReference(enemy);
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

        template <typename T, std::size_t N>
        [[nodiscard]] T& ArrayAt(std::array<T, N>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= N)
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }

        template <typename T, std::size_t N>
        [[nodiscard]] const T& ArrayAt(const std::array<T, N>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= N)
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
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

        [[nodiscard]] Vector3 DivideVector(Vector3 value, float divisor) noexcept
        {
            return Vector3(value.X / divisor, value.Y / divisor, value.Z / divisor);
        }

        [[nodiscard]] std::int32_t AddInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t sum
                = std::bit_cast<std::uint32_t>(left) + std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(sum);
        }

        [[nodiscard]] std::int32_t MulInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t product
                = std::bit_cast<std::uint32_t>(left) * std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(product);
        }

        [[nodiscard]] std::int32_t SubInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t difference
                = std::bit_cast<std::uint32_t>(left) - std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(difference);
        }

        [[nodiscard]] std::int32_t DecrementInt32Unchecked(std::int32_t value) noexcept
        {
            return SubInt32Unchecked(value, 1);
        }

        [[nodiscard]] std::int32_t RoundToInt32ToEven(float value) noexcept
        {
            if (!std::isfinite(value))
            {
                return value > 0.0F
                    ? std::numeric_limits<std::int32_t>::max()
                    : std::numeric_limits<std::int32_t>::min();
            }
            const float floorValue = std::floor(value);
            const float fraction = value - floorValue;
            double rounded = floorValue;
            if (fraction > 0.5F)
            {
                rounded = static_cast<double>(floorValue) + 1.0;
            }
            else if (fraction == 0.5F)
            {
                const auto floorInteger = static_cast<std::int64_t>(floorValue);
                rounded = (floorInteger & 1LL) == 0
                    ? floorValue
                    : static_cast<double>(floorValue) + 1.0;
            }
            if (rounded >= static_cast<double>(std::numeric_limits<std::int32_t>::max()))
            {
                return std::numeric_limits<std::int32_t>::max();
            }
            if (rounded <= static_cast<double>(std::numeric_limits<std::int32_t>::min()))
            {
                return std::numeric_limits<std::int32_t>::min();
            }
            return static_cast<std::int32_t>(rounded);
        }

        [[nodiscard]] std::uint8_t RoundToByteUnchecked(float value) noexcept
        {
            return static_cast<std::uint8_t>(RoundToInt32ToEven(value));
        }

        [[nodiscard]] std::uint8_t IntToByteUnchecked(std::int32_t value) noexcept
        {
            return static_cast<std::uint8_t>(static_cast<std::uint32_t>(value));
        }

        [[nodiscard]] std::uint16_t IntToUInt16Unchecked(std::int32_t value) noexcept
        {
            return static_cast<std::uint16_t>(static_cast<std::uint32_t>(value));
        }

        [[nodiscard]] bool ContainsNode(
            const std::shared_ptr<const std::vector<std::shared_ptr<Node>>>& nodes,
            Node* node)
        {
            if (!nodes)
            {
                throw System::NullReferenceException();
            }
            return std::any_of(nodes->begin(), nodes->end(),
                [node](const std::shared_ptr<Node>& current)
                {
                    return current.get() == node;
                });
        }
    }

    const std::array<std::int32_t, 6> Enemy24Entity::_chargeEffects{
        48, 46, 49, 47, 50, 41
    };

    const std::array<std::int32_t, 6> Enemy24Entity::_shotEffects{
        54, 51, 55, 53, 56, 52
    };

    const std::array<BeamType, 6> Enemy24Entity::_beamTypes{
        BeamType::Battlehammer,
        BeamType::VoltDriver,
        BeamType::Magmaul,
        BeamType::Judicator,
        BeamType::Imperialist,
        BeamType::PowerBeam
    };

    const std::array<std::int32_t, 6> Enemy24Entity::_weaponAnimIds{
        11, 3, 7, 4, 12, 14
    };

    const std::array<std::int32_t, 6> Enemy24Entity::_chargeChances{
        40, 100, 40, 30, 100, 100
    };

    const std::array<float, 3> Enemy24Entity::_speedFactors{
        Fixed::ToFloat(341), Fixed::ToFloat(426), Fixed::ToFloat(568)
    };

    GoreaEnemyEntityBase::GoreaEnemyEntityBase(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene)
    {
    }

    void GoreaEnemyEntityBase::InitializeCommon(EnemySpawnEntity* spawner)
    {
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::NoHomingNc;
        Flags |= EnemyFlags::NoHomingCo;
        Flags |= EnemyFlags::NoMaxDistance;
        SetHealthbarMessageId(3);
        EnemySpawnEntity& spawnerRef = RequireReference(spawner);
        SetTransform(spawnerRef.FacingVector(), Vector3(0.0F, 1.0F, 0.0F), spawnerRef.Position);
        _prevPos = Position;
        _boundingRadius = 1.0F;
        _healthMax = _health = std::numeric_limits<std::uint16_t>::max();
    }

    bool GoreaEnemyEntityBase::AnimationEnded() const
    {
        const ModelInstance& model = RequireReference(_model);
        return TypeExtensions::TestFlag((*model.AnimInfo->Flags)[0], AnimFlags::Ended);
    }

    void GoreaEnemyEntityBase::SpawnEffect(std::int32_t effectId, Vector3 position)
    {
        SpawnEffect(effectId, position,
            Vector3(1.0F, 0.0F, 0.0F), Vector3(0.0F, 1.0F, 0.0F));
    }

    void GoreaEnemyEntityBase::SpawnEffect(
        std::int32_t effectId, Vector3 position, Vector3 facing, Vector3 up)
    {
        RequireReference(_scene).SpawnEffect(effectId, facing, up, position);
    }

    std::shared_ptr<Effects::EffectEntry> GoreaEnemyEntityBase::SpawnEffectGetEntry(
        std::int32_t effectId, Vector3 position, bool extensionFlag)
    {
        return SpawnEffectGetEntry(effectId, position,
            Vector3(1.0F, 0.0F, 0.0F), Vector3(0.0F, 1.0F, 0.0F), extensionFlag);
    }

    std::shared_ptr<Effects::EffectEntry> GoreaEnemyEntityBase::SpawnEffectGetEntry(
        std::int32_t effectId, Vector3 position, Vector3 facing, Vector3 up, bool extensionFlag)
    {
        Scene& scene = RequireReference(_scene);
        std::shared_ptr<Effects::EffectEntry> effect
            = scene.SpawnEffectGetEntry(effectId, facing, up, position);
        if (effect)
        {
            effect->ResetElements(scene.ElapsedTime());
            effect->SetElementExtension(extensionFlag);
        }
        return effect;
    }

    Matrix4 GoreaEnemyEntityBase::GetNodeTransform(
        GoreaEnemyEntityBase* entity, Node* node)
    {
        GoreaEnemyEntityBase& entityRef = RequireReference(entity);
        Scene& scene = RequireReference(_scene);
        if (entityRef._lastNodeTransformUpdate != scene.FrameCount())
        {
            entityRef._lastNodeTransformUpdate = scene.FrameCount();
            std::shared_ptr<ModelInstance> model;
            const std::vector<std::shared_ptr<ModelInstance>>& models = entityRef.GetModels();
            if (!models.empty())
            {
                model = models.front();
            }
            assert(model != nullptr
                && model->Model() != nullptr
                && ContainsNode(model->Model()->Nodes, node));
            ModelInstance& modelRef = RequireReference(model);
            std::shared_ptr<Model> modelData = modelRef.Model();
            Model& modelDataRef = RequireReference(modelData);
            const Matrix4 transform = entityRef.GetModelTransform(modelRef, 0);
            modelDataRef.AnimateNodes(
                0, false, transform, modelDataRef.Scale, modelRef.AnimInfo);
        }
        return RequireReference(node).Animation;
    }

    void GoreaEnemyEntityBase::TransformHurtVolumeToNode(Node* node, Vector3 offset)
    {
        ModelInstance& model = RequireReference(_model);
        std::shared_ptr<Model> modelData = model.Model();
        Model& modelDataRef = RequireReference(modelData);
        assert(ContainsNode(modelDataRef.Nodes, node));
        modelDataRef.AnimateNodes(
            0, false, IdentityMatrix(), modelDataRef.Scale, model.AnimInfo);
        const Matrix4 transform = GetTransformMatrix(FacingVector(), UpVector());
        const Matrix4 finalTransform
            = RequireReference(node).Animation * CreateScale(static_cast<Vector3>(Scale)) * transform;
        const Vector3 position = Matrix::Vec3MultMtx4(offset, finalTransform);
        _hurtVolumeInit = CollisionVolume(position, _hurtVolumeInit.SphereRadius);
    }

    bool GoreaEnemyEntityBase::SeekTargetFacing(Vector3 target, float angle)
    {
        angle = DegreesToRadians(angle / 2.0F);
        const float sine = std::sin(angle);
        const float cosine = std::cos(angle);
        (void)sine;
        Vector3 facing = FacingVector();
        const float dot = Vector3::Dot(target, facing);
        if (std::fabs(cosine - dot) > Fixed::ToFloat(7))
        {
            const Vector3 cross = Vector3::Cross(target, facing);
            const Matrix4 rotY = CreateRotationY(angle * (cross.Y > 0.0F ? -1.0F : 1.0F));
            facing = Matrix::Vec3MultMtx4(facing, rotY).Normalized();
            SetTransform(facing, UpVector(), Position);
            return false;
        }
        SetTransform(target.Normalized(), UpVector(), Position);
        return true;
    }

    Vector3 GoreaEnemyEntityBase::SeekTargetSetAnim(Vector3 target, std::int32_t index)
    {
        return SeekTargetSetAnim(target, index, 0, SetFlags::Unused, true);
    }

    Vector3 GoreaEnemyEntityBase::SeekTargetSetAnim(
        Vector3 target, std::int32_t index, std::int32_t slot, SetFlags setFlags)
    {
        return SeekTargetSetAnim(target, index, slot, setFlags, true);
    }

    Vector3 GoreaEnemyEntityBase::SeekTargetSetAnim(
        Vector3 target, std::int32_t index, std::int32_t slot, SetFlags setFlags, bool useSlot)
    {
        if (!Equal(target, Vector3::Zero) && SeekTargetFacing(target, 3.0F))
        {
            target = Vector3::Zero;
            if (useSlot)
            {
                EnsureAnimation(index, slot, setFlags);
            }
            else
            {
                EnsureAnimation(index);
            }
        }
        if (Equal(target, Vector3::Zero))
        {
            Vector3 between = TypeExtensions::WithY(
                static_cast<Vector3>(MainPlayer().Position) - static_cast<Vector3>(Position), 0.0F);
            if (LengthSquared(between) > 1.0F / 128.0F)
            {
                between = between.Normalized();
                if (Vector3::Dot(between, FacingVector()) < Fixed::ToFloat(3547))
                {
                    target = between;
                    if (useSlot)
                    {
                        EnsureAnimation(index, slot, setFlags);
                    }
                    else
                    {
                        EnsureAnimation(index);
                    }
                }
            }
        }
        return target;
    }

    bool GoreaEnemyEntityBase::CheckFacingAngle(float minCos, Vector3 position)
    {
        Vector3 facing = TypeExtensions::WithY(FacingVector(), 0.0F);
        if (LengthSquared(facing) > 1.0F / 128.0F)
        {
            facing = facing.Normalized();
            Vector3 between = TypeExtensions::WithY(
                position - static_cast<Vector3>(Position), 0.0F);
            if (LengthSquared(between) > 1.0F / 128.0F)
            {
                between = between.Normalized();
                if (Vector3::Dot(between, facing) > minCos)
                {
                    return true;
                }
            }
        }
        return false;
    }

    void GoreaEnemyEntityBase::EnsureAnimation(std::int32_t index, AnimFlags animFlags)
    {
        ModelInstance& model = RequireReference(_model);
        if ((*model.AnimInfo->Index)[0] != index)
        {
            model.SetAnimation(index, animFlags);
        }
    }

    void GoreaEnemyEntityBase::EnsureAnimation(
        std::int32_t index, std::int32_t slot, SetFlags setFlags, AnimFlags animFlags)
    {
        ModelInstance& model = RequireReference(_model);
        if ((*model.AnimInfo->Index)[static_cast<std::size_t>(slot)] != index)
        {
            model.SetAnimation(index, slot, setFlags, animFlags);
        }
    }

    bool GoreaEnemyEntityBase::IsAtEndFrame() const
    {
        const Scene& scene = RequireReference(_scene);
        return scene.FrameCount() > 0 && scene.FrameCount() % 2 == 0
            && (*RequireReference(_model).AnimInfo->Frame)[0]
                >= (*RequireReference(_model).AnimInfo->FrameCount)[0] - 1;
    }

    void GoreaEnemyEntityBase::IncrementMaterialColors(
        Material* material, ColorRgb ambient, ColorRgb diffuse,
        std::int32_t frame, std::int32_t frameCount)
    {
        Material& materialRef = RequireReference(material);
        const ColorRgb oldAmbient = materialRef.Ambient;
        materialRef.Ambient = ColorRgb(
            IntToByteUnchecked(static_cast<std::int32_t>(oldAmbient.Red)
                + static_cast<std::int32_t>(InterpolateColor(
                    frame, frameCount,
                    static_cast<std::int32_t>(ambient.Red) - static_cast<std::int32_t>(oldAmbient.Red)))),
            IntToByteUnchecked(static_cast<std::int32_t>(oldAmbient.Green)
                + static_cast<std::int32_t>(InterpolateColor(
                    frame, frameCount,
                    static_cast<std::int32_t>(ambient.Green) - static_cast<std::int32_t>(oldAmbient.Green)))),
            IntToByteUnchecked(static_cast<std::int32_t>(oldAmbient.Blue)
                + static_cast<std::int32_t>(InterpolateColor(
                    frame, frameCount,
                    static_cast<std::int32_t>(ambient.Blue) - static_cast<std::int32_t>(oldAmbient.Blue)))));
        const ColorRgb oldDiffuse = materialRef.Diffuse;
        materialRef.Diffuse = ColorRgb(
            IntToByteUnchecked(static_cast<std::int32_t>(oldDiffuse.Red)
                + static_cast<std::int32_t>(InterpolateColor(
                    frame, frameCount,
                    static_cast<std::int32_t>(diffuse.Red) - static_cast<std::int32_t>(oldDiffuse.Red)))),
            IntToByteUnchecked(static_cast<std::int32_t>(oldDiffuse.Green)
                + static_cast<std::int32_t>(InterpolateColor(
                    frame, frameCount,
                    static_cast<std::int32_t>(diffuse.Green) - static_cast<std::int32_t>(oldDiffuse.Green)))),
            IntToByteUnchecked(static_cast<std::int32_t>(oldDiffuse.Blue)
                + static_cast<std::int32_t>(InterpolateColor(
                    frame, frameCount,
                    static_cast<std::int32_t>(diffuse.Blue) - static_cast<std::int32_t>(oldDiffuse.Blue)))));
    }

    std::uint8_t GoreaEnemyEntityBase::InterpolateColor(
        std::int32_t frame, std::int32_t frameCount, std::int32_t color) const
    {
        const std::int32_t diff = SubInt32Unchecked(frameCount, frame);
        if (frame >= 0 && frame <= frameCount && diff != 0)
        {
            return RoundToByteUnchecked(
                static_cast<float>(color) / static_cast<float>(diff));
        }
        return IntToByteUnchecked(color);
    }

    LightInfo GoreaEnemyEntityBase::GetLightInfo()
    {
        if (_lightOverride)
        {
            Scene& scene = RequireReference(_scene);
            return LightInfo(
                scene.Light1Vector(), scene.Light1Color(), scene.Light2Vector(),
                Vector3(1.0F, 1.0F, 1.0F));
        }
        return EnemyInstanceEntity::GetLightInfo();
    }

    Enemy24Entity::Enemy24Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : GoreaEnemyEntityBase(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(15);
        (*processes)[0] = [this]() { State00(); };
        (*processes)[1] = [this]() { State01(); };
        (*processes)[2] = [this]() { State02(); };
        (*processes)[3] = [this]() { State03(); };
        (*processes)[4] = [this]() { State04(); };
        (*processes)[5] = [this]() { State05(); };
        (*processes)[6] = [this]() { State06(); };
        (*processes)[7] = [this]() { State07(); };
        (*processes)[8] = [this]() { State08(); };
        (*processes)[9] = [this]() { State09(); };
        (*processes)[10] = [this]() { State10(); };
        (*processes)[11] = [this]() { State11(); };
        (*processes)[12] = [this]() { State12(); };
        (*processes)[13] = [this]() { State13(); };
        (*processes)[14] = [this]() { State14(); };
        _stateProcesses = std::move(processes);
    }

    EnemySpawnEntity* Enemy24Entity::Spawner() const noexcept
    {
        return _spawner;
    }

    const std::vector<ColorRgb>* Enemy24Entity::Colors() const noexcept
    {
        return _colors;
    }

    std::int32_t Enemy24Entity::WeaponIndex() const noexcept
    {
        return _weaponIndex;
    }

    void Enemy24Entity::EnemyInitialize()
    {
        InitializeCommon(_spawner);
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::OnRadar;
        _hurtVolumeInit = CollisionVolume(Vector3::Zero, Fixed::ToFloat(4098));
        Scale = Vector3(1.5F, 1.5F, 1.5F);
        _model = &SetUpModel("Gorea1A_lod0");
        _model->NodeAnimIgnoreRoot = true;
        RequireReference(_model->Model()).ComputeNodeMatrices(0);
        _model->SetAnimation(17, 0, _animSetNoMat);
        _model->SetAnimation(26, 1, SetFlags::Material);
        _regenModel = &SetUpModel("goreaArmRegen");
        _regenModel->Active = false;
        _spineNode = RequireReference(_model->Model()).GetNodeByName("Spine_02");
        EnemySpawnEntity& spawner = RequireReference(_spawner);
        _volume = CollisionVolume::Move(
            CollisionVolume(
                spawner.Data.Fields.S11().Sphere1Position.ToFloatVector(),
                spawner.Data.Fields.S11().Sphere1Radius.FloatValue()),
            Position);
        UpdateSpeed();
        _field23C = 210 * 2;
        _field23E = 510 * 2;
        _field240 = static_cast<std::int32_t>(Rng::GetRandomInt2(90) + 150U) * 2;
        _goreaFlags |= Gorea1AFlags::Bit0;
        _goreaFlags |= Gorea1AFlags::Bit2;
        ResetMaterialColors();
        SpawnHead();
        SpawnArms();
        SpawnLegs();
        SpawnGorea1B();
        ChangeWeapon();
    }

    void Enemy24Entity::SpawnHead()
    {
        std::shared_ptr<EnemyInstanceEntity> enemy
            = EnemySpawnEntity::SpawnEnemy(this, MphRead::EnemyType::GoreaHead, NodeRef, _scene);
        std::shared_ptr<Enemy25Entity> head = std::dynamic_pointer_cast<Enemy25Entity>(enemy);
        if (head)
        {
            RequireReference(_scene).AddEntity(head);
            _head = std::move(head);
        }
    }

    void Enemy24Entity::SpawnArms()
    {
        for (std::int32_t i = 0; i < 2; ++i)
        {
            std::shared_ptr<EnemyInstanceEntity> enemy
                = EnemySpawnEntity::SpawnEnemy(this, MphRead::EnemyType::GoreaArm, NodeRef, _scene);
            std::shared_ptr<Enemy26Entity> arm = std::dynamic_pointer_cast<Enemy26Entity>(enemy);
            if (arm)
            {
                arm->Index = i;
                RequireReference(_scene).AddEntity(arm);
                ArrayAt(_arms, i) = std::move(arm);
                _armBits |= 1 << i;
            }
        }
    }

    void Enemy24Entity::SpawnLegs()
    {
        for (std::int32_t i = 0; i < 3; ++i)
        {
            std::shared_ptr<EnemyInstanceEntity> enemy
                = EnemySpawnEntity::SpawnEnemy(this, MphRead::EnemyType::GoreaLeg, NodeRef, _scene);
            std::shared_ptr<Enemy27Entity> leg = std::dynamic_pointer_cast<Enemy27Entity>(enemy);
            if (leg)
            {
                leg->Index = i;
                RequireReference(_scene).AddEntity(leg);
                ArrayAt(_legs, i) = std::move(leg);
            }
        }
    }

    void Enemy24Entity::SpawnGorea1B()
    {
        std::shared_ptr<EnemyInstanceEntity> enemy
            = EnemySpawnEntity::SpawnEnemy(this, MphRead::EnemyType::Gorea1B, NodeRef, _scene);
        std::shared_ptr<Enemy28Entity> gorea1B = std::dynamic_pointer_cast<Enemy28Entity>(enemy);
        if (gorea1B)
        {
            RequireReference(_scene).AddEntity(gorea1B);
            _gorea1B = std::move(gorea1B);
        }
    }

    void Enemy24Entity::Activate()
    {
        _scanId = VectorAt(Metadata::EnemyScanIds,
            static_cast<std::int32_t>(EnemyType()));
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::CollidePlayer;
        Flags |= EnemyFlags::CollideBeam;
        Flags |= EnemyFlags::NoHomingNc;
        Flags |= EnemyFlags::NoHomingCo;
        Flags |= EnemyFlags::OnRadar;
        _targetFacing = Vector3::Zero;
        Enemy25Entity& head = RequireReference(_head);
        head.Flags |= EnemyFlags::CollidePlayer;
        head.Flags |= EnemyFlags::CollideBeam;
        head.Flags |= EnemyFlags::NoHomingNc;
        head.Flags |= EnemyFlags::NoHomingCo;
        for (std::int32_t i = 0; i < 2; ++i)
        {
            _armBits |= 1 << i;
            RequireReference(ArrayAt(_arms, i)).Activate();
        }
        _speed = Vector3::Zero;
        RequireReference(_model).SetAnimation(0, 0, _animSetNoMat, AnimFlags::NoLoop);
        _soundSource.PlaySfx(SfxId::GOREA_REGEN_ARM_SCR);
        for (std::int32_t i = 0; i < 3; ++i)
        {
            Enemy27Entity& leg = RequireReference(ArrayAt(_legs, i));
            leg.Flags |= EnemyFlags::CollidePlayer;
            leg.Flags |= EnemyFlags::CollideBeam;
            leg.Flags |= EnemyFlags::NoHomingNc;
            leg.Flags |= EnemyFlags::NoHomingCo;
            leg.SetKneeNode(this);
        }
        _goreaFlags |= Gorea1AFlags::Bit2;
        UpdateSpeed();
        Enemy28Entity& gorea1B = RequireReference(_gorea1B);
        SetTransform(gorea1B.FacingVector(), UpVector(), gorea1B.Position);
    }

    void Enemy24Entity::ChangeWeapon()
    {
        _weaponIndex = AddInt32Unchecked(_weaponIndex, 1);
        if (_weaponIndex >= 6)
        {
            _weaponIndex = 0;
        }
        _colors = &VectorAt(Metadata::Enemy24Colors, _weaponIndex);
        if (MainPlayer().Health() > 0)
        {
            Music::PlayMusic(ArrayAt(_musicTracks, _weaponIndex));
        }
        const Weapons::WeaponList& goreaWeapons
            = RequireReference(Weapons::GoreaWeapons);
        const std::shared_ptr<WeaponInfo> weapon
            = VectorAt(goreaWeapons, _weaponIndex);
        const std::int32_t effectiveness
            = VectorAt(Metadata::GoreaEffectiveness, _weaponIndex);
        for (std::int32_t i = 0; i < 2; ++i)
        {
            Enemy26Entity& arm = RequireReference(ArrayAt(_arms, i));
            arm.UpdateWeapon(weapon);
            Metadata::LoadEffectiveness(effectiveness, arm.BeamEffectiveness);
        }
        Enemy28Entity& gorea1B = RequireReference(_gorea1B);
        Metadata::LoadEffectiveness(effectiveness,
            RequireReference(gorea1B.SealSphere).BeamEffectiveness);
    }

    void Enemy24Entity::EnemyProcess()
    {
        if (!TypeExtensions::TestFlag(static_cast<EnemyFlags>(Flags), EnemyFlags::Visible))
        {
            return;
        }
        if (_field23E >= 0
            && (TypeExtensions::TestFlag(
                    RequireReference(ArrayAt(_arms, 0)).ArmFlags, GoreaArmFlags::Bit0)
                || TypeExtensions::TestFlag(
                    RequireReference(ArrayAt(_arms, 1)).ArmFlags, GoreaArmFlags::Bit0)))
        {
            --_field23E;
        }
        if (_field240 >= 0 && (_armBits & 3) == 3)
        {
            --_field240;
        }
        IncrementAllMaterialColors();
        CheckPlayerCollision();
        CallStateProcess();
        if (Active)
        {
            UpdateAnimFrames(RequireReference(_model));
        }
        TransformHurtVolumeToNode(
            _spineNode.get(), Vector3(0.0F, Fixed::ToFloat(970), Fixed::ToFloat(622)));
    }

    bool Enemy24Entity::BaseProcess()
    {
        return true;
    }

    bool Enemy24Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        _health = std::numeric_limits<std::uint16_t>::max();
        return true;
    }

    void Enemy24Entity::IncrementAllMaterialColors()
    {
        ModelInstance& model = RequireReference(_model);
        const std::int32_t frame = AddInt32Unchecked(
            MulInt32Unchecked((*model.AnimInfo->Frame)[0], 2),
            static_cast<std::int32_t>(RequireReference(_scene).FrameCount() % 2));
        const std::int32_t frameCount
            = MulInt32Unchecked((*model.AnimInfo->FrameCount)[0], 2);
        Model& modelData = RequireReference(model.Model());
        for (std::int32_t i = 0; i < 2; ++i)
        {
            for (std::int32_t j = 2; j < 5; ++j)
            {
                const std::int32_t nameIndex = i * 6 + j;
                std::shared_ptr<Material> material = modelData.GetMaterialByName(
                    std::string(ArrayAt(_armMatNames, nameIndex)));
                const ColorRgb ambient = VectorAt(RequireReference(_colors), 0);
                const ColorRgb diffuse = VectorAt(RequireReference(_colors), 1);
                IncrementMaterialColors(material.get(), ambient, diffuse, frame, frameCount);
            }
        }
        for (std::int32_t i = 0; i < 10; ++i)
        {
            std::shared_ptr<Material> material = modelData.GetMaterialByName(
                std::string(ArrayAt(_bodyMatNames1, i)));
            const ColorRgb ambient = VectorAt(RequireReference(_colors), 2);
            const ColorRgb diffuse = VectorAt(RequireReference(_colors), 3);
            IncrementMaterialColors(material.get(), ambient, diffuse, frame, frameCount);
        }
        for (std::int32_t i = 0; i < 2; ++i)
        {
            std::shared_ptr<Material> material = modelData.GetMaterialByName(
                std::string(ArrayAt(_bodyMatNames2, i)));
            const ColorRgb ambient = VectorAt(RequireReference(_colors), 0);
            const ColorRgb diffuse = VectorAt(RequireReference(_colors), 1);
            IncrementMaterialColors(material.get(), ambient, diffuse, frame, frameCount);
        }
    }

    void Enemy24Entity::CheckPlayerCollision()
    {
        PlayerEntity& player = MainPlayer();
        if (!ArrayAt(HitPlayers, player.SlotIndex()))
        {
            return;
        }
        Vector3 between = TypeExtensions::WithY(
            static_cast<Vector3>(player.Position) - static_cast<Vector3>(Position), 0.0F);
        between = LengthSquared(between) > 1.0F / 128.0F
            ? between.Normalized()
            : FacingVector();
        player.SetSpeed(player.Speed() + ScaleVector(between, 1.0F / 4.0F));
        player.TakeDamage(10, DamageFlags::None, std::nullopt, this);
    }

    void Enemy24Entity::State00()
    {
        if (TypeExtensions::TestFlag(_goreaFlags, Gorea1AFlags::Bit2))
        {
            for (std::int32_t i = 0; i < 2; ++i)
            {
                RequireReference(ArrayAt(_arms, i)).Flags |= EnemyFlags::Invincible;
            }
            _goreaFlags &= ~Gorea1AFlags::Bit2;
            RequireReference(_model).SetAnimation(0, 0, _animSetNoMat, AnimFlags::NoLoop);
            RequireReference(_head).RespawnFlashEffect();
        }
        if (TypeExtensions::TestFlag(_goreaFlags, Gorea1AFlags::Bit0))
        {
            _goreaFlags &= ~Gorea1AFlags::Bit0;
            SpawnEffect(175, Position);
        }
        ModelInstance& model = RequireReference(_model);
        if ((*model.AnimInfo->Index)[0] == 0)
        {
            UpdateArmMaterialAlpha();
            if (AnimationEnded())
            {
                if (RequireReference(_gorea1B).PhasesLeft != 3)
                {
                    ChangeWeapon();
                }
                _soundSource.PlaySfx(SfxId::GOREA_ROAR_SCR);
                model.SetAnimation(22, 0, _animSetNoMat, AnimFlags::NoLoop);
            }
        }
        if (CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this))
        {
            for (std::int32_t i = 0; i < 2; ++i)
            {
                RequireReference(ArrayAt(_arms, i)).Flags &= ~EnemyFlags::Invincible;
            }
            _field240 = static_cast<std::int32_t>(Rng::GetRandomInt2(90) + 150U) * 2;
        }
    }

    void Enemy24Entity::UpdateArmMaterialAlpha()
    {
        ModelInstance& model = RequireReference(_model);
        Model& modelData = RequireReference(model.Model());
        for (std::int32_t i = 0; i < 2; ++i)
        {
            std::shared_ptr<Material> material = modelData.GetMaterialByName(
                i == 0 ? "L_Bisep" : "R_Bisep");
            Material& materialRef = RequireReference(material);
            if (materialRef.CurrentAlpha < 1.0F)
            {
                const std::int32_t frame
                    = SubInt32Unchecked((*model.AnimInfo->Frame)[0], 10);
                if (frame >= 0)
                {
                    const float alpha = static_cast<float>(frame)
                        / static_cast<float>(SubInt32Unchecked(
                            (*model.AnimInfo->FrameCount)[0], 11)) * 31.0F;
                    SetArmMaterialAlpha(i, RoundToByteUnchecked(alpha));
                }
            }
        }
    }

    void Enemy24Entity::SetArmMaterialAlpha(std::int32_t index, std::uint8_t alpha)
    {
        Model& model = RequireReference(RequireReference(_model).Model());
        const std::int32_t end = index * 6 + 6;
        for (std::int32_t i = index * 6; i < end; ++i)
        {
            RequireReference(model.GetMaterialByName(
                std::string(ArrayAt(_armMatNames, i)))).Alpha = alpha;
        }
    }

    void Enemy24Entity::State01()
    {
        if (!Equal(_targetFacing, Vector3::Zero))
        {
            EnsureAnimation(24, 0, _animSetNoMat);
            if (SeekTargetFacing(_targetFacing, 3.0F))
            {
                _targetFacing = Vector3::Zero;
            }
        }
        else
        {
            EnsureAnimation(17, 0, _animSetNoMat);
            (void)UpdateTargetFacing();
        }
        (void)CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this);
    }

    bool Enemy24Entity::UpdateTargetFacing()
    {
        Vector3 between = TypeExtensions::WithY(
            static_cast<Vector3>(MainPlayer().Position) - static_cast<Vector3>(Position), 0.0F);
        if (LengthSquared(between) > 1.0F / 128.0F)
        {
            _targetFacing = between.Normalized();
            return true;
        }
        return false;
    }

    void Enemy24Entity::State02()
    {
        if (UpdateTargetFacing())
        {
            (void)SeekTargetFacing(_targetFacing, 3.0F);
        }
        _speed = DivideVector(
            ScaleVector(TypeExtensions::WithY(FacingVector(), 0.0F), _speedFactor), 2.0F);
        EnsureAnimation(25, 0, _animSetNoMat);
        (void)CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this);
    }

    void Enemy24Entity::State03()
    {
        _targetFacing = SeekTargetSetAnim(
            _targetFacing, (*RequireReference(_model).AnimInfo->Index)[0], 0, _animSetNoMat);
        _speed = DivideVector(
            ScaleVector(ScaleVector(FacingVector(), _speedFactor), 5.0F), 2.0F);
        (void)CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this);
    }

    void Enemy24Entity::State04()
    {
        _targetFacing = SeekTargetSetAnim(_targetFacing, 16, 0, _animSetNoMat);
        (void)CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this);
    }

    void Enemy24Entity::State05()
    {
        for (std::int32_t i = 0; i < 2; ++i)
        {
            const std::shared_ptr<Enemy26Entity>& arm = ArrayAt(_arms, i);
            if (CheckTargeting(arm.get()))
            {
                Vector3 spawnPos;
                Vector3 spawnDir;
                GetArmAim(arm.get(), spawnPos, spawnDir);
                Enemy26Entity& armRef = RequireReference(arm);
                armRef.Ammo = 65535;
                (void)BeamProjectileEntity::Spawn(
                    arm, armRef.EquipInfo(), spawnPos, spawnDir,
                    BeamSpawnFlags::None, armRef.NodeRef, _scene);
                const std::int32_t shotEffect = ArrayAt(_shotEffects, WeaponIndex());
                CreateShotEffectLoose(arm.get(), shotEffect);
                const BeamType stopBeam = ArrayAt(_beamTypes, WeaponIndex());
                StopBeamChargeSfx(stopBeam);
                const BeamType shotBeam = ArrayAt(_beamTypes, WeaponIndex());
                const bool charged
                    = TypeExtensions::TestFlag(armRef.ArmFlags, GoreaArmFlags::Bit2);
                PlayBeamShotSfx(shotBeam, charged);
            }
        }
        _targetFacing = SeekTargetSetAnim(
            _targetFacing, (*RequireReference(_model).AnimInfo->Index)[0], 0, _animSetNoMat);
        (void)CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this);
    }

    bool Enemy24Entity::CheckTargeting(Enemy26Entity* armValue)
    {
        const std::int32_t frame = AddInt32Unchecked(
            MulInt32Unchecked((*RequireReference(_model).AnimInfo->Frame)[0], 2),
            static_cast<std::int32_t>(RequireReference(_scene).FrameCount() % 2));
        Enemy26Entity& arm = RequireReference(armValue);
        if (WeaponIndex() == 0 || WeaponIndex() == 5)
        {
            if (TypeExtensions::TestFlag(arm.ArmFlags, GoreaArmFlags::Bit0))
            {
                return false;
            }
            bool shoot = false;
            if (TypeExtensions::TestFlag(arm.ArmFlags, GoreaArmFlags::Bit2))
            {
                WeaponInfo& weapon = RequireReference(RequireReference(arm.EquipInfo()).Weapon);
                const std::int32_t shotCooldown
                    = MulInt32Unchecked(weapon.ShotCooldown, 2);
                const std::int32_t autoCooldown
                    = MulInt32Unchecked(weapon.AutofireCooldown, 2);
                if (shotCooldown <= frame && frame <= autoCooldown
                    && (WeaponIndex() == 5 || frame % 8 == 7))
                {
                    shoot = true;
                }
                if (_field242 > 0)
                {
                    if (frame == autoCooldown
                        && _field242 > SubInt32Unchecked(
                            MulInt32Unchecked(
                                (*RequireReference(_model).AnimInfo->FrameCount)[0], 2),
                            AddInt32Unchecked(shotCooldown, autoCooldown)))
                    {
                        (*RequireReference(_model).AnimInfo->Frame)[0] = shotCooldown / 2;
                    }
                    --_field242;
                }
            }
            else if (TypeExtensions::TestFlag(arm.ArmFlags, GoreaArmFlags::Bit1)
                && arm.Cooldown == frame)
            {
                shoot = true;
            }
            return shoot;
        }
        if (!TypeExtensions::TestFlag(arm.ArmFlags, GoreaArmFlags::Bit0)
            && TypeExtensions::TestAny(
                arm.ArmFlags, GoreaArmFlags::Bit1 | GoreaArmFlags::Bit2)
            && frame == arm.Cooldown)
        {
            return true;
        }
        return false;
    }

    void Enemy24Entity::CreateShotEffectLoose(Enemy26Entity* armValue, std::int32_t effectId)
    {
        Enemy26Entity& arm = RequireReference(armValue);
        const std::optional<std::uint64_t> prevUpdate = _lastNodeTransformUpdate;
        Vector3 spawnPos;
        Vector3 spawnFacing;
        Vector3 spawnUp;
        arm.GetElbowNodeVectors(spawnPos, spawnFacing, spawnUp);
        _lastNodeTransformUpdate = prevUpdate;
        spawnFacing = spawnFacing.Normalized();
        spawnUp = spawnUp.Normalized();
        spawnPos = spawnPos + ScaleVector(spawnFacing, Fixed::ToFloat(8343));
        RequireReference(_scene).SpawnEffect(effectId, spawnFacing, spawnUp, spawnPos);
    }

    void Enemy24Entity::GetArmAim(
        Enemy26Entity* armValue, Vector3& position, Vector3& direction)
    {
        Enemy26Entity& arm = RequireReference(armValue);
        const std::optional<std::uint64_t> prevUpdate = _lastNodeTransformUpdate;
        Vector3 ignored;
        arm.GetElbowNodeVectors(position, direction, ignored);
        _lastNodeTransformUpdate = prevUpdate;
        position = position + ScaleVector(direction, Fixed::ToFloat(8343));
        const Vector3 playerPosition = TypeExtensions::AddY(
            static_cast<Vector3>(MainPlayer().Position), 0.5F);
        if (!HalfturretEntity::UpdateAim(
                position, playerPosition, arm.EquipInfo(), direction))
        {
            _goreaFlags |= Gorea1AFlags::Bit4;
        }
    }

    std::int32_t Enemy24Entity::GetBeamChargeSfx(BeamType beam) const
    {
        if (beam == BeamType::Missile)
        {
            const std::shared_ptr<std::vector<std::vector<std::int32_t>>> hunterSfx
                = Metadata::HunterSfx();
            return VectorAt(VectorAt(RequireReference(hunterSfx), 0),
                static_cast<std::int32_t>(HunterSfx::MissileCharge));
        }
        const std::shared_ptr<std::vector<std::vector<std::int32_t>>> beamSfx
            = Metadata::BeamSfx();
        return VectorAt(
            VectorAt(RequireReference(beamSfx), static_cast<std::int32_t>(beam)),
            static_cast<std::int32_t>(BeamSfx::Charge));
    }

    void Enemy24Entity::PlayBeamChargeSfx(BeamType beam)
    {
        const std::int32_t sfx = GetBeamChargeSfx(beam);
        if (sfx != -1)
        {
            _soundSource.PlaySfx(sfx, true);
        }
    }

    void Enemy24Entity::StopBeamChargeSfx(BeamType beam)
    {
        const std::int32_t sfx = GetBeamChargeSfx(beam);
        if (sfx != -1)
        {
            _soundSource.StopSfx(sfx);
        }
    }

    void Enemy24Entity::PlayBeamShotSfx(BeamType beam, bool charged)
    {
        StopBeamChargeSfx(beam);
        BeamSfx sfx;
        if (charged)
        {
            sfx = beam == VectorAt(Weapons::AffinityWeapons, 0)
                ? BeamSfx::AffinityChargeShot
                : BeamSfx::ChargeShot;
        }
        else
        {
            sfx = BeamSfx::Shot;
        }
        const std::shared_ptr<std::vector<std::vector<std::int32_t>>> beamSfx
            = Metadata::BeamSfx();
        const std::int32_t id = VectorAt(
            VectorAt(RequireReference(beamSfx), static_cast<std::int32_t>(beam)),
            static_cast<std::int32_t>(sfx));
        if (id != -1 && (_weaponIndex != 5 || _soundSource.CountSourcePlayingSfx(id) == 0))
        {
            _soundSource.PlaySfx(id);
        }
    }

    void Enemy24Entity::State06()
    {
        _targetFacing = SeekTargetSetAnim(
            _targetFacing, (*RequireReference(_model).AnimInfo->Index)[0], 0, _animSetNoMat);
        (void)CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this);
    }

    void Enemy24Entity::State07()
    {
        ModelInstance& model = RequireReference(_model);
        const std::int32_t anim = (*model.AnimInfo->Index)[0];
        if (std::find(_weaponAnimIds.begin(), _weaponAnimIds.end(), anim) == _weaponAnimIds.end()
            && anim != 8 && anim != 9 && anim != 10 && anim != 13)
        {
            ChangeWeapon();
            model.SetAnimation(13, 0, _animSetNoMat, AnimFlags::NoLoop);
            _speed = Vector3::Zero;
        }
        if (CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this))
        {
            _soundSource.PlaySfx(SfxId::GOREA_WEAPON_SWITCH_SCR);
        }
    }

    void Enemy24Entity::State08()
    {
        if ((*RequireReference(_model).AnimInfo->Frame)[0] == 60
            && RequireReference(_scene).FrameCount() != 0
            && RequireReference(_scene).FrameCount() % 2 == 0)
        {
            Vector3 between;
            float distance;
            if (GetHorizontalToPlayer(25.0F, between, distance))
            {
                if (distance > 1.0F / 128.0F)
                {
                    between = between.Normalized();
                }
                between = TypeExtensions::AddY(ScaleVector(between, 1.5F), Fixed::ToFloat(682));
                MainPlayer().TakeDamage(40, DamageFlags::None, between, this);
                RequireReference(MainPlayer().CameraInfo()).SetShake(0.75F);
            }
            SpawnEffect(71, Position);
        }
        (void)CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this);
    }

    bool Enemy24Entity::GetHorizontalToPlayer(
        float maxDistance, Vector3& between, float& distance) const
    {
        between = Vector3::Zero;
        distance = 0.0F;
        PlayerEntity& player = MainPlayer();
        if (player.Health() > 0)
        {
            between = static_cast<Vector3>(player.Position) - static_cast<Vector3>(Position);
            distance = LengthSquared(TypeExtensions::WithY(between, 0.0F));
            if (distance < maxDistance)
            {
                return true;
            }
        }
        return false;
    }

    void Enemy24Entity::State09()
    {
        (void)CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this);
    }

    void Enemy24Entity::State10()
    {
        UpdateArmMaterialAlpha();
        (void)CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this);
    }

    void Enemy24Entity::State11()
    {
        (void)CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this);
    }

    void Enemy24Entity::State12()
    {
        if ((*RequireReference(_model).AnimInfo->Frame)[0] == 24
            && RequireReference(_scene).FrameCount() != 0
            && RequireReference(_scene).FrameCount() % 2 == 0)
        {
            Vector3 between;
            float distance;
            if (GetHorizontalToPlayer(37.5F, between, distance))
            {
                if (distance > 1.0F / 128.0F)
                {
                    between = between.Normalized();
                }
                between = TypeExtensions::AddY(ScaleVector(between, 1.5F), Fixed::ToFloat(682));
                MainPlayer().TakeDamage(25, DamageFlags::None, between, this);
                RequireReference(MainPlayer().CameraInfo()).SetShake(0.75F);
            }
        }
        (void)CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this);
    }

    void Enemy24Entity::State13()
    {
        if (TypeExtensions::TestFlag(static_cast<EnemyFlags>(Flags), EnemyFlags::Visible))
        {
            _scanId = 0;
            Flags &= ~EnemyFlags::Visible;
            Flags &= ~EnemyFlags::CollidePlayer;
            Flags &= ~EnemyFlags::CollideBeam;
            Flags |= EnemyFlags::NoHomingNc;
            Flags |= EnemyFlags::NoHomingCo;
            Flags &= ~EnemyFlags::OnRadar;
            _field23C = 210 * 2;
            _field23E = 510 * 2;
            _field240 = static_cast<std::int32_t>(Rng::GetRandomInt2(90) + 150U) * 2;
            Enemy25Entity& head = RequireReference(_head);
            head.Flags &= ~EnemyFlags::Visible;
            head.Flags &= ~EnemyFlags::CollidePlayer;
            head.Flags &= ~EnemyFlags::CollideBeam;
            head.Flags |= EnemyFlags::NoHomingNc;
            head.Flags |= EnemyFlags::NoHomingCo;
            for (std::int32_t i = 0; i < 2; ++i)
            {
                Enemy26Entity& arm = RequireReference(ArrayAt(_arms, i));
                arm.Flags &= ~EnemyFlags::Visible;
                arm.Flags &= ~EnemyFlags::CollidePlayer;
                arm.Flags &= ~EnemyFlags::CollideBeam;
                arm.Flags |= EnemyFlags::NoHomingNc;
                arm.Flags |= EnemyFlags::NoHomingCo;
                arm.Flags |= EnemyFlags::Invincible;
            }
            Enemy28Entity& gorea1B = RequireReference(_gorea1B);
            gorea1B.Activate();
            for (std::int32_t i = 0; i < 3; ++i)
            {
                RequireReference(ArrayAt(_legs, i)).SetKneeNode(&gorea1B);
            }
        }
        (void)CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this);
    }

    void Enemy24Entity::State14()
    {
        (void)CallSubroutine<Enemy24Entity>(Metadata::Enemy24Subroutines, this);
        _state2 = _nextState;
    }

    bool Enemy24Entity::Behavior00()
    {
        if (!AnimationEnded())
        {
            return false;
        }
        RequireReference(_model).SetAnimation(17, 0, _animSetNoMat);
        if (_state1 == 9)
        {
            _targetFacing = Vector3::Zero;
        }
        return true;
    }

    bool Enemy24Entity::Behavior01()
    {
        return true;
    }

    bool Enemy24Entity::Behavior02()
    {
        return true;
    }

    bool Enemy24Entity::Behavior03()
    {
        return AnimationEnded();
    }

    bool Enemy24Entity::Behavior04()
    {
        std::int32_t index = 0;
        while ((_armBits & (1 << index)) == 0
            || !TypeExtensions::TestFlag(
                RequireReference(ArrayAt(_arms, index)).ArmFlags, GoreaArmFlags::Bit0))
        {
            if (++index >= 2)
            {
                return false;
            }
        }
        _armBits &= ~(1 << index);
        RequireReference(_model).SetAnimation(
            20 + index, 0, _animSetNoMat, AnimFlags::NoLoop);
        if (index == 0)
        {
            _goreaFlags |= Gorea1AFlags::Bit3;
        }
        else
        {
            _goreaFlags &= ~Gorea1AFlags::Bit3;
        }
        _speed = Vector3::Zero;
        SetArmMaterialAlpha(index, 0);
        const Vector3 spawnPos = RequireReference(ArrayAt(_arms, index)).Position;
        SpawnEffect(45, spawnPos);
        StopShots(index, false);
        if (_armBits != 0)
        {
            const std::uint32_t random = Rng::GetRandomInt2(100);
            const ItemType itemType = random >= 80 ? ItemType::UABig : ItemType::HealthBig;
            const std::int32_t despawnTime = 300 * 2;
            auto item = std::make_shared<ItemInstanceEntity>(
                ItemInstanceEntityData(spawnPos, itemType, despawnTime), NodeRef, _scene);
            RequireReference(_scene).AddEntity(item);
        }
        return true;
    }

    void Enemy24Entity::StopShots(std::int32_t index, bool detach)
    {
        RequireReference(ArrayAt(_arms, index)).StopShotEffect(detach);
        StopBeamChargeSfx(ArrayAt(_beamTypes, WeaponIndex()));
    }

    bool Enemy24Entity::Behavior05()
    {
        ModelInstance& model = RequireReference(_model);
        if ((*model.AnimInfo->Frame)[0]
                >= SubInt32Unchecked((*model.AnimInfo->FrameCount)[0], 1)
            && RequireReference(_scene).FrameCount() > 0
            && RequireReference(_scene).FrameCount() % 2 == 0)
        {
            const Weapons::WeaponList& goreaWeapons
                = RequireReference(Weapons::GoreaWeapons);
            const std::uint16_t charge
                = RequireReference(VectorAt(goreaWeapons, WeaponIndex())).FullCharge;
            for (std::int32_t i = 0; i < 2; ++i)
            {
                Enemy26Entity& arm = RequireReference(ArrayAt(_arms, i));
                RequireReference(arm.EquipInfo()).ChargeLevel
                    = IntToUInt16Unchecked(static_cast<std::int32_t>(charge) * 2);
                arm.ArmFlags |= GoreaArmFlags::Bit1;
            }
            model.SetAnimation(ArrayAt(_weaponAnimIds, WeaponIndex()), 0, _animSetNoMat);
            return true;
        }
        return false;
    }

    bool Enemy24Entity::Behavior06()
    {
        if (_armBits == 0 && (*RequireReference(_model).AnimInfo->Frame)[0] == 5
            && RequireReference(_scene).FrameCount() > 0
            && RequireReference(_scene).FrameCount() % 2 == 0)
        {
            const std::int32_t anim
                = TypeExtensions::TestFlag(_goreaFlags, Gorea1AFlags::Bit3) ? 5 : 6;
            const SetFlags setFlags = SetFlags::Texture | SetFlags::Texcoord
                | SetFlags::Unused | SetFlags::Node | SetFlags::Material;
            const std::vector<std::shared_ptr<ModelInstance>>& models
                = RequireReference(_gorea1B).GetModels();
            ModelInstance& goreaModel = RequireReference(VectorAt(models, 0));
            goreaModel.SetAnimation(anim, 0, setFlags, AnimFlags::NoLoop);
            UpdateAnimFrames(goreaModel);
            (*goreaModel.AnimInfo->Frame)[0] = 5;
            if (MainPlayer().Health() > 0)
            {
                Music::PlayMusic(MusicId::SEQ_GOREA_1_M21);
            }
            _soundSource.PlaySfx(SfxId::GOREA_TRANSFORM1_SCR);
            return true;
        }
        return false;
    }

    bool Enemy24Entity::Behavior07()
    {
        if (CheckOffsetOutsideVolume(_speed))
        {
            _speed = Vector3::Zero;
            bool update = true;
            if (_state1 == 3)
            {
                if (_nextState == 5)
                {
                    StartShots();
                    _field23C = static_cast<std::int32_t>(Rng::GetRandomInt2(60) + 90U) * 2;
                    update = false;
                }
                else if (_nextState == 3)
                {
                    _nextState = 4;
                }
            }
            if (update)
            {
                RequireReference(_model).SetAnimation(17, 0, _animSetNoMat);
                StopAndSetUp();
            }
            return true;
        }
        return false;
    }

    bool Enemy24Entity::CheckOffsetOutsideVolume(Vector3 offset) const
    {
        return !_volume.TestPoint(static_cast<Vector3>(Position) + offset);
    }

    void Enemy24Entity::StopAndSetUp()
    {
        _goreaFlags &= ~Gorea1AFlags::Bit4;
        const std::shared_ptr<Enemy26Entity>& armL = ArrayAt(_arms, 0);
        const std::shared_ptr<Enemy26Entity>& armR = ArrayAt(_arms, 1);
        RequireReference(RequireReference(armL).EquipInfo()).ChargeLevel = 0;
        RequireReference(RequireReference(armR).EquipInfo()).ChargeLevel = 0;
        const Weapons::WeaponList& goreaWeapons
            = RequireReference(Weapons::GoreaWeapons);
        const std::shared_ptr<WeaponInfo> weapon = VectorAt(goreaWeapons, WeaponIndex());
        WeaponInfo& weaponRef = RequireReference(weapon);
        RequireReference(armL).Cooldown = MulInt32Unchecked(weaponRef.ShotCooldown, 2);
        RequireReference(armR).Cooldown = MulInt32Unchecked(weaponRef.AutofireCooldown, 2);
        RequireReference(_head).RespawnFlashEffect();
        _field23C = 60 * 2;
    }

    void Enemy24Entity::StartShots()
    {
        const std::uint32_t chargeRandom = Rng::GetRandomInt2(100);
        const std::int32_t chargeChance = ArrayAt(_chargeChances, WeaponIndex());
        const bool charge = chargeRandom < static_cast<std::uint32_t>(chargeChance);
        const std::shared_ptr<Enemy26Entity>& armL = ArrayAt(_arms, 0);
        const std::shared_ptr<Enemy26Entity>& armR = ArrayAt(_arms, 1);
        if (charge)
        {
            RequireReference(armL).ArmFlags |= GoreaArmFlags::Bit2;
            RequireReference(armR).ArmFlags |= GoreaArmFlags::Bit2;
            const Weapons::WeaponList& goreaWeapons
                = RequireReference(Weapons::GoreaWeapons);
            WeaponInfo& weapon = RequireReference(VectorAt(goreaWeapons, WeaponIndex()));
            RequireReference(RequireReference(armL).EquipInfo()).ChargeLevel
                = IntToUInt16Unchecked(static_cast<std::int32_t>(weapon.FullCharge) * 2);
            RequireReference(armL).Cooldown = MulInt32Unchecked(weapon.ShotCooldown, 2);
            RequireReference(armR).Cooldown = MulInt32Unchecked(weapon.ShotCooldown, 2);
            _field242 = static_cast<std::int32_t>(Rng::GetRandomInt2(30) + 60U) * 2;
            RequireReference(_model).SetAnimation(15, 0, _animSetNoMat);
            if (!TypeExtensions::TestFlag(RequireReference(armL).ArmFlags, GoreaArmFlags::Bit0))
            {
                CreateChargeEffect(0);
            }
            if (!TypeExtensions::TestFlag(RequireReference(armR).ArmFlags, GoreaArmFlags::Bit0))
            {
                CreateChargeEffect(1);
            }
            _nextState = 6;
            PlayBeamChargeSfx(ArrayAt(_beamTypes, WeaponIndex()));
        }
        else
        {
            RequireReference(armL).ArmFlags &= ~GoreaArmFlags::Bit2;
            RequireReference(armR).ArmFlags &= ~GoreaArmFlags::Bit2;
            RequireReference(RequireReference(armL).EquipInfo()).ChargeLevel = 0;
            RequireReference(RequireReference(armR).EquipInfo()).ChargeLevel = 0;
            RequireReference(armL).Cooldown = 6 * 2;
            RequireReference(armR).Cooldown = 6 * 2;
            SetShotAnimation();
            _nextState = 5;
        }
    }

    void Enemy24Entity::CreateChargeEffect(std::int32_t index)
    {
        StopShots(index, true);
        RequireReference(ArrayAt(_arms, index)).SpawnShotEffect(
            ArrayAt(_chargeEffects, WeaponIndex()));
    }

    void Enemy24Entity::SetShotAnimation()
    {
        const std::shared_ptr<Enemy26Entity>& armL = ArrayAt(_arms, 0);
        const std::shared_ptr<Enemy26Entity>& armR = ArrayAt(_arms, 1);
        std::int32_t animId;
        if (TypeExtensions::TestFlag(RequireReference(armL).ArmFlags, GoreaArmFlags::Bit0))
        {
            RequireReference(armL).ArmFlags &= ~GoreaArmFlags::Bit1;
            RequireReference(armR).ArmFlags |= GoreaArmFlags::Bit1;
            animId = 10;
        }
        else if (TypeExtensions::TestFlag(RequireReference(armR).ArmFlags, GoreaArmFlags::Bit0))
        {
            RequireReference(armL).ArmFlags |= GoreaArmFlags::Bit1;
            animId = 9;
        }
        else
        {
            const std::uint32_t random = Rng::GetRandomInt2(3);
            if (random == 0)
            {
                RequireReference(armL).ArmFlags |= GoreaArmFlags::Bit1;
                RequireReference(armR).ArmFlags &= ~GoreaArmFlags::Bit1;
                animId = 9;
            }
            else if (random == 1)
            {
                RequireReference(armL).ArmFlags &= ~GoreaArmFlags::Bit1;
                RequireReference(armR).ArmFlags |= GoreaArmFlags::Bit1;
                animId = 10;
            }
            else
            {
                RequireReference(armL).ArmFlags |= GoreaArmFlags::Bit1;
                RequireReference(armR).ArmFlags |= GoreaArmFlags::Bit1;
                animId = 8;
            }
        }
        RequireReference(_model).SetAnimation(animId, 0, _animSetNoMat, AnimFlags::NoLoop);
    }

    bool Enemy24Entity::Behavior08()
    {
        if (_field23C > 0)
        {
            --_field23C;
        }
        if (_field23C == 0)
        {
            _speed = Vector3::Zero;
            if (_nextState == 5)
            {
                StartShots();
                _field23C = static_cast<std::int32_t>(Rng::GetRandomInt2(60) + 90U) * 2;
            }
            else
            {
                if (_nextState == 3)
                {
                    _nextState = 4;
                }
                RequireReference(_model).SetAnimation(17, 0, _animSetNoMat);
                StopAndSetUp();
            }
            return true;
        }
        return false;
    }

    bool Enemy24Entity::Behavior09()
    {
        Vector3 ignoredBetween;
        float ignoredDistance;
        if (TypeExtensions::TestFlag(MainPlayer().Flags1(), PlayerFlags1::AltForm)
            && GetHorizontalToPlayer(25.0F, ignoredBetween, ignoredDistance))
        {
            _speed = Vector3::Zero;
            _nextState = _state1;
            StopBeamChargeSfx(ArrayAt(_beamTypes, WeaponIndex()));
            _soundSource.PlaySfx(SfxId::GOREA_ROAR_SCR);
            RequireReference(_model).SetAnimation(13, 0, _animSetNoMat, AnimFlags::NoLoop);
            return true;
        }
        return false;
    }

    bool Enemy24Entity::Behavior10()
    {
        Vector3 between;
        float distance;
        if (!TypeExtensions::TestFlag(MainPlayer().Flags1(), PlayerFlags1::AltForm)
            && GetHorizontalToPlayer(37.5F, between, distance))
        {
            _speed = Vector3::Zero;
            SetSwingAnimation(between, distance);
            StopBeamChargeSfx(ArrayAt(_beamTypes, WeaponIndex()));
            _soundSource.PlaySfx(SfxId::GOREA_ARM_SWING_ATTACK_SCR);
            return true;
        }
        return false;
    }

    void Enemy24Entity::SetSwingAnimation(Vector3 between, float distance)
    {
        const std::shared_ptr<Enemy26Entity>& armL = ArrayAt(_arms, 0);
        const std::shared_ptr<Enemy26Entity>& armR = ArrayAt(_arms, 1);
        std::int32_t animId = 5;
        if (TypeExtensions::TestFlag(RequireReference(armL).ArmFlags, GoreaArmFlags::Bit0))
        {
            animId = 6;
        }
        else if (!TypeExtensions::TestFlag(RequireReference(armR).ArmFlags, GoreaArmFlags::Bit0)
            && distance > 1.0F / 128.0F)
        {
            between = TypeExtensions::WithY(between, 0.0F).Normalized();
            Vector3 facing = FacingVector();
            facing = Vector3(facing.Z, 0.0F, -facing.X);
            if (LengthSquared(facing) > 1.0F / 128.0F)
            {
                facing = facing.Normalized();
                const float dot = Vector3::Dot(facing, between);
                if (dot <= Fixed::ToFloat(-214)
                    || (dot < Fixed::ToFloat(214) && Rng::GetRandomInt2(255) % 2 != 0))
                {
                    animId = 6;
                }
            }
        }
        RequireReference(_model).SetAnimation(animId, 0, _animSetNoMat, AnimFlags::NoLoop);
    }

    bool Enemy24Entity::Behavior11()
    {
        const Vector3 between
            = static_cast<Vector3>(MainPlayer().Position) - static_cast<Vector3>(Position);
        if (LengthSquared(between) > 19.0F * 19.0F)
        {
            if (_field244 > 0)
            {
                --_field244;
            }
            if (_field244 == 0 && TrySprintingRoomInVolume())
            {
                return true;
            }
        }
        return false;
    }

    bool Enemy24Entity::TrySprintingRoomInVolume()
    {
        const float factor = _speedFactor * 5.0F * 30.0F;
        const Vector3 offset = ScaleVector(FacingVector(), factor);
        if (!CheckOffsetOutsideVolume(offset) && UpdateTargetFacing())
        {
            _nextState = _state1;
            _field244 = 0;
            _field23C = 150 * 2;
            RequireReference(_model).SetAnimation(23, 0, _animSetNoMat);
            return true;
        }
        return false;
    }

    bool Enemy24Entity::Behavior12()
    {
        if (_field23C > 0)
        {
            --_field23C;
        }
        if (_field23C <= 0)
        {
            StartShots();
            _field23C = static_cast<std::int32_t>(Rng::GetRandomInt2(60) + 90U) * 2;
            return true;
        }
        return false;
    }

    bool Enemy24Entity::Behavior13()
    {
        if (AnimationEnded()
            && TypeExtensions::TestFlag(_goreaFlags, Gorea1AFlags::Bit4)
            && TrySprintingRoomInVolume())
        {
            StopBeamChargeSfx(ArrayAt(_beamTypes, WeaponIndex()));
            return true;
        }
        return false;
    }

    bool Enemy24Entity::Behavior14()
    {
        if (AnimationEnded())
        {
            return Behavior09();
        }
        return false;
    }

    bool Enemy24Entity::Behavior15()
    {
        if (_field23C > 0)
        {
            --_field23C;
        }
        if (_field23C > 0 || !IsAtEndFrame())
        {
            return false;
        }
        RequireReference(_model).SetAnimation(17, 0, _animSetNoMat);
        StopShots(0, true);
        StopShots(1, true);
        _field23C = 210 * 2;
        return true;
    }

    bool Enemy24Entity::Behavior16()
    {
        if (IsAtEndFrame())
        {
            StartShots();
            return true;
        }
        return false;
    }

    bool Enemy24Entity::Behavior17()
    {
        Enemy25Entity& head = RequireReference(_head);
        if (head.Damage >= 1000)
        {
            head.Damage = 0;
            _nextState = _state1;
            StopBeamChargeSfx(ArrayAt(_beamTypes, WeaponIndex()));
            RequireReference(_model).SetAnimation(19, 0, _animSetNoMat, AnimFlags::NoLoop);
            return true;
        }
        return false;
    }

    bool Enemy24Entity::Behavior18()
    {
        if (!Equal(_targetFacing, Vector3::Zero)
            || !CheckFacingAngle(-1.0F, MainPlayer().Position))
        {
            return false;
        }
        _field23C = 60 * 2;
        return true;
    }

    bool Enemy24Entity::Behavior19()
    {
        _field23C = DecrementInt32Unchecked(_field23C);
        if (_field23C > 0)
        {
            return false;
        }
        _speed = Vector3::Zero;
        RequireReference(_model).SetAnimation(17, 0, _animSetNoMat);
        StopAndSetUp();
        return true;
    }

    bool Enemy24Entity::Behavior20()
    {
        if (_field23E >= 0)
        {
            return false;
        }
        _field23E = 510 * 2;
        if (!TypeExtensions::TestFlag(
                RequireReference(ArrayAt(_arms, 0)).ArmFlags, GoreaArmFlags::Bit0)
            && !TypeExtensions::TestFlag(
                RequireReference(ArrayAt(_arms, 1)).ArmFlags, GoreaArmFlags::Bit0))
        {
            return false;
        }
        _soundSource.PlaySfx(SfxId::GOREA_REGEN_ARM_SCR);
        RegenerateArms();
        return true;
    }

    void Enemy24Entity::RegenerateArms()
    {
        for (std::int32_t i = 0; i < 2; ++i)
        {
            Enemy26Entity& arm = RequireReference(ArrayAt(_arms, i));
            arm.Damage = 0;
            if (TypeExtensions::TestFlag(arm.ArmFlags, GoreaArmFlags::Bit0))
            {
                std::int32_t anim = 1;
                _armBits |= 1 << i;
                arm.SetScanId(VectorAt(Metadata::EnemyScanIds,
                    static_cast<std::int32_t>(MphRead::EnemyType::GoreaArm)));
                arm.Flags |= EnemyFlags::CollidePlayer;
                arm.Flags |= EnemyFlags::CollideBeam;
                arm.Flags &= ~EnemyFlags::Invincible;
                arm.Flags |= EnemyFlags::NoHomingNc;
                arm.Flags &= ~EnemyFlags::NoHomingCo;
                arm.ArmFlags &= ~GoreaArmFlags::Bit0;
                arm.RegenTimer = 60 * 2;
                if (i == 1)
                {
                    anim = 2;
                }
                RequireReference(_model).SetAnimation(
                    anim, 0, _animSetNoMat, AnimFlags::NoLoop);
            }
        }
        _speed = Vector3::Zero;
    }

    bool Enemy24Entity::Behavior21()
    {
        if (_field240 < 0)
        {
            _field240 = static_cast<std::int32_t>(Rng::GetRandomInt2(90) + 150U) * 2;
            return true;
        }
        return false;
    }

    bool Enemy24Entity::Behavior22()
    {
        return Behavior19();
    }

    void Enemy24Entity::UpdateSpeed()
    {
        std::int32_t phase = 0;
        if (_gorea1B)
        {
            phase = SubInt32Unchecked(3, _gorea1B->PhasesLeft);
            if (phase > 2)
            {
                phase = 2;
            }
        }
        _speedFactor = ArrayAt(_speedFactors, phase);
    }

    bool Enemy24Entity::EnemyGetDrawInfo()
    {
        DrawArmRegen();
        UpdateArmMaterials();
        _lightOverride = true;
        DrawGeneric();
        Enemy28Entity& gorea1B = RequireReference(_gorea1B);
        if (TypeExtensions::TestFlag(static_cast<EnemyFlags>(gorea1B.Flags), EnemyFlags::Visible))
        {
            gorea1B.DrawSelf();
            gorea1B.Flags &= ~EnemyFlags::Visible;
            gorea1B.Flags &= ~EnemyFlags::OnRadar;
        }
        _lightOverride = false;
        return true;
    }

    void Enemy24Entity::DrawArmRegen()
    {
        for (std::int32_t i = 0; i < 2; ++i)
        {
            Enemy26Entity& arm = RequireReference(ArrayAt(_arms, i));
            if (arm.RegenTimer != 0)
            {
                arm.DrawRegen(RequireReference(_regenModel));
                Scene& scene = RequireReference(_scene);
                if (scene.ProcessFrame()
                    && scene.FrameCount() != 0 && scene.FrameCount() % 2 == 0)
                {
                    RequireReference(_regenModel).UpdateAnimFrames();
                }
                break;
            }
        }
    }

    void Enemy24Entity::UpdateArmMaterials()
    {
        Scene& scene = RequireReference(_scene);
        if (!scene.ProcessFrame())
        {
            return;
        }
        const ColorRgb white(31, 31, 31);
        Model& model = RequireReference(RequireReference(_model).Model());
        for (std::int32_t i = 0; i < 2; ++i)
        {
            const std::shared_ptr<Enemy26Entity>& arm = ArrayAt(_arms, i);
            const char* matName = i == 0 ? "L_ShoulderTarget" : "R_ShoulderTarget";
            std::shared_ptr<Material> material = model.GetMaterialByName(matName);
            Material& materialRef = RequireReference(material);
            const std::int32_t maxFrame = 10 * 2;
            const std::int32_t frame
                = SubInt32Unchecked(maxFrame, RequireReference(arm).ColorTimer());
            IncrementMaterialColors(&materialRef, white, white, frame, maxFrame);
            if (frame == maxFrame)
            {
                materialRef.AnimationFlags &= ~MatAnimFlags::DisableColor;
            }
            else
            {
                materialRef.AnimationFlags |= MatAnimFlags::DisableColor;
            }
        }
    }

    void Enemy24Entity::ResetMaterialColors()
    {
        Model& model = RequireReference(RequireReference(_model).Model());
        const std::vector<std::shared_ptr<Material>>& materials
            = RequireReference(model.Materials);
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(materials.size()); ++i)
        {
            Material& material = RequireReference(VectorAt(materials, i));
            if (material.Name == "L_ShoulderTarget" || material.Name == "R_ShoulderTarget")
            {
                material.AnimationFlags |= MatAnimFlags::DisableAlpha;
            }
            else if (material.Name != "BackTarget")
            {
                material.AnimationFlags |= MatAnimFlags::DisableColor;
                material.Ambient = ColorRgb(0, 0, 0);
                material.Diffuse = ColorRgb(0, 0, 0);
            }
        }
    }

    bool Enemy24Entity::Behavior00(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior00(); }
    bool Enemy24Entity::Behavior01(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior01(); }
    bool Enemy24Entity::Behavior02(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior02(); }
    bool Enemy24Entity::Behavior03(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior03(); }
    bool Enemy24Entity::Behavior04(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior04(); }
    bool Enemy24Entity::Behavior05(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior05(); }
    bool Enemy24Entity::Behavior06(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior06(); }
    bool Enemy24Entity::Behavior07(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior07(); }
    bool Enemy24Entity::Behavior08(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior08(); }
    bool Enemy24Entity::Behavior09(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior09(); }
    bool Enemy24Entity::Behavior10(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior10(); }
    bool Enemy24Entity::Behavior11(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior11(); }
    bool Enemy24Entity::Behavior12(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior12(); }
    bool Enemy24Entity::Behavior13(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior13(); }
    bool Enemy24Entity::Behavior14(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior14(); }
    bool Enemy24Entity::Behavior15(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior15(); }
    bool Enemy24Entity::Behavior16(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior16(); }
    bool Enemy24Entity::Behavior17(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior17(); }
    bool Enemy24Entity::Behavior18(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior18(); }
    bool Enemy24Entity::Behavior19(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior19(); }
    bool Enemy24Entity::Behavior20(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior20(); }
    bool Enemy24Entity::Behavior21(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior21(); }
    bool Enemy24Entity::Behavior22(Enemy24Entity* enemy) { return RequireEnemy(enemy).Behavior22(); }
}
