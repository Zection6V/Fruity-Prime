#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

namespace fruityprime::scene {
class Room;
}

namespace fruityprime::net {

// The native counterpart of Mods/Network/WeaponDps.cs.  The managed probe is
// a GameWindow because it drives the real scene loop; the native probe drives
// the same engine-neutral Session directly so it can be run against an NDS
// without creating a graphics context.
struct WeaponDpsOptions {
    std::uint8_t hunter = 0;
    // Persisted Formats.BeamType ordinal (PowerBeam = 0, VoltDriver = 1,
    // Missile = 2, ...), not the native weapon-table slot.
    std::int32_t beam_type = 0;
    double seconds = 10.0;
    float distance = 4.0F;
};

struct WeaponDpsResult {
    std::string room;
    std::string hunter;
    std::string beam;
    std::string error;
    std::int32_t beam_type = -1;
    std::uint8_t native_weapon = 0xff;
    float distance = 0.0F;
    double requested_seconds = 0.0;
    bool ok = false;
    bool placed = false;
    bool killed = false;
    int start_health = 0;
    int firing_frames = 0;
    int damage = 0;
    int hits = 0;
    int kill_frames = -1;
    int beam_frames = 0;
};

class WeaponDps final {
public:
    WeaponDps() = delete;

    [[nodiscard]] static WeaponDpsResult run(
        const scene::Room& room, WeaponDpsOptions options);

    static void print(std::ostream& output, const WeaponDpsResult& result);
};

} // namespace fruityprime::net
