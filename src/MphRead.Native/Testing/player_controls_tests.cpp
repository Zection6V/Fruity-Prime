// PlayerInput's keybind edges and the adventure-mode bot weapon reset.
//
// A binding carries three edge states, not one held flag; collapsing them is
// what makes a menu advance twice on a single press.  The weapon reset parks
// every damage term at its maximum rather than zero, so a story bot's shots
// do not register at all until the encounter sets its real numbers.
#include "Entities/Players/player_controls.hpp"
#include "Entities/Players/player_profile.hpp"
#include "Mods/InputSettings.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
int main() {
    using namespace fruityprime;
    players::Keybind jump, shoot;
    players::PlayerControls c;
    c.All = {&jump, &shoot};
    jump.IsPressed = true; jump.IsDown = true;
    shoot.IsPressed = true; shoot.IsReleased = true;
    c.ClearPressed();
    // Only the pressed edge clears; held and released survive the frame.
    const bool ok = !jump.IsPressed && jump.IsDown
                 && !shoot.IsPressed && shoot.IsReleased;
    players::PlayerControls defaults = players::PlayerControls::GetDefault();
    const bool controls = defaults.All.size() == 35
        && defaults.MouseAim && defaults.KeyboardAim
        && defaults.ScrollAllWeapons
        && defaults.MoveLeft.Key == players::key_code::A
        && defaults.Shoot.Type == players::ButtonType::Mouse
        && defaults.Shoot.MouseButton == players::mouse_button::Left
        && defaults.NextWeapon.Type == players::ButtonType::ScrollDown
        && defaults.PrevWeapon.Type == players::ButtonType::ScrollUp
        && defaults.AffinitySlot.Key == players::key_code::Unknown;
    settings::InputConfig input_config;
    mods::InputSettings::BindRuntime(&input_config);
    auto& canonical = mods::InputSettings::Current();
    canonical.Jump.Type = players::ButtonType::Key;
    canonical.Jump.Key = players::key_code::Q;
    mods::InputSettings::ScrollAllWeapons(false);
    players::PlayerControls rebound = players::PlayerControls::GetDefault();
    const bool settings_apply = rebound.Jump.Key == players::key_code::Q
        && !rebound.ScrollAllWeapons;
    players::PlayerControls copied = rebound;
    copied.All[14]->IsDown = true;
    const bool copy_aliases_self = copied.All[14] == &copied.Jump
        && copied.Jump.IsDown && !rebound.Jump.IsDown;
    mods::InputSettings::GamepadDeadZone(2.0F);
    const bool settings_clamp = input_config.gamepad_dead_zone == 0.9F;
    const auto controls_directory = std::filesystem::temp_directory_path()
        / "fruityprime-native-player-controls";
    std::filesystem::create_directories(controls_directory);
    {
        std::ofstream file(controls_directory / "controls.txt");
        file << "sensitivity=1.375\ninvert_y=true\n"
             << "gamepad_look=2.25\nchat_key=none\n"
             << "Jump=Key:E\nShoot=Mouse:Right\n"
             << "NextWeapon=ScrollUp\n";
    }
    mods::InputSettings::Load(controls_directory);
    const bool loaded_bindings =
        mods::InputSettings::ChatKey() == players::key_code::Unknown
        && mods::InputSettings::Bind(4).Key == players::key_code::E
        && mods::InputSettings::Bind(6).Type == players::ButtonType::Mouse
        && mods::InputSettings::Bind(6).MouseButton
            == players::mouse_button::Right
        && mods::InputSettings::Bind(10).Type
            == players::ButtonType::ScrollUp
        && input_config.mouse_sensitivity == 1.375F
        && input_config.invert_mouse_y
        && input_config.gamepad_look_sensitivity == 2.25F;
    const bool labels = mods::InputSettings::Describe(
            mods::InputSettings::Bind(6)) == "Mouse right"
        && mods::InputSettings::KeyName(players::key_code::LeftShift)
            == "Left shift"
        && mods::InputSettings::ActionName(14) == "Scoreboard";
    mods::InputSettings::Save(controls_directory);
    const auto round_trip = settings::load_input(controls_directory);
    const bool saved_settings = round_trip.gamepad_dead_zone == 0.9F;
    mods::InputSettings::Reset();
    metadata::EquipInfo equip;
    equip.InfiniteAmmo = true;
    std::array<std::uint8_t, 2> draw{}, dirs{};
    std::array<std::uint16_t, 9> dmg{};
    std::int32_t homing = 0;
    players::reset_adventure_mode_bot_weapon(equip, draw, dirs, dmg, homing);
    const bool reset = draw[0] == 255 && dirs[1] == 255 && dmg[0] == 65535
        && dmg[8] == 65535 && homing == 2147483647 && !equip.InfiniteAmmo;

    metadata::EquipInfo exact;
    exact.Weapon = &metadata::weapon_table::Weapons1P[0];
    const bool weapon_defaults = exact.UnchargedDamage() == 6
        && exact.ChargedDamage() == 36
        && exact.HomingTolerance() == 3896;
    exact.UnchargedDamage(42);
    exact.HomingTolerance(123);
    const bool weapon_overrides = exact.UnchargedDamage() == 42
        && exact.HomingTolerance() == 123;

    std::int32_t ammo = 7;
    metadata::EquipInfo ammo_info;
    ammo_info.AmmoPool = &ammo;
    const bool pool_ammo = ammo_info.ammo() == 7;
    ammo_info.set_ammo(9);
    const bool pool_set = ammo == 9;
    ammo_info.GetAmmo = [] { return 11; };
    ammo_info.SetAmmo = [&ammo](std::int32_t value) { ammo = value; };
    const bool delegates = ammo_info.Ammo() == 11;
    ammo_info.Ammo(13);
    const bool delegate_set = ammo == 13;
    ammo_info.InfiniteAmmo = true;
    const bool infinite = ammo_info.ammo() == 2147483647;
    std::printf("native player controls: edges=%d defaults=%d reset=%d\n",
                ok, controls, reset);
    std::array<bool, 9> live_weapons{};
    players::AvailableArray available;
    available.bind(live_weapons);
    available[3] = true;
    const bool live_available = live_weapons[3];

    return (ok && controls && settings_apply && copy_aliases_self
            && settings_clamp && loaded_bindings && labels && saved_settings
            && live_available && reset
            && weapon_defaults && weapon_overrides && pool_ammo
            && pool_set && delegates && delegate_set && infinite) ? 0 : 1;
}
