#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fruityprime {

enum class GameMode : std::uint8_t {
    None = 0,
    SinglePlayer = 2,
    Battle = 3,
    BattleTeams = 4,
    Survival = 5,
    SurvivalTeams = 6,
    Capture = 7,
    Bounty = 8,
    BountyTeams = 9,
    Nodes = 10,
    NodesTeams = 11,
    Defender = 12,
    DefenderTeams = 13,
    PrimeHunter = 14,
    Unknown15 = 15
};

struct RotationEntry {
    std::string room_key = "MP3 PROVING GROUND";
    GameMode mode = GameMode::Battle;
    float time_limit = 7.0f * 60.0f;
    int point_goal = 7;
};

class MapRotation {
public:
    MapRotation();

    [[nodiscard]] const std::vector<RotationEntry>& entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] const RotationEntry& current() const noexcept;
    [[nodiscard]] const RotationEntry& next() const noexcept;
    [[nodiscard]] std::size_t index() const noexcept { return index_; }

    static MapRotation single_match(std::string room_key, GameMode mode,
                                    float time_limit, int point_goal);
    static MapRotation load(const std::filesystem::path& path);
    static MapRotation load_or_create(const std::filesystem::path& path);
    static void write_default(const std::filesystem::path& path);

    const RotationEntry& advance() noexcept;
    void add(RotationEntry entry);

private:
    static RotationEntry fallback();

    std::vector<RotationEntry> entries_;
    std::size_t index_ = 0;
};

[[nodiscard]] const char* game_mode_name(GameMode mode) noexcept;
[[nodiscard]] GameMode parse_game_mode(std::string value) noexcept;

} // namespace fruityprime
