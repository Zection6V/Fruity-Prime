#pragma once

#include "Mods/settings.hpp"

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace fruityprime::players {
enum class ButtonType : std::uint8_t;
struct Keybind;
struct PlayerControls;
}

namespace fruityprime::mods {

// Process-wide native counterpart of Mods.InputSettings. InputConfig remains
// the portable file/frontend value; this class owns the canonical keyboard
// controls copied into every PlayerEntity, just as the managed static class
// does through PlayerControls.GetDefault().
class InputSettings final {
public:
    static void BindRuntime(settings::InputConfig* config) noexcept;
    [[nodiscard]] static float MouseSensitivity() noexcept;
    static void MouseSensitivity(float value) noexcept;
    [[nodiscard]] static bool InvertMouseY() noexcept;
    static void InvertMouseY(bool value) noexcept;
    [[nodiscard]] static bool InvertMouseX() noexcept;
    static void InvertMouseX(bool value) noexcept;
    [[nodiscard]] static bool ScrollAllWeapons() noexcept;
    static void ScrollAllWeapons(bool value) noexcept;
    [[nodiscard]] static int ChatKey() noexcept;
    static void ChatKey(int value) noexcept;
    [[nodiscard]] static float GamepadDeadZone() noexcept;
    static void GamepadDeadZone(float value) noexcept;
    [[nodiscard]] static float GamepadLookSensitivity() noexcept;
    static void GamepadLookSensitivity(float value) noexcept;
    [[nodiscard]] static bool GamepadInvertY() noexcept;
    static void GamepadInvertY(bool value) noexcept;

    [[nodiscard]] static players::PlayerControls& Current();
    [[nodiscard]] static std::span<const std::string_view> Bindings() noexcept;
    [[nodiscard]] static players::Keybind& Bind(std::size_t index);
    [[nodiscard]] static std::string Describe(
        const players::Keybind& bind);
    [[nodiscard]] static std::string KeyName(int key);
    [[nodiscard]] static std::string ActionName(std::size_t index);
    static void Rebind(std::size_t index, players::ButtonType type, int key,
                       int mouse_button);
    static void Apply(players::PlayerControls& controls);
    static void ApplyToPlayers() noexcept;
    static void Load(const std::filesystem::path& directory);
    static void Save(const std::filesystem::path& directory) noexcept;
    static void Reset();
};

} // namespace fruityprime::mods

namespace MphReadNative::Mods {
using InputSettings = ::fruityprime::mods::InputSettings;
}
