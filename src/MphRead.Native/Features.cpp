#include "Features.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

namespace fruityprime::features {
namespace {

template <typename Function>
void load_bool(const std::map<std::string, std::string>& values,
               std::string_view key, bool& destination, Function&& fallback) {
    const auto found = values.find(std::string(key));
    if (found != values.end()) {
        destination = parse_bool(found->second, fallback());
    }
}

template <typename Function>
void load_float(const std::map<std::string, std::string>& values,
                std::string_view key, float& destination, Function&& fallback) {
    const auto found = values.find(std::string(key));
    if (found != values.end()) {
        destination = parse_float(found->second, fallback());
    }
}

void put_bool(std::map<std::string, std::string>& values,
              std::string_view key, bool value) {
    values.emplace(key, value ? "true" : "false");
}

void put_float(std::map<std::string, std::string>& values,
               std::string_view key, float value) {
    if (std::isnan(value)) {
        values.emplace(std::string(key), "NaN");
        return;
    }
    if (std::isinf(value)) {
        values.emplace(std::string(key), value < 0 ? "-Infinity" : "Infinity");
        return;
    }
    std::array<char, 32> buffer{};
    const auto converted = std::to_chars(buffer.data(),
                                         buffer.data() + buffer.size(), value);
    if (converted.ec == std::errc{}) {
        values.emplace(std::string(key),
                       std::string(buffer.data(), converted.ptr));
    }
}

} // namespace

bool parse_bool(std::string_view value, bool fallback) noexcept {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return fallback;
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    std::string normalized(value.substr(first, last - first + 1));
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    // Boolean.TryParse in Features.cs accepts only the two Boolean tokens,
    // case-insensitively (and with surrounding whitespace).  Do not widen
    // this to the command-line settings' on/off/1/0 grammar.
    if (normalized == "true") {
        return true;
    }
    if (normalized == "false") {
        return false;
    }
    return fallback;
}

float parse_float(std::string_view value, float fallback) noexcept {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return fallback;
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    const std::string trimmed(value.substr(first, last - first + 1));
    const std::string lower = [&trimmed] {
        std::string result = trimmed;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char character) {
                           return static_cast<char>(std::tolower(character));
                       });
        return result;
    }();
    if (lower == "inf" || lower == "+inf" || lower == "-inf") {
        // Single.TryParse accepts Infinity but not the C spelling "inf".
        return fallback;
    }
    if (lower.find("0x") != std::string::npos
        || lower.find('p') != std::string::npos) {
        return fallback;
    }
    std::string copy;
    copy.reserve(trimmed.size());
    for (const char character : trimmed) {
        if (character != ',') {
            copy.push_back(character);
        }
    }
    char* end = nullptr;
    const float parsed = std::strtof(copy.c_str(), &end);
    if (end == copy.c_str() || end == nullptr || *end != '\0') {
        return fallback;
    }
    return parsed;
}

void BugfixSettings::load(const std::map<std::string, std::string>& values) {
    load_bool(values, "SmoothCamSeqHandoff", smooth_cam_seq_handoff,
              [this] { return smooth_cam_seq_handoff; });
    load_bool(values, "BetterCamSeqNodeRef", better_cam_seq_node_ref,
              [this] { return better_cam_seq_node_ref; });
    load_bool(values, "NoStrayRespawnText", no_stray_respawn_text,
              [this] { return no_stray_respawn_text; });
    load_bool(values, "CorrectBountySfx", correct_bounty_sfx,
              [this] { return correct_bounty_sfx; });
    load_bool(values, "NoDoubleEnemyDeath", no_double_enemy_death,
              [this] { return no_double_enemy_death; });
    load_bool(values, "NoSlenchRollTimerUnderflow",
              no_slench_roll_timer_underflow,
              [this] { return no_slench_roll_timer_underflow; });
}

std::map<std::string, std::string> BugfixSettings::commit() const {
    std::map<std::string, std::string> result;
    put_bool(result, "SmoothCamSeqHandoff", smooth_cam_seq_handoff);
    put_bool(result, "BetterCamSeqNodeRef", better_cam_seq_node_ref);
    put_bool(result, "NoStrayRespawnText", no_stray_respawn_text);
    put_bool(result, "CorrectBountySfx", correct_bounty_sfx);
    put_bool(result, "NoDoubleEnemyDeath", no_double_enemy_death);
    put_bool(result, "NoSlenchRollTimerUnderflow",
             no_slench_roll_timer_underflow);
    return result;
}

float FeatureSettings::effective_helmet_opacity() const noexcept {
    return pro_hud ? 0.0F : helmet_opacity;
}

float FeatureSettings::effective_visor_opacity() const noexcept {
    return pro_hud ? 0.0F : visor_opacity;
}

bool FeatureSettings::effective_fixed_crosshair() const noexcept {
    return pro_hud || fixed_crosshair;
}

bool FeatureSettings::effective_custom_crosshair() const noexcept {
    return pro_hud || custom_crosshair;
}

bool FeatureSettings::effective_modern_hud() const noexcept {
    return pro_hud || modern_hud;
}

float FeatureSettings::effective_weapon_list_scale() const noexcept {
    return pro_hud ? ProHudWeaponListScale : weapon_list_scale;
}

bool FeatureSettings::effective_fixed_weapon() const noexcept {
    return pro_hud || fixed_weapon;
}

void FeatureSettings::load(const std::map<std::string, std::string>& values) {
    // Match Features.cs: only the two still-persisted values are loaded.
    load_float(values, "ReticleOpacity", reticle_opacity,
               [this] { return reticle_opacity; });
    load_bool(values, "ProHud", pro_hud, [this] { return pro_hud; });
}

