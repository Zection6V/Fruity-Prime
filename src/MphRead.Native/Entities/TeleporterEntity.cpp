#include "TeleporterEntity.hpp"

#include "../Formats/CollisionDetection.hpp"
#include "../Formats/Formats.hpp"
#include "../GameState.hpp"
#include "../MemoryArrays.hpp"
#include "../Messaging.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Metadata/Rooms.hpp"
#include "../Mods/WorldEvents.hpp"
#include "../Scene.hpp"
#include "Players/PlayerEntity.hpp"
#include "RoomEntity.hpp"
#include "../NativeRuntime/System/Managed.hpp"
#include "../Formats/Types.hpp"

#include <any>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::TestFlag;
using ::OpenTK::Mathematics::AddY;
using ::OpenTK::Mathematics::CreateRotationY;
using ::OpenTK::Mathematics::CreateRotationZ;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;
using ::OpenTK::Mathematics::Multiply;
using ::OpenTK::Mathematics::SetRow3;

namespace
{
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    constexpr Vector3 UnitX(1.0F, 0.0F, 0.0F);

    [[nodiscard]] MphRead::StorySave& RequireStorySave()
    {
        if (MphRead::GameState::StorySave == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *MphRead::GameState::StorySave;
    }

    [[nodiscard]] std::int32_t UnboxInt32(const MphRead::MessageObject& value)
    {
        if (!value || !value->has_value())
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        try
        {
            return std::any_cast<std::int32_t>(*value);
        }
        catch (const std::bad_any_cast&)
        {
            throw MphRead::Memory::Detail::InvalidCastException();
        }
    }

    template <typename T>
    [[nodiscard]] T& ManagedArrayAt(
        const std::shared_ptr<MphRead::ManagedArray<T>>& values, std::int32_t index)
    {
        MphRead::ManagedArray<T>& array = RequireReference(values);
        if (index < 0 || static_cast<std::size_t>(index) >= array.Length())
        {
            throw MphRead::Memory::Detail::IndexOutOfRangeException();
        }
        return array[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] std::size_t CheckedSlotIndex(std::int32_t index, std::size_t size)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= size)
        {
            throw MphRead::Memory::Detail::IndexOutOfRangeException();
        }
        return static_cast<std::size_t>(index);
    }

    [[nodiscard]] std::int32_t ManagedSubtract(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t value = std::bit_cast<std::uint32_t>(left)
            - std::bit_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(value);
    }

    template <std::size_t Size>
    [[nodiscard]] std::string MarshalString(const char (&value)[Size])
    {
        std::size_t length = 0;
        while (length < Size && value[length] != '\0')
        {
            ++length;
        }
        return std::string(value, length);
    }

    [[nodiscard]] unsigned char FoldInvariantAscii(unsigned char value) noexcept
    {
        if (value >= static_cast<unsigned char>('A')
            && value <= static_cast<unsigned char>('Z'))
        {
            return static_cast<unsigned char>(value + ('a' - 'A'));
        }
        return value;
    }

    [[nodiscard]] bool StartsWithInvariantIgnoreCase(
        std::span<const char> room, std::span<const char> data) noexcept
    {
        if (room.size() < data.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < data.size(); ++i)
        {
            const auto left = FoldInvariantAscii(
                static_cast<unsigned char>(room[i]));
            const auto right = FoldInvariantAscii(
                static_cast<unsigned char>(data[i]));
            if (left != right)
            {
                return false;
            }
        }
        return true;
    }

}

namespace MphRead::Entities
{
    std::vector<bool> TeleporterEntity::CreateTriggeredSlots()
    {
        return std::vector<bool>(
            static_cast<std::size_t>(PlayerEntity::SlotCapacity), true);
    }

    TeleporterEntity::TeleporterEntity(
        TeleporterEntityData data,
        std::string nodeName,
        Scene* scene,
        bool forceMultiplayer)
        : EntityBase(EntityType::Teleporter, std::move(nodeName), scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);

