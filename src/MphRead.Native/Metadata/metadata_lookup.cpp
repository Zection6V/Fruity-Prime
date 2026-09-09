#include "Metadata/metadata_lookup.hpp"

#include "Metadata/metadata.hpp"
#include "Metadata/Rooms.hpp"
#include "Metadata/metadata_extra.hpp"
#include "Assets/model_catalog.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

namespace fruityprime::metadata {
namespace {

using formats::Vector3;

struct ModeLayers {
    game::Mode mode;
    std::array<int, 3> layers;
    int count;
};

// Metadata._modeLayers
constexpr std::array<ModeLayers, 13> ModeLayerTable{{
    {game::Mode::Battle, {0, 1, 2}, 3},
    {game::Mode::BattleTeams, {3, 0, 0}, 1},
    {game::Mode::Survival, {15, 0, 0}, 1},
    {game::Mode::SurvivalTeams, {15, 0, 0}, 1},
    {game::Mode::Capture, {12, 0, 0}, 1},
    {game::Mode::Bounty, {8, 9, 10}, 3},
    {game::Mode::BountyTeams, {11, 0, 0}, 1},
    {game::Mode::Nodes, {4, 5, 6}, 3},
    {game::Mode::NodesTeams, {7, 0, 0}, 1},
    {game::Mode::Defender, {14, 0, 0}, 1},
    {game::Mode::DefenderTeams, {14, 0, 0}, 1},
    {game::Mode::PrimeHunter, {0, 1, 2}, 3},
    {game::Mode::Unknown15, {13, 0, 0}, 1}
}};

[[nodiscard]] const ModeLayers* find_mode(game::Mode mode) noexcept {
    for (const auto& row : ModeLayerTable) {
        if (row.mode == mode) {
            return &row;
        }
    }
    return nullptr;
}

[[nodiscard]] std::string_view mode_enum_name(game::Mode mode) noexcept {
    switch (mode) {
    case game::Mode::None:
    case game::Mode::Story: return {};
    case game::Mode::Battle: return "Battle";
    case game::Mode::BattleTeams: return "BattleTeams";
    case game::Mode::Survival: return "Survival";
    case game::Mode::SurvivalTeams: return "SurvivalTeams";
    case game::Mode::Capture: return "Capture";
    case game::Mode::Bounty: return "Bounty";
    case game::Mode::BountyTeams: return "BountyTeams";
    case game::Mode::Nodes: return "Nodes";
    case game::Mode::NodesTeams: return "NodesTeams";
    case game::Mode::Defender: return "Defender";
    case game::Mode::DefenderTeams: return "DefenderTeams";
    case game::Mode::PrimeHunter: return "PrimeHunter";
    case game::Mode::Unknown15: return "Unknown15";
    }
    return {};
}

[[nodiscard]] constexpr Vector3 rgb(float r, float g, float b) noexcept {
    return {r, g, b};
}

} // namespace

std::span<const int> mode_layers(game::Mode mode) noexcept {
    const ModeLayers* row = find_mode(mode);
    if (row == nullptr) {
        return {};
    }
    return std::span<const int>(row->layers.data(),
                                static_cast<std::size_t>(row->count));
}

int multiplayer_entity_layer(game::Mode mode, int player_count) noexcept {
    const ModeLayers* row = find_mode(mode);
    if (row == nullptr) {
        return 0;
    }
    if (row->count == 1) {
        return row->layers[0];
    }
    // The managed index picks the three-player entry, then the four-player
    // entry, and otherwise the two-player one.
    const int index = player_count == 3 ? 1 : (player_count == 4 ? 2 : 0);
    return row->layers[static_cast<std::size_t>(index)];
}

std::string layer_names(int layer_mask, bool multiplayer) {
    if (!multiplayer) {
        switch (layer_mask & 3) {
        case 0: return "FirstVisit";
        case 1: return "Escape";
        case 2: return "Cleared";
        case 3: return "SpLayer3";
        }
        return "UNKNOWN" + std::to_string(layer_mask);
    }
    // A mode is named when every layer it uses is present; a mode with more
    // than one layer also names the player counts it covers.
    std::string result;
    const auto has = [layer_mask](int layer) {
        return (layer_mask & (1 << layer)) != 0;
    };
    for (const auto& row : ModeLayerTable) {
        const auto used = std::span<const int>(
            row.layers.data(), static_cast<std::size_t>(row.count));
        const bool all = std::all_of(used.begin(), used.end(), has);
        std::string entry;
        if (all) {
            entry = std::string(mode_enum_name(row.mode));
        } else if (row.count > 1) {
            std::string players;
            const char* labels[] = {"2P", "3P", "4P"};
            for (int i = 0; i < row.count; ++i) {
                if (!has(row.layers[static_cast<std::size_t>(i)])) {
                    continue;
                }
                if (!players.empty()) {
                    players += "/";
                }
                players += labels[i];
            }
            if (!players.empty()) {
                entry = std::string(mode_enum_name(row.mode)) + players;
            }
        }
        if (entry.empty()) {
            continue;
        }
        if (!result.empty()) {
            result += " | ";
        }
        result += entry;
    }
    return result;
}

std::string layer_name(int layer_id, bool multiplayer) {
    if (!multiplayer) {
        switch (layer_id) {
        case 0: return "FirstVisit";
        case 1: return "Escape";
        case 2: return "Cleared";
        case 3: return "SpLayer3";
        default: return "NoLayer" + std::to_string(layer_id);
        }
    }
    return layer_names(1 << layer_id, true);
}

Vector3 event_color(formats::Message event_id) noexcept {
    switch (event_id) {
    case formats::Message::None: return rgb(0.0F, 0.0F, 0.0F);
    case formats::Message::SetActive: return rgb(0.615F, 0.0F, 0.909F);
    case formats::Message::Damage: return rgb(1.0F, 0.0F, 0.0F);
    case formats::Message::Gravity: return rgb(0.141F, 1.0F, 1.0F);
    case formats::Message::Activate: return rgb(0.0F, 1.0F, 0.0F);
    case formats::Message::Death: return rgb(0.0F, 0.0F, 0.858F);
    case formats::Message::ShipHatch: return rgb(1.0F, 1.0F, 0.6F);
    case formats::Message::Unused25: return rgb(1.0F, 0.792F, 0.6F);
    case formats::Message::PreventFormSwitch:
        return rgb(0.964F, 1.0F, 0.058F);
    case formats::Message::PlatformWakeup: return rgb(0.5F, 0.5F, 0.5F);
    case formats::Message::DripMoatPlatform:
        return rgb(0.596F, 0.658F, 0.964F);
    case formats::Message::UnlockOubliette:
        return rgb(0.964F, 0.596F, 0.596F);
    case formats::Message::Checkpoint: return rgb(0.972F, 0.086F, 0.831F);
    case formats::Message::EscapeUpdate1: return rgb(0.619F, 0.980F, 0.678F);
    case formats::Message::Trigger: return rgb(0.549F, 0.18F, 0.18F);
    case formats::Message::UpdateMusic: return rgb(0.094F, 0.506F, 0.51F);
    case formats::Message::Unlock: return rgb(0.094F, 0.094F, 0.557F);
    case formats::Message::Lock: return rgb(0.647F, 0.663F, 0.169F);
    case formats::Message::ShowPrompt: return rgb(0.118F, 0.588F, 0.118F);
    case formats::Message::ShowWarning: return rgb(0.784F, 0.325F, 1.0F);
    case formats::Message::ShowOverlay: return rgb(1.0F, 0.612F, 0.153F);
    case formats::Message::UnlockConnectors:
        return rgb(0.906F, 0.702F, 1.0F);
    case formats::Message::LockConnectors:
        return rgb(0.784F, 0.984F, 0.988F);
    case formats::Message::Gorea2Trigger: return rgb(1.0F, 0.325F, 0.294F);
    case formats::Message::SetTriggerState:
        return rgb(0.988F, 0.463F, 0.824F);
    case formats::Message::PlatformSleep: return rgb(0.165F, 0.894F, 0.678F);
    case formats::Message::SetPlatformIndex:
        return rgb(0.549F, 0.345F, 0.102F);
    case formats::Message::PlaySfxScript: return rgb(0.471F, 0.769F, 0.525F);
    case formats::Message::LoadOubliette: return rgb(1.0F, 0.765F, 0.49F);
    case formats::Message::EscapeUpdate2: return rgb(0.165F, 0.816F, 0.894F);
    default: return rgb(1.0F, 1.0F, 1.0F);
    }
}

Vector3 event_color(formats::FhMessage event_id) noexcept {
    switch (event_id) {
    case formats::FhMessage::None: return rgb(0.0F, 0.0F, 0.0F);
    case formats::FhMessage::Activate: return rgb(0.0F, 1.0F, 0.0F);
    case formats::FhMessage::Damage: return rgb(1.0F, 0.0F, 0.0F);
    case formats::FhMessage::Death: return rgb(0.0F, 0.0F, 0.858F);
    case formats::FhMessage::Trigger: return rgb(0.549F, 0.18F, 0.18F);
    case formats::FhMessage::Unlock: return rgb(0.094F, 0.094F, 0.557F);
    default: return rgb(1.0F, 1.0F, 1.0F);
    }
}

const RoomMetadata* room_by_id(int id) noexcept {
    try {
        return get_room_by_id(id, true);
    } catch (const std::out_of_range&) {
        return nullptr;
    }
}

formats::BeamType affinity_beam(formats::Hunter hunter) noexcept {
    // Weapons.AffinityWeapons, indexed by hunter.
    constexpr std::array<formats::BeamType, 8> AffinityWeapons{{
        formats::BeamType::Missile,
        formats::BeamType::VoltDriver,
        formats::BeamType::Imperialist,
        formats::BeamType::ShockCoil,
        formats::BeamType::Judicator,
        formats::BeamType::Magmaul,
        formats::BeamType::Battlehammer,
        formats::BeamType::PowerBeam
    }};
    const std::size_t index = static_cast<std::size_t>(hunter);
    return index < AffinityWeapons.size() ? AffinityWeapons[index]
                                          : formats::BeamType::PowerBeam;
}

int weapon_ammo_type(formats::BeamType beam) noexcept {
    const std::size_t index = static_cast<std::size_t>(beam);
    if (index >= weapon_table::WeaponsMP.size()) {
        return -1;
    }
    return weapon_table::WeaponsMP[index].ammo_type;
}

std::string_view enemy_model_name(formats::EnemyType type) noexcept {
    const std::size_t index = static_cast<std::size_t>(type);
    if (index >= EnemyModelNames.size()) {
        return {};
    }
    // The managed accessor returns null for an empty name; an empty view is
    // the same statement here.
    return EnemyModelNames[index];
}

int enemy_death_effect(formats::EnemyType type) noexcept {
    const std::size_t index = static_cast<std::size_t>(type);
    return index < EnemyDeathEffects.size() ? EnemyDeathEffects[index] : 0;
}

std::string_view double_damage_img_name() noexcept { return "doubleDamage_img"; }

std::string_view invisible_plat_name() noexcept { return "N/A"; }

std::string model_path_by_name(std::string_view name) {
    const auto candidates = assets::model_path_candidates(name);
    return candidates.empty() ? std::string() : candidates.front();
}

std::string first_hunt_model_path_by_name(std::string_view name) {
    // First Hunt keeps its models under the same relative layout; the store
    // selects which root that is.
    return model_path_by_name(name);
}

const ObjectInfo* object_by_id(std::uint32_t id) noexcept {
    return object_info(id);
}

std::string entity_path_for(std::string_view filename) {
    return "levels/entities/" + std::string(filename);
}

} // namespace fruityprime::metadata
