#include "DoorEntity.hpp"

#include "../Features.hpp"
#include "../Formats/Collision.hpp"
#include "../GameState.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Metadata/Rooms.hpp"
#include "../Program.hpp"
#include "../Scene.hpp"
#include "../Sound/Sfx.hpp"
#include "Players/PlayerEntity.hpp"
#include "RoomEntity.hpp"
#include "../NativeRuntime/System/Managed.hpp"
#include "../NativeRuntime/OpenTK/Mathematics.hpp"
#include "../Formats/Types.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::TestFlag;
using ::OpenTK::Mathematics::CreateScale;
using ::OpenTK::Mathematics::CreateTranslation;
using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::Multiply;
using ::OpenTK::Mathematics::Negate;
using ::OpenTK::Mathematics::ScaleVector;

namespace
{
    using MphRead::AnimFlags;
    using MphRead::AnimationInfo;
    using MphRead::DoorType;
    using MphRead::Formats::Collision::Portal;
    using MphRead::SetFlags;
    using MphRead::Entities::DoorFlags;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

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
        std::string_view room, const char* data, std::size_t count) noexcept
    {
        if (room.size() < count)
        {
            return false;
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto left = FoldInvariantAscii(static_cast<unsigned char>(room[i]));
            const auto right = FoldInvariantAscii(static_cast<unsigned char>(data[i]));
            if (left != right)
            {
                return false;
            }
        }
        return true;
    }

    template <typename T, std::size_t Size>
    [[nodiscard]] const T& GetChecked(
        const std::array<T, Size>& values, std::uint32_t index)
    {
        if (index >= Size)
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return values[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] bool IsAsciiWhiteSpace(char value) noexcept
    {
        return value == ' ' || value == '\t' || value == '\r' || value == '\n'
            || value == '\f' || value == '\v';
    }

    [[nodiscard]] bool TryParseInt32TwoChars(const char* value, std::int32_t& result) noexcept
    {
        std::size_t start = 0;
        std::size_t end = 2;
        while (start < end && IsAsciiWhiteSpace(value[start]))
        {
            ++start;
        }
        while (end > start && IsAsciiWhiteSpace(value[end - 1]))
        {
            --end;
        }
        if (start == end)
        {
            result = 0;
            return false;
        }

        bool negative = false;
        if (value[start] == '+' || value[start] == '-')
        {
            negative = value[start] == '-';
            ++start;
        }
        if (start == end)
        {
            result = 0;
            return false;
        }

        std::int32_t parsed = 0;
        for (std::size_t i = start; i < end; ++i)
        {
            if (value[i] < '0' || value[i] > '9')
            {
                result = 0;
                return false;
            }
            parsed = parsed * 10 + static_cast<std::int32_t>(value[i] - '0');
        }
        result = negative ? -parsed : parsed;
        return true;
    }
}

namespace MphRead::Entities
{
    const std::array<std::int32_t, 10> DoorEntity::_scanIds{
        0, 255, 264, 252, 256, 253, 254, 249, 266, 265
    };

    const std::array<float, 4> DoorEntity::_portHeights{
        3.4F, 3.4F, 6.4F, 3.4F
    };

