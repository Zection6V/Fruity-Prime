#include "Mods/settings.hpp"
#include "Mods/InputSettings.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Entities/Players/player_controls.hpp"
#include "Settings/settings_common.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace fruityprime::settings {

InputConfig load_input(const std::filesystem::path& directory) {
    InputConfig config;
    std::ifstream input(directory / "controls.txt");
    if (!input) {
        return config;
    }
    std::string line;
    try {
        while (std::getline(input, line)) {
            detail::trim(line);
            if (line.empty() || line.front() == '#') {
                continue;
            }
            const auto split = line.find('=');
            if (split == std::string::npos || split == 0) {
                continue;
            }
            std::string key = line.substr(0, split);
            std::string value = line.substr(split + 1);
            detail::trim(key);
            detail::trim(value);
            float parsed = 0.0F;
            if (key == "sensitivity" && detail::parse_float(value, parsed)) {
                config.mouse_sensitivity = parsed;
            } else if (key == "invert_y") {
                config.invert_mouse_y = detail::parse_bool(value,
                                                   config.invert_mouse_y);
            } else if (key == "invert_x") {
                config.invert_mouse_x = detail::parse_bool(value,
                                                   config.invert_mouse_x);
            } else if (key == "scroll_all_weapons") {
                config.scroll_all_weapons = detail::parse_bool(
                    value, config.scroll_all_weapons);
            } else if (key == "gamepad_deadzone"
                       && detail::parse_float(value, parsed)) {
                config.gamepad_dead_zone = parsed;
            } else if (key == "gamepad_look"
                       && detail::parse_float(value, parsed)) {
                config.gamepad_look_sensitivity = parsed;
            } else if (key == "gamepad_invert_y") {
                config.gamepad_invert_y = detail::parse_bool(
                    value, config.gamepad_invert_y);
            } else {
                for (const auto& entry : detail::binding_entries()) {
                    if (key != entry.key) {
                        continue;
                    }
                    input::GamepadButtons binding;
                    if (detail::parse_buttons(value, binding)) {
                        config.pad_bindings.*(entry.member) = binding;
                    }
                    break;
                }
            }
        }
    } catch (...) {
        // A malformed or unreadable controls file must not block startup.
    }
    detail::clamp(config);
    return config;
}

bool save_input(const std::filesystem::path& directory,
                const InputConfig& source) {
    InputConfig config = source;
    detail::clamp(config);
    std::ofstream output(directory / "controls.txt", std::ios::trunc);
    if (!output) {
        return false;
    }
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<float>::max_digits10);
    output << "# Fruity Prime controls. Delete a line to use the default.\n"
           << "sensitivity=" << config.mouse_sensitivity << '\n'
           << "invert_y=" << (config.invert_mouse_y ? "true" : "false")
           << '\n'
           << "invert_x=" << (config.invert_mouse_x ? "true" : "false")
           << '\n'
           << "scroll_all_weapons="
           << (config.scroll_all_weapons ? "true" : "false") << '\n'
           << "gamepad_deadzone=" << config.gamepad_dead_zone << '\n'
           << "gamepad_look=" << config.gamepad_look_sensitivity << '\n'
           << "gamepad_invert_y="
           << (config.gamepad_invert_y ? "true" : "false") << '\n';
    for (const auto& entry : detail::binding_entries()) {
        output << entry.key << '='
               << detail::format_buttons(config.pad_bindings.*(entry.member)) << '\n';
    }
    return static_cast<bool>(output);
}

} // namespace fruityprime::settings

