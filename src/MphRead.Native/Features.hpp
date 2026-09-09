#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>

namespace fruityprime::features {

// Native equivalent of the three static settings groups in Features.cs.
// They are value types here so the launcher, tests, and a future frontend do
// not share mutable process globals accidentally.
struct BugfixSettings {
    bool smooth_cam_seq_handoff = false;
    bool better_cam_seq_node_ref = true;
    bool no_stray_respawn_text = false;
    bool correct_bounty_sfx = true;
    bool no_double_enemy_death = true;
    bool no_slench_roll_timer_underflow = true;

    void load(const std::map<std::string, std::string>& values);
    [[nodiscard]] std::map<std::string, std::string> commit() const;
};

struct FeatureSettings {
    // These values mirror the current code defaults. They remain part of the
    // runtime contract even though the managed launcher no longer exposes
    // rows for them.
    bool no_repeat_encounters = false;
    bool allow_invalid_teams = true;
    bool top_screen_target_info = true;
    bool hud_sway = true;
    bool target_info_sway = false;
    bool delayed_idle_sway = true;
    bool no_idle_sway = false;
    bool no_map_centering = false;
    bool max_room_detail = false;
    bool max_player_detail = true;
    bool log_spatial_audio = false;
    bool half_second_alarm = false;
    bool full_boost_charge = false;
    bool boost_opens_doors = false;
    bool alternate_hunters_1p = true;

    bool pro_hud = false;
    float helmet_opacity = 1.0F;
    float visor_opacity = 0.5F;
    float hud_opacity = 1.0F;
    float reticle_opacity = 1.0F;
    bool fixed_crosshair = false;
    bool custom_crosshair = false;
    bool modern_hud = false;
    float weapon_list_scale = 1.0F;
    bool fixed_weapon = false;

    static constexpr float ProHudWeaponListScale = 1.7F;

    [[nodiscard]] float effective_helmet_opacity() const noexcept;
    [[nodiscard]] float effective_visor_opacity() const noexcept;
    [[nodiscard]] bool effective_fixed_crosshair() const noexcept;
    [[nodiscard]] bool effective_custom_crosshair() const noexcept;
    [[nodiscard]] bool effective_modern_hud() const noexcept;
    [[nodiscard]] float effective_weapon_list_scale() const noexcept;
    [[nodiscard]] bool effective_fixed_weapon() const noexcept;

    void load(const std::map<std::string, std::string>& values);
    [[nodiscard]] std::map<std::string, std::string> commit() const;
};

struct CheatSettings {
    bool free_weapon_select = false;
    bool unlimited_jumps = false;
    bool no_random_encounters = false;
    bool unlock_all_doors = false;
    bool continue_from_current_room = false;
    bool skip_planet_intros = false;
    bool start_with_all_upgrades = false;
    bool start_with_all_octoliths = false;
    bool walk_through_walls = false;
    bool always_fight_gorea2 = false;
    bool quadruple_damage = false;

    void load(const std::map<std::string, std::string>& values);
    [[nodiscard]] std::map<std::string, std::string> commit() const;
};

struct Registry {
    BugfixSettings bugfixes;
    FeatureSettings features;
    CheatSettings cheats;