    DoorEntity::DoorEntity(
        DoorEntityData data,
        std::string nodeName,
        Scene* scene,
        std::int32_t targetRoomId,
        std::int32_t targetLayerId)
        : EntityBase(EntityType::Door, std::move(nodeName), scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        const DoorMetadata& meta = GetChecked(Metadata::Doors, static_cast<std::uint32_t>(data.DoorType));
        _radius = meta.Radius;
        _radiusSquared = _radius * _radius;
        std::int32_t recolorId = 0;
        if (data.DoorType == DoorType::Standard || data.DoorType == DoorType::Thin)
        {
            recolorId = GetChecked(Metadata::DoorPalettes, data.PaletteId);
        }
        SetRecolor(recolorId);
        // in practice (actual palette indices, not the index into the metadata):
        // - standard = 0, 1, 2, 3, 4, 6
        // - morph ball = 0
        // - boss = 0
        // - thin = 0, 7
        ModelInstance& inst = SetUpModel(meta.Name);
        if (_data.DoorType == DoorType::Thin)
        {
            inst.SetAnimation(1, 0,
                SetFlags::Texture | SetFlags::Texcoord | SetFlags::Node,
                AnimFlags::None);
        }
        else
        {
            inst.SetAnimation(0, 0,
                SetFlags::Texture | SetFlags::Texcoord | SetFlags::Node,
                AnimFlags::Ended | AnimFlags::NoLoop);
            (*inst.AnimInfo->Flags)[0] |= AnimFlags::Reverse;
        }
        inst.SetAnimation(0, 1, SetFlags::Material, AnimFlags::Ended | AnimFlags::NoLoop);
        (*inst.AnimInfo->Flags)[1] |= AnimFlags::Reverse;
        _lock = &SetUpModel(meta.LockName);
        _lockTransform = CreateTranslation(0.0F, meta.LockOffset, 0.0F);
        assert(GameState::Mode() == GameMode::SinglePlayer);
        const std::int32_t state = RequireReference(::MphRead::GameState::StorySave).InitRoomState(
            RequireReference(_scene).RoomId(), Id, _data.Locked != 0);
        if (state != 0 && !Cheats::UnlockAllDoors())
        {
            _flags |= DoorFlags::Locked;
        }
        UpdateScanId();
        _flags |= DoorFlags::Closed;
        if (_data.PaletteId == 9) // any beam door
        {
            _flags |= DoorFlags::ShowLock;
        }
        _targetRoomId = targetRoomId;
        if (_targetRoomId == -1 && _data.EntityFilename[0] != '\0')
        {
            for (std::size_t i = 0; i < Metadata::RoomList.size(); ++i)
            {
                const std::shared_ptr<RoomMetadata>& roomValue = Metadata::RoomList[i];
                RoomMetadata& room = RequireReference(roomValue);
                const std::optional<std::string>& filename = room.EntityFilename;
                if (filename.has_value() && Compare(_data.EntityFilename, *filename))
                {
                    _targetRoomId = room.Id;
                    break;
                }
            }
        }
        if (_data.ConnectorId != 255 && _targetRoomId == -1)
        {
            throw ProgramException("Loader door failed to find target room.");
        }
        _targetLayerId = targetLayerId;
    }

    float DoorEntity::Radius() const noexcept
    {
        return _radius;
    }

    float DoorEntity::RadiusSquared() const noexcept
    {
        return _radiusSquared;
    }

    DoorFlags DoorEntity::Flags() const noexcept
    {
        return _flags;
    }

    void DoorEntity::SetFlags(DoorFlags value) noexcept
    {
        _flags = value;
    }

    std::int32_t DoorEntity::TargetRoomId() const noexcept
    {
        return _targetRoomId;
    }

    std::int32_t DoorEntity::TargetLayerId() const noexcept
    {
        return _targetLayerId;
    }

    std::shared_ptr<DoorEntity> DoorEntity::LoaderDoor() const noexcept
    {
        return _loaderDoor;
    }

    void DoorEntity::SetLoaderDoor(std::shared_ptr<DoorEntity> value) noexcept
    {
        _loaderDoor = std::move(value);
    }

    std::shared_ptr<DoorEntity> DoorEntity::ConnectorDoor() const noexcept
    {
        return _connectorDoor;
    }

    void DoorEntity::SetConnectorDoor(std::shared_ptr<DoorEntity> value) noexcept
    {
        _connectorDoor = std::move(value);
    }

    std::shared_ptr<Formats::Collision::Portal> DoorEntity::Portal() const noexcept
    {
        return _portal;
    }

    std::shared_ptr<ModelInstance> DoorEntity::ConnectorModel() const noexcept
    {
        return _connectorModel;
    }

    void DoorEntity::SetConnectorModel(std::shared_ptr<ModelInstance> value) noexcept
    {
        _connectorModel = std::move(value);
    }

    std::shared_ptr<Formats::Collision::CollisionInstance>
        DoorEntity::ConnectorCollision() const noexcept
    {
        return _connectorCollision;
    }