namespace fruityprime::mods {

namespace {

settings::InputConfig default_config;
settings::InputConfig* bound_config = &default_config;
std::unique_ptr<players::PlayerControls> current;
bool creating = false;
int chat_key = players::key_code::T;

constexpr std::array<std::string_view, 35> binding_names = {
    "MoveUp", "MoveDown", "MoveLeft", "MoveRight", "Jump", "Boost",
    "Shoot", "Zoom", "Morph", "AltAttack", "NextWeapon", "PrevWeapon",
    "WeaponMenu", "ScanVisor", "Pause", "HudOverlay", "RolltLeft",
    "RollRight", "RollUp", "RollDown", "AimLeft", "AimRight", "AimUp",
    "AimDown", "Scan", "PowerBeam", "Missile", "VoltDriver",
    "Battlehammer", "Imperialist", "Judicator", "Magmaul", "ShockCoil",
    "OmegaCannon", "AffinitySlot"
};

struct KeyNameEntry {
    std::string_view name;
    int code;
};

constexpr std::array<KeyNameEntry, 45> named_keys{{
    {"Unknown", 0}, {"Space", 32}, {"Apostrophe", 39}, {"Comma", 44},
    {"Minus", 45}, {"Period", 46}, {"Slash", 47}, {"Semicolon", 59},
    {"Equal", 61}, {"LeftBracket", 91}, {"Backslash", 92},
    {"RightBracket", 93}, {"GraveAccent", 96}, {"Escape", 256},
    {"Enter", 257}, {"Tab", 258}, {"Backspace", 259}, {"Insert", 260},
    {"Delete", 261}, {"Right", 262}, {"Left", 263}, {"Down", 264},
    {"Up", 265}, {"PageUp", 266}, {"PageDown", 267}, {"Home", 268},
    {"End", 269}, {"CapsLock", 280}, {"NumLock", 282},
    {"PrintScreen", 283}, {"Pause", 284}, {"KeyPadDecimal", 330},
    {"KeyPadDivide", 331}, {"KeyPadMultiply", 332},
    {"KeyPadSubtract", 333}, {"KeyPadAdd", 334},
    {"KeyPadEnter", 335}, {"KeyPadEqual", 336}, {"LeftShift", 340},
    {"LeftControl", 341}, {"LeftAlt", 342}, {"RightShift", 344},
    {"RightControl", 345}, {"RightAlt", 346}
}};

bool equal_ignore_case(std::string_view left,
                       std::string_view right) noexcept {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(left[index]))
            != std::tolower(static_cast<unsigned char>(right[index]))) {
            return false;
        }
    }
    return true;
}

std::optional<int> parse_key_name(std::string_view name) noexcept {
    if (name.size() == 1) {
        const unsigned char value = static_cast<unsigned char>(name.front());
        if (std::isalnum(value)) return std::toupper(value);
    }
    if (name.size() == 2 && (name[0] == 'D' || name[0] == 'd')
        && std::isdigit(static_cast<unsigned char>(name[1]))) {
        return static_cast<int>(name[1]);
    }
    if (name.size() >= 2 && (name[0] == 'F' || name[0] == 'f')) {
        int number = 0;
        const auto result = std::from_chars(
            name.data() + 1, name.data() + name.size(), number);
        if (result.ec == std::errc{} && result.ptr == name.data() + name.size()
            && number >= 1 && number <= 25) {
            return 289 + number;
        }
    }
    constexpr std::string_view KeyPad = "KeyPad";
    if (name.size() == KeyPad.size() + 1
        && equal_ignore_case(name.substr(0, KeyPad.size()), KeyPad)
        && std::isdigit(static_cast<unsigned char>(name.back()))) {
        return 320 + name.back() - '0';
    }
    for (const auto& entry : named_keys) {
        if (equal_ignore_case(name, entry.name)) return entry.code;
    }
    return std::nullopt;
}

std::string raw_key_name(int key) {
    if (key >= 'A' && key <= 'Z') {
        return std::string(1, static_cast<char>(key));
    }
    if (key >= '0' && key <= '9') {
        return "D" + std::string(1, static_cast<char>(key));
    }
    if (key >= 290 && key <= 314) {
        return "F" + std::to_string(key - 289);
    }
    if (key >= 320 && key <= 329) {
        return "KeyPad" + std::to_string(key - 320);
    }
    for (const auto& entry : named_keys) {
        if (entry.code == key) return std::string(entry.name);
    }
    return "Unknown";
}

std::string humanize(std::string name) {
    if (name.size() == 2 && name[0] == 'D'
        && std::isdigit(static_cast<unsigned char>(name[1]))) {
        return std::string(1, name[1]);
    }
    std::string result;
    result.reserve(name.size() + 4);
    for (std::size_t index = 0; index < name.size(); ++index) {
        const unsigned char value = static_cast<unsigned char>(name[index]);
        if (index > 0 && std::isupper(value)
            && !std::isupper(static_cast<unsigned char>(name[index - 1]))) {
            result.push_back(' ');
            result.push_back(static_cast<char>(std::tolower(value)));
        } else {
            result.push_back(static_cast<char>(value));
        }
    }
    return result;
}

std::optional<int> parse_mouse_name(std::string_view name) noexcept {
    if (equal_ignore_case(name, "Left")
        || equal_ignore_case(name, "Button1")) return 0;
    if (equal_ignore_case(name, "Right")
        || equal_ignore_case(name, "Button2")) return 1;
    if (equal_ignore_case(name, "Middle")
        || equal_ignore_case(name, "Button3")) return 2;
    constexpr std::string_view Button = "Button";
    if (name.size() == Button.size() + 1
        && equal_ignore_case(name.substr(0, Button.size()), Button)
        && name.back() >= '4' && name.back() <= '8') {
        return name.back() - '1';
    }
    return std::nullopt;
}

