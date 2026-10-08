#pragma once

#include "../Entities/Players/PlayerInput.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace MphRead::Mods
{
    using InputButtonType = std::remove_cvref_t<
        decltype(std::declval<Entities::Keybind&>().Type())>;
    using InputKey = std::remove_cvref_t<
        decltype(std::declval<Entities::Keybind&>().Key())>;
    using InputMouseButton = std::remove_cvref_t<
        decltype(std::declval<Entities::Keybind&>().MouseButton())>;

    // Native descriptor for one rebindable action. Name is the logical action
    // name used by native code and the settings UI. ConfigKey is an optional
    // compatibility boundary for controls.txt; when empty, Name is persisted.
    struct InputBindingProperty final
    {
        std::string_view Name;
        Entities::Keybind& (*GetValue)(Entities::PlayerControls&);
        std::string_view ConfigKey{};

        [[nodiscard]] constexpr std::string_view PersistedName() const noexcept
        {
            return ConfigKey.empty() ? Name : ConfigKey;
        }
    };

    class InputSettings final
    {
    public:
        InputSettings() = delete;
        ~InputSettings() = delete;
        InputSettings(const InputSettings&) = delete;
        InputSettings& operator=(const InputSettings&) = delete;
        InputSettings(InputSettings&&) = delete;
        InputSettings& operator=(InputSettings&&) = delete;

        [[nodiscard]] static float MouseSensitivity() noexcept;
        static void MouseSensitivity(float value) noexcept;

        [[nodiscard]] static bool InvertMouseY() noexcept;
        static void InvertMouseY(bool value) noexcept;

        [[nodiscard]] static bool InvertMouseX() noexcept;
        static void InvertMouseX(bool value) noexcept;

        // Classic = the DS's own aim (touch producer and Dual keys on the
        // DS's 30 Hz gameplay tick). Modern = every source each 60 Hz step.
        [[nodiscard]] static bool ClassicAim();
        static void ClassicAim(bool value);

        [[nodiscard]] static bool ScrollAllWeapons() noexcept;
        static void ScrollAllWeapons(bool value) noexcept;

        [[nodiscard]] static InputKey ChatKey() noexcept;
        static void ChatKey(InputKey value) noexcept;

        [[nodiscard]] static InputKey ClipKey() noexcept;
        static void ClipKey(InputKey value);

        [[nodiscard]] static float GamepadDeadZone();
        static void GamepadDeadZone(float value);

        [[nodiscard]] static float GamepadLookSensitivity();
        static void GamepadLookSensitivity(float value);

        [[nodiscard]] static bool GamepadInvertY();
        static void GamepadInvertY(bool value);

        [[nodiscard]] static Entities::PlayerControls& Current();
        [[nodiscard]] static const std::array<InputBindingProperty, 35>& Bindings();

        [[nodiscard]] static Entities::Keybind& Bind(
            const InputBindingProperty& property);
        [[nodiscard]] static std::string Describe(const Entities::Keybind& bind);
        [[nodiscard]] static std::string KeyName(InputKey key);
        [[nodiscard]] static std::string ActionName(
            const InputBindingProperty& property);

        static void Rebind(const InputBindingProperty& property,
            InputButtonType type, InputKey key, InputMouseButton button);
        static void Apply(Entities::PlayerControls& controls);
        static void ApplyToPlayers();

        static void Load();
        static void Save();
        static void Reset();

    private:
        [[nodiscard]] static std::filesystem::path Path();
        [[nodiscard]] static std::array<InputBindingProperty, 35> FindBindings();
        static void ParseBind(const InputBindingProperty& property,
            std::string_view value);

        static float _mouseSensitivity;
        static bool _invertMouseY;
        static bool _invertMouseX;
        static bool _scrollAllWeapons;
        static InputKey _chatKey;
        static InputKey _clipKey;
        static bool _creating;
        static std::unique_ptr<Entities::PlayerControls> _current;
        static std::optional<std::array<InputBindingProperty, 35>> _bindings;
        static const std::array<std::string_view, 16> _order;
    };
}