std::map<std::string, std::string> FeatureSettings::commit() const {
    std::map<std::string, std::string> result;
    put_float(result, "ReticleOpacity", reticle_opacity);
    put_bool(result, "ProHud", pro_hud);
    return result;
}

void CheatSettings::load(const std::map<std::string, std::string>& values) {
    load_bool(values, "FreeWeaponSelect", free_weapon_select,
              [this] { return free_weapon_select; });
    load_bool(values, "UnlimitedJumps", unlimited_jumps,
              [this] { return unlimited_jumps; });
    load_bool(values, "NoRandomEncounters", no_random_encounters,
              [this] { return no_random_encounters; });
    load_bool(values, "UnlockAllDoors", unlock_all_doors,
              [this] { return unlock_all_doors; });
    load_bool(values, "ContinueFromCurrentRoom", continue_from_current_room,
              [this] { return continue_from_current_room; });
    load_bool(values, "SkipPlanetIntros", skip_planet_intros,
              [this] { return skip_planet_intros; });
    load_bool(values, "StartWithAllUpgrades", start_with_all_upgrades,
              [this] { return start_with_all_upgrades; });
    load_bool(values, "StartWithAllOctoliths", start_with_all_octoliths,
              [this] { return start_with_all_octoliths; });
    load_bool(values, "WalkThroughWalls", walk_through_walls,
              [this] { return walk_through_walls; });
    load_bool(values, "AlwaysFightGorea2", always_fight_gorea2,
              [this] { return always_fight_gorea2; });
    load_bool(values, "QuadrupleDamage", quadruple_damage,
              [this] { return quadruple_damage; });
}

std::map<std::string, std::string> CheatSettings::commit() const {
    std::map<std::string, std::string> result;
    put_bool(result, "FreeWeaponSelect", free_weapon_select);
    put_bool(result, "UnlimitedJumps", unlimited_jumps);
    // Preserve the spelling in Cheats.Commit(): these six managed entries
    // intentionally use Boolean.ToString() without ToLower().
    result.emplace("NoRandomEncounters",
                   no_random_encounters ? "True" : "False");
    result.emplace("UnlockAllDoors", unlock_all_doors ? "True" : "False");
    result.emplace("ContinueFromCurrentRoom",
                   continue_from_current_room ? "True" : "False");
    result.emplace("SkipPlanetIntros",
                   skip_planet_intros ? "True" : "False");
    result.emplace("StartWithAllUpgrades",
                   start_with_all_upgrades ? "True" : "False");
    put_bool(result, "StartWithAllOctoliths", start_with_all_octoliths);
    put_bool(result, "WalkThroughWalls", walk_through_walls);
    put_bool(result, "AlwaysFightGorea2", always_fight_gorea2);
    put_bool(result, "QuadrupleDamage", quadruple_damage);
    return result;
}

void Registry::load(const std::map<std::string, std::string>& values) {
    bugfixes.load(values);
    features.load(values);
    cheats.load(values);
}

std::map<std::string, std::string> Registry::commit() const {
    std::map<std::string, std::string> result = bugfixes.commit();
    for (const auto& [key, value] : features.commit()) {
        result[key] = value;
    }
    for (const auto& [key, value] : cheats.commit()) {
        result[key] = value;
    }
    return result;
}

BugfixSettings& Bugfixes::state() noexcept {
    static BugfixSettings value;
    return value;
}

void Bugfixes::Load(const std::map<std::string, std::string>& values) {
    state().load(values);
}

std::map<std::string, std::string> Bugfixes::Commit() {
    return state().commit();
}

FeatureSettings& Features::state() noexcept {
    static FeatureSettings value;
    return value;
}

float Features::HelmetOpacity() noexcept {
    return state().effective_helmet_opacity();
}

void Features::SetHelmetOpacity(float value) noexcept {
    state().helmet_opacity = value;
}

float Features::VisorOpacity() noexcept {
    return state().effective_visor_opacity();
}

void Features::SetVisorOpacity(float value) noexcept {
    state().visor_opacity = value;
}

bool Features::FixedCrosshair() noexcept {
    return state().effective_fixed_crosshair();
}

void Features::SetFixedCrosshair(bool value) noexcept {
    state().fixed_crosshair = value;
}

bool Features::CustomCrosshair() noexcept {
    return state().effective_custom_crosshair();
}

void Features::SetCustomCrosshair(bool value) noexcept {
    state().custom_crosshair = value;
}

bool Features::ModernHud() noexcept {
    return state().effective_modern_hud();
}

void Features::SetModernHud(bool value) noexcept {
    state().modern_hud = value;
}

float Features::WeaponListScale() noexcept {
    return state().effective_weapon_list_scale();
}

void Features::SetWeaponListScale(float value) noexcept {
    state().weapon_list_scale = value;
}

bool Features::FixedWeapon() noexcept {
    return state().effective_fixed_weapon();
}

void Features::SetFixedWeapon(bool value) noexcept {
    state().fixed_weapon = value;
}

void Features::Load(const std::map<std::string, std::string>& values) {
    state().load(values);
}

std::map<std::string, std::string> Features::Commit() {
    return state().commit();
}

CheatSettings& Cheats::state() noexcept {
    static CheatSettings value;
    return value;
}

void Cheats::Load(const std::map<std::string, std::string>& values) {
    state().load(values);
}

std::map<std::string, std::string> Cheats::Commit() {
    return state().commit();
}

} // namespace fruityprime::features