    void DoorEntity::SetConnectorCollision(
        std::shared_ptr<Formats::Collision::CollisionInstance> value) noexcept
    {
        _connectorCollision = std::move(value);
    }

    bool DoorEntity::ConnectorInactive() const noexcept
    {
        return _connectorInactive;
    }

    void DoorEntity::SetConnectorInactive(bool value) noexcept
    {
        _connectorInactive = value;
    }

    AnimationInfo& DoorEntity::AnimInfo()
    {
        return RequireReference(_models[0].AnimInfo);
    }

    const AnimationInfo& DoorEntity::AnimInfo() const
    {
        return RequireReference(_models[0].AnimInfo);
    }

    bool DoorEntity::Locked() const noexcept
    {
        return TestFlag(_flags, DoorFlags::Locked);
    }

    bool DoorEntity::Unlocked() const noexcept
    {
        return TestFlag(_flags, DoorFlags::Unlocked);
    }

    Vector3 DoorEntity::LockPosition() const
    {
        return Multiply(_transform, _lockTransform).Row3().Xyz();
    }

    DoorEntityData DoorEntity::Data() const
    {
        return _data;
    }

    bool DoorEntity::Compare(const char (&data)[16], const std::string& room) const
    {
        return StartsWithInvariantIgnoreCase(room, data, 15);
    }

    void DoorEntity::Initialize()
    {
        EntityBase::Initialize();
        _scene->LoadEffect(114, false); // lockDefeat
        if (_data.ConnectorId != 255 && _scene->Room() != nullptr)
        {
            _scene->Room()->AddConnector(this);
        }
        else
        {
            const std::string portalName = ::MphRead::MarshalExtensions::MarshalUtf8(_data.NodeName) + "_" + *_nodeName;
            std::shared_ptr<Formats::Collision::Portal> portal = _scene->Room() != nullptr
                ? _scene->Room()->GetPortalByName(portalName)
                : nullptr;
            if (portal != nullptr)
            {
                portal->Active = false;
                _portal = std::move(portal);
            }
        }
    }

    std::shared_ptr<Portal> DoorEntity::SetUpPort(
        std::string roomNodeName, std::string conNodeName)
    {
        assert(_portal == nullptr);
        const float height = GetChecked(_portHeights, static_cast<std::uint32_t>(_data.DoorType));
        const Vector3 facing = FacingVector();
        const Vector3 pos = static_cast<Vector3>(Position);
        const Vector3 up = UpVector();
        const Vector3 negUp = Negate(up);
        const Vector3 right = Vector3::Cross(facing, up).Normalized();
        const Vector3 negRight = Negate(right);
        const Vector3 widthVec = ScaleVector(right, _portWidth);
        const Vector3 heightVec = ScaleVector(up, height);
        auto points = std::make_shared<std::vector<Vector3>>();
        points->reserve(4);
        points->push_back(pos - widthVec);
        points->push_back(pos - widthVec + heightVec);
        points->push_back(pos + widthVec + heightVec);
        points->push_back(pos + widthVec);
        const Vector4 plane(facing, Vector3::Dot(facing, pos));
        // unlike with portals in collision files, these planes are computed
        // based on the assumption that doors are always axis aligned
        auto planes = std::make_shared<std::vector<Vector4>>();
        planes->reserve(4);
        planes->emplace_back(negRight, Vector3::Dot(negRight, (*points)[3]));
        planes->emplace_back(negUp, Vector3::Dot(negUp, (*points)[2]));
        planes->emplace_back(right, Vector3::Dot(right, (*points)[1]));
        planes->emplace_back(up, Vector3::Dot(up, (*points)[0]));
        _portal = std::make_shared<Formats::Collision::Portal>(
            std::move(roomNodeName), std::move(conNodeName), points, planes, plane);
        _portal->Active = false;
        return _portal;
    }

