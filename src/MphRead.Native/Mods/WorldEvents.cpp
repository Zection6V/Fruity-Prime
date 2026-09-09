#include "Mods/world_events.hpp"

#include <algorithm>

namespace fruityprime::world {
namespace {

std::array<int, WorldEvents::SlotCapacity> jump_pads{};
std::array<int, WorldEvents::SlotCapacity> teleports{};
std::array<int, WorldEvents::SlotCapacity> last_jump_pad_id{};
std::array<int, WorldEvents::SlotCapacity> last_teleporter_id{};

} // namespace

void WorldEvents::Reset() noexcept {
    jump_pads.fill(0);
    teleports.fill(0);
    last_jump_pad_id.fill(-1);
    last_teleporter_id.fill(-1);
}

void WorldEvents::NoteJumpPad(int slot, int entity_id) noexcept {
    if (Watching && Valid(slot)) {
        ++jump_pads[static_cast<std::size_t>(slot)];
        last_jump_pad_id[static_cast<std::size_t>(slot)] = entity_id;
    }
}

void WorldEvents::NoteTeleport(int slot, int entity_id) noexcept {
    if (Watching && Valid(slot)) {
        ++teleports[static_cast<std::size_t>(slot)];
        last_teleporter_id[static_cast<std::size_t>(slot)] = entity_id;
    }
}

int WorldEvents::JumpPadsFor(int slot) noexcept {
    return Valid(slot) ? jump_pads[static_cast<std::size_t>(slot)] : 0;
}

int WorldEvents::TeleportsFor(int slot) noexcept {
    return Valid(slot) ? teleports[static_cast<std::size_t>(slot)] : 0;
}

int WorldEvents::LastJumpPadId(int slot) noexcept {
    return Valid(slot) ? last_jump_pad_id[static_cast<std::size_t>(slot)] : -1;
}

int WorldEvents::LastTeleporterId(int slot) noexcept {
    return Valid(slot)
        ? last_teleporter_id[static_cast<std::size_t>(slot)] : -1;
}

void Events::reset() noexcept {
    WorldEvents::Reset();
}

void Events::note_jump_pad(int slot, int entity_id) noexcept {
    WorldEvents::NoteJumpPad(slot, entity_id);
}

void Events::note_teleport(int slot, int entity_id) noexcept {
    WorldEvents::NoteTeleport(slot, entity_id);
}

int Events::jump_pads_for(int slot) const noexcept {
    return WorldEvents::JumpPadsFor(slot);
}

int Events::teleports_for(int slot) const noexcept {
    return WorldEvents::TeleportsFor(slot);
}

int Events::last_jump_pad_id(int slot) const noexcept {
    return WorldEvents::LastJumpPadId(slot);
}

int Events::last_teleporter_id(int slot) const noexcept {
    return WorldEvents::LastTeleporterId(slot);
}

} // namespace fruityprime::world
