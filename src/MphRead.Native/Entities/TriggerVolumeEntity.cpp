#include "TriggerVolumeEntity.hpp"

#include "../Formats/CollisionDetection.hpp"
#include "../GameState.hpp"
#include "../MemoryArrays.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Scene.hpp"
#include "../SceneSetup.hpp"
#include "BeamProjectileEntity.hpp"
#include "Players/PlayerEntity.hpp"

#include <any>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

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

    // C# masks Int32 shift counts to five bits. The unsigned shift plus bit_cast
    // preserves the Int32 bit pattern without invoking C++ signed-shift UB.
    [[nodiscard]] constexpr std::int32_t ShiftOneInt32(std::int32_t count) noexcept
    {
        const std::uint32_t shift = static_cast<std::uint32_t>(count) & 0x1FU;
        return std::bit_cast<std::int32_t>(std::uint32_t{1} << shift);
    }

    [[nodiscard]] constexpr std::int32_t UncheckedIncrement(std::int32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(value) + std::uint32_t{1});
    }

    [[nodiscard]] constexpr bool Int32LessThanUInt32(
        std::int32_t left, std::uint32_t right) noexcept
    {
        return static_cast<std::int64_t>(left) < static_cast<std::int64_t>(right);
    }

    [[nodiscard]] constexpr bool Int32EqualsUInt32(
        std::int32_t left, std::uint32_t right) noexcept
    {
        return static_cast<std::int64_t>(left) == static_cast<std::int64_t>(right);
    }

    [[nodiscard]] std::int32_t GetRoomId(MphRead::Scene* scene)
    {
        if (scene == nullptr)
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        return scene->RoomId();
    }

    [[nodiscard]] MphRead::StorySave& RequireStorySave()
    {
        if (MphRead::GameState::StorySave == nullptr)
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        return *MphRead::GameState::StorySave;
    }

    void SetRoomState(MphRead::Scene* scene, std::int32_t id, std::int32_t state)
    {
        MphRead::StorySave* storySave = MphRead::GameState::StorySave.get();
        const std::int32_t roomId = GetRoomId(scene);
        if (storySave == nullptr)
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        storySave->SetRoomState(roomId, id, state);
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

    [[nodiscard]] MphRead::BeamProjectileArray& RequireBeams(
        const std::shared_ptr<MphRead::EquipInfo>& equip)
    {
        if (!equip || !equip->Beams)
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        return *equip->Beams;
    }
}

namespace MphRead::Entities
{
    TriggerVolumeEntity::TriggerVolumeEntity(TriggerVolumeEntityData data, Scene* scene)
        : EntityBase(EntityType::TriggerVolume, scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        // C# source: todo: change the display/color when inactive (same for AreaVolumes).
        _parentEventColor = Metadata::GetEventColor(data.ParentMessage);
        _childEventColor = Metadata::GetEventColor(data.ChildMessage);
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        _volume = CollisionVolume::Move(data.Volume, Position);
        AddPlaceholderModel();

        assert(GameState::Mode() == GameMode::SinglePlayer);
        StorySave* storySave = GameState::StorySave.get();
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

        _delayTimer = static_cast<std::int32_t>(data.RepeatDelay) * 2;
        _parentMsgParam2 = GetParam2(data.ParentMessage, data.ParentMsgParam2);
        _childMsgParam2 = GetParam2(data.ChildMessage, data.ChildMsgParam2);
    }

    CollisionVolume TriggerVolumeEntity::Volume() const
    {
        return _volume;
    }

    TriggerVolumeEntityData TriggerVolumeEntity::Data() const
    {
        return _data;
    }

    std::optional<::OpenTK::Mathematics::Vector4> TriggerVolumeEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    void TriggerVolumeEntity::Initialize()
    {
        EntityBase::Initialize();

        std::shared_ptr<EntityBase> parent;
        if (_scene->TryGetEntity(_data.ParentId, parent))
        {
            _parent = parent;
        }

        std::shared_ptr<EntityBase> child;
        if (_scene->TryGetEntity(_data.ChildId, child))
        {
            _child = child;
        }
    }

    bool TriggerVolumeEntity::GetTargetable()
    {
        return false;
    }

    void TriggerVolumeEntity::GetDisplayVolumes()
    {
        if (_data.Subtype == TriggerType::Volume
            && (_scene->ShowVolumes() == VolumeDisplay::TriggerParent
                || _scene->ShowVolumes() == VolumeDisplay::TriggerChild))
        {
            const ::OpenTK::Mathematics::Vector3 color
                = _scene->ShowVolumes() == VolumeDisplay::TriggerParent
                ? _parentEventColor
                : _childEventColor;
            AddVolumeItem(_volume, color);
        }
    }