    void load(const std::map<std::string, std::string>& values);
    [[nodiscard]] std::map<std::string, std::string> commit() const;
};

// The managed API is three process-wide static classes. Keep the value
// records above as compatibility adapters for existing native call sites,
// while these classes expose one shared state exactly like Features.cs.
class Bugfixes final {
private:
    [[nodiscard]] static BugfixSettings& state() noexcept;
public:
    inline static bool& SmoothCamSeqHandoff = state().smooth_cam_seq_handoff;
    inline static bool& BetterCamSeqNodeRef = state().better_cam_seq_node_ref;
    inline static bool& NoStrayRespawnText = state().no_stray_respawn_text;
    inline static bool& CorrectBountySfx = state().correct_bounty_sfx;
    inline static bool& NoDoubleEnemyDeath = state().no_double_enemy_death;
    inline static bool& NoSlenchRollTimerUnderflow =
        state().no_slench_roll_timer_underflow;
    static void Load(const std::map<std::string, std::string>& values);
    [[nodiscard]] static std::map<std::string, std::string> Commit();
};

class Features final {
private:
    [[nodiscard]] static FeatureSettings& state() noexcept;
public:
    inline static bool& NoRepeatEncounters = state().no_repeat_encounters;
    inline static bool& AllowInvalidTeams = state().allow_invalid_teams;
    inline static bool& TopScreenTargetInfo = state().top_screen_target_info;
    inline static bool& HudSway = state().hud_sway;
    inline static bool& TargetInfoSway = state().target_info_sway;
    inline static bool& DelayedIdleSway = state().delayed_idle_sway;
    inline static bool& NoIdleSway = state().no_idle_sway;
    inline static bool& NoMapCentering = state().no_map_centering;
    inline static bool& MaxRoomDetail = state().max_room_detail;
    inline static bool& MaxPlayerDetail = state().max_player_detail;
    inline static bool& LogSpatialAudio = state().log_spatial_audio;
    inline static bool& HalfSecondAlarm = state().half_second_alarm;
    inline static bool& FullBoostCharge = state().full_boost_charge;
    inline static bool& BoostOpensDoors = state().boost_opens_doors;
    inline static bool& AlternateHunters1P = state().alternate_hunters_1p;
    inline static bool& ProHud = state().pro_hud;
    inline static float& HudOpacity = state().hud_opacity;
    inline static float& ReticleOpacity = state().reticle_opacity;
    static constexpr float ProHudWeaponListScale = 1.7F;

    [[nodiscard]] static float HelmetOpacity() noexcept;
    static void SetHelmetOpacity(float value) noexcept;
    [[nodiscard]] static float VisorOpacity() noexcept;
    static void SetVisorOpacity(float value) noexcept;
    [[nodiscard]] static bool FixedCrosshair() noexcept;
    static void SetFixedCrosshair(bool value) noexcept;
    [[nodiscard]] static bool CustomCrosshair() noexcept;
    static void SetCustomCrosshair(bool value) noexcept;
    [[nodiscard]] static bool ModernHud() noexcept;
    static void SetModernHud(bool value) noexcept;
    [[nodiscard]] static float WeaponListScale() noexcept;
    static void SetWeaponListScale(float value) noexcept;
    [[nodiscard]] static bool FixedWeapon() noexcept;
    static void SetFixedWeapon(bool value) noexcept;
    static void Load(const std::map<std::string, std::string>& values);
    [[nodiscard]] static std::map<std::string, std::string> Commit();
};

class Cheats final {
private:
    [[nodiscard]] static CheatSettings& state() noexcept;
public:
    inline static bool& FreeWeaponSelect = state().free_weapon_select;
    inline static bool& UnlimitedJumps = state().unlimited_jumps;
    inline static bool& NoRandomEncounters = state().no_random_encounters;
    inline static bool& UnlockAllDoors = state().unlock_all_doors;
    inline static bool& ContinueFromCurrentRoom =
        state().continue_from_current_room;
    inline static bool& SkipPlanetIntros = state().skip_planet_intros;
    inline static bool& StartWithAllUpgrades = state().start_with_all_upgrades;
    inline static bool& StartWithAllOctoliths =
        state().start_with_all_octoliths;
    inline static bool& WalkThroughWalls = state().walk_through_walls;
    inline static bool& AlwaysFightGorea2 = state().always_fight_gorea2;
    inline static bool& QuadrupleDamage = state().quadruple_damage;
    static void Load(const std::map<std::string, std::string>& values);
    [[nodiscard]] static std::map<std::string, std::string> Commit();
};

[[nodiscard]] bool parse_bool(std::string_view value, bool fallback) noexcept;
[[nodiscard]] float parse_float(std::string_view value, float fallback) noexcept;

} // namespace fruityprime::features

namespace MphReadNative {
namespace Features = ::fruityprime::features;
namespace Bugfixes = ::fruityprime::features;
namespace Cheats = ::fruityprime::features;
}
