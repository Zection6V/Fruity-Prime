#pragma once

// OpenTK.Windowing.GraphicsLibraryFramework.GLFW: the members the game calls
// outside the window, which is the joystick and gamepad API. The desktop build
// links GLFW; the Android build has none, and there every call throws
// GlfwUnavailableException where the C# would throw DllNotFoundException.

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace OpenTK::Windowing::GraphicsLibraryFramework
{
    // DllNotFoundException for the GLFW native library.
    class GlfwUnavailableException final : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // GamepadState: GLFW_GAMEPAD_BUTTON_* then GLFW_GAMEPAD_AXIS_*.
    struct GamepadState final
    {
        std::array<std::uint8_t, 15> Buttons{};
        std::array<float, 6> Axes{};
    };

    namespace GLFW
    {
        [[nodiscard]] bool Init();
        void Terminate();
        void PollEvents();
        [[nodiscard]] bool JoystickPresent(std::int32_t jid);
        [[nodiscard]] bool JoystickIsGamepad(std::int32_t jid);
        [[nodiscard]] bool GetGamepadState(std::int32_t jid, GamepadState& state);
        // The strings are null in OpenTK where GLFW returns none.
        [[nodiscard]] std::optional<std::string> GetGamepadName(std::int32_t jid);
        [[nodiscard]] std::optional<std::string> GetJoystickName(std::int32_t jid);
        [[nodiscard]] std::optional<std::string> GetJoystickGUID(std::int32_t jid);
        // Empty where OpenTK returns null: the joystick is not connected.
        [[nodiscard]] std::vector<float> GetJoystickAxes(std::int32_t jid);
        [[nodiscard]] std::vector<std::uint8_t> GetJoystickButtons(std::int32_t jid);
        [[nodiscard]] std::vector<std::uint8_t> GetJoystickHats(std::int32_t jid);
        [[nodiscard]] bool UpdateGamepadMappings(const std::string& mappings);
    }
}
