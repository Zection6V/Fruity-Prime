#include "NetHealthSync.hpp"

#include "NetSession.hpp"

#include "../Multiplayer/MapResourceRules.hpp"
#include "../../Entities/EntityBase.hpp"
#include "../../Entities/ItemInstanceEntity.hpp"
#include "../../Entities/ItemSpawnEntity.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/BinaryPrimitives.hpp"
#include "../../Program.hpp"

namespace MphRead::Mods::Network
{
    namespace Runtime = ::MphRead::NativeRuntime;

    std::vector<std::shared_ptr<::MphRead::Entities::ItemSpawnEntity>> NetHealthSync::_spawns{};
    std::map<std::int16_t, HealthSpawnState> NetHealthSync::_states{};

    const std::vector<std::shared_ptr<::MphRead::Entities::ItemSpawnEntity>>&
        NetHealthSync::RegisteredSpawns() noexcept
    {
        return _spawns;
    }

    bool NetHealthSync::IsReplica()
    {
        return NetSession::Active() && !NetSession::IsAuthority();
    }

    bool NetHealthSync::OwnsPickup(const ::MphRead::Entities::ItemInstanceEntity& item)
    {
        return !IsReplica() || item.Owner() == nullptr
            || !Multiplayer::MapResourceRules::IsHealth(item.ItemType());
    }

    void NetHealthSync::BeginRoom()
    {
        _spawns.clear();
        _states.clear();
    }

    void NetHealthSync::Register(::MphRead::Entities::ItemSpawnEntity& spawn)
    {
        if (!NetSession::Active() || !Multiplayer::MapResourceRules::IsHealth(spawn.Data().ItemType))
        {
            return;
        }
        if (static_cast<std::int32_t>(_spawns.size()) == MaxSpawns)
        {
            throw ::MphRead::ProgramException("Network maps support at most "
                + std::to_string(MaxSpawns) + " health spawners.");
        }
        _spawns.push_back(::MphRead::Entities::SharedFrom(spawn));
    }

    bool NetHealthSync::TryGet(std::int16_t id, HealthSpawnState& state)
    {
        const auto found = _states.find(id);
        if (found == _states.end())
        {
            state = HealthSpawnState{};
            return false;
        }
        state = found->second;
        return true;
    }

    bool NetHealthSync::IsCurrentMatch(std::span<const std::uint8_t> src)
    {
        return src.size() >= static_cast<std::size_t>(HeaderSize)
            && Runtime::ReadUInt16LittleEndian(src) == NetSession::CurrentMatchId();
    }

    std::int32_t NetHealthSync::Write(std::span<std::uint8_t> dest)
    {
        const std::int32_t length = HeaderSize + EntrySize * static_cast<std::int32_t>(_spawns.size());
        if (static_cast<std::int32_t>(dest.size()) < length)
        {
            throw ::MphRead::ProgramException("Health state exceeds snapshot capacity.");
        }
        Runtime::WriteUInt16LittleEndian(dest, NetSession::CurrentMatchId());
        dest[2] = static_cast<std::uint8_t>(_spawns.size());
        std::size_t offset = HeaderSize;
        for (const std::shared_ptr<::MphRead::Entities::ItemSpawnEntity>& spawn : _spawns)
        {
            const HealthSpawnState state = spawn->ModHealthState();
            const std::int32_t encodedPicker = state.PickerSlot + 1;
            if (encodedPicker < 0 || encodedPicker > ::MphRead::Entities::PlayerEntity::SlotCapacity)
            {
                throw ::MphRead::ProgramException("Invalid health pickup slot "
                    + std::to_string(state.PickerSlot) + ".");
            }
            Runtime::WriteInt16LittleEndian(Runtime::SpanSlice(dest, offset), static_cast<std::int16_t>(spawn->Id));
            dest[offset + 2] = static_cast<std::uint8_t>((state.Available ? 1 : 0) | (state.Active ? 2 : 0)
                | (encodedPicker << PickerShift));
            Runtime::WriteUInt16LittleEndian(Runtime::SpanSlice(dest, offset + 3), state.Cooldown);
            Runtime::WriteUInt16LittleEndian(Runtime::SpanSlice(dest, offset + 5), state.SpawnCount);
            offset += EntrySize;
        }
        return length;
    }

    bool NetHealthSync::Validate(std::span<const std::uint8_t> src)
    {
        if (src.size() < static_cast<std::size_t>(HeaderSize) || src[2] > MaxSpawns
            || src.size() != static_cast<std::size_t>(HeaderSize + src[2] * EntrySize))
        {
            return false;
        }
        for (std::size_t offset = HeaderSize; offset < src.size(); offset += EntrySize)
        {
            const std::uint8_t flags = src[offset + 2];
            const std::int32_t encodedPicker = (flags >> PickerShift) & PickerMask;
            if ((flags & ReservedMask) != 0 || encodedPicker > ::MphRead::Entities::PlayerEntity::SlotCapacity)
            {
                return false;
            }
            const std::int16_t id = Runtime::ReadInt16LittleEndian(Runtime::SpanSlice(src, offset));
            if (id < 0)
            {
                return false;
            }
            for (std::size_t previous = HeaderSize; previous < offset; previous += EntrySize)
            {
                if (Runtime::ReadInt16LittleEndian(Runtime::SpanSlice(src, previous)) == id)
                {
                    return false;
                }
            }
        }
        return true;
    }

    void NetHealthSync::Receive(std::span<const std::uint8_t> src)
    {
        if (!Validate(src) || !IsCurrentMatch(src))
        {
            return;
        }
        _states.clear();
        for (std::size_t offset = HeaderSize; offset < src.size(); offset += EntrySize)
        {
            const std::uint8_t flags = src[offset + 2];
            const auto pickerSlot = static_cast<std::int8_t>(((flags >> PickerShift) & PickerMask) - 1);
            const std::int16_t id = Runtime::ReadInt16LittleEndian(Runtime::SpanSlice(src, offset));
            HealthSpawnState state;
            state.Available = (flags & 1) != 0;
            state.Active = (flags & 2) != 0;
            state.Cooldown = Runtime::ReadUInt16LittleEndian(Runtime::SpanSlice(src, offset + 3));
            state.SpawnCount = Runtime::ReadUInt16LittleEndian(Runtime::SpanSlice(src, offset + 5));
            state.PickerSlot = pickerSlot;
            // Dictionary.Add: Validate has already refused a repeated id.
            _states.emplace(id, state);
        }
    }
}
