#pragma once

#include "Metadata/metadata.hpp"

#include <cstdint>
#include <string>

namespace fruityprime::launcher {

// Matches the portable launch choices in the managed launcher.  The native
// front end can build this value without pulling UI types into gameplay.
enum class LaunchKind : std::uint8_t {
    None,
    Online,
    Offline,
    Host,
    Adventure,
    Demo
};

class Hunters {
public:
    explicit Hunters(std::uint32_t seed = 0x9e3779b9U) noexcept
        : seed_(seed) {}

    [[nodiscard]] metadata::Hunter resolve(metadata::Hunter hunter) noexcept;
    void reroll() noexcept { rolled_ = metadata::Hunter::Random; }

private:
    std::uint32_t seed_;
    metadata::Hunter rolled_ = metadata::Hunter::Random;
};

struct LaunchPlan {
    LaunchKind kind = LaunchKind::None;
    metadata::Hunter hunter = metadata::Hunter::Samus;
    std::string room_key;
    std::uint8_t mode = 3;
    int bots = 0;
    int bot_level = 1;
    int port = 27888;
    std::string server_address;
    std::string master_host;
    std::string player_name = "Player";
    std::uint8_t save_slot = 0;
    bool new_game = false;
    std::string demo_path;

    // Resolve Random exactly once before a plan is handed to networking and
    // gameplay.  This mirrors the managed launch boundary where the roster
    // announcement and the scene must see the same hunter.
    void resolve_hunter(Hunters& choices) noexcept {
        hunter = choices.resolve(hunter);
    }
};

} // namespace fruityprime::launcher
