#include "JumpPadEntity.hpp"

#include "../GameState.hpp"
#include "../MemoryArrays.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Mods/WorldEvents.hpp"
#include "../Renderer.hpp"
#include "../Scene.hpp"
#include "Players/PlayerEntity.hpp"
#include "TriggerVolumeEntity.hpp"
#include "../NativeRuntime/System/Managed.hpp"
#include "../Formats/Types.hpp"

#include <any>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

using ::MphRead::NativeRuntime::RequireReference;
using ::OpenTK::Mathematics::CreateScale;
using ::OpenTK::Mathematics::Equal;
using ::OpenTK::Mathematics::Multiply;
using ::OpenTK::Mathematics::ScaleVector;

namespace
{
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    constexpr Vector3 UnitX(1.0F, 0.0F, 0.0F);
    constexpr Vector3 UnitY(0.0F, 1.0F, 0.0F);

    [[nodiscard]] std::int32_t GetRoomId(MphRead::Scene* scene)
    {
        if (scene == nullptr)
        {
            throw System::NullReferenceException();
        }
        return scene->RoomId();
    }

    [[nodiscard]] std::int32_t UnboxInt32(const MphRead::MessageObject& value)
    {
        if (!value || !value->has_value())
        {
            throw System::NullReferenceException();
        }
        try
        {
            return std::any_cast<std::int32_t>(*value);
        }
        catch (const std::bad_any_cast&)
        {
            throw MphRead::SceneDetail::InvalidCastException();
        }
    }

    [[nodiscard]] const std::string& GetJumpPadName(std::uint32_t modelId)
    {
        const std::int32_t index = std::bit_cast<std::int32_t>(modelId);
        if (index < 0
            || static_cast<std::size_t>(index) >= MphRead::Metadata::JumpPads.size())
        {
            throw System::ArgumentOutOfRangeException();
        }
        return MphRead::Metadata::JumpPads[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] float Determinant(Matrix4 value) noexcept
    {
        const float m11 = value.M11;
        const float m12 = value.M12;
        const float m13 = value.M13;
        const float m14 = value.M14;
        const float m21 = value.M21;
        const float m22 = value.M22;
        const float m23 = value.M23;
        const float m24 = value.M24;
        const float m31 = value.M31;
        const float m32 = value.M32;
        const float m33 = value.M33;
        const float m34 = value.M34;
        const float m41 = value.M41;
        const float m42 = value.M42;
        const float m43 = value.M43;
        const float m44 = value.M44;

        return
            (m11 * m22 * m33 * m44) - (m11 * m22 * m34 * m43) + (m11 * m23 * m34 * m42) - (m11 * m23 * m32 * m44)
            + (m11 * m24 * m32 * m43) - (m11 * m24 * m33 * m42) - (m12 * m23 * m34 * m41) + (m12 * m23 * m31 * m44)
            - (m12 * m24 * m31 * m43) + (m12 * m24 * m33 * m41) - (m12 * m21 * m33 * m44) + (m12 * m21 * m34 * m43)
            + (m13 * m24 * m31 * m42) - (m13 * m24 * m32 * m41) + (m13 * m21 * m32 * m44) - (m13 * m21 * m34 * m42)
            + (m13 * m22 * m34 * m41) - (m13 * m22 * m31 * m44) - (m14 * m21 * m32 * m43) + (m14 * m21 * m33 * m42)
            - (m14 * m22 * m33 * m41) + (m14 * m22 * m31 * m43) - (m14 * m23 * m31 * m42) + (m14 * m23 * m32 * m41);
    }

    [[nodiscard]] Matrix4 Invert(Matrix4 value)
    {
        if (Determinant(value) == 0.0F)
        {
            return value;
        }

        const float a = value.M11;
        const float b = value.M21;
        const float c = value.M31;
        const float d = value.M41;
        const float e = value.M12;
        const float f = value.M22;
        const float g = value.M32;
        const float h = value.M42;
        const float i = value.M13;
        const float j = value.M23;
        const float k = value.M33;
        const float l = value.M43;
        const float m = value.M14;
        const float n = value.M24;
        const float o = value.M34;
        const float p = value.M44;

        const float kpLo = k * p - l * o;
        const float jpLn = j * p - l * n;
        const float joKn = j * o - k * n;
        const float ipLm = i * p - l * m;
        const float ioKm = i * o - k * m;
        const float inJm = i * n - j * m;

        const float a11 = +(f * kpLo - g * jpLn + h * joKn);
        const float a12 = -(e * kpLo - g * ipLm + h * ioKm);
        const float a13 = +(e * jpLn - f * ipLm + h * inJm);
        const float a14 = -(e * joKn - f * ioKm + g * inJm);

        const float det = a * a11 + b * a12 + c * a13 + d * a14;
        if (std::abs(det) < std::numeric_limits<float>::denorm_min())
        {
            throw MphRead::SceneDetail::InvalidOperationException();
        }

        const float invDet = 1.0F / det;
        Matrix4 result{};
        result.M11 = a11 * invDet;
        result.M12 = a12 * invDet;
        result.M13 = a13 * invDet;
        result.M14 = a14 * invDet;
        result.M21 = -(b * kpLo - c * jpLn + d * joKn) * invDet;
        result.M22 = +(a * kpLo - c * ipLm + d * ioKm) * invDet;
        result.M23 = -(a * jpLn - b * ipLm + d * inJm) * invDet;
        result.M24 = +(a * joKn - b * ioKm + c * inJm) * invDet;

        const float gpHo = g * p - h * o;
        const float fpHn = f * p - h * n;
        const float foGn = f * o - g * n;
        const float epHm = e * p - h * m;
        const float eoGm = e * o - g * m;
        const float enFm = e * n - f * m;

        result.M31 = +(b * gpHo - c * fpHn + d * foGn) * invDet;
        result.M32 = -(a * gpHo - c * epHm + d * eoGm) * invDet;
        result.M33 = +(a * fpHn - b * epHm + d * enFm) * invDet;
        result.M34 = -(a * foGn - b * eoGm + c * enFm) * invDet;

        const float glHk = g * l - h * k;
        const float flHj = f * l - h * j;
        const float fkGj = f * k - g * j;
        const float elHi = e * l - h * i;
        const float ekGi = e * k - g * i;
        const float ejFi = e * j - f * i;

        result.M41 = -(b * glHk - c * flHj + d * fkGj) * invDet;
        result.M42 = +(a * glHk - c * elHi + d * ekGi) * invDet;
        result.M43 = -(a * flHj - b * elHi + d * ejFi) * invDet;
        result.M44 = +(a * fkGj - b * ekGi + c * ejFi) * invDet;
        return result;
    }
}

namespace MphRead::Entities
{
    JumpPadEntity::JumpPadEntity(
        JumpPadEntityData data, std::string nodeName, Scene* scene)
        : EntityBase(EntityType::JumpPad, std::move(nodeName), scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        _prevPos = Position;
        _volume = CollisionVolume::Move(_data.Volume, Position);

        const std::string modelName = GetJumpPadName(data.ModelId);
        SetUpModel(modelName);
        ModelInstance& beamInst = SetUpModel("JumpPad_Beam");

        const Vector3 beamVector = data.BeamVector.ToFloatVector().Normalized();
        _beamTransform = GetTransformMatrix(
            beamVector,
            beamVector.X != 0.0F || beamVector.Z != 0.0F ? UnitY : UnitX);
        _beamTransform.M42 = 0.25F;
        const Vector3 transformedBeam = Matrix::Vec3MultMtx3(beamVector, Transform);
        const float beamSpeed = _data.Speed.FloatValue();
        _beamVector = ScaleVector(transformedBeam, beamSpeed);

        if (GameState::Mode() == GameMode::SinglePlayer)
        {
            const std::shared_ptr<StorySave> storySave = GameState::StorySave;
            const std::int32_t roomId = GetRoomId(_scene);
            const std::int32_t entityId = Id;
            const bool active = data.Active != 0;
            Active = RequireReference(storySave).InitRoomState(
                roomId,
                entityId,
                active) != 0;
        }
        else
        {
            Active = data.Active != 0;
        }
        beamInst.Active = Active;
    }

