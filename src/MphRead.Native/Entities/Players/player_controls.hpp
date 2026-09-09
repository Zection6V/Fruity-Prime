#pragma once

// Native counterpart of PlayerInput's Keybind, PlayerControls and the
// adventure-mode bot weapon reset.
//
// A Keybind carries three edge states rather than one held flag: pressed is
// the frame the button went down, released the frame it came up, and
// NeedsRepress makes a binding refuse to retrigger until it has been let go.
// Collapsing those into one boolean is what makes a menu advance twice on a
// single press.

#include "Metadata/equip_info.hpp"
#include "Formats/enum_tables.hpp"
#include "Mods/InputSettings.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace fruityprime::players {

enum class ButtonType : std::uint8_t {
    Key = 0,
    Mouse = 1,
    ScrollUp = 2,
    ScrollDown = 3
};

// OpenTK's Keys/MouseButton values are GLFW-compatible in the managed build.
// Keep the portable native controls in the same numeric space so settings and
// a future window frontend can exchange bindings without a translation table.
namespace key_code {
inline constexpr int Unknown = 0;
inline constexpr int Space = 32;
inline constexpr int A = 65;
inline constexpr int C = 67;
inline constexpr int D = 68;
inline constexpr int E = 69;
inline constexpr int Q = 81;
inline constexpr int S = 83;
inline constexpr int T = 84;
inline constexpr int W = 87;
inline constexpr int D1 = 49;
inline constexpr int D2 = 50;
inline constexpr int D3 = 51;
inline constexpr int D4 = 52;
inline constexpr int D5 = 53;
inline constexpr int D6 = 54;
inline constexpr int D7 = 55;
inline constexpr int D8 = 56;
inline constexpr int D9 = 57;
inline constexpr int Left = 263;
inline constexpr int Right = 262;
inline constexpr int Up = 265;
inline constexpr int Down = 264;
inline constexpr int Tab = 258;
inline constexpr int LeftShift = 340;
} // namespace key_code

namespace mouse_button {
inline constexpr int Left = 0;
inline constexpr int Right = 1;
inline constexpr int Middle = 2;
} // namespace mouse_button

// PlayerInput.Keybind
struct Keybind {
    ButtonType Type = ButtonType::Key;
    // The platform key code, or the mouse button ordinal.
    int Key = 0;
    int MouseButton = 0;

    bool IsPressed = false;
    bool IsDown = false;
    bool IsReleased = false;
    bool NeedsRepress = false;

    constexpr Keybind() noexcept = default;
    constexpr explicit Keybind(int key) noexcept
        : Type(ButtonType::Key), Key(key) {}
    Keybind(ButtonType scroll_type) {
        if (scroll_type != ButtonType::ScrollUp
            && scroll_type != ButtonType::ScrollDown) {
            throw std::invalid_argument("Unexpected control type.");
        }
        Type = scroll_type;
    }

    [[nodiscard]] constexpr bool operator==(const Keybind& other) const noexcept {
        return Type == other.Type && Key == other.Key
            && MouseButton == other.MouseButton;
    }
    [[nodiscard]] constexpr bool operator!=(const Keybind& other) const noexcept {
        return !(*this == other);
    }
};

// PlayerInput.PlayerControls: every binding, plus the flat view the input
// pass walks.
struct PlayerControls {
    bool MouseAim = true;
    bool KeyboardAim = true;

    Keybind MoveLeft;
    Keybind MoveRight;
    Keybind MoveUp;
    Keybind MoveDown;
    Keybind RolltLeft;
    Keybind RollRight;
    Keybind RollUp;
    Keybind RollDown;
    Keybind AimLeft;
    Keybind AimRight;
    Keybind AimUp;
    Keybind AimDown;
    Keybind Shoot;
    Keybind Zoom;
    Keybind Jump;
    Keybind Morph;
    Keybind Boost;
    Keybind AltAttack;
    Keybind ScanVisor;
    Keybind Scan;
    Keybind NextWeapon;
    Keybind PrevWeapon;
    Keybind WeaponMenu;
    Keybind PowerBeam;
    Keybind Missile;
    Keybind VoltDriver;
    Keybind Battlehammer;
    Keybind Imperialist;
    Keybind Judicator;
    Keybind Magmaul;
    Keybind ShockCoil;
    Keybind OmegaCannon;
    Keybind AffinitySlot;
    Keybind Pause;
    Keybind HudOverlay;

