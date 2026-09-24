#include "44_SlenchSynapse.hpp"

#include "41_Slench.hpp"
#include "45_SlenchTurret.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Messaging.hpp"
#include "../../Scene.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <any>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using ::MphRead::NativeRuntime::ManagedAt;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;
using ::MphRead::NativeRuntime::UncheckedSubtract;
using ::OpenTK::Mathematics::CreateFromAxisAngle;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;
        using OpenTK::Mathematics::Vector4;

        [[nodiscard]] Enemy41Entity* CastSlench(EntityBase* spawner) noexcept
        {
            Enemy41Entity* typedSpawner = dynamic_cast<Enemy41Entity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
        }

        [[nodiscard]] std::int32_t DivideInt32(
            std::int32_t dividend, std::int32_t divisor)
        {
            if (divisor == 0)
            {
                throw std::domain_error("Attempted to divide by zero.");
            }
            if (dividend == std::numeric_limits<std::int32_t>::min() && divisor == -1)
            {
                throw std::overflow_error("Arithmetic operation resulted in an overflow.");
            }
            return dividend / divisor;
        }

    }

    Enemy44Entity::Enemy44Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _slench(CastSlench(data.Spawner))
    {
    }

    SynapseState Enemy44Entity::State() const noexcept
    {
        return static_cast<SynapseState>(_state1);
    }

    Enemy44Values Enemy44Entity::GetValues() const
    {
        const Enemy41Entity& slench = RequireReference(_slench);
        const std::int32_t index = UncheckedMultiply(slench.Subtype(), 3);
        return ManagedAt(Metadata::Enemy44Values, index);
    }

    Enemy44Values Enemy44Entity::GetPhaseValues() const
    {
        const Enemy41Entity& slench = RequireReference(_slench);
        const std::int32_t baseIndex = UncheckedMultiply(slench.Subtype(), 3);
        const std::int32_t index = UncheckedAdd(baseIndex, slench.Phase());
        return ManagedAt(Metadata::Enemy44Values, index);
    }

    void Enemy44Entity::EnemyInitialize()
    {
        _scanId = GetValues().ScanId;

        const Enemy44Values values = GetPhaseValues();
        Enemy41Entity& slench = RequireReference(_slench);
        _index = slench.SynapseIndex();
        _boundingRadius = Fixed::ToFloat(values.ColRadius);
        UpdateCollisionVolume(_boundingRadius);

        float angle;
        if (_index == 0)
        {
            angle = 0.0F;
        }
        else if (_index == 1)
        {
            angle = 120.0F;
        }
        else if (_index == 2)
        {
            angle = -120.0F;
        }
        else
        {
            throw SceneDetail::IndexOutOfRangeException();
        }

        const Vector3 facing = slench.FacingVector();
        Vector3 up = slench.UpVector();
        const Matrix4 rotMtx = CreateFromAxisAngle(facing, DegreesToRadians(angle));
        up = Matrix::Vec3MultMtx3(up, rotMtx).Normalized();
        const Vector3 position = slench.Position;
        SetTransform(facing, up, position);

        _health = _healthMax = values.Health;
        _healthForTurretUpdate = _health;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::NoMaxDistance;
        SetHealthbarMessageId(2);

        const std::int32_t effectivenessIndex = slench.Subtype();
        const std::int32_t effectiveness
            = ManagedAt(Metadata::SlenchSynapseEffectiveness, effectivenessIndex);
        Metadata::LoadEffectiveness(effectiveness, BeamEffectiveness);

        ChangeState(SynapseState::Initial);

        Scene& scene = RequireReference(_scene);
        std::string model = "BigEyeSynapse_01";
        if (scene.RoomId() == 82)
        {
            model = "BigEyeSynapse_02";
        }
        else if (scene.RoomId() == 64)
        {
            model = "BigEyeSynapse_03";
        }
        else if (scene.RoomId() == 76)
        {
            model = "BigEyeSynapse_04";
        }
        _model = &SetUpModel(model);
    }

    void Enemy44Entity::UpdateCollisionVolume(float radius)
    {
        _hurtVolumeInit = CollisionVolume(Vector3(0.0F, -5.5F, -3.25F), radius);
    }

    void Enemy44Entity::ChangeState(SynapseState state)
    {
        const Enemy44Values values = GetPhaseValues();
        switch (state)
        {
        case SynapseState::Initial:
            Flags &= ~EnemyFlags::Visible;
            break;
        case SynapseState::Appear:
        {
            Flags |= EnemyFlags::Visible;
            _health = _healthMax = values.Health;
            UpdateCollisionVolume(Fixed::ToFloat(values.ColRadius));
            RequireReference(_model).SetAnimation(0, AnimFlags::NoLoop);
            _soundSource.PlaySfx(
                SfxId::BIGEYE_SYNAPSE_REGEN_SCR,
                false,
                false,
                std::numeric_limits<float>::max(),
                true);
            break;
        }
        case SynapseState::Idle:
            Flags &= ~EnemyFlags::Invincible;
            _timer = UncheckedMultiply(static_cast<std::int32_t>(values.HealTimer), 2);
            RequireReference(_model).SetAnimation(1);
            break;
        case SynapseState::Damaged:
            Flags |= EnemyFlags::Invincible;
            RequireReference(_model).SetAnimation(2, AnimFlags::NoLoop);
            break;
        case SynapseState::Dying:
        {
            Flags |= EnemyFlags::Invincible;
            RequireReference(_model).SetAnimation(4, AnimFlags::NoLoop);
            const Vector3 spawnPos
                = Matrix::Vec3MultMtx4(_hurtVolumeInit.SpherePosition, Transform);
            Scene& scene = RequireReference(_scene);
            const Vector3 facing = FacingVector();
            const Vector3 up = UpVector();
            scene.SpawnEffect(135, facing, up, spawnPos);
            break;
        }
        case SynapseState::Dead:
            Flags &= ~EnemyFlags::Visible;
            Flags |= EnemyFlags::Invincible;
            SetTurretActive(false);
            _timer = UncheckedMultiply(static_cast<std::int32_t>(values.ReappearTimer), 2);
            break;
        }
        _state2 = static_cast<std::uint8_t>(state);
    }

    void Enemy44Entity::EnemyProcess()
    {
        const Enemy44Values values = GetPhaseValues();

        if (State() == SynapseState::Appear)
        {
            ModelInstance& model = RequireReference(_model);
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            if (TypeExtensions::TestFlag(
                ManagedAt(animInfo.Flags, 0), AnimFlags::Ended))
            {
                SetTurretActive(true);
                ChangeState(SynapseState::Idle);
            }
        }
        else if (State() == SynapseState::Idle)
        {
            if (_health < values.Health && _timer != 0)
            {
                _timer = UncheckedSubtract(_timer, 1);
                if (_timer == 0)
                {
                    ++_health;
                    if (_health < values.Health)
                    {
                        _timer = UncheckedMultiply(
                            static_cast<std::int32_t>(values.HealTimer), 2);
                    }
                }
            }
        }
        else if (State() == SynapseState::Damaged)
        {
            ModelInstance& model = RequireReference(_model);
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            if (TypeExtensions::TestFlag(
                ManagedAt(animInfo.Flags, 0), AnimFlags::Ended))
            {
                ChangeState(SynapseState::Idle);
            }
        }
        else if (State() == SynapseState::Dying)
        {
            ModelInstance& model = RequireReference(_model);
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            if (TypeExtensions::TestFlag(
                ManagedAt(animInfo.Flags, 0), AnimFlags::Ended))
            {
                ChangeState(SynapseState::Dead);
            }
        }
        else if (State() == SynapseState::Dead)
        {
            if (RequireReference(_slench).CanSynapsesRespawn() && _timer != 0)
            {
                _timer = UncheckedSubtract(_timer, 1);
                if (_timer == 0)
                {
                    ChangeState(SynapseState::Appear);
                }
            }
        }

        if (_health != _healthForTurretUpdate)
        {
            Enemy45Entity* turret = FindTurret();
            if (turret != nullptr)
            {
                const std::int32_t frameCount = turret->GetMaxFrameCount();
                if (frameCount == 0)
                {
                    return;
                }

                const std::int32_t segments
                    = DivideInt32(static_cast<std::int32_t>(values.Health), frameCount);
                const std::int32_t healthDifference
                    = static_cast<std::int32_t>(_health)
                    - static_cast<std::int32_t>(_healthForTurretUpdate);
                const std::int32_t increment = DivideInt32(healthDifference, segments);
                if (increment != 0)
                {
                    const std::int32_t product = UncheckedMultiply(increment, segments);
                    const std::uint16_t delta
                        = static_cast<std::uint16_t>(static_cast<std::uint32_t>(product));
                    _healthForTurretUpdate = static_cast<std::uint16_t>(
                        static_cast<std::uint32_t>(_healthForTurretUpdate)
                        + static_cast<std::uint32_t>(delta));

                    std::int32_t param1;
                    Message message;
                    if (increment < 0)
                    {
                        param1 = UncheckedSubtract(0, increment);
                        message = Message::DecreaseTurretLights;
                    }
                    else
                    {
                        param1 = increment;
                        message = Message::IncreaseTurretLights;
                    }

                    Scene& scene = RequireReference(_scene);
                    const MessageObject param1Object = BoxInt32(param1);
                    const MessageObject param2Object = BoxInt32(0);
                    scene.SendMessage(
                        message, this, turret, param1Object, param2Object);
                }
            }
        }
    }

    void Enemy44Entity::SetTurretActive(bool activate)
    {
        Enemy45Entity* turret = FindTurret();
        if (turret != nullptr)
        {
            const Message message = activate
                ? Message::ActivateTurret
                : Message::DeactivateTurret;
            Scene& scene = RequireReference(_scene);
            const MessageObject param1 = BoxInt32(0);
            const MessageObject param2 = BoxInt32(0);
            scene.SendMessage(message, this, turret, param1, param2);
        }
    }

    Enemy45Entity* Enemy44Entity::FindTurret()
    {
        Scene& scene = RequireReference(_scene);
        auto enumerator = scene.GetEnemyInstanceEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            const std::shared_ptr<EnemyInstanceEntity> enemy = enumerator.Current();
            const std::shared_ptr<Enemy45Entity> turret
                = std::dynamic_pointer_cast<Enemy45Entity>(enemy);
            if (turret && turret->Index() == _index)
            {
                return turret.get();
            }
        }
        return nullptr;
    }

    bool Enemy44Entity::EnemyTakeDamage(EntityBase* source)
    {
        if (State() == SynapseState::Idle
            && _state2 == static_cast<std::uint8_t>(SynapseState::Idle))
        {
            Effectiveness effectiveness = Effectiveness::Normal;
            if (source != nullptr && source->Type == EntityType::BeamProjectile)
            {
                BeamProjectileEntity* beamSource
                    = dynamic_cast<BeamProjectileEntity*>(source);
                if (beamSource == nullptr)
                {
                    throw SceneDetail::InvalidCastException();
                }
                effectiveness = GetEffectiveness(beamSource->Beam());
            }

            if (effectiveness != Effectiveness::Zero)
            {
                if (_health != 0)
                {
                    _soundSource.PlaySfx(SfxId::BIGEYE_SYNAPSE_DAMAGE);
                }
                else
                {
                    _soundSource.PlaySfx(SfxId::BIGEYE_SYNAPSE_DIE_SCR);
                }
            }

            if (_health != 0)
            {
                ChangeState(SynapseState::Damaged);
            }
            else
            {
                ChangeState(SynapseState::Dying);
            }
        }

        if (_health == 0)
        {
            _health = 1;
        }
        return false;
    }
}
