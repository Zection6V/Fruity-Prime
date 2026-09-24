#include "31_Gorea2.hpp"

#include "32_GoreaSealSphere2.hpp"
#include "33_GoreaMeteor.hpp"
#include "../../Formats/CollisionDetection.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Scene.hpp"
#include "../../Sound/Music.hpp"
#include "../../Utility/Rng.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../TriggerVolumeEntity.hpp"
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
#include <utility>

using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::TestAny;
using ::MphRead::TestFlag;
using ::OpenTK::Mathematics::Length;
using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;

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

        template <typename T>
        [[nodiscard]] T& CastReference(EntityBase* value)
        {
            T* cast = dynamic_cast<T*>(value);
            if (cast == nullptr)
            {
                throw SceneDetail::InvalidCastException();
            }
            return *cast;
        }

        [[nodiscard]] Enemy31Entity& RequireEnemy(Enemy31Entity* enemy)
        {
            return RequireReference(enemy);
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

        [[nodiscard]] std::int32_t GetFlagValue(
            Gorea2Flags value, Gorea2Flags mask, std::uint32_t shift) noexcept
        {
            return static_cast<std::int32_t>(
                static_cast<std::uint32_t>(value & mask) >> shift);
        }

        [[nodiscard]] Vector3 ScaleVector(Vector3 value, float scale) noexcept
        {
            return Vector3(value.X * scale, value.Y * scale, value.Z * scale);
        }

        [[nodiscard]] Vector3 DivideVector(Vector3 value, float divisor) noexcept
        {
            return Vector3(value.X / divisor, value.Y / divisor, value.Z / divisor);
        }

        [[nodiscard]] Vector3 WithY(Vector3 value, float y) noexcept
        {
            value.Y = y;
            return value;
        }

        [[nodiscard]] Vector3 AddY(Vector3 value, float amount) noexcept
        {
            value.Y += amount;
            return value;
        }

        [[nodiscard]] Matrix4 CreateFromAxisAngle(Vector3 axis, float angle) noexcept
        {
            axis = axis.Normalized();
            const float axisX = axis.X;
            const float axisY = axis.Y;
            const float axisZ = axis.Z;

            const float cosine = std::cos(-angle);
            const float sine = std::sin(-angle);
            const float t = 1.0F - cosine;

            const float tXX = t * axisX * axisX;
            const float tXY = t * axisX * axisY;
            const float tXZ = t * axisX * axisZ;
            const float tYY = t * axisY * axisY;
            const float tYZ = t * axisY * axisZ;
            const float tZZ = t * axisZ * axisZ;

            const float sinX = sine * axisX;
            const float sinY = sine * axisY;
            const float sinZ = sine * axisZ;

            return Matrix4(
                Vector4(tXX + cosine, tXY - sinZ, tXZ + sinY, 0.0F),
                Vector4(tXY + sinZ, tYY + cosine, tYZ - sinX, 0.0F),
                Vector4(tXZ - sinY, tYZ + sinX, tZZ + cosine, 0.0F),
                Vector4(0.0F, 0.0F, 0.0F, 1.0F));
        }

        void ScaleRow2Xyz(Matrix4& value, float scale) noexcept
        {
            value.M31 *= scale;
            value.M32 *= scale;
            value.M33 *= scale;
        }

        void SetRow3Xyz(Matrix4& value, Vector3 position) noexcept
        {
            value.M41 = position.X;
            value.M42 = position.Y;
            value.M43 = position.Z;
        }

        [[nodiscard]] bool HitMainPlayer(Enemy31Entity& enemy)
        {
            const std::int32_t slotIndex = MainPlayer().SlotIndex();
            if (slotIndex < 0
                || static_cast<std::size_t>(slotIndex) >= enemy.HitPlayers.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return enemy.HitPlayers[static_cast<std::size_t>(slotIndex)];
        }

        [[nodiscard]] Model& GetModel(ModelInstance& instance)
        {
            return RequireReference(instance.Model());
        }

        [[nodiscard]] std::int32_t AnimationIndex(ModelInstance& instance)
        {
            return (*RequireReference(instance.AnimInfo).Index)[0];
        }

        [[nodiscard]] std::int32_t AnimationFrame(ModelInstance& instance)
        {
            return (*RequireReference(instance.AnimInfo).Frame)[0];
        }
    }

    const std::array<const char*, 6> Enemy31Entity::_lightMaterialNames{
        "light1", "Light2", "Light3", "Light4", "Light5", "Light6"
    };

    const std::array<const char*, 12> Enemy31Entity::_allMaterialNames{
        "BackTarget", "Eye", "Head1", "HeadFullLit", "Torso", "ChestCore",
        "light1", "Light2", "Light3", "Light4", "Light5", "Light6"
    };

    Enemy31Entity::Enemy31Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : GoreaEnemyEntityBase(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner)),
          _spawnerField28(RequireReference(_spawner)
              .Data.Fields.S12().Field28.ToFloatVector()),
          _spawnerField34(RequireReference(_spawner)
              .Data.Fields.S12().Field34.FloatValue()),
          _spawnerField38(RequireReference(_spawner)
              .Data.Fields.S12().Field38.FloatValue())
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(19);
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
        (*processes)[15] = [this]() { State15(); };
        (*processes)[16] = [this]() { State16(); };
        (*processes)[17] = [this]() { State17(); };
        (*processes)[18] = [this]() { State18(); };
        _stateProcesses = std::move(processes);
    }

    std::uint8_t Enemy31Entity::Field244() const noexcept
    {
        return _field244;
    }

    void Enemy31Entity::EnemyInitialize()
    {
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::NoHomingNc;
        Flags |= EnemyFlags::NoHomingCo;
        Flags &= ~EnemyFlags::Invincible;
        Flags |= EnemyFlags::CollidePlayer;
        Flags |= EnemyFlags::CollideBeam;
        Flags |= EnemyFlags::NoMaxDistance;
        Flags |= EnemyFlags::OnRadar;
        SetHealthbarMessageId(3);

        EnemySpawnEntity& spawner = RequireReference(_spawner);
        SetTransform(spawner.FacingVector(),
            Vector3(0.0F, 1.0F, 0.0F), spawner.Position);
        _prevPos = Position;
        _boundingRadius = 1.0F;
        _hurtVolumeInit = CollisionVolume(Vector3::Zero, 1.0F);
        Scale = Vector3(2.0F, 2.0F, 2.0F);
        _health = _healthMax = 65535;

        _model = &SetUpModel("Gorea2_lod0");
        _model->NodeAnimIgnoreRoot = true;
        GetModel(*_model).ComputeNodeMatrices(0);
        _model->SetAnimation(7);

        _laserModel = &SetUpModel("goreaLaser");
        _laserModel->Active = false;
        _headNode = GetModel(*_model).GetNodeByName("Head");
        _laserColNormal = UpVector();
        _field20C = Position;
        _field22C = 90 * 2;
        _field22E = 120 * 2;
        _field234 = 22 * 2;
        _field23E = 180.0F;
        _field244 = 0x7F;
        GoreaFlags |= Gorea2Flags::Bit6;
        GoreaFlags |= Gorea2Flags::Bit17;

        InitTrigger();

        std::shared_ptr<EnemyInstanceEntity> enemy = EnemySpawnEntity::SpawnEnemy(
            this, MphRead::EnemyType::GoreaSealSphere2, NodeRef, _scene);
        std::shared_ptr<Enemy32Entity> sphere
            = std::dynamic_pointer_cast<Enemy32Entity>(enemy);
        if (sphere)
        {
            RequireReference(_scene).AddEntity(sphere);
            _sealSphere = std::move(sphere);
        }
    }

    void Enemy31Entity::EnemyProcess()
    {
        if (!TypeExtensions::TestFlag(
            static_cast<EnemyFlags>(Flags), EnemyFlags::Visible))
        {
            return;
        }

        const bool flagSet = TestFlag(GoreaFlags, Gorea2Flags::Bit11);
        if (!flagSet)
        {
            UpdateLaserTargeting();
            CheckLaserHit();
            CheckPlayerCollision();
            if (_field236 > 0)
            {
                --_field236;
            }
        }

        if (CheckFacingAngle(-1.0F, MainPlayer().Position))
        {
            RequireReference(_sealSphere).UpdateVisibility();
        }

        CallStateProcess();

        if (!flagSet)
        {
            UpdateMaterialColors();
            Func213D7C4();
            Func213D194();
        }
        Func213D5D0();
    }

    void Enemy31Entity::CheckPlayerCollision()
    {
        if (!HitMainPlayer(*this))
        {
            return;
        }

        const Vector3 between
            = static_cast<Vector3>(MainPlayer().Position)
            - static_cast<Vector3>(Position);
        PlayerEntity& player = MainPlayer();
        const Vector3 speed = player.Speed();
        player.SetSpeed(
            speed + DivideVector(DivideVector(between, 4.0F), 2.0F));
        MainPlayer().TakeDamage(
            10, DamageFlags::None, std::nullopt, this);
    }

    void Enemy31Entity::CreateTeleportEffect(bool useNode)
    {
        Enemy32Entity& sealSphere = RequireReference(_sealSphere);
        Vector3 spawnPos = sealSphere.Position;
        if (useNode)
        {
            const Matrix4 transform
                = GetNodeTransform(this, sealSphere.AttachNode());
            spawnPos = transform.Row3().Xyz() + _teleportDestination;
        }
        SpawnEffect(80, spawnPos);
    }

    bool Enemy31Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        _health = std::numeric_limits<std::uint16_t>::max();
        return false;
    }

    bool Enemy31Entity::EnemyGetDrawInfo()
    {
        if (TestFlag(GoreaFlags, Gorea2Flags::LaserActive))
        {
            DrawLaser();
        }
        UpdateChestMaterial();
        _timeSinceDamage = 510;
        _lightOverride = true;
        DrawGeneric();
        _lightOverride = false;
        return true;
    }

    void Enemy31Entity::UpdateChestMaterial()
    {
        Scene& scene = RequireReference(_scene);
        if (!scene.ProcessFrame())
        {
            return;
        }

        Material& material
            = RequireReference(GetModel(RequireReference(_model))
                .GetMaterialByName("ChestCore"));
        const ColorRgb white(31, 31, 31);
        const std::int32_t maxFrame = 10 * 2;
        const std::int32_t frame = RequireReference(_sealSphere).DamageTimer();
        IncrementMaterialColors(
            &material, white, white, frame, maxFrame);
    }

    void Enemy31Entity::DrawLaser()
    {
        Enemy32Entity& sealSphere = RequireReference(_sealSphere);
        Vector3 laserVec
            = _laserTargetPos - static_cast<Vector3>(sealSphere.Position);
        const float length = Length(laserVec);
        if (length <= 1.0F / 128.0F)
        {
            return;
        }

        laserVec = laserVec.Normalized();
        Vector3 unitVec(0.0F, 1.0F, 0.0F);
        const float dot = std::fabs(Vector3::Dot(unitVec, laserVec));
        if (dot > Fixed::ToFloat(4065))
        {
            unitVec = Vector3(0.0F, 0.0F, 1.0F);
        }

        const Vector3 cross1 = Vector3::Cross(laserVec, unitVec);
        const Vector3 cross2 = Vector3::Cross(cross1, laserVec);
        Matrix4 transform = GetTransformMatrix(laserVec, cross2);
        ScaleRow2Xyz(transform, length);
        SetRow3Xyz(transform, sealSphere.Position);
        UpdateTransforms(RequireReference(_laserModel), transform, 0);
        GetDrawItems(RequireReference(_laserModel), 0);
    }

    void Enemy31Entity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::Destroyed)
        {
            if (info.Sender != nullptr
                && info.Sender->Type == EntityType::EnemyInstance)
            {
                EnemyInstanceEntity& enemy
                    = CastReference<EnemyInstanceEntity>(info.Sender);
                if (enemy.EnemyType() == MphRead::EnemyType::GoreaMeteor
                    && _field242 > 0)
                {
                    --_field242;
                }
            }
        }
        else if (info.Message == Message::Gorea2Trigger)
        {
            if (info.Sender != nullptr
                && info.Sender->Type == EntityType::TriggerVolume)
            {
                TriggerVolumeEntity& trigger
                    = CastReference<TriggerVolumeEntity>(info.Sender);
                const std::int32_t value1 = GetFlagValue(
                    GoreaFlags,
                    Gorea2Flags::Bit14 | Gorea2Flags::Bit15, 14);
                const std::int32_t value2 = GetFlagValue(
                    GoreaFlags,
                    Gorea2Flags::Bit4 | Gorea2Flags::Bit5, 4);
                if (TestFlag(GoreaFlags, Gorea2Flags::Bit10)
                    || value1 == 1)
                {
                    return;
                }
                if (value2 == 0 && _field22C == 0)
                {
                    Teleport(&trigger, std::nullopt);
                }
                else if (value2 == 3)
                {
                    if (&trigger == _currentTrigger)
                    {
                        _field22C = 90 * 2;
                    }
                    else
                    {
                        Teleport(&trigger, std::nullopt);
                    }
                }
            }
        }
    }

    void Enemy31Entity::Teleport(
        TriggerVolumeEntity* trigger, std::optional<Vector3> position)
    {
        GoreaFlags &= ~(Gorea2Flags::Bit4 | Gorea2Flags::Bit5);
        GoreaFlags |= Gorea2Flags::Bit4;
        GoreaFlags |= Gorea2Flags::Bit6;
        _field22C = 15 * 2;
        _currentTrigger = trigger;
        if (trigger != nullptr)
        {
            _teleportDestination = trigger->Volume().GetCenter();
        }
        else if (position.has_value())
        {
            _teleportDestination = *position;
        }
        _soundSource.PlaySfx(SfxId::GOREA2_TELEPORT_OUT);
        CreateTeleportEffect(false);
    }

    void Enemy31Entity::UpdateLaserTargeting()
    {
        if (!TestFlag(GoreaFlags, Gorea2Flags::LaserActive))
        {
            return;
        }

        UpdateAnimFrames(RequireReference(_laserModel));

        Vector3 posToPlayer
            = static_cast<Vector3>(MainPlayer().Position) - _laserTargetPos;
        if (Length(posToPlayer) <= 0.125F)
        {
            _laserTargetPos = MainPlayer().Position;
            GoreaFlags |= Gorea2Flags::LaserOnTarget;
        }
        else
        {
            _laserTargetPos = _laserTargetPos
                + ScaleVector(posToPlayer, 1.0F / (12.0F * 2.0F));
            GoreaFlags &= ~Gorea2Flags::LaserOnTarget;
        }

        const Vector3 spherePos = RequireReference(_sealSphere).Position;
        Formats::CollisionResult res{};
        const bool blocked = Formats::CollisionDetection::CheckBetweenPoints(
            spherePos, _laserTargetPos, Formats::TestFlags::None, _scene, res);
        if (blocked)
        {
            GoreaFlags |= Gorea2Flags::LaserBlocked;
            _laserTargetPos = res.Position;
            _laserColNormal = res.Plane.Xyz();
        }
        else
        {
            GoreaFlags &= ~Gorea2Flags::LaserBlocked;
            Vector3 laserVec = _laserTargetPos - spherePos;
            if (LengthSquared(laserVec) > 0.0F)
            {
                laserVec = laserVec.Normalized();
                _laserTargetPos = _laserTargetPos
                    + ScaleVector(laserVec, 1.0F / (12.0F * 2.0F));
            }
        }
    }

    void Enemy31Entity::CheckLaserHit()
    {
        if (!TestFlag(GoreaFlags, Gorea2Flags::LaserActive))
        {
            return;
        }

        const Vector3 spherePos = RequireReference(_sealSphere).Position;
        bool laserHit = TestFlag(GoreaFlags, Gorea2Flags::LaserOnTarget);
        if (!laserHit)
        {
            Formats::CollisionResult discard{};
            if (Length(_laserTargetPos - spherePos) < 0.5F)
            {
                laserHit = true;
            }
            else if (Formats::CollisionDetection::CheckCylinderOverlapVolume(
                &MainPlayer().Volume(), spherePos, _laserTargetPos,
                0.5F, discard))
            {
                laserHit = true;
            }
        }

        if (laserHit)
        {
            std::optional<Vector3> direction{};
            Vector3 between = Func204D518(
                _laserTargetPos - spherePos, MainPlayer().UpVector());
            if (LengthSquared(between) > 1.0F / 128.0F)
            {
                between = between.Normalized();
                direction = ScaleVector(between, 1.0F / 30.0F);
            }
            MainPlayer().TakeDamage(
                55, DamageFlags::None, direction, this);
        }
    }

    void Enemy31Entity::UpdateMaterialColors()
    {
        Scene& scene = RequireReference(_scene);
        if (scene.FrameCount() == 0 || scene.FrameCount() % 2 != 0)
        {
            return;
        }

        Model& model = GetModel(RequireReference(_model));
        for (std::int32_t i = 0;
            i < static_cast<std::int32_t>(_lightMaterialNames.size()); ++i)
        {
            const ColorRgb color
                = ((1 << i) & _field244) != 0
                ? ColorRgb(31, 31, 31)
                : ColorRgb();
            UpdateMaterialColor(
                model.GetMaterialByName(
                    _lightMaterialNames[static_cast<std::size_t>(i)]).get(),
                color);
        }
    }

    void Enemy31Entity::UpdateMaterialColor(Material* material, ColorRgb color)
    {
        Material& materialRef = RequireReference(material);

        std::uint8_t red = materialRef.Diffuse.Red;
        if (red < color.Red)
        {
            ++red;
        }
        else if (red > color.Red)
        {
            --red;
        }

        std::uint8_t green = materialRef.Diffuse.Green;
        if (green < color.Green)
        {
            ++green;
        }
        else if (green > color.Green)
        {
            --green;
        }

        std::uint8_t blue = materialRef.Diffuse.Blue;
        if (blue < color.Blue)
        {
            ++blue;
        }
        else if (blue > color.Blue)
        {
            --blue;
        }

        materialRef.Diffuse = ColorRgb(red, green, blue);
    }

    void Enemy31Entity::Func213D194()
    {
        const std::int32_t value = GetFlagValue(
            GoreaFlags,
            Gorea2Flags::Bit4 | Gorea2Flags::Bit5, 4);
        if (value == 0)
        {
            if (_field22C > 0)
            {
                --_field22C;
            }
        }
        else if (value == 1)
        {
            Func213D3C8();
        }
        else if (value == 2)
        {
            Func213D30C();
        }
        else if (value == 3)
        {
            Func213D204();
        }
    }

    void Enemy31Entity::InterpolateAlpha(
        std::int32_t frame, std::int32_t frameCount, std::int32_t color)
    {
        if (frame >= 0 && frame <= frameCount)
        {
            Model& model = GetModel(RequireReference(_model));
            const std::uint8_t alpha = RequireReference(
                model.GetMaterialByName(_allMaterialNames[0])).Alpha;
            const std::uint8_t value = InterpolateColor(
                frame, frameCount,
                color - static_cast<std::int32_t>(alpha));

            for (std::int32_t i = 0;
                i < static_cast<std::int32_t>(_allMaterialNames.size()); ++i)
            {
                Material& material = RequireReference(
                    model.GetMaterialByName(
                        _allMaterialNames[static_cast<std::size_t>(i)]));
                material.Alpha = static_cast<std::uint8_t>(
                    static_cast<std::uint32_t>(alpha)
                    + static_cast<std::uint32_t>(value));
            }
        }
    }

    void Enemy31Entity::Func213D3C8()
    {
        if (_field22C > 0)
        {
            --_field22C;
        }
        InterpolateAlpha(15 * 2 - _field22C, 15 * 2, 0);

        if (_field22C == 0)
        {
            _soundSource.PlaySfx(SfxId::GOREA2_TELEPORT_IN_SCR);
            if (TestFlag(GoreaFlags, Gorea2Flags::Bit13))
            {
                GoreaFlags &= ~Gorea2Flags::Bit13;
                RequireReference(_sealSphere).Flags
                    &= ~EnemyFlags::Invincible;
            }
            GoreaFlags &= ~(Gorea2Flags::Bit4 | Gorea2Flags::Bit5);
            GoreaFlags |= Gorea2Flags::Bit5;
            _field22C = 15 * 2;
            _field20C = _teleportDestination;

            Vector3 facing = WithY(
                static_cast<Vector3>(MainPlayer().Position) - _field20C,
                0.0F);
            if (LengthSquared(facing) > 1.0F / 128.0F)
            {
                facing = facing.Normalized();
            }
            else
            {
                facing = Vector3(0.0F, 0.0F, 1.0F);
            }
            SetTransform(facing, UpVector(), Position);
            CreateTeleportEffect(true);
        }
    }

    void Enemy31Entity::Func213D30C()
    {
        if (_field22C > 0)
        {
            --_field22C;
        }
        InterpolateAlpha(15 * 2 - _field22C, 15 * 2, 31);

        if (_field22C == 0)
        {
            _soundSource.PlaySfx(SfxId::GOREA2_TELEPORT_IN_SCR);
            if (TestFlag(GoreaFlags, Gorea2Flags::Bit13))
            {
                GoreaFlags &= ~Gorea2Flags::Bit13;
                RequireReference(_sealSphere).Flags
                    &= ~EnemyFlags::Invincible;
            }
            if (TestFlag(GoreaFlags, Gorea2Flags::Bit6))
            {
                GoreaFlags |= Gorea2Flags::Bit4 | Gorea2Flags::Bit5;
            }
            else
            {
                GoreaFlags &= ~(Gorea2Flags::Bit4 | Gorea2Flags::Bit5);
            }
            _field22C = 90 * 2;
        }
    }

    void Enemy31Entity::Func213D204()
    {
        if (_field23C > 0)
        {
            --_field23C;
        }
        InterpolateAlpha(15 * 2 - _field23C, 15 * 2, 31);

        if (_field22C > 0)
        {
            --_field22C;
        }

        _field1E8 = SeekTargetSetAnim(
            _field1E8, AnimationIndex(_models[0]));

        if (TestFlag(GoreaFlags, Gorea2Flags::Bit13))
        {
            GoreaFlags &= ~Gorea2Flags::Bit13;
            RequireReference(_sealSphere).Flags &= ~EnemyFlags::Invincible;
        }

        if (_field22C == 0)
        {
            GoreaFlags &= ~(Gorea2Flags::Bit4 | Gorea2Flags::Bit5);
            if (!TestFlag(GoreaFlags, Gorea2Flags::Bit10))
            {
                GoreaFlags |= Gorea2Flags::Bit4;
                _field22C = 15 * 2;
                _teleportDestination = Func21405FC();
                CreateTeleportEffect(false);
            }
            GoreaFlags &= ~Gorea2Flags::Bit6;
        }
    }

    void Enemy31Entity::Func213D5D0()
    {
        UpdateHoverPosition();
        _field20C = _field20C + _speed;
    }

    void Enemy31Entity::UpdateHoverPosition()
    {
        _field23E += 4.0F;
        if (_field23E >= 360.0F)
        {
            _field23E -= 360.0F;
            GoreaFlags ^= Gorea2Flags::Bit7;
        }

        const float v5 = std::sin(DegreesToRadians(_field23E)) * 1.5F;
        float v6 = _field23E / 360.0F * 3.0F;
        if (TestFlag(GoreaFlags, Gorea2Flags::Bit7))
        {
            v6 = 3.0F - v6;
        }

        _field240 += 2.0F;
        if (_field240 >= 360.0F)
        {
            _field240 -= 360.0F;
        }

        const float rand1
            = (static_cast<std::int32_t>(Rng::GetRandomInt2(227)) - 113)
                / 4096.0F;
        const Matrix4 mtx = CreateFromAxisAngle(
            FacingVector(), DegreesToRadians(_field240));
        Vector3 vec = Matrix::Vec3MultMtx3(
            Vector3::Cross(UpVector(), FacingVector()), mtx);
        Position = _field20C
            + ScaleVector(vec, v6 - 1.5F - rand1);

        const float rand2
            = (static_cast<std::int32_t>(Rng::GetRandomInt2(227)) - 113)
                / 4096.0F;
        vec = Matrix::Vec3MultMtx3(UpVector(), mtx);
        Position = static_cast<Vector3>(Position)
            + ScaleVector(vec, v5 - rand2);
    }

    void Enemy31Entity::State00()
    {
        ModelInstance& model = RequireReference(_model);
        if (AnimationIndex(model) != 9)
        {
            _soundSource.PlaySfx(SfxId::GOREA2_ATTACK1A);
            model.SetAnimation(9, AnimFlags::NoLoop);
        }
        if (CallSubroutine<Enemy31Entity>(
            Metadata::Enemy31Subroutines, this))
        {
            model.SetAnimation(7);
            GoreaFlags |= Gorea2Flags::Bit17;
            RequireReference(_sealSphere).Flags
                &= ~EnemyFlags::Invincible;
        }
    }

    void Enemy31Entity::State01()
    {
        if (TestFlag(GoreaFlags, Gorea2Flags::Bit17))
        {
            GoreaFlags &= ~Gorea2Flags::Bit17;
            const std::int32_t value = GetFlagValue(
                GoreaFlags,
                Gorea2Flags::Bit14 | Gorea2Flags::Bit15, 14);
            _field230 = value == 0 ? 90 * 2 : 0;
        }

        if (CallSubroutine<Enemy31Entity>(
            Metadata::Enemy31Subroutines, this))
        {
            GoreaFlags |= Gorea2Flags::Bit17;
        }
    }

    void Enemy31Entity::State02()
    {
        if (CallSubroutine<Enemy31Entity>(
            Metadata::Enemy31Subroutines, this))
        {
            GoreaFlags |= Gorea2Flags::Bit17;
        }
    }

    void Enemy31Entity::State03()
    {
        Vector3 effectPos = RequireReference(_sealSphere).Position;
        if (!_chargeEffect)
        {
            _chargeEffect = SpawnEffectGetEntry(210, effectPos, true);
        }
        if (_chargeEffect)
        {
            _chargeEffect->Transform(
                FacingVector(), UpVector(), effectPos);
        }

        const Matrix4 transform
            = GetNodeTransform(this, _headNode.get());
        effectPos = transform.Row3().Xyz();

        if (!_flashEffect)
        {
            _flashEffect = SpawnEffectGetEntry(104, effectPos, false);
        }
        if (_flashEffect)
        {
            _flashEffect->Transform(
                FacingVector(), UpVector(), effectPos);
        }

        if (CallSubroutine<Enemy31Entity>(
            Metadata::Enemy31Subroutines, this))
        {
            if (_chargeEffect)
            {
                RequireReference(_scene)
                    .DetachEffectEntry(_chargeEffect, true);
                _chargeEffect.reset();
            }
            if (_flashEffect)
            {
                RequireReference(_scene)
                    .DetachEffectEntry(_flashEffect, true);
                _flashEffect.reset();
            }
            GoreaFlags |= Gorea2Flags::Bit17;
        }
    }

    void Enemy31Entity::State04()
    {
        ModelInstance& model = RequireReference(_model);
        Enemy32Entity& sealSphere = RequireReference(_sealSphere);

        if (TestFlag(GoreaFlags, Gorea2Flags::Bit17))
        {
            GoreaFlags &= ~Gorea2Flags::Bit17;
            GoreaFlags |= Gorea2Flags::LaserActive;
            _field230 = 45 * 2;
            _laserTargetPos = AddY(sealSphere.Position, -25.0F);
            UpdateLaserTargeting();
            _soundSource.PlaySfx(
                SfxId::GOREA2_ATTACK1B, true);
        }

        if (TestFlag(GoreaFlags, Gorea2Flags::LaserBlocked)
            && !_colEffect)
        {
            _colEffect = SpawnEffectGetEntry(
                224, _laserTargetPos, true);
        }
        if (_colEffect)
        {
            const Vector3 effectFacing
                = Func21418EC(_laserColNormal, FacingVector());
            _colEffect->Transform(
                effectFacing, _laserColNormal, _laserTargetPos);
        }

        if (AnimationIndex(model) == 0 && IsAtEndFrame())
        {
            model.SetAnimation(7);
        }

        if (CallSubroutine<Enemy31Entity>(
            Metadata::Enemy31Subroutines, this))
        {
            GoreaFlags &= ~Gorea2Flags::LaserActive;
            _soundSource.StopSfx(SfxId::GOREA2_ATTACK1B);
            GoreaFlags |= Gorea2Flags::Bit17;
            if (_colEffect)
            {
                RequireReference(_scene)
                    .DetachEffectEntry(_colEffect, true);
                _colEffect.reset();
            }
        }
    }

    Vector3 Enemy31Entity::Func21418EC(Vector3 vec1, Vector3 vec2)
    {
        Vector3 cross = Vector3::Cross(vec1, vec2);
        if (LengthSquared(cross) <= 1.0F / 128.0F)
        {
            cross = Vector3::Cross(
                vec1, Vector3(1.0F, 0.0F, 0.0F));
            if (LengthSquared(cross) <= 1.0F / 128.0F)
            {
                cross = Vector3::Cross(
                    vec1, Vector3(0.0F, 1.0F, 0.0F));
                if (LengthSquared(cross) <= 1.0F / 128.0F)
                {
                    cross = Vector3::Cross(
                        vec1, Vector3(0.0F, 0.0F, 1.0F));
                    if (LengthSquared(cross) <= 1.0F / 128.0F)
                    {
                        return Vector3::Zero;
                    }
                }
            }
        }
        return Vector3::Cross(cross.Normalized(), vec1);
    }

    void Enemy31Entity::State05()
    {
        State02();
    }

    void Enemy31Entity::State06()
    {
        if (AnimationFrame(RequireReference(_model)) == 27
            && RequireReference(_scene).FrameCount() % 2 == 0)
        {
            ShootMeteor();
        }
        if (CallSubroutine<Enemy31Entity>(
            Metadata::Enemy31Subroutines, this))
        {
            GoreaFlags |= Gorea2Flags::Bit17;
            RequireReference(_model).SetAnimation(7);
        }
    }

    void Enemy31Entity::ShootMeteor()
    {
        std::shared_ptr<EnemyInstanceEntity> enemy
            = EnemySpawnEntity::SpawnEnemy(
                this, MphRead::EnemyType::GoreaMeteor, NodeRef, _scene);
        std::shared_ptr<Enemy33Entity> meteor
            = std::dynamic_pointer_cast<Enemy33Entity>(enemy);
        if (meteor)
        {
            RequireReference(_scene).AddEntity(meteor);
            ++_field242;

            const char* const nodeName
                = TestFlag(GoreaFlags, Gorea2Flags::Bit8)
                ? "L_BodySpike"
                : "R_BodySpike";
            std::shared_ptr<Node> node
                = GetModel(RequireReference(_model))
                    .GetNodeByName(nodeName);
            const Matrix4 transform
                = GetNodeTransform(this, node.get());
            const Vector3 position = transform.Row3().Xyz();
            meteor->InitializePosition(position);
            SpawnEffect(174, position);
        }
    }

    void Enemy31Entity::State07()
    {
        if (CallSubroutine<Enemy31Entity>(
            Metadata::Enemy31Subroutines, this))
        {
            _field234 = 22 * 2;
            GoreaFlags |= Gorea2Flags::Bit17;
        }
    }

    void Enemy31Entity::State08()
    {
        if (TestFlag(GoreaFlags, Gorea2Flags::Bit17))
        {
            GoreaFlags &= ~Gorea2Flags::Bit17;
            const std::int32_t value = GetFlagValue(
                GoreaFlags,
                Gorea2Flags::Bit14 | Gorea2Flags::Bit15, 14);
            _field230 = value == 0 ? 90 * 2 : 0;
        }

        if (_field236 == 0)
        {
            const std::uint32_t timer
                = Rng::GetRandomInt2(300) + 300U;
            _field236 = static_cast<std::int32_t>(timer) * 2;
            SearchAndTeleport(1, false);
        }

        if (CallSubroutine<Enemy31Entity>(
            Metadata::Enemy31Subroutines, this))
        {
            GoreaFlags |= Gorea2Flags::Bit17;
            GoreaFlags |= Gorea2Flags::Bit14 | Gorea2Flags::Bit15;
            _field22C = _field236;
            if (_field23C == 0)
            {
                _field23C = std::max(_field22C, 15 * 2);
            }
        }
    }

    void Enemy31Entity::SearchAndTeleport(
        std::int32_t mode, bool checkCollision)
    {
        TriggerVolumeEntity* trigger
            = FindTrigger(mode, checkCollision);
        if (trigger != nullptr)
        {
            Teleport(trigger, std::nullopt);
        }
        else
        {
            const float y = Position.Y;
            const Vector3 current = Position;
            Vector3 position(-current.X, -current.Y, -current.Z);
            position = WithY(position, y);
            Teleport(nullptr, position);
        }
    }

    TriggerVolumeEntity* Enemy31Entity::FindTrigger(
        std::int32_t mode, bool checkCollision)
    {
        TriggerVolumeEntity* minTrigger = nullptr;
        TriggerVolumeEntity* maxTrigger = nullptr;
        TriggerVolumeEntity* randTrigger = nullptr;
        float minDist = std::numeric_limits<float>::max();
        float maxDist = 0.0F;
        const std::int32_t value = GetFlagValue(
            GoreaFlags,
            Gorea2Flags::Bit14 | Gorea2Flags::Bit15, 14);

        auto enumerator
            = RequireReference(_scene)
                .GetTriggerVolumeEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<TriggerVolumeEntity> triggerPtr
                = enumerator.Current();
            TriggerVolumeEntity& trigger
                = RequireReference(triggerPtr);

            const auto data1 = trigger.Data();
            if (data1.ParentMessage != Message::Gorea2Trigger
                && data1.ChildMessage != Message::Gorea2Trigger)
            {
                continue;
            }

            if (value != 1 && randTrigger == nullptr)
            {
                randTrigger = &trigger;
            }

            if (&trigger == _currentTrigger)
            {
                continue;
            }

            const Vector3 triggerPos
                = trigger.Data().Header.Position.ToFloatVector();
            const Vector3 between
                = static_cast<Vector3>(MainPlayer().Position) - triggerPos;
            const float lengthSquared = LengthSquared(between);
            const bool randomChance
                = (Rng::GetRandomInt2(255) & 1U) != 0;

            if (value != 1 || triggerPos.Y >= _spawnerField38)
            {
                if (lengthSquared < minDist)
                {
                    minTrigger = &trigger;
                    minDist = lengthSquared;
                }
                if (lengthSquared > maxDist)
                {
                    maxTrigger = &trigger;
                    maxDist = lengthSquared;
                }
                if (randomChance
                    && (value != 1 || randTrigger == nullptr))
                {
                    randTrigger = &trigger;
                }
            }
        }

        TriggerVolumeEntity* chosen = nullptr;
        if (mode == 0)
        {
            chosen = randTrigger;
        }
        else if (mode == 1)
        {
            chosen = maxTrigger;
        }
        else if (mode == 2)
        {
            chosen = minTrigger;
        }

        if (chosen != nullptr)
        {
            const Vector3 chosenPos
                = chosen->Data().Header.Position.ToFloatVector();
            Formats::CollisionResult discard{};
            if (checkCollision
                && Formats::CollisionDetection::CheckBetweenPoints(
                    chosenPos, MainPlayer().Position,
                    Formats::TestFlags::None, _scene, discard))
            {
                return nullptr;
            }
            return chosen;
        }
        return nullptr;
    }

    void Enemy31Entity::State09()
    {
        if (CallSubroutine<Enemy31Entity>(
            Metadata::Enemy31Subroutines, this))
        {
            SearchAndTeleport(1, false);
            const std::uint32_t timer
                = Rng::GetRandomInt2(300) + 300U;
            _field236 = static_cast<std::int32_t>(timer) * 2;
            GoreaFlags |= Gorea2Flags::Bit13;
            GoreaFlags |= Gorea2Flags::Bit17;
            RequireReference(_model).SetAnimation(7);
        }
    }

    void Enemy31Entity::State10()
    {
        State02();
    }

    void Enemy31Entity::State11()
    {
        State03();
    }

    void Enemy31Entity::State12()
    {
        State04();
    }

    void Enemy31Entity::State13()
    {
        if (CallSubroutine<Enemy31Entity>(
            Metadata::Enemy31Subroutines, this))
        {
            GoreaFlags |= Gorea2Flags::Bit17;
            _field236 = 0;
        }
    }

    void Enemy31Entity::State14()
    {
        if (TestFlag(GoreaFlags, Gorea2Flags::Bit17))
        {
            GoreaFlags &= ~Gorea2Flags::Bit16;
            GoreaFlags &= ~Gorea2Flags::Bit17;
            RequireReference(_model).SetAnimation(
                8, AnimFlags::NoLoop);
        }

        if (CallSubroutine<Enemy31Entity>(
            Metadata::Enemy31Subroutines, this))
        {
            GoreaFlags |= Gorea2Flags::Bit17;
            SearchAndTeleport(0, false);
            RequireReference(_model).SetAnimation(7);
        }
    }

    void Enemy31Entity::State15()
    {
        if (CallSubroutine<Enemy31Entity>(
            Metadata::Enemy31Subroutines, this))
        {
            GoreaFlags |= Gorea2Flags::Bit17;
            RequireReference(_model).SetAnimation(7);
        }
    }

    void Enemy31Entity::State16()
    {
        State15();
    }

    void Enemy31Entity::State17()
    {
        if (!TestFlag(GoreaFlags, Gorea2Flags::Bit12))
        {
            GoreaFlags |= Gorea2Flags::Bit12;
            _scanId = 0;
            Flags &= ~EnemyFlags::CollidePlayer;
            Flags &= ~EnemyFlags::CollideBeam;
            Flags |= EnemyFlags::Invincible;
            Flags |= EnemyFlags::NoHomingNc;
            Flags |= EnemyFlags::NoHomingCo;
            Flags &= ~EnemyFlags::OnRadar;
            _health = 1;
            RequireReference(_sealSphere).SetDead();
            RequireReference(_scene).StartMovie(
                Movie::GoodEnding,
                FadeType::FadeOutInWhite,
                60.0F / 30.0F,
                FadeType::FadeOutBlack,
                0.0F,
                static_cast<AfterMovie>(2));
        }

        if (Behavior03())
        {
            RequireReference(_model).SetAnimation(7);
        }
    }

    void Enemy31Entity::State18()
    {
        State02();
        _state2 = _field243;
    }

    void Enemy31Entity::InitTrigger()
    {
        const CollisionVolume volume(
            _spawnerField28, _spawnerField34);
        if (volume.TestPoint(Position))
        {
            Func213D7C4();
        }
        else
        {
            TriggerVolumeEntity* found = nullptr;
            auto enumerator
                = RequireReference(_scene)
                    .GetTriggerVolumeEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                std::shared_ptr<TriggerVolumeEntity> triggerPtr
                    = enumerator.Current();
                TriggerVolumeEntity& trigger
                    = RequireReference(triggerPtr);
                const auto data = trigger.Data();
                if ((data.ParentMessage == Message::Gorea2Trigger
                        || data.ChildMessage == Message::Gorea2Trigger)
                    && trigger.Volume().TestPoint(Position))
                {
                    found = &trigger;
                    break;
                }
            }

            if (found != nullptr)
            {
                _currentTrigger = found;
                GoreaFlags |= Gorea2Flags::Bit4 | Gorea2Flags::Bit5;
                _field22C = 90 * 2;
            }
        }
    }

    void Enemy31Entity::Func213D7C4()
    {
        if (!TestAny(
            GoreaFlags,
            Gorea2Flags::Bit4
                | Gorea2Flags::Bit5
                | Gorea2Flags::Bit10))
        {
            Func213D80C();
        }
    }

    void Enemy31Entity::Func213D80C()
    {
        Vector3 vec = Func21405FC();
        const Vector3 between = _field20C - vec;
        if (LengthSquared(between) > 1.0F / (3.0F * 2.0F))
        {
            vec = Func213F9B8(vec).Normalized();
            _field20C = _field20C
                + ScaleVector(vec, 1.0F / (3.0F * 2.0F));
        }
        else
        {
            _field20C = vec;
        }

        const Vector3 toPos
            = (_field20C - _spawnerField28).Normalized();
        _field20C = _spawnerField28
            + ScaleVector(toPos, _spawnerField34);

        Vector3 toPlayer
            = static_cast<Vector3>(Position)
            - static_cast<Vector3>(MainPlayer().Position);
        if (LengthSquared(toPlayer) > 1.0F / 128.0F)
        {
            toPlayer = toPlayer.Normalized();
        }
        Func213D974(toPlayer);
    }

    Vector3 Enemy31Entity::Func213F9B8(Vector3 vec)
    {
        const Vector3 between1 = vec - _field20C;
        const Vector3 between2 = _field20C - _spawnerField28;
        vec = Func204D518(between1, between2);
        if (LengthSquared(vec) >= 1.0F / 128.0F)
        {
            return vec;
        }
        return Func213FA58();
    }

    Vector3 Enemy31Entity::Func204D518(Vector3 vec, Vector3 axis)
    {
        return vec - Func204D57C(vec, axis);
    }

    Vector3 Enemy31Entity::Func204D57C(Vector3 vec, Vector3 axis)
    {
        const float dot1 = Vector3::Dot(axis, axis);
        if (dot1 == 0.0F)
        {
            return Vector3::Zero;
        }
        const float dot2 = Vector3::Dot(vec, axis);
        return DivideVector(ScaleVector(axis, dot2), dot1);
    }

    Vector3 Enemy31Entity::Func213FA58()
    {
        const Vector3 between = _field20C - _spawnerField28;
        Vector3 unitVec(0.0F, 1.0F, 0.0F);
        Vector3 vec = Func204D518(between, unitVec);
        if (LengthSquared(vec) < 1.0F / 128.0F)
        {
            unitVec = Vector3(1.0F, 0.0F, 0.0F);
            vec = Func204D518(between, unitVec);
        }
        return Vector3::Cross(vec, unitVec);
    }

    void Enemy31Entity::Func213D974(Vector3 toPlayer)
    {
        if (!MainPlayer().Field6D0())
        {
            toPlayer = Vector3(
                -toPlayer.X, -toPlayer.Y, -toPlayer.Z);
        }

        const Vector3 vec
            = Func204D518(toPlayer, UpVector());
        if (LengthSquared(vec) > 0.0F)
        {
            _field1E8 = vec.Normalized();
            _field1E8 = SeekTargetSetAnim(
                _field1E8, AnimationIndex(_models[0]));
        }
    }

    Vector3 Enemy31Entity::Func21405FC()
    {
        Vector3 toPos
            = static_cast<Vector3>(MainPlayer().Position)
            - _spawnerField28;
        if (LengthSquared(toPos) <= 1.0F / 128.0F)
        {
            return Vector3::Zero;
        }
        toPos = toPos.Normalized();
        return Func204E2A8(
            _spawnerField28, toPos, _spawnerField34);
    }

    Vector3 Enemy31Entity::Func204E2A8(
        Vector3 vec1, Vector3 vec2, float a4)
    {
        const float dot1 = LengthSquared(vec2);
        const float v10
            = -(dot1 * -(a4 * a4) * 4.0F);
        if (v10 < 0.0F)
        {
            return Vector3::Zero;
        }

        float v11 = 0.0F;
        if (v10 > 4.0F)
        {
            const float v12 = std::sqrt(v10);
            const float v15 = v12 / (dot1 * 2.0F);
            const float v17 = -v12 / (dot1 * 2.0F);
            v11 = v15;
            if (v17 > 0.0F && v17 < v15)
            {
                v11 = v17;
            }
        }
        return vec1 + ScaleVector(vec2, v11);
    }

    bool Enemy31Entity::Func214080C()
    {
        const std::int32_t value = GetFlagValue(
            GoreaFlags,
            Gorea2Flags::Bit4 | Gorea2Flags::Bit5, 4);
        return value == 1 || value == 2;
    }

    void Enemy31Entity::UpdatePhase()
    {
        Enemy32Entity& sealSphere = RequireReference(_sealSphere);
        if (sealSphere.Damage() <= 210)
        {
            GoreaFlags
                &= ~(Gorea2Flags::Bit14 | Gorea2Flags::Bit15);
        }
        else if (sealSphere.Damage() <= 503)
        {
            GoreaFlags
                &= ~(Gorea2Flags::Bit14 | Gorea2Flags::Bit15);
            GoreaFlags |= Gorea2Flags::Bit14;
        }
        else
        {
            GoreaFlags
                &= ~(Gorea2Flags::Bit14 | Gorea2Flags::Bit15);
            GoreaFlags |= Gorea2Flags::Bit15;
        }
    }

    bool Enemy31Entity::BehaviorXX()
    {
        return AnimationEnded();
    }

    bool Enemy31Entity::Behavior00()
    {
        return true;
    }

    bool Enemy31Entity::Behavior01()
    {
        return true;
    }

    bool Enemy31Entity::Behavior02()
    {
        if (TestFlag(GoreaFlags, Gorea2Flags::Bit11))
        {
            SpawnEffect(72, RequireReference(_sealSphere).Position);
            Music::Stop();
            _soundSource.PlaySfx(
                SfxId::GOREA2_DEATH_SCR,
                false, false, -1.0F, true);
            return true;
        }
        return false;
    }

    bool Enemy31Entity::Behavior03()
    {
        const std::int32_t value = GetFlagValue(
            GoreaFlags,
            Gorea2Flags::Bit4 | Gorea2Flags::Bit5, 4);
        return value == 0 || value == 3;
    }

    bool Enemy31Entity::Behavior04()
    {
        if (TestFlag(GoreaFlags, Gorea2Flags::Bit16))
        {
            _field243 = _state1;
            return true;
        }
        return false;
    }

    bool Enemy31Entity::Behavior05()
    {
        const std::int32_t damage
            = RequireReference(_sealSphere).Damage();

        std::int32_t v5 = 0;
        if (damage > 720)
        {
            v5 = 6;
        }
        else if (damage > 600)
        {
            v5 = 5;
        }
        else if (damage > 480)
        {
            v5 = 4;
        }
        else if (damage > 360)
        {
            v5 = 3;
        }
        else if (damage > 240)
        {
            v5 = 2;
        }
        else if (damage > 120)
        {
            v5 = 1;
        }

        if (v5 != 0
            && (_field244 & (1 << (v5 - 1))) != 0)
        {
            const std::int32_t clear
                = 2 * (1 << (v5 - 1)) - 1;
            _field244 = static_cast<std::uint8_t>(
                _field244
                & static_cast<std::uint8_t>(~clear));
            _field232 = 0;
            _field243 = _state1;
            _soundSource.StopSfx(SfxId::GOREA2_DAMAGE1);
            _soundSource.PlaySfx(SfxId::GOREA2_DAMAGE2_SCR);
            return true;
        }
        return false;
    }

    bool Enemy31Entity::Behavior06()
    {
        if (_field232 > 0)
        {
            --_field232;
            if (_field232 == 0)
            {
                std::int32_t index = static_cast<std::int32_t>(
                    std::countr_zero(
                        static_cast<std::uint32_t>(_field244)));
                if (index > 6)
                {
                    index = 6;
                }
                if (index != 0)
                {
                    _field244 = static_cast<std::uint8_t>(
                        _field244
                        | static_cast<std::uint8_t>(
                            1U << (index - 1)));
                    _field232 = 0;
                    RequireReference(_sealSphere)
                        .SetDamage(120 * (index - 1));
                    UpdatePhase();
                    _soundSource.PlaySfx(SfxId::GOREA2_REGEN);
                    RequireReference(_model).SetAnimation(
                        7, AnimFlags::NoLoop);
                    return true;
                }
            }
        }
        return false;
    }

    bool Enemy31Entity::Behavior07()
    {
        ModelInstance& model = _models[0];
        if (AnimationIndex(model) == 0)
        {
            return AnimationFrame(model) >= 26;
        }

        if (AnimationIndex(model) != 4)
        {
            RequireReference(_model).SetAnimation(
                4, AnimFlags::NoLoop);
        }
        else if (IsAtEndFrame())
        {
            _soundSource.PlaySfx(SfxId::GOREA2_ATTACK1A);
            RequireReference(_model).SetAnimation(
                0, AnimFlags::NoLoop);
        }
        return false;
    }

    bool Enemy31Entity::Behavior08()
    {
        return !RequireReference(_sealSphere).Visible();
    }

    bool Enemy31Entity::Behavior09()
    {
        if (Func2140844())
        {
            return false;
        }

        if (Behavior11())
        {
            _field234 = 22 * 2;
        }
        else
        {
            if (_field234 > 0)
            {
                --_field234;
            }
            if (_field234 == 0)
            {
                const std::int32_t value = GetFlagValue(
                    GoreaFlags,
                    Gorea2Flags::Bit4 | Gorea2Flags::Bit5, 4);
                if (value == 0 || value == 3)
                {
                    TriggerVolumeEntity* trigger
                        = FindTrigger(2, true);
                    if (trigger != nullptr)
                    {
                        Teleport(trigger, std::nullopt);
                        _field234 = _state1;
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool Enemy31Entity::Func2140844()
    {
        const std::int32_t value = GetFlagValue(
            GoreaFlags,
            Gorea2Flags::Bit14 | Gorea2Flags::Bit15, 14);
        if (value != 0
            && TestFlag(GoreaFlags, Gorea2Flags::Bit9))
        {
            return MainPlayer().CurrentWeapon()
                == BeamType::OmegaCannon;
        }
        return false;
    }

    bool Enemy31Entity::Behavior10()
    {
        if (_field230 > 0)
        {
            --_field230;
        }
        if (_field230 == 0)
        {
            _soundSource.PlaySfx(SfxId::GOREA2_ATTACK1B);
            GoreaFlags &= ~Gorea2Flags::LaserActive;
            _field230 = 0;
            return true;
        }
        return false;
    }

    bool Enemy31Entity::Behavior11()
    {
        return RequireReference(_sealSphere).Visible();
    }

    bool Enemy31Entity::Behavior12()
    {
        if (_field230 > 0)
        {
            --_field230;
        }
        if (_field230 == 0)
        {
            const float dist = Length(
                static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position));
            if (dist < (MainPlayer().Field6D0()
                ? 130.0F
                : 50.0F))
            {
                return true;
            }
        }
        return false;
    }

    bool Enemy31Entity::Behavior13()
    {
        if (Func2140844() && !Func214080C())
        {
            Enemy32Entity& sealSphere
                = RequireReference(_sealSphere);
            sealSphere.Flags |= EnemyFlags::Invincible;
            RequireReference(_model).SetAnimation(
                10, AnimFlags::NoLoop);
            SpawnEffect(225, sealSphere.Position);
            GoreaFlags |= Gorea2Flags::Bit10;
            return true;
        }
        return false;
    }

    bool Enemy31Entity::Behavior14()
    {
        if (Behavior11())
        {
            return false;
        }
        const bool result = Func2140390();
        if (result)
        {
            _soundSource.PlaySfx(
                SfxId::GOREA2_ATTACK2_SCR);
        }
        return result;
    }

    bool Enemy31Entity::Func2140390()
    {
        if (Func21403FC())
        {
            if (_field22E > 0)
            {
                --_field22E;
            }
            if (_field22E == 0 && _field242 < 2)
            {
                _field22E = 120 * 2;
                Func2140414();
                _field243 = _state1;
                return true;
            }
        }
        return false;
    }

    bool Enemy31Entity::Func21403FC()
    {
        const std::int32_t value = GetFlagValue(
            GoreaFlags,
            Gorea2Flags::Bit14 | Gorea2Flags::Bit15, 14);
        return value != 0;
    }

    void Enemy31Entity::Func2140414()
    {
        std::int32_t animId = 3;
        GoreaFlags &= ~Gorea2Flags::Bit8;
        if (Rng::GetRandomInt2(2) == 1)
        {
            GoreaFlags |= Gorea2Flags::Bit8;
            animId = 2;
        }
        RequireReference(_model).SetAnimation(
            animId, AnimFlags::NoLoop);
    }

    bool Enemy31Entity::Behavior15()
    {
        if (RequireReference(_sealSphere).Visible())
        {
            return !Func214080C();
        }
        return false;
    }

    bool Enemy31Entity::Behavior16()
    {
        const bool result = !Func2140844();
        if (result)
        {
            GoreaFlags &= ~Gorea2Flags::Bit10;
        }
        return result;
    }

    bool Enemy31Entity::Behavior17()
    {
        if (Func21403FC())
        {
            return false;
        }

        if (Behavior11())
        {
            _field234 = 22 * 2;
        }
        else
        {
            if (_field234 > 0)
            {
                --_field234;
            }
            if (_field234 == 0)
            {
                const std::int32_t value = GetFlagValue(
                    GoreaFlags,
                    Gorea2Flags::Bit4 | Gorea2Flags::Bit5, 4);
                if (value == 0 || value == 3)
                {
                    TriggerVolumeEntity* trigger
                        = FindTrigger(2, false);
                    if (trigger != nullptr)
                    {
                        Teleport(trigger, std::nullopt);
                        _field234 = _state1;
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool Enemy31Entity::BehaviorXX(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).BehaviorXX();
    }

    bool Enemy31Entity::Behavior00(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy31Entity::Behavior01(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy31Entity::Behavior02(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy31Entity::Behavior03(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }

    bool Enemy31Entity::Behavior04(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior04();
    }

    bool Enemy31Entity::Behavior05(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior05();
    }

    bool Enemy31Entity::Behavior06(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior06();
    }

    bool Enemy31Entity::Behavior07(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior07();
    }

    bool Enemy31Entity::Behavior08(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior08();
    }

    bool Enemy31Entity::Behavior09(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior09();
    }

    bool Enemy31Entity::Behavior10(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior10();
    }

    bool Enemy31Entity::Behavior11(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior11();
    }

    bool Enemy31Entity::Behavior12(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior12();
    }

    bool Enemy31Entity::Behavior13(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior13();
    }

    bool Enemy31Entity::Behavior14(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior14();
    }

    bool Enemy31Entity::Behavior15(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior15();
    }

    bool Enemy31Entity::Behavior16(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior16();
    }

    bool Enemy31Entity::Behavior17(Enemy31Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior17();
    }
}
