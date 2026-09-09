#include "Mods/Network/map_rotation.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace fruityprime {
namespace {

[[nodiscard]] std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }).base();
    if (first >= last) {
        return {};
    }
    return {first, last};
}

[[nodiscard]] std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

} // namespace

MapRotation::MapRotation() = default;

RotationEntry MapRotation::fallback() {
    return RotationEntry{};
}

const RotationEntry& MapRotation::current() const noexcept {
    static const RotationEntry default_entry{};
    return entries_.empty() ? default_entry : entries_[index_];
}

const RotationEntry& MapRotation::next() const noexcept {
    static const RotationEntry default_entry{};
    if (entries_.empty()) {
        return default_entry;
    }
    return entries_[(index_ + 1) % entries_.size()];
}

MapRotation MapRotation::single_match(std::string room_key, GameMode mode,
                                      float time_limit, int point_goal) {
    MapRotation rotation;
    rotation.entries_.push_back(RotationEntry{
        std::move(room_key),
        mode == GameMode::None ? GameMode::Battle : mode,
        time_limit,
        point_goal
    });
    return rotation;
}

MapRotation MapRotation::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("could not open map rotation: " + path.string());
    }

    MapRotation rotation;
    std::string raw;
    while (std::getline(input, raw)) {
        if (const std::size_t comment = raw.find('#'); comment != std::string::npos) {
            raw.resize(comment);
        }
        raw = trim(std::move(raw));
        if (raw.empty()) {
            continue;
        }

        std::vector<std::string> parts;
        std::size_t start = 0;
        while (start <= raw.size()) {
            const std::size_t separator = raw.find('|', start);
            parts.push_back(trim(raw.substr(start,
                                            separator == std::string::npos
                                                ? std::string::npos
                                                : separator - start)));
            if (separator == std::string::npos) {
                break;
            }
            start = separator + 1;
        }
        if (parts.empty() || parts[0].empty()) {
            continue;
        }

        RotationEntry entry;
        entry.room_key = std::move(parts[0]);
        entry.mode = GameMode::Battle;
        entry.time_limit = 7.0f * 60.0f;
        entry.point_goal = 7;
        if (parts.size() > 1 && !parts[1].empty()) {
            entry.mode = parse_game_mode(parts[1]);
        }
        if (parts.size() > 2 && !parts[2].empty()) {
            try {
                entry.time_limit = std::stof(parts[2]) * 60.0f;
            } catch (const std::exception&) {
                // The C# loader keeps the default when a numeric field is
                // malformed; preserve that forgiving server configuration.
            }
        }
        if (parts.size() > 3 && !parts[3].empty()) {
            try {
                entry.point_goal = std::stoi(parts[3]);
            } catch (const std::exception&) {
                // Keep the default point goal.
            }
        }
        rotation.entries_.push_back(std::move(entry));
    }
    return rotation;
}

MapRotation MapRotation::load_or_create(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        write_default(path);
        std::cout << "[server] wrote a starter rotation to " << path.string() << '\n';
    }
    MapRotation rotation = load(path);
    if (rotation.entries_.empty()) {
        std::cout << "[server] " << path.string()
                  << " has no usable entries; using a single default map\n";
        rotation.entries_.push_back(fallback());
    }
    return rotation;
}

void MapRotation::write_default(const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create map rotation: " + path.string());
    }
    output << "# Fruity Prime dedicated server map rotation.\n"
           << "# One match per line:  ROOM KEY | mode | minutes | points\n"
           << "# Mode and the numbers are optional; '#' starts a comment.\n"
           << "# Room keys are the names MphRead uses internally.\n\n"
           << "MP1 SANCTORUS      | Battle | 7 | 7\n"
           << "MP3 PROVING GROUND | Battle | 7 | 7\n"
           << "MP4 HIGHGROUND     | Battle | 7 | 7\n"
           << "MP2 HARVESTER      | Battle | 7 | 7\n"
           << "MP6 HEADSHOT       | Battle | 7 | 7\n";
}

const RotationEntry& MapRotation::advance() noexcept {
    if (!entries_.empty()) {
        index_ = (index_ + 1) % entries_.size();
    }
    return current();
}

void MapRotation::add(RotationEntry entry) {
    entries_.push_back(std::move(entry));
}

const char* game_mode_name(GameMode mode) noexcept {
    switch (mode) {
    case GameMode::None: return "None";
    case GameMode::SinglePlayer: return "SinglePlayer";
    case GameMode::Battle: return "Battle";
    case GameMode::BattleTeams: return "BattleTeams";
    case GameMode::Survival: return "Survival";
    case GameMode::SurvivalTeams: return "SurvivalTeams";
    case GameMode::Capture: return "Capture";
    case GameMode::Bounty: return "Bounty";
    case GameMode::BountyTeams: return "BountyTeams";
    case GameMode::Nodes: return "Nodes";
    case GameMode::NodesTeams: return "NodesTeams";
    case GameMode::Defender: return "Defender";
    case GameMode::DefenderTeams: return "DefenderTeams";
    case GameMode::PrimeHunter: return "PrimeHunter";
    case GameMode::Unknown15: return "Unknown15";
    }
    return "Battle";
}

GameMode parse_game_mode(std::string value) noexcept {
    value = lower(trim(std::move(value)));
    if (value == "none") return GameMode::None;
    if (value == "singleplayer" || value == "single player") return GameMode::SinglePlayer;
    if (value == "battle") return GameMode::Battle;
    if (value == "battleteams" || value == "battle teams") return GameMode::BattleTeams;
    if (value == "survival") return GameMode::Survival;
    if (value == "survivalteams" || value == "survival teams") return GameMode::SurvivalTeams;
    if (value == "capture") return GameMode::Capture;
    if (value == "bounty") return GameMode::Bounty;
    if (value == "bountyteams" || value == "bounty teams") return GameMode::BountyTeams;
    if (value == "nodes") return GameMode::Nodes;
    if (value == "nodesteams" || value == "nodes teams") return GameMode::NodesTeams;
    if (value == "defender") return GameMode::Defender;
    if (value == "defenderteams" || value == "defender teams") return GameMode::DefenderTeams;
    if (value == "primehunter" || value == "prime hunter") return GameMode::PrimeHunter;
    if (value == "unknown15") return GameMode::Unknown15;
    return GameMode::Battle;
}

} // namespace fruityprime