        const bool multiplayer = GameState::Multiplayer() || forceMultiplayer;
        if (data.Invisible != 0)
        {
            AddPlaceholderModel();
        }
        else
        {
            SetRecolor(multiplayer ? 0 : RequireReference(scene).AreaId());
            std::string modelName;
            if (data.ArtifactId >= 8)
            {
                modelName = multiplayer ? "TeleporterMP" : "TeleporterSmall";
            }
            else
            {
                modelName = "Teleporter";
                _big = true;
            }
            ModelInstance& inst = SetUpModel(modelName);
            inst.SetAnimation(2, AnimFlags::NoLoop | AnimFlags::Reverse);
            AnimationInfo& animInfo = RequireReference(inst.AnimInfo);
            ManagedArrayAt(animInfo.Frame, 0) = 0;
            ManagedArrayAt(animInfo.Flags, 0) |= AnimFlags::Ended;
        }

        if (data.EntityFilename[0] != '\0')
        {
            for (std::int32_t i = 0;
                i < static_cast<std::int32_t>(Metadata::RoomList.size());
                i = Memory::Detail::UncheckedAdd(i, 1))
            {
                if (i < 0 || static_cast<std::size_t>(i) >= Metadata::RoomList.size())
                {
                    throw Memory::Detail::ArgumentOutOfRangeException();
                }
                RoomMetadata& room = RequireReference(
                    Metadata::RoomList[static_cast<std::size_t>(i)]);
                const std::optional<std::string>& filename = room.EntityFilename;
                if (filename.has_value()
                    && Compare(
                        std::span<const char>(data.EntityFilename, 15),
                        std::span<const char>(filename->data(), filename->size())))
                {
                    _targetRoomId = room.Id;
                    break;
                }
            }
        }

        if (GameState::Mode() == GameMode::SinglePlayer)
        {
            const std::int32_t state = RequireStorySave().InitRoomState(
                RequireReference(_scene).RoomId(),
                Id,
                data.Active != 0);
            Active = state != 0;
        }
        else
        {
            Active = data.Active != 0;
        }

        if (data.ArtifactId < 8)
        {
            const std::string name = "Artifact0"
                + std::to_string(static_cast<std::int32_t>(data.ArtifactId) + 1);
            ModelInstance* inst = &SetUpModel(name);
            inst->SetAnimation(-1);
            inst = &SetUpModel(name);
            inst->SetAnimation(-1);
            inst = &SetUpModel(name);
            inst->SetAnimation(-1);

            float angleY = DegreesToRadians(337.0F * (360.0F / 4096.0F));
            const float angleZ = DegreesToRadians(360.0F * (360.0F / 4096.0F));
            Matrix4 transform = Multiply(
                CreateRotationY(angleY), CreateRotationZ(angleZ));
            SetRow3(transform, Vector3(
                Fixed::ToFloat(7208), Fixed::ToFloat(2375), 0.0F));
            _artifact1Transform = transform;

            angleY = DegreesToRadians(1365.0F * (360.0F / 4096.0F));
            _artifact2Transform = Multiply(
                _artifact1Transform, CreateRotationY(angleY));

            angleY = DegreesToRadians(2730.0F * (360.0F / 4096.0F));
            _artifact3Transform = Multiply(
                _artifact1Transform, CreateRotationY(angleY));
        }

        if (multiplayer)
        {
            AddPlaceholderModel();
            _targetPos = data.TargetPosition.ToFloatVector();
        }