std::string raw_mouse_name(int button) {
    if (button == 0) return "Left";
    if (button == 1) return "Right";
    if (button == 2) return "Middle";
    return "Button" + std::to_string(button + 1);
}

std::optional<bool> parse_managed_bool(std::string_view value) noexcept {
    if (equal_ignore_case(value, "true")) return true;
    if (equal_ignore_case(value, "false")) return false;
    return std::nullopt;
}

std::string format_three_decimals(float value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(3) << value;
    std::string result = output.str();
    while (!result.empty() && result.back() == '0') result.pop_back();
    if (!result.empty() && result.back() == '.') result.pop_back();
    return result.empty() ? "0" : result;
}

players::Keybind* named_binding(players::PlayerControls& controls,
                                std::string_view name) noexcept {
#define FRUITY_BINDING(member) if (name == #member) return &controls.member
    FRUITY_BINDING(MoveLeft); FRUITY_BINDING(MoveRight);
    FRUITY_BINDING(MoveUp); FRUITY_BINDING(MoveDown);
    FRUITY_BINDING(RolltLeft); FRUITY_BINDING(RollRight);
    FRUITY_BINDING(RollUp); FRUITY_BINDING(RollDown);
    FRUITY_BINDING(AimLeft); FRUITY_BINDING(AimRight);
    FRUITY_BINDING(AimUp); FRUITY_BINDING(AimDown);
    FRUITY_BINDING(Shoot); FRUITY_BINDING(Zoom); FRUITY_BINDING(Jump);
    FRUITY_BINDING(Morph); FRUITY_BINDING(Boost); FRUITY_BINDING(AltAttack);
    FRUITY_BINDING(ScanVisor); FRUITY_BINDING(Scan);
    FRUITY_BINDING(NextWeapon); FRUITY_BINDING(PrevWeapon);
    FRUITY_BINDING(WeaponMenu); FRUITY_BINDING(PowerBeam);
    FRUITY_BINDING(Missile); FRUITY_BINDING(VoltDriver);
    FRUITY_BINDING(Battlehammer); FRUITY_BINDING(Imperialist);
    FRUITY_BINDING(Judicator); FRUITY_BINDING(Magmaul);
    FRUITY_BINDING(ShockCoil); FRUITY_BINDING(OmegaCannon);
    FRUITY_BINDING(AffinitySlot); FRUITY_BINDING(Pause);
    FRUITY_BINDING(HudOverlay);
#undef FRUITY_BINDING
    return nullptr;
}

} // namespace

void InputSettings::BindRuntime(settings::InputConfig* config) noexcept {
    bound_config = config == nullptr ? &default_config : config;
    if (current != nullptr) {
        current->ScrollAllWeapons = bound_config->scroll_all_weapons;
    }
}

float InputSettings::MouseSensitivity() noexcept { return bound_config->mouse_sensitivity; }
void InputSettings::MouseSensitivity(float value) noexcept {
    bound_config->mouse_sensitivity = std::clamp(value, 0.05F, 10.0F);
}
bool InputSettings::InvertMouseY() noexcept { return bound_config->invert_mouse_y; }
void InputSettings::InvertMouseY(bool value) noexcept { bound_config->invert_mouse_y = value; }
bool InputSettings::InvertMouseX() noexcept { return bound_config->invert_mouse_x; }
void InputSettings::InvertMouseX(bool value) noexcept { bound_config->invert_mouse_x = value; }
bool InputSettings::ScrollAllWeapons() noexcept { return bound_config->scroll_all_weapons; }
void InputSettings::ScrollAllWeapons(bool value) noexcept {
    bound_config->scroll_all_weapons = value;
    if (current != nullptr) current->ScrollAllWeapons = value;
}
int InputSettings::ChatKey() noexcept { return chat_key; }
void InputSettings::ChatKey(int value) noexcept { chat_key = value; }
float InputSettings::GamepadDeadZone() noexcept { return bound_config->gamepad_dead_zone; }
void InputSettings::GamepadDeadZone(float value) noexcept {
    bound_config->gamepad_dead_zone = std::clamp(value, 0.0F, 0.9F);
}
float InputSettings::GamepadLookSensitivity() noexcept {
    return bound_config->gamepad_look_sensitivity;
}
void InputSettings::GamepadLookSensitivity(float value) noexcept {
    bound_config->gamepad_look_sensitivity = std::clamp(value, 0.1F, 5.0F);
}
bool InputSettings::GamepadInvertY() noexcept { return bound_config->gamepad_invert_y; }
void InputSettings::GamepadInvertY(bool value) noexcept { bound_config->gamepad_invert_y = value; }