    std::shared_ptr<Formats::NodeData3> JumpPadEntity::ClosestNode() const noexcept
    {
        return _closestNode;
    }

    void JumpPadEntity::ClosestNode(std::shared_ptr<Formats::NodeData3> value) noexcept
    {
        _closestNode = std::move(value);
    }

    CollisionVolume JumpPadEntity::ModVolume() const noexcept
    {
        return _volume;
    }

    void JumpPadEntity::Initialize()
    {
        EntityBase::Initialize();
        if (_data.ParentId != -1)
        {
            std::shared_ptr<EntityBase> parent;
            if (RequireReference(_scene).TryGetEntity(_data.ParentId, parent))
            {
                _parent = std::move(parent);
            }
        }
    }

    bool JumpPadEntity::GetTargetable()
    {
        return false;
    }

    bool JumpPadEntity::Process()
    {
        if (_parent)
        {
            if (!_invSetUp)
            {
                _invPos = Matrix::Vec3MultMtx4(
                    Position,
                    Invert(_parent->CollisionTransform()));
                _invSetUp = true;
            }
            Position = Matrix::Vec3MultMtx4(_invPos, _parent->CollisionTransform());
        }

        if (!Equal(_prevPos, static_cast<Vector3>(Position)))
        {
            _volume = CollisionVolume::Move(
                _data.Volume,
                static_cast<Vector3>(Position));
            _prevPos = static_cast<Vector3>(Position);
        }

        if (Active && _cooldownTimer == 0)
        {
            auto enumerator = RequireReference(_scene).GetPlayerEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                PlayerEntity& player = RequireReference(enumerator.Current());
                if (player.Health() == 0)
                {
                    continue;
                }

                Vector3 position;
                if (player.IsAltForm())
                {
                    if (!TypeExtensions::TestFlag(
                        _data.TriggerFlags, TriggerFlags::PlayerAlt))
                    {
                        continue;
                    }
                    position = player.Volume().SpherePosition;
                }
                else
                {
                    if (!TypeExtensions::TestFlag(
                        _data.TriggerFlags, TriggerFlags::PlayerBiped))
                    {
                        continue;
                    }
                    position = player.Position;
                }

                if (_volume.TestPoint(position))
                {
                    player.ActivateJumpPad(this, _beamVector, _data.ControlLockTime);
                    Mods::WorldEvents::NoteJumpPad(player, Id);
                    _cooldownTimer = static_cast<std::uint16_t>(
                        static_cast<std::uint32_t>(_data.CooldownTime) * 2U);
                }
            }
        }

