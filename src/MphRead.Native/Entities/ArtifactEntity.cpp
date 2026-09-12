#include "ArtifactEntity.hpp"

#include "../GameState.hpp"
#include "../MemoryArrays.hpp"
#include "../Messaging.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Renderer.hpp"
#include "../Scene.hpp"
#include "../Sound/Sfx.hpp"
#include "CamSeq/CameraSequence.hpp"
#include "EnemySpawnEntity.hpp"
#include "Players/PlayerEntity.hpp"

#include <any>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace
{
    using MessageInvalidCastException = MphRead::Memory::Detail::InvalidCastException;
    using MessageNullReferenceException = MphRead::Memory::Detail::NullReferenceException;
    using OpenTK::Mathematics::Matrix3;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;

    [[nodiscard]] Vector3 AddY(Vector3 value, float y) noexcept
    {
        return Vector3(value.X, value.Y + y, value.Z);
    }

    [[nodiscard]] Vector3 Scale(Vector3 value, float scale) noexcept
    {
        return Vector3(value.X * scale, value.Y * scale, value.Z * scale);
    }

    [[nodiscard]] Vector3 Multiply(Vector3 value, Matrix3 matrix) noexcept
    {
        return Vector3(
            value.X * matrix.M11 + value.Y * matrix.M21 + value.Z * matrix.M31,
            value.X * matrix.M12 + value.Y * matrix.M22 + value.Z * matrix.M32,
            value.X * matrix.M13 + value.Y * matrix.M23 + value.Z * matrix.M33);
    }

    [[nodiscard]] Matrix4 Invert(Matrix4 value)
    {
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
            throw std::runtime_error("Matrix is singular and cannot be inverted.");
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

    template <std::size_t Size>
    [[nodiscard]] std::int32_t GetChecked(
        const std::array<std::int32_t, Size>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= Size)
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return values[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] MphRead::MessageObject BoxInt32(std::int32_t value)
    {
        return std::make_shared<const std::any>(value);
    }

    [[nodiscard]] std::int32_t UnboxInt32(const MphRead::MessageObject& value)
    {
        if (!value || !value->has_value())
        {
            throw MessageNullReferenceException();
        }
        try
        {
            return std::any_cast<std::int32_t>(*value);
        }
        catch (const std::bad_any_cast&)
        {
            throw MessageInvalidCastException();
        }
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
    [[nodiscard]] T& RequireReference(std::shared_ptr<T>& value)
    {
        if (!value)
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
    [[nodiscard]] T& RequireReference(T& value) noexcept
    {
        return value;
    }

    template <typename T>
    [[nodiscard]] T* RawPointer(T* value) noexcept
    {
        return value;
    }

    template <typename T>
    [[nodiscard]] T* RawPointer(std::shared_ptr<T>& value) noexcept
    {
        return value.get();
    }

    template <typename T>
    [[nodiscard]] T* RawPointer(const std::shared_ptr<T>& value) noexcept
    {
        return value.get();
    }

    template <typename T>
    [[nodiscard]] T* RawPointer(T& value) noexcept
    {
        return std::addressof(value);
    }

    [[nodiscard]] MphRead::StorySave& StorySave()
    {
        if (MphRead::GameState::StorySave == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *MphRead::GameState::StorySave;
    }

    [[nodiscard]] std::int32_t RoomId(MphRead::Scene* scene)
    {
        if (scene == nullptr)
        {
            throw System::NullReferenceException();
        }
        return scene->RoomId;
    }
}

namespace MphRead::Entities
{
    ArtifactEntity::ArtifactEntity(ArtifactEntityData data, std::string nodeName, Scene* scene)
        : EntityBase(EntityType::Artifact, std::move(nodeName), scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        const std::string name = data.ModelId >= 8
            ? "Octolith"
            : "Artifact0" + std::to_string(static_cast<std::int32_t>(data.ModelId) + 1);
        ModelInstance& inst = SetUpModel(name);
        if (Id != -1)
        {
            if (data.ModelId >= 8)
            {
                _heightOffset = 1.75F;
            }
            else
            {
                const std::shared_ptr<MphRead::Model> model = inst.Model();
                if (!model || !model->Nodes)
                {
                    throw System::NullReferenceException();
                }
                if (model->Nodes->empty())
                {
                    throw SceneDetail::IndexOutOfRangeException();
                }
                const std::shared_ptr<Node>& node = (*model->Nodes)[0];
                if (!node)
                {
                    throw System::NullReferenceException();
                }
                _heightOffset = node->BoundingRadius;
            }
        }
        if (data.HasBase != 0)
        {
            SetUpModel("ArtifactBase");
        }
        assert(GameState::Mode == GameMode::SinglePlayer);
        Active = StorySave().InitRoomState(
            RoomId(_scene), Id, _data.Active != 0, 2) != 0;
        if (data.ModelId < 8)
        {
            if (StorySave().CheckFoundArtifact(data.ArtifactId, data.ModelId))
            {
                Active = false;
            }
        }
        else if (Id != -1)
        {
            if (StorySave().CheckFoundOctolith(data.ArtifactId))
            {
                Active = false;
            }
        }
    }

    std::uint8_t ArtifactEntity::ModelId() const noexcept
    {
        return _data.ModelId;
    }

    std::uint8_t ArtifactEntity::ArtifactId() const noexcept
    {
        return _data.ArtifactId;
    }

    void ArtifactEntity::Initialize()
    {
        EntityBase::Initialize();

        if (_data.LinkedEntityId != -1)
        {
            std::shared_ptr<EntityBase> parent{};
            if (_scene->TryGetEntity(_data.LinkedEntityId, parent))
            {
                _parent = std::move(parent);
            }
        }

        std::shared_ptr<EntityBase> target{};
        if (_scene->TryGetEntity(_data.Message1Target, target))
        {
            _msgTarget1 = target;
        }
        if (_scene->TryGetEntity(_data.Message2Target, target))
        {
            _msgTarget2 = target;
        }
        if (_scene->TryGetEntity(_data.Message3Target, target))
        {
            _msgTarget3 = target;
        }
    }

    bool ArtifactEntity::Process()
    {
        if (_parent != nullptr)
        {
            if (!_invSetUp)
            {
                _parent->GetDrawInfo();
                _invPos = Matrix::Vec3MultMtx4(
                    Position, Invert(_parent->CollisionTransform()));
                _invSetUp = true;
            }
            Position = Matrix::Vec3MultMtx4(_invPos, _parent->CollisionTransform());
        }

        if (Active)
        {
            _soundSource.Update(Position, 7);
            UpdateNodeRefVolume();
            _soundSource.PlaySfx(SfxId::ARTIFACT_LOOP, true);

            if (_data.ModelId >= 8)
            {
                _scanId = GetChecked(_scanIds, _data.ArtifactId);
            }
            else
            {
                const std::int32_t index =
                    static_cast<std::int32_t>(_data.ModelId) * 3
                    + 8
                    + static_cast<std::int32_t>(_data.ArtifactId);
                _scanId = GetChecked(_scanIds, index);
            }

            auto current = Formats::CameraSequence::Current;
            if (current != nullptr && current->BlockInput())
            {
                return EntityBase::Process();
            }

            auto&& mainValue = PlayerEntity::Main();
            PlayerEntity& player = RequireReference(mainValue);
            if (_data.ModelId >= 8 && Id == -1)
            {
                const Vector3 direction = AddY(
                    static_cast<Vector3>(player.Position) - static_cast<Vector3>(Position), 0.5F);
                Position = static_cast<Vector3>(Position) + Scale(Scale(direction, 0.1F), 0.5F);
            }

            if (player.Health() == 0)
            {
                return EntityBase::Process();
            }

            const Vector3 position = AddY(Position, _heightOffset);
            bool pickedUp = false;
            const float radii = player.Volume().SphereRadius
                + (_data.ModelId >= 8 ? 1.0F : 0.1F);
            if (player.IsAltForm())
            {
                const Vector3 between = position - player.Volume().SpherePosition;
                if (Vector3::Dot(between, between) < radii * radii)
                {
                    pickedUp = true;
                }
            }
            else
            {
                const Vector3 between = position - static_cast<Vector3>(player.Position);
                if (between.X * between.X + between.Z * between.Z < radii * radii)
                {
                    const float diffY = position.Y - player.Position.Y;
                    const float maxY = Fixed::ToFloat(player.Values().MaxPickupHeight);
                    if (diffY <= maxY + 0.5F)
                    {
                        const float minY = Fixed::ToFloat(player.Values().MinPickupHeight);
                        if (diffY >= minY - 0.5F)
                        {
                            pickedUp = true;
                        }
                    }
                }
            }

            if (!pickedUp)
            {
                return EntityBase::Process();
            }

            if (_data.Message1 != Message::None)
            {
                _scene->SendMessage(
                    _data.Message1, this, _msgTarget1.get(), BoxInt32(0), BoxInt32(0));
            }
            if (_data.Message2 != Message::None)
            {
                _scene->SendMessage(
                    _data.Message2, this, _msgTarget2.get(), BoxInt32(0), BoxInt32(0));
            }
            if (_data.Message3 != Message::None)
            {
                _scene->SendMessage(
                    _data.Message3, this, _msgTarget3.get(), BoxInt32(0), BoxInt32(0));
            }

            if (_data.ModelId >= 8)
            {
                StorySave().UpdateFoundOctolith(_data.ArtifactId);
                if (Id == -1)
                {
                    auto&& dialogMain = PlayerEntity::Main();
                    RequireReference(dialogMain).ShowDialog(
                        DialogType::Event, 54, static_cast<std::int32_t>(EventType::Octolith));
                }
                else
                {
                    GameState::UpdateCleanSave(false);
                    GameState::PausePrevented = true;
                    _scene->StartMovie(
                        Movie::OctolithPickUp,
                        FadeType::FadeOutInWhite,
                        5.0F / 30.0F,
                        FadeType::FadeOutInWhite,
                        5.0F / 30.0F);
                    GameState::UpdateBossFlags(_scene->AreaId);
                    const std::int32_t collected = StorySave().CountFoundOctoliths();
                    GameState::QueuedOctolithMessageId =
                        GetChecked(_octolithMessageIds, collected - 1);
                }
            }
            else
            {
                const std::int32_t collected = StorySave().CountFoundArtifacts(_data.ModelId);
                if (collected >= 2)
                {
                    _soundSource.PlayFreeSfx(SfxId::ARTIFACT3);
                }
                else if (collected == 1)
                {
                    _soundSource.PlayFreeSfx(SfxId::ARTIFACT2);
                }
                else
                {
                    _soundSource.PlayFreeSfx(SfxId::ARTIFACT1);
                }

                StorySave().UpdateFoundArtifact(_data.ArtifactId, _data.ModelId);
                auto&& dialogMain = PlayerEntity::Main();
                RequireReference(dialogMain).ShowDialog(
                    DialogType::Event, 6, static_cast<std::int32_t>(EventType::Artifact));

                if (collected >= 2)
                {
                    _scene->SendMessage(
                        Message::ShowPrompt,
                        this,
                        nullptr,
                        BoxInt32(13),
                        BoxInt32(0),
                        1);
                }
            }

            Active = false;
            StorySave().SetRoomState(RoomId(_scene), Id, 1);
            _soundSource.StopAllSfx(true);
        }
        else
        {
            _scanId = 0;
        }

        return EntityBase::Process();
    }

    void ArtifactEntity::GetDrawInfo()
    {
        if (Active && IsVisible(NodeRef))
        {
            EntityBase::GetDrawInfo();
        }
    }

    void ArtifactEntity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::Activate)
        {
            Active = true;
            StorySave().SetRoomState(RoomId(_scene), Id, 3);
        }
        else if (info.Message == Message::SetActive)
        {
            if (UnboxInt32(info.Param1) != 0)
            {
                Active = true;
                StorySave().SetRoomState(RoomId(_scene), Id, 3);
            }
            else
            {
                Active = false;
                StorySave().SetRoomState(RoomId(_scene), Id, 1);
            }
        }
        else if (info.Message == Message::MoveItemSpawner && info.Sender != nullptr)
        {
            if (info.Sender->Type == EntityType::EnemySpawn)
            {
                auto* spawn = dynamic_cast<EnemySpawnEntity*>(info.Sender);
                if (spawn == nullptr)
                {
                    throw System::InvalidCastException();
                }
                if (spawn->Data().EnemyType == EnemyType::Hunter)
                {
                    auto enumerator = _scene->GetPlayerEntities().GetEnumerator();
                    while (enumerator.MoveNext())
                    {
                        const std::shared_ptr<PlayerEntity>& playerValue = enumerator.Current();
                        if (!playerValue)
                        {
                            throw System::NullReferenceException();
                        }
                        PlayerEntity& player = *playerValue;
                        auto&& enemySpawner = player.EnemySpawner;
                        if (RawPointer(enemySpawner) == info.Sender)
                        {
                            Vector3 position{};
                            player.GetPosition(position);
                            Position = position;
                            break;
                        }
                    }
                    return;
                }
            }

            Vector3 position{};
            info.Sender->GetPosition(position);
            Position = position;
        }
    }

    void ArtifactEntity::Destroy()
    {
        _soundSource.StopAllSfx(true);
        EntityBase::Destroy();
    }

    LightInfo ArtifactEntity::GetLightInfo()
    {
        if (_data.ModelId >= 8)
        {
            Vector3 player{};
            if (_scene->CameraMode == CameraMode::Player)
            {
                auto&& mainValue = PlayerEntity::Main();
                player = RequireReference(mainValue).CameraInfo().Position;
            }
            else
            {
                player = _scene->CameraPosition;
            }

            const Vector3 vector1(0.0F, 1.0F, 0.0F);
            const Vector3 vector2(
                player.X - Position.X, 0.0F, player.Z - Position.Z);
            const Matrix3 lightTransform = Matrix::GetTransform3(
                vector2.Normalized(), vector1);
            return LightInfo(
                Multiply(Metadata::OctolithLight1Vector, lightTransform).Normalized(),
                Metadata::OctolithLightColor,
                Multiply(Metadata::OctolithLight2Vector, lightTransform).Normalized(),
                Metadata::OctolithLightColor);
        }
        return EntityBase::GetLightInfo();
    }

    Matrix4 ArtifactEntity::GetModelTransform(ModelInstance& inst, std::int32_t index)
    {
        Matrix4 transform = EntityBase::GetModelTransform(inst, index);
        if (index == 0)
        {
            transform.M42 += _heightOffset;
        }
        else if (index == 1)
        {
            transform.M42 -= 0.2F;
        }
        return transform;
    }
}