    void DoorEntity::UpdateScanId()
    {
        if (_data.DoorType == DoorType::Boss)
        {
            _scanId = 269;
        }
        else if (TestFlag(_flags, DoorFlags::Locked))
        {
            _scanId = GetChecked(_scanIds, _data.PaletteId);
        }
        else
        {
            _scanId = 251;
        }
    }

    void DoorEntity::GetPosition(Vector3& position)
    {
        position = LockPosition();
    }

    void DoorEntity::GetVectors(Vector3& position, Vector3& up, Vector3& facing)
    {
        position = LockPosition();
        up = UpVector();
        facing = FacingVector();
    }

    std::int32_t DoorEntity::GetScanId(bool alternate)
    {
        static_cast<void>(alternate);
        if (TestFlag(_flags, DoorFlags::ShouldOpen))
        {
            return 0;
        }
        return _scanId;
    }

    bool DoorEntity::Process()
    {
        if (_connectorInactive)
        {
            _flags &= ~DoorFlags::ShotOpen;
            _flags &= ~DoorFlags::ShouldOpen;
            ForceClose();
        }
        if (Unlocked() && TestFlag((*RequireReference(_lock).AnimInfo->Flags)[0], AnimFlags::Ended))
        {
            _flags &= ~DoorFlags::Locked;
            // the game doesn't clear the unlocked flag, which results in the story save update happening
            // every frame after a door is unlocked. in our case, that can cause room state issues during
            // room transitions, so we clear it. shouldn't cause any differences in behavior.
            _flags &= ~DoorFlags::Unlocked;
            RequireReference(::MphRead::GameState::StorySave).SetRoomState(_scene->RoomId(), Id, 1);
        }
        UpdateScanId();
        if (Locked() && !Unlocked())
        {
            _flags &= ~DoorFlags::ShotOpen;
        }
        if (!GameState::InRoomTransition())
        {
            if (ShouldOpen())
            {
                _flags |= DoorFlags::ShouldOpen;
            }
            else
            {
                _flags &= ~DoorFlags::ShouldOpen;
            }
        }
        if (TestFlag(_flags, DoorFlags::ShouldOpen)
            && _data.ConnectorId == 255 && _targetRoomId >= 0)
        {
            // the game also checks for loading connectors behind doors, but we do that at room load
            _flags &= ~DoorFlags::ShouldOpen;
            GameState::TransitionState(TransitionState::Start);
            assert(_scene->Room() != nullptr && _scene->Room()->LoaderDoor == nullptr);
            _scene->Room()->LoaderDoor = this;
            GameState::TransitionRoomId(_targetRoomId);
            auto enumerator = _scene->GetDoorEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                DoorEntity& other = RequireReference(enumerator.Current());
                if (other.LoaderDoor().get() == this)
                {
                    other.ForceClose();
                }
            }
        }
        _soundSource.Update(static_cast<Vector3>(Position), 6);
        if (TestFlag(_flags, DoorFlags::ShouldOpen))
        {
            _flags &= ~DoorFlags::Closed;
            AnimationInfo& animInfo = AnimInfo();
            if ((*animInfo.Index)[0] != 0)
            {
                _models[0].SetAnimation(0, 0,
                    SetFlags::Texture | SetFlags::Texcoord | SetFlags::Node,
                    AnimFlags::NoLoop);
                _soundSource.PlaySfx(SfxId::DOOR3_OPEN_SCR);
            }
            else if (TestFlag((*animInfo.Flags)[0], AnimFlags::Ended)
                && TestFlag((*animInfo.Flags)[0], AnimFlags::Reverse))
            {
                (*animInfo.Flags)[0] &= ~AnimFlags::Ended;
                (*animInfo.Flags)[0] &= ~AnimFlags::Paused;
                (*animInfo.Flags)[0] &= ~AnimFlags::Reverse;
                _soundSource.PlaySfx(_data.DoorType == DoorType::Boss
                    ? SfxId::DOOR2_OPEN
                    : SfxId::DOOR_OPEN);
            }
        }
        else
        {
            AnimationInfo& animInfo = AnimInfo();
            if ((*animInfo.Index)[0] == 0 && TestFlag((*animInfo.Flags)[0], AnimFlags::Ended))
            {
                if (!TestFlag((*animInfo.Flags)[0], AnimFlags::Reverse))
                {
                    _flags |= DoorFlags::Closed;
                    (*animInfo.Flags)[0] &= ~AnimFlags::Ended;
                    (*animInfo.Flags)[0] &= ~AnimFlags::Paused;
                    (*animInfo.Flags)[0] |= AnimFlags::Reverse;
                    (*animInfo.Flags)[1] = (*animInfo.Flags)[0];
                    SfxId sfx = SfxId::DOOR_CLOSE;
                    if (_data.DoorType == DoorType::Boss)
                    {
                        sfx = SfxId::DOOR2_CLOSE_SCR;
                    }
                    else if (_data.DoorType == DoorType::Thin)
                    {
                        sfx = SfxId::DOOR3_CLOSE_SCR;
                    }
                    _soundSource.PlaySfx(sfx);
                }
                else if (_data.DoorType == DoorType::Thin)
                {
                    _models[0].SetAnimation(1, 0,
                        SetFlags::Texture | SetFlags::Texcoord | SetFlags::Node);
                }
            }
        }
        {
            AnimationInfo& animInfo = AnimInfo();
            if (TestFlag(_flags, DoorFlags::ShotOpen))
            {
                if (_data.DoorType == DoorType::Boss
                    && TestFlag((*animInfo.Flags)[1], AnimFlags::Reverse))
                {
                    _soundSource.StopSfx(SfxId::DOOR2_LOOP);
                    if ((*animInfo.Frame)[1] < 2)
                    {
                        _soundSource.PlaySfx(SfxId::DOOR2_PRE_OPEN,
                            false, false, std::numeric_limits<float>::max(), true);
                    }
                }
                (*animInfo.Flags)[1] &= ~AnimFlags::Ended;
                (*animInfo.Flags)[1] &= ~AnimFlags::Paused;
                (*animInfo.Flags)[1] &= ~AnimFlags::Reverse;
            }
            else if (_data.DoorType == DoorType::Boss
                && TestFlag((*animInfo.Flags)[1], AnimFlags::Reverse))
            {
                _soundSource.PlaySfx(SfxId::DOOR2_LOOP, true);
            }
        }
        UpdateAnimFrames(_models[0]);
        UpdateAnimFrames(_models[1]);
        bool portalActive = false;
        if (TestFlag(_flags, DoorFlags::ShouldOpen))
        {
            if (_connectorModel != nullptr && !_connectorModel->Active)
            {
                assert(_scene->Room() != nullptr);
                _scene->Room()->ActivateConnector(this);
            }
            if (_data.ConnectorId != 255)
            {
                const char* roomName = _data.RoomName;
                std::int32_t id = 0;
                if (roomName[0] == 'C' && roomName[1] == 'o' && roomName[2] == 'n'
                    && TryParseInt32TwoChars(roomName + 3, id) && id >= 1)
                {
                    RequireReference(::MphRead::GameState::StorySave).SetVisitedConnector(id - 1, _scene->AreaId());
                }
            }
            // todo: FPS stuff
            if ((*AnimInfo().Frame)[0] > (*AnimInfo().FrameCount)[0] / 2)
            {
                _flags |= DoorFlags::Open;
            }
            if (_data.DoorType != DoorType::Standard || (*AnimInfo().Frame)[0] >= 10)
            {
                portalActive = true;
            }
        }
        else
        {
            AnimationInfo& animInfo = AnimInfo();
            if (TestFlag(_flags, DoorFlags::Closed))
            {
                if (_data.DoorType == DoorType::Thin)
                {
                    if ((*animInfo.Index)[0] == 0)
                    {
                        portalActive = true;
                    }
                }
                else if (!TestFlag((*animInfo.Flags)[0], AnimFlags::Ended))
                {
                    portalActive = true;
                }
            }
            else
            {
                portalActive = true;
            }
            _flags &= ~DoorFlags::Open;
        }
        if (_portal != nullptr)
        {
            _portal->Active = portalActive;
        }
        _flags &= ~DoorFlags::Opening;
        _flags &= ~DoorFlags::Bit10;
        if (TestFlag(_flags, DoorFlags::ShouldOpen))
        {
            _flags |= DoorFlags::Opening;
        }
        if (TestFlag(_flags, DoorFlags::Bit8))
        {
            _flags |= DoorFlags::Bit10;
        }
        {
            AnimationInfo& animInfo = AnimInfo();
            if (Locked() && TestFlag(_flags, DoorFlags::ShowLock)
                && !TestFlag(_flags, DoorFlags::ShouldOpen)
                && ((*animInfo.Index)[0] != 0
                    || TestFlag((*animInfo.Flags)[0], AnimFlags::Ended)))
            {
                // todo: bits 8/9 and 10 are basically a counter and a bool, and we should just replace them with that
                if (TestFlag(_flags, DoorFlags::Bit10) && _scene->FrameCount() > 3 * 2)
                {
                    _soundSource.PlaySfx(SfxId::LOCK_ANIM,
                        false, false, std::numeric_limits<float>::max(), true);
                }
                std::uint32_t flags = static_cast<std::uint32_t>(_flags);
                std::uint32_t bits = (flags << 22) >> 30;
                if (bits < 2)
                {
                    bits = (bits + 1) & 3;
                    flags &= 0xFFFFFCFFU;
                    flags |= bits << 8;
                    _flags = static_cast<DoorFlags>(flags);
                }
            }
            else
            {
                _flags &= ~DoorFlags::Bit8;
                _flags &= ~DoorFlags::Bit9;
            }
        }
        return true;
    }

    void DoorEntity::SetAnimationFrame(std::int32_t frame)
    {
        (*_models[0].AnimInfo->Frame)[1] = frame;
    }

    std::int32_t DoorEntity::GetAnimationFrame() const
    {
        return (*_models[0].AnimInfo->Frame)[1];
    }

    bool DoorEntity::ShouldOpen()
    {
        if (Locked() || !TestFlag(_flags, DoorFlags::ShotOpen))
        {
            return false;
        }
        auto enumerator = _scene->GetPlayerEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            PlayerEntity& player = RequireReference(enumerator.Current());
            const Vector3 distance = static_cast<Vector3>(Position)
                - static_cast<Vector3>(player.Position);
            if (player.Health() > 0 && !player.IsBot() && LengthSquared(distance) < 16.0F)
            {
                return true;
            }
        }
        if (TestFlag(_flags, DoorFlags::Opening))
        {
            _flags &= ~DoorFlags::ShotOpen;
        }
        return false;
    }

    void DoorEntity::ForceClose()
    {
        ModelInstance& inst = _models[0];
        if (_data.DoorType == DoorType::Thin)
        {
            inst.SetAnimation(1, 0,
                SetFlags::Texture | SetFlags::Texcoord | SetFlags::Node,
                AnimFlags::None);
        }
        else
        {
            inst.SetAnimation(0, 0,
                SetFlags::Texture | SetFlags::Texcoord | SetFlags::Node,
                AnimFlags::Ended | AnimFlags::NoLoop);
            (*inst.AnimInfo->Flags)[0] |= AnimFlags::Reverse;
        }
        if (_portal != nullptr)
        {
            _portal->Active = false;
        }
    }

    void DoorEntity::Lock(bool updateState)
    {
        _flags |= DoorFlags::Locked;
        if (updateState)
        {
            RequireReference(::MphRead::GameState::StorySave).SetRoomState(_scene->RoomId(), Id, 3);
        }
    }

    void DoorEntity::Unlock(bool updateState, bool noLockAnimSfx)
    {
        if (GameState::InRoomTransition())
        {
            return;
        }
        _flags |= DoorFlags::Unlocked;
        // hack to prevent door SFX from playing after boss room transitions/movies
        // UNIT2_B1, UNIT3_B1, UNIT1_B2, UNIT4_B2 (Cretaphid)
        // UNIT1_B1, UNIT4_B1, UNIT2_B2, UNIT3_B2 (Slench)
        // the game has its own weird hacks for this where fade state is checked in the timed SFX code.
        // note: lock SFX is prevented by the frame count check in Process()
        if (_scene->RoomId() != 55 && _scene->RoomId() != 71
            && _scene->RoomId() != 44 && _scene->RoomId() != 88
            && _scene->RoomId() != 35 && _scene->RoomId() != 82
            && _scene->RoomId() != 64 && _scene->RoomId() != 76)
        {
            auto&& mainValue = PlayerEntity::Main();
            PlayerEntity& main = RequireReference(mainValue);
            main.DoorChimeSfxTimer = 2.0F / 30.0F;
            if (!noLockAnimSfx)
            {
                main.DoorUnlockSfxTimer = 2.0F / 30.0F;
            }
        }
        RequireReference(_lock).SetAnimation(1, AnimFlags::NoLoop);
        _scene->SpawnEffect(114, UpVector(), FacingVector(), LockPosition()); // lockDefeat
        if (updateState)
        {
            RequireReference(::MphRead::GameState::StorySave).SetRoomState(_scene->RoomId(), Id, 1);
        }
    }

    void DoorEntity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::Unlock)
        {
            Unlock(true, false);
        }
        else if (info.Message == Message::Lock)
        {
            Lock(true);
        }
        else if (info.Message == Message::UnlockConnectors)
        {
            auto enumerator = _scene->GetDoorEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                DoorEntity& door = RequireReference(enumerator.Current());
                if (door.Id == -1)
                {
                    door.Unlock(true, false);
                }
            }
        }
        else if (info.Message == Message::LockConnectors)
        {
            auto enumerator = _scene->GetDoorEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                DoorEntity& door = RequireReference(enumerator.Current());
                if (door.Id == -1)
                {
                    door.Lock(true);
                }
            }
        }
    }

    void DoorEntity::Destroy()
    {
        _soundSource.StopSfx(SfxId::DOOR_OPEN);
        _loaderDoor.reset();
        _connectorDoor.reset();
        _portal.reset();
        _connectorModel.reset();
        _connectorCollision.reset();
        EntityBase::Destroy();
    }

    void DoorEntity::GetDrawInfo()
    {
        if (_connectorInactive)
        {
            return;
        }
        if (!IsVisible(NodeRef)
            && (_portal == nullptr
                || (!IsVisible(_portal->NodeRef1) && !IsVisible(_portal->NodeRef2))))
        {
            return;
        }
        RequireReference(_lock).Active = false;
        AnimationInfo& animInfo = AnimInfo();
        if (Locked() && TestFlag(_flags, DoorFlags::ShowLock)
            && !TestFlag(_flags, DoorFlags::ShouldOpen)
            && ((*animInfo.Index)[0] != 0
                || TestFlag((*animInfo.Flags)[0], AnimFlags::Ended)))
        {
            RequireReference(_lock).Active = true;
        }
        EntityBase::GetDrawInfo();
    }

    Matrix4 DoorEntity::GetModelTransform(ModelInstance& inst, std::int32_t index)
    {
        if (index == 1)
        {
            const std::shared_ptr<Model> model = inst.Model();
            const Matrix4 scale = CreateScale(RequireReference(model).Scale);
            return Multiply(Multiply(scale, _transform), _lockTransform);
        }
        return EntityBase::GetModelTransform(inst, index);
    }

    std::int32_t DoorEntity::GetModelRecolor(ModelInstance& inst, std::int32_t index)
    {
        static_cast<void>(inst);
        if (index == 1 || !TestFlag(_flags, DoorFlags::Locked))
        {
            return 0;
        }
        return Recolor();
    }

    FhDoorEntity::FhDoorEntity(FhDoorEntityData data, Scene* scene)
        : EntityBase(EntityType::FhDoor, scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        ModelInstance& inst = SetUpModel(
            GetChecked(Metadata::FhDoors, data.ModelId), 0,
            AnimFlags::None, true);
        inst.SetAnimation(0, AnimFlags::Ended | AnimFlags::NoLoop);
    }
}
