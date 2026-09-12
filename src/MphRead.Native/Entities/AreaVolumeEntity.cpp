#include "AreaVolumeEntity.hpp"

#include "../Formats/CollisionDetection.hpp"
#include "../GameState.hpp"
#include "../MemoryArrays.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Renderer.hpp"
#include "../Scene.hpp"
#include "BeamProjectileEntity.hpp"
#include "Players/PlayerEntity.hpp"
#include "TriggerVolumeEntity.hpp"

#include <any>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace
{
    [[nodiscard]] MphRead::MessageObject BoxInt32(std::int32_t value)
    {
        return std::make_shared<const std::any>(value);
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

    template <typename TEnum>
    [[nodiscard]] constexpr bool TestFlag(TEnum value, TEnum flag) noexcept
    {
        using Underlying = std::underlying_type_t<TEnum>;
        return (static_cast<Underlying>(value) & static_cast<Underlying>(flag)) != 0;
    }

    // C# masks Int32 shift counts to five bits. Using an unsigned shift followed by
    // bit_cast also preserves the signed Int32 bit pattern without C++ signed-shift UB.
    [[nodiscard]] constexpr std::int32_t ShiftOneInt32(std::int32_t count) noexcept
    {
        const std::uint32_t shift = static_cast<std::uint32_t>(count) & 0x1FU;
        return std::bit_cast<std::int32_t>(std::uint32_t{1} << shift);
    }

    [[nodiscard]] std::int32_t GetRoomId(MphRead::Scene* scene)
    {
        if (scene == nullptr)
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        return scene->RoomId;
    }

    [[nodiscard]] MphRead::Entities::AreaVolumeEntity* RequireAreaVolume(
        const std::shared_ptr<MphRead::Entities::AreaVolumeEntity>& entity)
    {
        if (!entity)
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        return entity.get();
    }

    [[nodiscard]] MphRead::Entities::PlayerEntity* RequirePlayer(
        const std::shared_ptr<MphRead::Entities::PlayerEntity>& entity)
    {
        if (!entity)
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        return entity.get();
    }

    [[nodiscard]] MphRead::Entities::BeamProjectileEntity* RequireBeam(
        const std::shared_ptr<MphRead::Entities::BeamProjectileEntity>& entity)
    {
        if (!entity)
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        return entity.get();
    }
}

namespace MphRead::Entities
{
    AreaVolumeEntity::AreaVolumeEntity(
        AreaVolumeEntityData data, std::string nodeName, Scene* scene)
        : EntityBase(EntityType::AreaVolume, std::move(nodeName), scene),
          _data(data),
          _cooldownSlots(PlayerEntity::SlotCapacity, 0),
          _triggeredSlots(PlayerEntity::SlotCapacity, false),
          _prioritySlots(PlayerEntity::SlotCapacity, 0)
    {
        Id = data.Header.EntityId;
        _insideEventColor = Metadata::GetEventColor(data.InsideMessage);
        _exitEventColor = Metadata::GetEventColor(data.ExitMessage);
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        _volume = CollisionVolume::Move(_data.Volume, Position);
        AddPlaceholderModel();

        for (std::size_t i = 0; i < _prioritySlots.size(); ++i)
        {
            _prioritySlots[i] = _data.Priority;
        }

        _cooldownTime = _data.Cooldown;
        if (_cooldownTime > 0)
        {
            _cooldownTime--;
        }
        _cooldownTime *= 2;

        if (GameState::Mode == GameMode::SinglePlayer)
        {
            StorySave* storySave = GameState::StorySave;
            const std::int32_t roomId = GetRoomId(_scene);
            if (storySave == nullptr)
            {
                throw Memory::Detail::NullReferenceException();
            }

            const std::int32_t state
                = storySave->InitRoomState(roomId, Id, data.Active != 0);
            if (data.AlwaysActive != 0)
            {
                Active = data.Active != 0;
            }
            else
            {
                Active = state != 0;
            }
        }
        else
        {
            Active = data.Active != 0;
        }
    }

    AreaVolumeEntityData AreaVolumeEntity::Data() const
    {
        return _data;
    }

    std::optional<::OpenTK::Mathematics::Vector4> AreaVolumeEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    void AreaVolumeEntity::Initialize()
    {
        EntityBase::Initialize();

        std::shared_ptr<EntityBase> entity;
        if (_scene->TryGetEntity(_data.ParentId, entity))
        {
            _parent = entity;
        }
        if (_scene->TryGetEntity(_data.ChildId, entity))
        {
            _child = entity;
        }
    }

    bool AreaVolumeEntity::GetTargetable()
    {
        return false;
    }

    void AreaVolumeEntity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::Activate)
        {
            Active = true;
            if (GameState::Mode == GameMode::SinglePlayer)
            {
                StorySave* storySave = GameState::StorySave;
                const std::int32_t roomId = GetRoomId(_scene);
                if (storySave == nullptr)
                {
                    throw Memory::Detail::NullReferenceException();
                }
                storySave->SetRoomState(roomId, Id, 3);
            }
        }
        else if (info.Message == Message::SetActive)
        {
            if (UnboxInt32(info.Param1) != 0)
            {
                Active = true;
                if (GameState::Mode == GameMode::SinglePlayer)
                {
                    StorySave* storySave = GameState::StorySave;
                    const std::int32_t roomId = GetRoomId(_scene);
                    if (storySave == nullptr)
                    {
                        throw Memory::Detail::NullReferenceException();
                    }
                    storySave->SetRoomState(roomId, Id, 3);
                }
            }
            else
            {
                Active = false;
                if (GameState::Mode == GameMode::SinglePlayer)
                {
                    StorySave* storySave = GameState::StorySave;
                    const std::int32_t roomId = GetRoomId(_scene);
                    if (storySave == nullptr)
                    {
                        throw Memory::Detail::NullReferenceException();
                    }
                    storySave->SetRoomState(roomId, Id, 1);
                }
            }
        }
    }

    void AreaVolumeEntity::GetDisplayVolumes()
    {
        if (_scene->ShowVolumes == VolumeDisplay::AreaInside
            || _scene->ShowVolumes == VolumeDisplay::AreaExit)
        {
            const ::OpenTK::Mathematics::Vector3 color
                = _scene->ShowVolumes == VolumeDisplay::AreaInside
                ? _insideEventColor
                : _exitEventColor;
            AddVolumeItem(_volume, color);
        }
    }

    EntityBase* AreaVolumeEntity::GetParent()
    {
        return _parent.get();
    }

    EntityBase* AreaVolumeEntity::GetChild()
    {
        return _child.get();
    }

    std::size_t AreaVolumeEntity::CheckedSlotIndex(std::int32_t slot) const
    {
        if (slot < 0 || static_cast<std::size_t>(slot) >= _triggeredSlots.size())
        {
            throw Memory::Detail::IndexOutOfRangeException();
        }
        return static_cast<std::size_t>(slot);
    }

    void AreaVolumeEntity::Trigger(PlayerEntity& player)
    {
        const std::size_t slot = CheckedSlotIndex(player.SlotIndex());
        if (!_triggeredSlots[slot])
        {
            SendInsideEvent(player);
        }
        else if (_data.AllowMultiple != 0)
        {
            const std::int32_t cooldown = _cooldownSlots[slot];
            if (cooldown > 0)
            {
                _cooldownSlots[slot] = cooldown - 1;
            }
            else
            {
                SendInsideEvent(player);
                _cooldownSlots[slot] = _cooldownTime;
            }
        }
    }

    void AreaVolumeEntity::SendInsideEvent(PlayerEntity& player)
    {
        const std::size_t slot = CheckedSlotIndex(player.SlotIndex());
        _triggeredSlots[slot] = true;

        if (_data.AllowMultiple == 0)
        {
            auto enumerator = _scene->GetAreaVolumeEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                AreaVolumeEntity* other = RequireAreaVolume(enumerator.Current());
                const std::size_t otherSlot = other->CheckedSlotIndex(player.SlotIndex());
                if (other != this
                    && other->_parent == _parent
                    && other->_triggeredSlots[otherSlot]
                    && other->Data().InsideMessage == _data.InsideMessage
                    && other->Data().InsideMsgParam1 == _data.InsideMsgParam1
                    && other->Data().InsideMsgParam2 == _data.InsideMsgParam2)
                {
                    return;
                }
            }
        }

        const Message message = _data.InsideMessage;
        if (message == Message::Damage
            || message == Message::Death
            || message == Message::Gravity
            || message == Message::PreventFormSwitch
            || message == Message::DripMoatPlatform)
        {
            _scene->SendMessage(
                message, this, &player,
                BoxInt32(_data.InsideMsgParam1), BoxInt32(_data.InsideMsgParam2));
        }
        else if (message != Message::Unused22)
        {
            _scene->SendMessage(
                message, this, _parent.get(),
                BoxInt32(_data.InsideMsgParam1), BoxInt32(_data.InsideMsgParam2),
                _data.MessageDelay);
        }
    }

    void AreaVolumeEntity::SendExitEvent(PlayerEntity& player)
    {
        const std::size_t slot = CheckedSlotIndex(player.SlotIndex());
        if (!_triggeredSlots[slot])
        {
            return;
        }

        _triggeredSlots[slot] = false;
        _prioritySlots[slot] = _data.Priority;
        _cooldownSlots[slot] = _cooldownTime;

        if (_data.InsideMessage == Message::Gravity)
        {
            auto enumerator = _scene->GetAreaVolumeEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                AreaVolumeEntity* other = RequireAreaVolume(enumerator.Current());
                const std::size_t otherSlot = other->CheckedSlotIndex(player.SlotIndex());
                other->_prioritySlots[otherSlot] = other->Data().Priority;
            }
        }

        auto enumerator = _scene->GetAreaVolumeEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            AreaVolumeEntity* other = RequireAreaVolume(enumerator.Current());
            const std::size_t otherSlot = other->CheckedSlotIndex(player.SlotIndex());
            if (other != this
                && other->_child == _child
                && other->_triggeredSlots[otherSlot]
                && other->Data().ExitMessage == _data.ExitMessage
                && other->Data().ExitMsgParam1 == _data.ExitMsgParam1
                && other->Data().ExitMsgParam2 == _data.ExitMsgParam2)
            {
                return;
            }
        }

        const Message message = _data.ExitMessage;
        if (message == Message::Damage || message == Message::Death)
        {
            _scene->SendMessage(
                message, this, &player,
                BoxInt32(_data.ExitMsgParam1), BoxInt32(_data.ExitMsgParam2));
        }
        else
        {
            _scene->SendMessage(
                message, this, _child.get(),
                BoxInt32(_data.ExitMsgParam1), BoxInt32(_data.ExitMsgParam2),
                _data.MessageDelay);
        }
    }

    bool AreaVolumeEntity::PrioritizeGravity(
        ::OpenTK::Mathematics::Vector3 position, std::int32_t slot)
    {
        const std::size_t slotIndex = CheckedSlotIndex(slot);
        bool result = true;

        auto enumerator = _scene->GetAreaVolumeEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            AreaVolumeEntity* other = RequireAreaVolume(enumerator.Current());
            if (other->Data().InsideMessage == _data.InsideMessage
                && other->_volume.TestPoint(position))
            {
                if (other->Data().Priority > _prioritySlots[slotIndex])
                {
                    _prioritySlots[slotIndex] = other->Data().Priority;
                }
                if (_data.Priority != _prioritySlots[slotIndex])
                {
                    result = false;
                }
            }
        }
        return result;
    }

    bool AreaVolumeEntity::Process()
    {
        if (!Active)
        {
            return EntityBase::Process();
        }

        const TriggerFlags flags = _data.TriggerFlags;
        auto enumerator = _scene->GetPlayerEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            PlayerEntity* player = RequirePlayer(enumerator.Current());

            if (GameState::Mode == GameMode::SinglePlayer
                && player != PlayerEntity::Main())
            {
                continue;
            }

            for (std::size_t i = 0; i < player->EquipInfo().Beams().size(); ++i)
            {
                BeamProjectileEntity* beam = RequireBeam(player->EquipInfo().Beams()[i]);
                if (beam->Lifespan() > 0)
                {
                    Formats::CollisionResult discard{};
                    const std::int32_t beamBit
                        = ShiftOneInt32(static_cast<std::int32_t>(beam->Beam()));
                    if ((static_cast<std::int32_t>(flags) & beamBit) != 0
                        && (TestFlag(beam->Flags(), BeamFlags::Charged)
                            || !TestFlag(flags, TriggerFlags::BeamCharged)))
                    {
                        if (Formats::CollisionDetection::CheckCylinderOverlapVolume(
                            &_volume, beam->BackPosition(), beam->Position, 0.1F, discard))
                        {
                            if (_data.InsideMessage == Message::Gravity)
                            {
                                PrioritizeGravity(beam->Position, player->SlotIndex());
                            }
                            Trigger(*player);
                        }
                        else
                        {
                            SendExitEvent(*player);
                        }
                    }
                }
            }

            if ((player->IsAltForm() && TestFlag(flags, TriggerFlags::PlayerAlt))
                || (!player->IsAltForm() && TestFlag(flags, TriggerFlags::PlayerBiped)))
            {
                if (TestFlag(player->LoadFlags(), LoadFlags::Spawned)
                    && _volume.TestPoint(player->Position))
                {
                    bool trigger = true;
                    if (_data.InsideMessage == Message::Gravity)
                    {
                        trigger = PrioritizeGravity(player->Position, player->SlotIndex());
                    }
                    if (trigger)
                    {
                        Trigger(*player);
                    }
                }
                else
                {
                    SendExitEvent(*player);
                }
            }
        }

        return EntityBase::Process();
    }

    FhAreaVolumeEntity::FhAreaVolumeEntity(FhAreaVolumeEntityData data, Scene* scene)
        : EntityBase(EntityType::FhAreaVolume, scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        _insideEventColor = Metadata::GetEventColor(data.InsideMessage);
        _exitEventColor = Metadata::GetEventColor(data.ExitMessage);
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        _volume = CollisionVolume::Move(_data.ActiveVolume(), Position);
        AddPlaceholderModel();
    }

    FhAreaVolumeEntityData FhAreaVolumeEntity::Data() const
    {
        return _data;
    }

    std::optional<::OpenTK::Mathematics::Vector4> FhAreaVolumeEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    void FhAreaVolumeEntity::GetDisplayVolumes()
    {
        if (_scene->ShowVolumes == VolumeDisplay::AreaInside
            || _scene->ShowVolumes == VolumeDisplay::AreaExit)
        {
            const ::OpenTK::Mathematics::Vector3 color
                = _scene->ShowVolumes == VolumeDisplay::AreaInside
                ? _insideEventColor
                : _exitEventColor;
            AddVolumeItem(_volume, color);
        }
    }
}