    EntityBase* TriggerVolumeEntity::GetParent()
    {
        return _parent.get();
    }

    EntityBase* TriggerVolumeEntity::GetChild()
    {
        return _child.get();
    }

    bool TriggerVolumeEntity::Trigger()
    {
        if (_delayTimer > 0)
        {
            _delayTimer--;
            return false;
        }

        if (_data.ParentMessage != Message::None)
        {
            if (_data.DeactivateAfterUse != 0)
            {
                Deactivate();
            }
            _scene->SendMessage(
                _data.ParentMessage, this, _parent.get(),
                BoxInt32(_data.ParentMsgParam1), BoxInt32(_parentMsgParam2));
        }

        if (_data.ChildMessage != Message::None)
        {
            if (_data.DeactivateAfterUse != 0)
            {
                Deactivate();
            }
            _scene->SendMessage(
                _data.ChildMessage, this, _child.get(),
                BoxInt32(_data.ChildMsgParam1), BoxInt32(_childMsgParam2));
        }

        _delayTimer = static_cast<std::int32_t>(_data.RepeatDelay) * 2;
        if (_data.Subtype == TriggerType::StateBits)
        {
            Active = false;
        }
        return true;
    }

    std::int32_t TriggerVolumeEntity::GetParam2(
        Message message, std::int32_t param2) const noexcept
    {
        if (message != Message::UpdateMusic)
        {
            return param2;
        }

        // Preserve the source's base-10/base-16 field conversion exactly.
        const std::uint32_t param = static_cast<std::uint32_t>(param2);
        const std::uint32_t result
            = ((param % 100U) & 0x7FU)
            | ((((param / 100U % 10U) & 1U) << 7U) & ~0x100U)
            | (((param / 0x3E8U % 10U) & 1U) << 8U);
        return std::bit_cast<std::int32_t>(result);
    }

    bool TriggerVolumeEntity::Process()
    {
        if (!Active)
        {
            return EntityBase::Process();
        }

        if (_data.Subtype == TriggerType::Volume)
        {
            bool colliding = false;
            const TriggerFlags flags = _data.TriggerFlags;

            auto enumerator = _scene->GetPlayerEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                PlayerEntity* player = RequirePlayer(enumerator.Current());

                for (std::int32_t i = 0; i < RequireBeams(player->EquipInfo()).Length(); ++i)
                {
                    BeamProjectileEntity* beam = RequireBeam(RequireBeams(player->EquipInfo())[i]);
                    if (beam->Lifespan() > 0)
                    {
                        Formats::CollisionResult discard{};
                        const std::int32_t beamBit
                            = ShiftOneInt32(static_cast<std::int32_t>(beam->Beam()));
                        const std::int32_t flagBits = std::bit_cast<std::int32_t>(
                            static_cast<std::uint32_t>(flags));
                        if ((flagBits & beamBit) != 0
                            && (TestFlag(beam->Flags(), BeamFlags::Charged)
                                || !TestFlag(flags, TriggerFlags::BeamCharged))
                            && Formats::CollisionDetection::CheckCylinderOverlapVolume(
                                &_volume, beam->BackPosition(), beam->Position, 0.1F, discard))
                        {
                            Trigger();
                            colliding = true;
                            break;
                        }
                    }
                }

                if ((!player->IsBot() || TestFlag(flags, TriggerFlags::IncludeBots))
                    && ((player->IsAltForm() && TestFlag(flags, TriggerFlags::PlayerAlt))
                        || (!player->IsAltForm() && TestFlag(flags, TriggerFlags::PlayerBiped)))
                    && TestFlag(player->LoadFlags(), LoadFlags::Spawned)
                    && _volume.TestPoint(player->Position))
                {
                    Trigger();
                    colliding = true;
                    break;
                }
            }

            if (!colliding && _data.CheckDelay != 0)
            {
                _delayTimer = static_cast<std::int32_t>(_data.CheckDelay) * 2;
            }
        }
        else if (_data.Subtype == TriggerType::Threshold)
        {
            if (Int32EqualsUInt32(_count, _data.TriggerThreshold) && Trigger())
            {
                _count = 0;
            }
        }
        else if (_data.Subtype == TriggerType::Automatic)
        {
            if (Trigger() && _data.DeactivateAfterUse != 0)
            {
                Deactivate();
            }
        }
        else if (_data.Subtype == TriggerType::StateBits)
        {
            const std::int32_t index = static_cast<std::int32_t>(_data.RequiredStateBit);
            const auto& triggerStateRef = RequireStorySave().TriggerState;
            if (!triggerStateRef)
            {
                throw Memory::Detail::NullReferenceException();
            }
            const auto& triggerState = *triggerStateRef;
            const std::size_t byteIndex = static_cast<std::size_t>(index / 8);
            if (byteIndex >= triggerState.Length())
            {
                throw Memory::Detail::IndexOutOfRangeException();
            }
            if ((static_cast<std::int32_t>(triggerState[byteIndex])
                    & ShiftOneInt32(index % 8)) != 0)
            {
                Trigger();
            }
        }

