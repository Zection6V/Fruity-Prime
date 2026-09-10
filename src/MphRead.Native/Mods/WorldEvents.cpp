#include "WorldEvents.hpp"

#include <bit>

namespace MphRead::Mods::Detail
{
    std::int32_t WorldEventsPlayerSlotIndex(const Entities::PlayerEntity& player) noexcept;
}

namespace
{
    void IncrementInt32(std::int32_t& value) noexcept
    {
        std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
        bits += 1U;
        value = std::bit_cast<std::int32_t>(bits);
    }
}

namespace MphRead::Mods
{
    bool WorldEvents::_watching = false;
    std::array<std::int32_t, static_cast<std::size_t>(WorldEvents::SlotCapacity)> WorldEvents::_jumpPads{};
    std::array<std::int32_t, static_cast<std::size_t>(WorldEvents::SlotCapacity)> WorldEvents::_teleports{};
    std::array<std::int32_t, static_cast<std::size_t>(WorldEvents::SlotCapacity)> WorldEvents::_lastJumpPadId{};
    std::array<std::int32_t, static_cast<std::size_t>(WorldEvents::SlotCapacity)> WorldEvents::_lastTeleporterId{};

    bool WorldEvents::Watching() noexcept
    {
        return _watching;
    }

    void WorldEvents::Watching(bool value) noexcept
    {
        _watching = value;
    }

    void WorldEvents::Reset() noexcept
    {
        for (std::int32_t i = 0; i < SlotCapacity; i++)
        {
            const std::size_t index = static_cast<std::size_t>(i);
            _jumpPads[index] = 0;
            _teleports[index] = 0;
            _lastJumpPadId[index] = -1;
            _lastTeleporterId[index] = -1;
        }
    }

    std::int32_t WorldEvents::JumpPadsFor(std::int32_t slot) noexcept
    {
        return Valid(slot) ? _jumpPads[static_cast<std::size_t>(slot)] : 0;
    }

    std::int32_t WorldEvents::TeleportsFor(std::int32_t slot) noexcept
    {
        return Valid(slot) ? _teleports[static_cast<std::size_t>(slot)] : 0;
    }

    std::int32_t WorldEvents::LastJumpPadId(std::int32_t slot) noexcept
    {
        return Valid(slot) ? _lastJumpPadId[static_cast<std::size_t>(slot)] : -1;
    }

    std::int32_t WorldEvents::LastTeleporterId(std::int32_t slot) noexcept
    {
        return Valid(slot) ? _lastTeleporterId[static_cast<std::size_t>(slot)] : -1;
    }

    void WorldEvents::NoteJumpPad(const Entities::PlayerEntity& player, std::int32_t entityId) noexcept
    {
        if (!Watching() || !Valid(Detail::WorldEventsPlayerSlotIndex(player)))
        {
            return;
        }
        const std::size_t countIndex = static_cast<std::size_t>(Detail::WorldEventsPlayerSlotIndex(player));
        IncrementInt32(_jumpPads[countIndex]);
        const std::size_t idIndex = static_cast<std::size_t>(Detail::WorldEventsPlayerSlotIndex(player));
        _lastJumpPadId[idIndex] = entityId;
    }

    void WorldEvents::NoteTeleport(const Entities::PlayerEntity& player, std::int32_t entityId) noexcept
    {
        if (!Watching() || !Valid(Detail::WorldEventsPlayerSlotIndex(player)))
        {
            return;
        }
        const std::size_t countIndex = static_cast<std::size_t>(Detail::WorldEventsPlayerSlotIndex(player));
        IncrementInt32(_teleports[countIndex]);
        const std::size_t idIndex = static_cast<std::size_t>(Detail::WorldEventsPlayerSlotIndex(player));
        _lastTeleporterId[idIndex] = entityId;
    }

    bool WorldEvents::Valid(std::int32_t slot) noexcept
    {
        return slot >= 0 && slot < SlotCapacity;
    }
}