    bool InvertAimY = false;
    bool InvertAimX = false;
    bool ScrollAllWeapons = true;

    // PlayerControls.All.  The managed array owns references to the fields
    // above; the native view uses pointers for the same aliasing behavior.
    std::vector<Keybind*> All;

    PlayerControls() { rebuild_all(); }

    PlayerControls(const PlayerControls& other) {
        rebuild_all();
        copy_values(other);
    }
    PlayerControls(PlayerControls&& other) noexcept {
        rebuild_all();
        copy_values(other);
    }
    PlayerControls& operator=(const PlayerControls& other) {
        if (this != &other) copy_values(other);
        return *this;
    }
    PlayerControls& operator=(PlayerControls&& other) noexcept {
        if (this != &other) copy_values(other);
        return *this;
    }

    PlayerControls(
        Keybind move_left, Keybind move_right, Keybind move_up,
        Keybind move_down, Keybind roll_left, Keybind roll_right,
        Keybind roll_up, Keybind roll_down, Keybind aim_left,
        Keybind aim_right, Keybind aim_up, Keybind aim_down, Keybind shoot,
        Keybind zoom, Keybind jump, Keybind morph, Keybind boost,
        Keybind alt_attack, Keybind scan_visor, Keybind scan,
        Keybind next_weapon, Keybind prev_weapon, Keybind weapon_menu,
        Keybind power_beam, Keybind missile, Keybind volt_driver,
        Keybind battlehammer, Keybind imperialist, Keybind judicator,
        Keybind magmaul, Keybind shock_coil, Keybind omega_cannon,
        Keybind affinity_slot, Keybind pause, Keybind hud_overlay)
        : MoveLeft(std::move(move_left)), MoveRight(std::move(move_right)),
          MoveUp(std::move(move_up)), MoveDown(std::move(move_down)),
          RolltLeft(std::move(roll_left)), RollRight(std::move(roll_right)),
          RollUp(std::move(roll_up)), RollDown(std::move(roll_down)),
          AimLeft(std::move(aim_left)), AimRight(std::move(aim_right)),
          AimUp(std::move(aim_up)), AimDown(std::move(aim_down)),
          Shoot(std::move(shoot)), Zoom(std::move(zoom)), Jump(std::move(jump)),
          Morph(std::move(morph)), Boost(std::move(boost)),
          AltAttack(std::move(alt_attack)), ScanVisor(std::move(scan_visor)),
          Scan(std::move(scan)), NextWeapon(std::move(next_weapon)),
          PrevWeapon(std::move(prev_weapon)), WeaponMenu(std::move(weapon_menu)),
          PowerBeam(std::move(power_beam)), Missile(std::move(missile)),
          VoltDriver(std::move(volt_driver)),
          Battlehammer(std::move(battlehammer)),
          Imperialist(std::move(imperialist)), Judicator(std::move(judicator)),
          Magmaul(std::move(magmaul)), ShockCoil(std::move(shock_coil)),
          OmegaCannon(std::move(omega_cannon)),
          AffinitySlot(std::move(affinity_slot)), Pause(std::move(pause)),
          HudOverlay(std::move(hud_overlay)) {
        rebuild_all();
    }