        if (data.Invisible == 0)
        {
            if (_big)
            {
                assert(GameState::Mode() == GameMode::SinglePlayer);
                const bool active
                    = RequireStorySave().CountFoundArtifacts(data.ArtifactId) > 2;
                if (active
                    && (GameState::EscapeTimer() == -1
                        || GameState::EscapeState() != EscapeState::Escape))
                {
                    Active = true;
                    RequireStorySave().SetRoomState(
                        RequireReference(scene).RoomId(), Id, 3);
                }
                else
                {
                    Active = false;
                    RequireStorySave().SetRoomState(
                        RequireReference(scene).RoomId(), Id, 1);
                }
            }

            if (Active)
            {
                _scanId = _big ? 46 : 26;
            }
            else
            {
                _scanId = _big ? 38 : 25;
            }
        }
    }

    TeleporterEntityData TeleporterEntity::Data() const
    {
        return _data;
    }

    bool TeleporterEntity::Compare(
        std::span<const char> data, std::span<const char> room)
    {
        if (data.size() < 15)
        {
            throw Memory::Detail::ArgumentOutOfRangeException();
        }
        return StartsWithInvariantIgnoreCase(room, data.first(15));
    }

    void TeleporterEntity::Initialize()
    {
        EntityBase::Initialize();
        if (_data.NodeName[0] != '\0')
        {
            _targetNodeRef = RequireReference(_scene).GetNodeRefByName(
                MarshalString(_data.NodeName));
        }
    }

    bool TeleporterEntity::Process()
    {
        if (_data.Invisible == 0)
        {
            (void)EntityBase::Process();
            ModelInstance& model = _models[0];
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            if (ManagedArrayAt(animInfo.Index, 0) == 2
                && !TestFlag(ManagedArrayAt(animInfo.Flags, 0), AnimFlags::Reverse)
                && TestFlag(ManagedArrayAt(animInfo.Flags, 0), AnimFlags::Ended))
            {
                model.SetAnimation(0);
            }
            else if (_big && !Active)
            {
                const bool active
                    = RequireStorySave().CountFoundArtifacts(_data.ArtifactId) > 2;
                if (active
                    && RequireReference(PlayerEntity::Main()).Health() > 0
                    && (GameState::EscapeTimer() == -1
                        || GameState::EscapeState() != EscapeState::Escape))
                {
                    Activate();
                }
            }

            if (_bool4
                && (ManagedArrayAt(animInfo.Index, 0) != 0
                    || ManagedArrayAt(animInfo.Frame, 0)
                        == ManagedSubtract(
                            ManagedArrayAt(animInfo.FrameCount, 0), 1)))
            {
                InitiateAnimaton();
            }
        }

        if (!Active)
        {
            return true;
        }

        _soundSource.Update(static_cast<Vector3>(Position), 23);
        UpdateNodeRefVolume();
        _soundSource.PlaySfx(SfxId::TELEPORTER_LOOP, true);

        bool activated = false;
        const Vector3 testPos = AddY(static_cast<Vector3>(Position), 1.0F);
        auto enumerator = RequireReference(_scene).GetPlayerEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            PlayerEntity& player = RequireReference(enumerator.Current());
            if (player.Health() == 0
                || (player.IsBot() && !GameState::Multiplayer()))
            {
                continue;
            }

            const Vector3 between = player.Volume().SpherePosition
                - static_cast<Vector3>(Position);
            if (between.Y < 1.5F
                && between.Y > -1.5F
                && between.X * between.X + between.Z * between.Z < 49.0F)
            {
                activated = true;
                ActivateAnimaton();

                Formats::CollisionResult discard{};
                const Vector3 prevPosition = player.PrevPosition();
                const Vector3 spherePosition = player.Volume().SpherePosition;
                if (Formats::CollisionDetection::CheckCylinderOverlapSphere(
                    prevPosition,
                    spherePosition,
                    testPos,
                    1.75F,
                    discard))
                {
                    if (!_triggeredSlots[CheckedSlotIndex(
                        player.SlotIndex(), _triggeredSlots.size())])
                    {
                        const float radius = _big ? 1.5F : 1.0F;
                        const Vector3 secondPrevPosition = player.PrevPosition();
                        const Vector3 secondSpherePosition
                            = player.Volume().SpherePosition;
                        if (Formats::CollisionDetection::CheckCylinderOverlapSphere(
                            secondPrevPosition,
                            secondSpherePosition,
                            testPos,
                            radius,
                            discard))
                        {
                            if (_targetRoomId == -1)
                            {
                                const Vector3 targetPosition = AddY(_targetPos, 0.5F);
                                const Vector3 facing = FacingVector();
                                const Formats::Culling::NodeRef targetNodeRef
                                    = _targetNodeRef;
                                player.Teleport(
                                    targetPosition, facing, targetNodeRef);
                            }
                            else if (GameState::TransitionRoomId() == -1)
                            {
                                Scene& currentScene = RequireReference(_scene);
                                assert(currentScene.Room() != nullptr);
                                if (_soundSource.CountPlayingSfx(
                                        SfxId::TELEPORT_OUT) == 0)
                                {
                                    (void)_soundSource.PlayFreeSfx(
                                        SfxId::TELEPORT_OUT);
                                }

                                GameState::TransitionAltForm(RequireReference(PlayerEntity::Main()).IsAltForm());
                                GameState::TransitionRoomId(_targetRoomId);
                                RequireReference(currentScene.Room()).LoadEntityId
                                    = _data.TargetIndex;
                                GameState::PausePrevented(true);
                                currentScene.SetFade(
                                    FadeType::FadeOutBlack,
                                    10.0F / 30.0F,
                                    true,
                                    AfterFade::LoadRoom);
                            }

                            const Vector3 speed = player.Speed();
                            player.SetSpeed(Vector3(0.0F, speed.Y, 0.0F));
                            if (player.IsBot())
                            {
                                RequireReference(player.AiData).Field118 = 148 * 2;
                            }
                            _triggeredSlots[CheckedSlotIndex(
                                player.SlotIndex(), _triggeredSlots.size())] = true;
                            Mods::WorldEvents::NoteTeleport(player, Id);
                        }
                    }
                }
                else
                {
                    _triggeredSlots[CheckedSlotIndex(
                        player.SlotIndex(), _triggeredSlots.size())] = false;
                }
            }
            else
            {
                _triggeredSlots[CheckedSlotIndex(
                    player.SlotIndex(), _triggeredSlots.size())] = false;
            }
        }

        if (!activated && _bool3)
        {
            _bool4 = true;
        }
        return true;
    }

    void TeleporterEntity::SetTriggered()
    {
        for (std::int32_t i = 0;
            i < static_cast<std::int32_t>(_triggeredSlots.size());
            i = Memory::Detail::UncheckedAdd(i, 1))
        {
            _triggeredSlots[CheckedSlotIndex(i, _triggeredSlots.size())] = true;
        }
    }

    void TeleporterEntity::HandleMessage(MessageInfo info)
    {
        if (_big)
        {
            return;
        }

        if (info.Message == Message::Activate)
        {
            Activate();
        }
        else if (info.Message == Message::SetActive)
        {
            if (UnboxInt32(info.Param1) != 0)
            {
                Activate();
            }
            else
            {
                Active = false;
                _scanId = 25;
                _bool4 = true;
                if (GameState::Mode() == GameMode::SinglePlayer)
                {
                    RequireStorySave().SetRoomState(
                        RequireReference(_scene).RoomId(), Id, 1);
                }
            }
        }
    }

    void TeleporterEntity::Activate()
    {
        if (!Active)
        {
            Active = true;
            _scanId = _big ? 46 : 26;
            ActivateAnimaton();
        }
    }

    void TeleporterEntity::ActivateAnimaton()
    {
        if (!_bool3 && _data.Invisible == 0)
        {
            _bool3 = true;
            _bool4 = false;
            _soundSource.Update(static_cast<Vector3>(Position), 23);

            AnimationInfo& animInfo = RequireReference(_models[0].AnimInfo);
            if (ManagedArrayAt(animInfo.Index, 0) == 2)
            {
                Scene& scene = RequireReference(_scene);
                if (scene.FrameCount() > 1
                    && TestFlag(
                        ManagedArrayAt(animInfo.Flags, 0), AnimFlags::Reverse)
                    && ManagedArrayAt(animInfo.Frame, 0)
                        < ManagedArrayAt(animInfo.FrameCount, 0) / 2
                    && scene.FrameCount() % 2 == 0)
                {
                    _soundSource.PlaySfx(SfxId::TELEPORT_ACTIVATE);
                }

                ManagedArrayAt(animInfo.Flags, 0) |= AnimFlags::NoLoop;
                ManagedArrayAt(animInfo.Flags, 0) &= ~AnimFlags::Ended;
                ManagedArrayAt(animInfo.Flags, 0) &= ~AnimFlags::Reverse;
            }
        }
    }

    void TeleporterEntity::InitiateAnimaton()
    {
        if (_bool3 && _data.Invisible == 0)
        {
            _bool3 = false;
            _bool4 = false;
            ModelInstance& model = _models[0];
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            if (ManagedArrayAt(animInfo.Index, 0) == 2)
            {
                ManagedArrayAt(animInfo.Flags, 0) |= AnimFlags::NoLoop;
                ManagedArrayAt(animInfo.Flags, 0) |= AnimFlags::Reverse;
                ManagedArrayAt(animInfo.Flags, 0) &= ~AnimFlags::Ended;
            }
            else if (ManagedArrayAt(animInfo.Index, 0) == 0)
            {
                model.SetAnimation(
                    2, AnimFlags::NoLoop | AnimFlags::Reverse);
            }
        }
        else
        {
            _bool4 = false;
        }
    }

    void TeleporterEntity::Destroy()
    {
        _soundSource.StopAllSfx(true);
        EntityBase::Destroy();
    }

    std::optional<Vector4> TeleporterEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    Matrix4 TeleporterEntity::GetModelTransform(
        ModelInstance& inst, std::int32_t index)
    {
        Matrix4 transform = EntityBase::GetModelTransform(inst, index);
        if (index != 0 && inst.IsPlaceholder)
        {
            SetRow3(transform, _targetPos);
        }
        else if (index == 1)
        {
            return Multiply(_artifact1Transform, _transform);
        }
        else if (index == 2)
        {
            return Multiply(_artifact2Transform, _transform);
        }
        else if (index == 3)
        {
            return Multiply(_artifact3Transform, _transform);
        }
        return transform;
    }

    std::optional<Vector4> TeleporterEntity::GetOverrideColor(
        ModelInstance& inst, std::int32_t index)
    {
        if (index != 0 && inst.IsPlaceholder)
        {
            return _overrideColor2;
        }
        return EntityBase::GetOverrideColor(inst, index);
    }

    std::int32_t TeleporterEntity::GetModelRecolor(
        ModelInstance& inst, std::int32_t index)
    {
        if (index != 0)
        {
            return 0;
        }
        return EntityBase::GetModelRecolor(inst, index);
    }

    void TeleporterEntity::GetDrawInfo()
    {
        if (_models.Size() == 4)
        {
            StorySave& save = RequireStorySave();
            _models[1].Active = save.CheckFoundArtifact(0, _data.ArtifactId);
            _models[2].Active = save.CheckFoundArtifact(1, _data.ArtifactId);
            _models[3].Active = save.CheckFoundArtifact(2, _data.ArtifactId);
        }
        if (IsVisible(NodeRef))
        {
            EntityBase::GetDrawInfo();
        }
    }

    void TeleporterEntity::GetDisplayVolumes()
    {
        if (RequireReference(_scene).ShowVolumes() == VolumeDisplay::Teleporter)
        {
            CollisionVolume volume;
            if (_data.Invisible != 0 || _data.ArtifactId < 8)
            {
                volume = CollisionVolume(
                    AddY(static_cast<Vector3>(Position), 1.0F), 1.0F);
            }
            else
            {
                volume = CollisionVolume(
                    AddY(static_cast<Vector3>(Position), 1.5F), 1.0F);
            }
            AddVolumeItem(volume, UnitX);
        }
    }
}