        if (_cooldownTimer > 0)
        {
            --_cooldownTimer;
        }
        return EntityBase::Process();
    }

    void JumpPadEntity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::Activate)
        {
            Active = true;
            if (GameState::Mode() == GameMode::SinglePlayer)
            {
                const std::shared_ptr<StorySave> storySave = GameState::StorySave;
                const std::int32_t roomId = GetRoomId(_scene);
                const std::int32_t entityId = Id;
                RequireReference(storySave).SetRoomState(roomId, entityId, 3);
            }
        }
        else if (info.Message == Message::SetActive)
        {
            if (UnboxInt32(info.Param1) != 0)
            {
                Active = true;
                if (GameState::Mode() == GameMode::SinglePlayer)
                {
                    const std::shared_ptr<StorySave> storySave = GameState::StorySave;
                    const std::int32_t roomId = GetRoomId(_scene);
                    const std::int32_t entityId = Id;
                    RequireReference(storySave).SetRoomState(roomId, entityId, 3);
                }
            }
            else
            {
                Active = false;
                if (GameState::Mode() == GameMode::SinglePlayer)
                {
                    const std::shared_ptr<StorySave> storySave = GameState::StorySave;
                    const std::int32_t roomId = GetRoomId(_scene);
                    const std::int32_t entityId = Id;
                    RequireReference(storySave).SetRoomState(roomId, entityId, 1);
                }
            }
        }
        _models[1].Active = Active;
    }

    Matrix4 JumpPadEntity::GetModelTransform(ModelInstance& inst, std::int32_t index)
    {
        if (index == 1)
        {
            const std::shared_ptr<Model> model = inst.Model();
            const Matrix4 scale = CreateScale(RequireReference(model).Scale);
            const Matrix4 scaleBeam = Multiply(scale, _beamTransform);
            return Multiply(scaleBeam, _transform);
        }
        return EntityBase::GetModelTransform(inst, index);
    }

    void JumpPadEntity::GetDrawInfo()
    {
        if (IsVisible(NodeRef))
        {
            EntityBase::GetDrawInfo();
        }
    }

    void JumpPadEntity::GetDisplayVolumes()
    {
        if (RequireReference(_scene).ShowVolumes() == VolumeDisplay::JumpPad)
        {
            AddVolumeItem(_volume, UnitY);
        }
    }

    void JumpPadEntity::SetActive(bool active)
    {
        EntityBase::SetActive(active);
        _models[1].Active = Active;
    }

    FhJumpPadEntity::FhJumpPadEntity(FhJumpPadEntityData data, Scene* scene)
        : EntityBase(EntityType::FhJumpPad, scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        _volume = CollisionVolume::Move(_data.ActiveVolume(), Position);

        std::string name = data.ModelId == 1U ? "balljump" : "jumppad_base";
        SetUpModel(name, 0, AnimFlags::None, true);
        name = data.ModelId == 1U ? "balljump_ray" : "jumppad_ray";
        SetUpModel(name, 0, AnimFlags::None, true);

        const Vector3 beamVector = data.BeamVector.ToFloatVector().Normalized();
        _beamTransform = GetTransformMatrix(
            beamVector,
            beamVector.X != 0.0F || beamVector.Z != 0.0F ? UnitY : UnitX);
    }

    Matrix4 FhJumpPadEntity::GetModelTransform(ModelInstance& inst, std::int32_t index)
    {
        if (index == 1)
        {
            const std::shared_ptr<Model> model = inst.Model();
            const Matrix4 scale = CreateScale(RequireReference(model).Scale);
            Matrix4 transform = Multiply(scale, _beamTransform);
            const Vector3 position = Position;
            transform.M41 = position.X;
            transform.M42 = position.Y + 0.25F;
            transform.M43 = position.Z;
            return transform;
        }
        return EntityBase::GetModelTransform(inst, index);
    }

    void FhJumpPadEntity::GetDisplayVolumes()
    {
        if (RequireReference(_scene).ShowVolumes() == VolumeDisplay::JumpPad)
        {
            AddVolumeItem(_volume, UnitY);
        }
    }
}
