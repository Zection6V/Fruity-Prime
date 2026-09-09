#pragma once

#include <array>
#include <cstddef>

namespace fruityprime::world {

class WorldEvents final {
public:
    static constexpr std::size_t SlotCapacity = 8;
    inline static bool Watching = false;

    static void Reset() noexcept;
    static void NoteJumpPad(int slot, int entity_id) noexcept;
    static void NoteTeleport(int slot, int entity_id) noexcept;

    [[nodiscard]] static int JumpPadsFor(int slot) noexcept;
    [[nodiscard]] static int TeleportsFor(int slot) noexcept;
    [[nodiscard]] static int LastJumpPadId(int slot) noexcept;
    [[nodiscard]] static int LastTeleporterId(int slot) noexcept;

private:
    [[nodiscard]] static bool Valid(int slot) noexcept {
        return slot >= 0 && slot < static_cast<int>(SlotCapacity);
    }
};

// Compatibility adapter for older native call sites. All instances address
// the one process-wide managed state; creating an Events object no longer
// creates an independent set of counters.
class Events {
public:
    static constexpr std::size_t SlotCapacity = WorldEvents::SlotCapacity;

    bool& watching = WorldEvents::Watching;

    void reset() noexcept;
    void note_jump_pad(int slot, int entity_id) noexcept;
    void note_teleport(int slot, int entity_id) noexcept;

    [[nodiscard]] int jump_pads_for(int slot) const noexcept;
    [[nodiscard]] int teleports_for(int slot) const noexcept;
    [[nodiscard]] int last_jump_pad_id(int slot) const noexcept;
    [[nodiscard]] int last_teleporter_id(int slot) const noexcept;

};

} // namespace fruityprime::world

namespace MphReadNative::Mods {
using WorldEvents = ::fruityprime::world::WorldEvents;
}