    [[nodiscard]] static PlayerControls GetDefault() {
        PlayerControls controls(
            Keybind(key_code::A), Keybind(key_code::D), Keybind(key_code::W),
            Keybind(key_code::S), Keybind(key_code::A), Keybind(key_code::D),
            Keybind(key_code::W), Keybind(key_code::S), Keybind(key_code::Left),
            Keybind(key_code::Right), Keybind(key_code::Up),
            Keybind(key_code::Down),
            mouse_key(mouse_button::Left), mouse_key(mouse_button::Right),
            Keybind(key_code::Space), Keybind(key_code::C),
            Keybind(key_code::Space), mouse_key(mouse_button::Left),
            Keybind(key_code::E), Keybind(key_code::Q),
            Keybind(ButtonType::ScrollDown), Keybind(ButtonType::ScrollUp),
            mouse_key(mouse_button::Middle), Keybind(key_code::D1),
            Keybind(key_code::D2), Keybind(key_code::D3), Keybind(key_code::D4),
            Keybind(key_code::D5), Keybind(key_code::D6), Keybind(key_code::D7),
            Keybind(key_code::D8), Keybind(key_code::D9),
            Keybind(key_code::Unknown), Keybind(key_code::Tab),
            Keybind(key_code::LeftShift));
        mods::InputSettings::Apply(controls);
        return controls;
    }

    void ClearAll() noexcept {
        for (Keybind* bind : All) {
            if (bind != nullptr) {
                bind->IsDown = false;
                bind->IsPressed = false;
                bind->IsReleased = false;
            }
        }
    }

    // PlayerControls.ClearPressed: the pressed edge lasts one frame only.
    void ClearPressed() noexcept {
        for (Keybind* bind : All) {
            if (bind != nullptr) {
                bind->IsPressed = false;
            }
        }
    }

    void rebuild_all() {
        All = {&MoveLeft, &MoveRight, &MoveUp, &MoveDown,
               &RolltLeft, &RollRight, &RollUp, &RollDown,
               &AimLeft, &AimRight, &AimUp, &AimDown,
               &Shoot, &Zoom, &Jump, &Morph, &Boost, &AltAttack,
               &ScanVisor, &Scan, &NextWeapon, &PrevWeapon, &WeaponMenu,
               &PowerBeam, &Missile, &VoltDriver, &Battlehammer,
               &Imperialist, &Judicator, &Magmaul, &ShockCoil, &OmegaCannon,
               &AffinitySlot, &Pause, &HudOverlay};
    }

private:
    void copy_values(const PlayerControls& other) {
        MouseAim = other.MouseAim;
        KeyboardAim = other.KeyboardAim;
        InvertAimY = other.InvertAimY;
        InvertAimX = other.InvertAimX;
        ScrollAllWeapons = other.ScrollAllWeapons;
        // All always aliases the fields of its own object. Copying the vector
        // itself would leave every pointer aimed into `other`.
        for (std::size_t index = 0; index < All.size(); ++index) {
            *All[index] = *other.All[index];
        }
    }

    static constexpr Keybind mouse_key(int button) noexcept {
        Keybind result;
        result.Type = ButtonType::Mouse;
        result.MouseButton = button;
        return result;
    }
};

// PlayerInput.ResetAdventureModeBotWeapon: a story bot's weapon is made
// harmless until the encounter sets its real numbers, by parking every damage
// term at its maximum and the draw/direction ids at 255.  Zeroing them
// instead would make the bot's shots land for no damage rather than not
// register at all.
inline void reset_adventure_mode_bot_weapon(
    metadata::EquipInfo& equip,
    std::array<std::uint8_t, 2>& draw_func_ids,
    std::array<std::uint8_t, 2>& damage_direction_types,
    std::array<std::uint16_t, 9>& damage_terms,
    std::int32_t& homing_tolerance) noexcept {
    draw_func_ids.fill(255);
    damage_direction_types.fill(255);
    // Uncharged, headshot, min-charge, charged, min-charge headshot, charged
    // headshot, splash, min-charge splash and charged splash.
    damage_terms.fill((std::numeric_limits<std::uint16_t>::max)());
    homing_tolerance = (std::numeric_limits<std::int32_t>::max)();
    equip.InfiniteAmmo = false;
}

} // namespace fruityprime::players
