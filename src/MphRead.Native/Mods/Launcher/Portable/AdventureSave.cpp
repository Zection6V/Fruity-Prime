#include "AdventureSave.hpp"

#include "../../../Formats/Types.hpp"
#include "../../../GameState.hpp"
#include "../../../Menu.hpp"
#include "../../../Metadata/Metadata.hpp"
#include "../../../Metadata/Rooms.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"

#include <new>
#include <string>
#include <vector>

namespace MphRead::Mods::Launcher
{
    const std::array<std::string_view, 9> AdventureSave::_areaNames =
    {
        "Alinos", "Alinos", "Celestial Archives", "Celestial Archives",
        "Vesper Defense Outpost", "Vesper Defense Outpost",
        "Arcterra", "Arcterra", "Oubliette"
    };

    AdventureSave::SlotInfo& AdventureSave::SlotInfo::operator=(const SlotInfo& other)
    {
        if (std::addressof(*this) != std::addressof(other))
        {
            this->~SlotInfo();
            ::new (static_cast<void*>(this)) SlotInfo(other);
        }
        return *this;
    }

    std::string AdventureSave::SlotInfo::Describe() const
    {
        if (!Used)
        {
            return "Empty";
        }
        return Area.value_or("") + " — " + ::MphRead::NativeRuntime::ToString(Octoliths) + "/8 octoliths";
    }

    AdventureSave::SlotInfo AdventureSave::Read(std::uint8_t slot)
    {
        std::shared_ptr<MphRead::StorySave> save = MphRead::GameState::PeekSave(slot);
        if (save == nullptr)
        {
            return SlotInfo
            {
                .Slot = slot,
                .Used = false,
                .Area = std::string()
            };
        }
        return SlotInfo
        {
            .Slot = slot,
            .Used = true,
            .Area = AreaName(save),
            .Octoliths = save->CountFoundOctoliths(),
            .Health = save->Health,
            .HealthMax = save->HealthMax
        };
    }

    std::vector<AdventureSave::SlotInfo> AdventureSave::ReadAll()
    {
        std::vector<SlotInfo> slots;
        slots.reserve(SlotCount);
        for (std::uint8_t slot = 1; slot <= SlotCount; slot++)
        {
            slots.push_back(Read(slot));
        }
        return slots;
    }

    std::string AdventureSave::AreaName(std::shared_ptr<MphRead::StorySave> save)
    {
        if (save == nullptr)
        {
            throw System::NullReferenceException();
        }
        const std::int32_t roomId = save->CheckpointRoomId;
        if (roomId < 0)
        {
            return std::string(_areaNames[2]);
        }
        const std::int32_t areaId = MphRead::Metadata::GetAreaInfo(roomId);
        if (areaId < 0 || areaId >= static_cast<std::int32_t>(_areaNames.size()))
        {
            return "Unknown";
        }
        return std::string(_areaNames[static_cast<std::size_t>(areaId)]);
    }

    std::string AdventureSave::Begin(std::uint8_t slot, bool newGame)
    {
        MphRead::Menu::SaveSlot = slot;
        if (newGame)
        {
            MphRead::GameState::StartNewSave();
        }
        else
        {
            MphRead::GameState::LoadSave();
        }
        return StartRoom(MphRead::GameState::StorySave);
    }

    std::string AdventureSave::StartRoom(std::shared_ptr<MphRead::StorySave> save)
    {
        if (save == nullptr)
        {
            throw System::NullReferenceException();
        }
        std::int32_t roomId = save->CheckpointRoomId;
        if (roomId < 0)
        {
            roomId = _newGameRoomId;
        }
        const MphRead::RoomMetadata* meta = MphRead::Metadata::GetRoomById(roomId, true);
        if (meta == nullptr)
        {
            meta = MphRead::Metadata::GetRoomById(_newGameRoomId, true);
        }
        return meta == nullptr ? std::string() : meta->Name;
    }
}