players::PlayerControls& InputSettings::Current() {
    if (current == nullptr) {
        creating = true;
        current = std::make_unique<players::PlayerControls>(
            players::PlayerControls::GetDefault());
        creating = false;
        current->ScrollAllWeapons = bound_config->scroll_all_weapons;
    }
    return *current;
}

std::span<const std::string_view> InputSettings::Bindings() noexcept {
    return binding_names;
}

players::Keybind& InputSettings::Bind(std::size_t index) {
    if (index >= binding_names.size()) throw std::out_of_range("input binding index");
    auto* result = named_binding(Current(), binding_names[index]);
    if (result == nullptr) throw std::logic_error("input binding table is inconsistent");
    return *result;
}

std::string InputSettings::Describe(const players::Keybind& bind) {
    switch (bind.Type) {
    case players::ButtonType::Mouse:
        if (bind.MouseButton == 0) return "Mouse left";
        if (bind.MouseButton == 1) return "Mouse right";
        if (bind.MouseButton == 2) return "Mouse middle";
        return "Mouse " + std::to_string(bind.MouseButton + 1);
    case players::ButtonType::ScrollUp: return "Scroll up";
    case players::ButtonType::ScrollDown: return "Scroll down";
    case players::ButtonType::Key: return KeyName(bind.Key);
    }
    return "unbound";
}

std::string InputSettings::KeyName(int key) {
    return key == players::key_code::Unknown
        ? "unbound" : humanize(raw_key_name(key));
}

std::string InputSettings::ActionName(std::size_t index) {
    if (index >= binding_names.size()) {
        throw std::out_of_range("input binding index");
    }
    std::string name(binding_names[index]);
    if (name == "Pause") name = "Scoreboard";
    else if (name == "RolltLeft") name = "Roll left";
    name = humanize(std::move(name));
    if (!name.empty()) {
        name[0] = static_cast<char>(std::toupper(
            static_cast<unsigned char>(name[0])));
    }
    return name;
}

void InputSettings::Rebind(std::size_t index, players::ButtonType type,
                           int key, int mouse_button) {
    auto& bind = Bind(index);
    bind.Type = type;
    bind.Key = type == players::ButtonType::Key ? key : players::key_code::Unknown;
    bind.MouseButton = mouse_button;
}

void InputSettings::Apply(players::PlayerControls& controls) {
    if (creating || current == nullptr) return;
    for (const auto name : binding_names) {
        const auto* source = named_binding(*current, name);
        auto* target = named_binding(controls, name);
        if (source != nullptr && target != nullptr) {
            target->Type = source->Type;
            target->Key = source->Key;
            target->MouseButton = source->MouseButton;
        }
    }
    controls.ScrollAllWeapons = bound_config->scroll_all_weapons;
}

void InputSettings::ApplyToPlayers() noexcept {
    for (auto* player : players::PlayerEntity::Players()) {
        if (player != nullptr) Apply(player->Controls());
    }
}

