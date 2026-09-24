#include "ArtifactEntity.hpp"

#include "../GameState.hpp"
#include "../Mods/DebugLog.hpp"
#include "../MemoryArrays.hpp"
#include "../Messaging.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Renderer.hpp"
#include "../Scene.hpp"
#include "../Sound/Sfx.hpp"
#include "CamSeq/CameraSequence.hpp"
#include "EnemySpawnEntity.hpp"

#include <sstream>
#include "Players/PlayerEntity.hpp"
#include "../NativeRuntime/System/Managed.hpp"
#include "../Formats/Types.hpp"

#include <any>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

using ::MphRead::NativeRuntime::RequireReference;
using ::OpenTK::Mathematics::AddY;
using ::OpenTK::Mathematics::Inverted;
using ::OpenTK::Mathematics::Multiply;
using ::OpenTK::Mathematics::ScaleVector;

namespace
{
    using MessageInvalidCastException = MphRead::Memory::Detail::InvalidCastException;
    using MessageNullReferenceException = MphRead::Memory::Detail::NullReferenceException;
    using OpenTK::Mathematics::Matrix3;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;

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

    [[nodiscard]] MphRead::StorySave& RequireStorySave()
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
        return scene->RoomId();
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
        assert(GameState::Mode() == GameMode::SinglePlayer);
        Active = RequireStorySave().InitRoomState(
            RoomId(_scene), Id, _data.Active != 0, 2) != 0;
        if (data.ModelId < 8)
        {
            if (RequireStorySave().CheckFoundArtifact(data.ArtifactId, data.ModelId))
            {
                Active = false;
            }
        }
        else if (Id != -1)
        {
            if (RequireStorySave().CheckFoundOctolith(data.ArtifactId))
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
                    Position, Inverted(_parent->CollisionTransform()));
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

            auto* current = Formats::CameraSequence::Current();
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
                Position = static_cast<Vector3>(Position) + ScaleVector(ScaleVector(direction, 0.1F), 0.5F);
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
                RequireStorySave().UpdateFoundOctolith(_data.ArtifactId);
                if (Id == -1)
                {
                    auto&& dialogMain = PlayerEntity::Main();
                    RequireReference(dialogMain).ShowDialog(
                        DialogType::Event, 54, static_cast<std::int32_t>(EventType::Octolith));
                }
                else
                {
                    GameState::UpdateCleanSave(false);
                    GameState::PausePrevented(true);
                    _scene->StartMovie(
                        Movie::OctolithPickUp,
                        FadeType::FadeOutInWhite,
                        5.0F / 30.0F,
                        FadeType::FadeOutInWhite,
                        5.0F / 30.0F);
                    GameState::UpdateBossFlags(_scene->AreaId());
                    const std::int32_t collected = RequireStorySave().CountFoundOctoliths();
                    GameState::QueuedOctolithMessageId(
                        GetChecked(_octolithMessageIds, collected - 1));
                }
            }
            else
            {
                const std::int32_t collected = RequireStorySave().CountFoundArtifacts(_data.ModelId);
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

                RequireStorySave().UpdateFoundArtifact(_data.ArtifactId, _data.ModelId);
                {
                    std::ostringstream line;
                    line << "artifact " << static_cast<std::int32_t>(_data.ArtifactId) << " of set "
                        << static_cast<std::int32_t>(_data.ModelId) << " picked up in room "
                        << RoomId(_scene) << ": artifacts=0x" << std::hex << std::uppercase
                        << RequireStorySave().Artifacts;
                    Mods::DebugLog::Line("save", line.str());
                }
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
            RequireStorySave().SetRoomState(RoomId(_scene), Id, 1);
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
            RequireStorySave().SetRoomState(RoomId(_scene), Id, 3);
        }
        else if (info.Message == Message::SetActive)
        {
            if (UnboxInt32(info.Param1) != 0)
            {
                Active = true;
                RequireStorySave().SetRoomState(RoomId(_scene), Id, 3);
            }
            else
            {
                Active = false;
                RequireStorySave().SetRoomState(RoomId(_scene), Id, 1);
            }
        }
        else if (info.Message == Message::MoveItemSpawner && info.Sender != nullptr)
        {
            if (info.Sender->Type == EntityType::EnemySpawn)
            {
                auto* spawn = dynamic_cast<EnemySpawnEntity*>(info.Sender);
                if (spawn == nullptr)
                {
                    throw SceneDetail::InvalidCastException();
                }
                if (spawn->Data.EnemyType == EnemyType::Hunter)
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
                        const std::shared_ptr<EnemySpawnEntity> enemySpawner = player.EnemySpawner();
                        if (enemySpawner.get() == info.Sender)
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
            if (_scene->CameraMode() == CameraMode::Player)
            {
                auto&& mainValue = PlayerEntity::Main();
                const std::shared_ptr<CameraInfo> cameraInfo = RequireReference(mainValue).CameraInfo();
                player = RequireReference(cameraInfo).Position;
            }
            else
            {
                player = _scene->CameraPosition();
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