        return EntityBase::Process();
    }

    void TriggerVolumeEntity::Deactivate()
    {
        Active = false;
        StorySave* storySave = GameState::StorySave.get();
        const std::int32_t roomId = GetRoomId(_scene);
        if (storySave == nullptr)
        {
            throw Memory::Detail::NullReferenceException();
        }
        storySave->SetRoomState(roomId, Id, 1);
    }

    void TriggerVolumeEntity::HandleMessage(MessageInfo info)
    {
        if (_data.Subtype == TriggerType::Relay)
        {
            if (_parent != nullptr)
            {
                _scene->SendMessage(
                    info.Message, info.Sender, _parent.get(), info.Param1, info.Param2);
            }
            if (_child != nullptr)
            {
                _scene->SendMessage(
                    info.Message, info.Sender, _child.get(), info.Param1, info.Param2);
            }
        }
        else
        {
            if (info.Message == Message::Trigger)
            {
                if (_data.Subtype == TriggerType::Threshold)
                {
                    if (Int32LessThanUInt32(_count, _data.TriggerThreshold))
                    {
                        _count = UncheckedIncrement(_count);
                    }
                    if (Int32EqualsUInt32(_count, _data.TriggerThreshold))
                    {
                        _delayTimer = static_cast<std::int32_t>(_data.CheckDelay) * 2;
                    }
                }
            }
            else if (info.Message == Message::Activate)
            {
                Active = true;
                SetRoomState(_scene, Id, 3);
            }
            else if (info.Message == Message::SetActive)
            {
                if (UnboxInt32(info.Param1) != 0)
                {
                    Active = true;
                    SetRoomState(_scene, Id, 3);
                }
                else
                {
                    Active = false;
                    if (_data.Subtype == TriggerType::Automatic)
                    {
                        _delayTimer = static_cast<std::int32_t>(_data.RepeatDelay) * 2;
                    }
                    SetRoomState(_scene, Id, 1);
                }
            }
        }
    }

    FhTriggerVolumeEntity::FhTriggerVolumeEntity(
        FhTriggerVolumeEntityData data, Scene* scene)
        : EntityBase(EntityType::FhTriggerVolume, scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        _parentEventColor = Metadata::GetEventColor(data.ParentMessage);
        _childEventColor = Metadata::GetEventColor(data.ChildMessage);
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        _volume = CollisionVolume::Move(_data.ActiveVolume(), Position);
        AddPlaceholderModel();
    }

    FhTriggerVolumeEntityData FhTriggerVolumeEntity::Data() const
    {
        return _data;
    }

    std::optional<::OpenTK::Mathematics::Vector4> FhTriggerVolumeEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    void FhTriggerVolumeEntity::Initialize()
    {
        EntityBase::Initialize();

        std::shared_ptr<EntityBase> parent;
        if (_scene->TryGetEntity(_data.ParentId, parent))
        {
            _parent = parent;
        }

        std::shared_ptr<EntityBase> child;
        if (_scene->TryGetEntity(_data.ChildId, child))
        {
            _child = child;
        }
    }

    void FhTriggerVolumeEntity::GetDisplayVolumes()
    {
        if (_data.Subtype != FhTriggerType::Threshold
            && (_scene->ShowVolumes() == VolumeDisplay::TriggerParent
                || _scene->ShowVolumes() == VolumeDisplay::TriggerChild))
        {
            const ::OpenTK::Mathematics::Vector3 color
                = _scene->ShowVolumes() == VolumeDisplay::TriggerParent
                ? _parentEventColor
                : _childEventColor;
            AddVolumeItem(_volume, color);
        }
    }

    EntityBase* FhTriggerVolumeEntity::GetParent()
    {
        return _parent.get();
    }

    EntityBase* FhTriggerVolumeEntity::GetChild()
    {
        return _child.get();
    }
}