void InputSettings::Load(const std::filesystem::path& directory) {
    std::ifstream input(directory / "controls.txt");
    if (!input) return;
    try {
        std::string line;
        while (std::getline(input, line)) {
            settings::detail::trim(line);
            if (line.empty() || line.front() == '#') continue;
            const std::size_t split = line.find('=');
            if (split == std::string::npos || split == 0) continue;
            std::string key = line.substr(0, split);
            std::string value = line.substr(split + 1);
            settings::detail::trim(key);
            settings::detail::trim(value);
            float parsed_float = 0.0F;
            if (key == "sensitivity") {
                if (settings::detail::parse_float(value, parsed_float)) {
                    MouseSensitivity(parsed_float);
                }
                continue;
            }
            if (key == "invert_y") {
                if (const auto parsed = parse_managed_bool(value)) {
                    InvertMouseY(*parsed);
                }
                continue;
            }
            if (key == "invert_x") {
                if (const auto parsed = parse_managed_bool(value)) {
                    InvertMouseX(*parsed);
                }
                continue;
            }
            if (key == "scroll_all_weapons") {
                if (const auto parsed = parse_managed_bool(value)) {
                    ScrollAllWeapons(*parsed);
                }
                continue;
            }
            if (key == "gamepad") continue;
            bool pad_binding = false;
            for (const auto& entry : settings::detail::binding_entries()) {
                if (key != entry.key) continue;
                fruityprime::input::GamepadButtons buttons;
                if (settings::detail::parse_buttons(value, buttons)) {
                    bound_config->pad_bindings.*(entry.member) = buttons;
                }
                pad_binding = true;
                break;
            }
            if (pad_binding) continue;
            if (key == "gamepad_deadzone") {
                if (settings::detail::parse_float(value, parsed_float)) {
                    GamepadDeadZone(parsed_float);
                }
                continue;
            }
            if (key == "gamepad_look") {
                if (settings::detail::parse_float(value, parsed_float)) {
                    GamepadLookSensitivity(parsed_float);
                }
                continue;
            }
            if (key == "gamepad_invert_y") {
                if (const auto parsed = parse_managed_bool(value)) {
                    GamepadInvertY(*parsed);
                }
                continue;
            }
            if (key == "chat_key") {
                if (equal_ignore_case(value, "none")) {
                    ChatKey(players::key_code::Unknown);
                } else if (const auto parsed = parse_key_name(value)) {
                    ChatKey(*parsed);
                }
                continue;
            }
            const auto binding = std::find(binding_names.begin(),
                                           binding_names.end(), key);
            if (binding == binding_names.end()) continue;
            const std::size_t index = static_cast<std::size_t>(
                binding - binding_names.begin());
            const std::size_t colon = value.find(':');
            const std::string_view type = colon == std::string::npos
                ? std::string_view(value)
                : std::string_view(value).substr(0, colon);
            const std::string_view name = colon == std::string::npos
                ? std::string_view{}
                : std::string_view(value).substr(colon + 1);
            if (type == "ScrollUp") {
                Rebind(index, players::ButtonType::ScrollUp,
                       players::key_code::Unknown, players::mouse_button::Left);
            } else if (type == "ScrollDown") {
                Rebind(index, players::ButtonType::ScrollDown,
                       players::key_code::Unknown, players::mouse_button::Left);
            } else if (type == "Mouse") {
                if (const auto mouse = parse_mouse_name(name)) {
                    Rebind(index, players::ButtonType::Mouse,
                           players::key_code::Unknown, *mouse);
                }
            } else if (type == "Key") {
                if (const auto parsed = parse_key_name(name)) {
                    Rebind(index, players::ButtonType::Key, *parsed,
                           players::mouse_button::Left);
                }
            }
        }
    } catch (...) {
        // An unreadable or malformed convenience file never blocks startup.
    }
}

void InputSettings::Save(const std::filesystem::path& directory) noexcept {
    try {
        settings::InputConfig config = *bound_config;
        settings::detail::clamp(config);
        std::ofstream output(directory / "controls.txt", std::ios::trunc);
        if (!output) return;
        output.imbue(std::locale::classic());
        output << "# Fruity Prime controls. Delete a line to go back to the default.\n"
               << "sensitivity="
               << format_three_decimals(config.mouse_sensitivity) << '\n'
               << "invert_y=" << (config.invert_mouse_y ? "true" : "false") << '\n'
               << "invert_x=" << (config.invert_mouse_x ? "true" : "false") << '\n'
               << "scroll_all_weapons="
               << (config.scroll_all_weapons ? "true" : "false") << '\n'
               << "chat_key="
               << (chat_key == players::key_code::Unknown
                       ? "none" : raw_key_name(chat_key)) << '\n'
               << std::setprecision(std::numeric_limits<float>::max_digits10)
               << "gamepad_deadzone=" << config.gamepad_dead_zone << '\n'
               << "gamepad_look=" << config.gamepad_look_sensitivity << '\n'
               << "gamepad_invert_y="
               << (config.gamepad_invert_y ? "true" : "false") << '\n';
        for (const auto& entry : settings::detail::binding_entries()) {
            output << entry.key << '=' << settings::detail::format_buttons(
                config.pad_bindings.*(entry.member)) << '\n';
        }
        for (std::size_t index = 0; index < binding_names.size(); ++index) {
            const auto& bind = Bind(index);
            output << binding_names[index] << '=';
            if (bind.Type == players::ButtonType::Mouse) {
                output << "Mouse:" << raw_mouse_name(bind.MouseButton);
            } else if (bind.Type == players::ButtonType::ScrollUp) {
                output << "ScrollUp";
            } else if (bind.Type == players::ButtonType::ScrollDown) {
                output << "ScrollDown";
            } else {
                output << "Key:" << raw_key_name(bind.Key);
            }
            output << '\n';
        }
    } catch (...) {
        // Match the managed best-effort persistence contract.
    }
}

void InputSettings::Reset() {
    creating = true;
    current = std::make_unique<players::PlayerControls>(
        players::PlayerControls::GetDefault());
    creating = false;
    *bound_config = settings::InputConfig{};
    current->ScrollAllWeapons = true;
    chat_key = players::key_code::T;
}

} // namespace fruityprime::mods
