#include "GLFW.hpp"

#include "../System/Encoding.hpp"

#include <string_view>

#if !defined(__ANDROID__)
#include <GLFW/glfw3.h>
#endif

namespace OpenTK::Windowing::GraphicsLibraryFramework::GLFW
{
#if defined(__ANDROID__)
    namespace
    {
        [[noreturn]] void Unavailable(const char* procedure)
        {
            throw GlfwUnavailableException(std::string("Unable to load GLFW: ") + procedure);
        }
    }

    bool Init() { Unavailable("glfwInit"); }
    void Terminate() { Unavailable("glfwTerminate"); }
    void PollEvents() { Unavailable("glfwPollEvents"); }
    bool JoystickPresent(std::int32_t) { Unavailable("glfwJoystickPresent"); }
    bool JoystickIsGamepad(std::int32_t) { Unavailable("glfwJoystickIsGamepad"); }
    bool GetGamepadState(std::int32_t, GamepadState&) { Unavailable("glfwGetGamepadState"); }
    std::optional<std::string> GetGamepadName(std::int32_t) { Unavailable("glfwGetGamepadName"); }
    std::optional<std::string> GetJoystickName(std::int32_t) { Unavailable("glfwGetJoystickName"); }
    std::optional<std::string> GetJoystickGUID(std::int32_t) { Unavailable("glfwGetJoystickGUID"); }
    std::vector<float> GetJoystickAxes(std::int32_t) { Unavailable("glfwGetJoystickAxes"); }
    std::vector<std::uint8_t> GetJoystickButtons(std::int32_t) { Unavailable("glfwGetJoystickButtons"); }
    std::vector<std::uint8_t> GetJoystickHats(std::int32_t) { Unavailable("glfwGetJoystickHats"); }
    bool UpdateGamepadMappings(const std::string&) { Unavailable("glfwUpdateGamepadMappings"); }
#else
    namespace
    {
        // OpenTK marshals GLFW's const char* with PtrToStringUTF8.
        [[nodiscard]] std::optional<std::string> ManagedString(const char* value)
        {
            if (value == nullptr)
            {
                return std::nullopt;
            }
            return ::MphRead::NativeRuntime::Utf8GetString(std::string_view(value));
        }

        template <typename T>
        [[nodiscard]] std::vector<T> ManagedArray(const T* values, int count)
        {
            if (values == nullptr)
            {
                return {};
            }
            if (count < 0)
            {
                throw std::out_of_range("GLFW returned a negative joystick item count.");
            }
            return std::vector<T>(values, values + count);
        }
    }

    bool Init()
    {
        return ::glfwInit() == GLFW_TRUE;
    }

    void Terminate()
    {
        ::glfwTerminate();
    }

    void PollEvents()
    {
        ::glfwPollEvents();
    }

    bool JoystickPresent(std::int32_t jid)
    {
        return ::glfwJoystickPresent(jid) == GLFW_TRUE;
    }

    bool JoystickIsGamepad(std::int32_t jid)
    {
        return ::glfwJoystickIsGamepad(jid) == GLFW_TRUE;
    }

    bool GetGamepadState(std::int32_t jid, GamepadState& state)
    {
        GLFWgamepadstate native{};
        const bool result = ::glfwGetGamepadState(jid, &native) == GLFW_TRUE;
        for (std::size_t i = 0; i < state.Buttons.size(); ++i)
        {
            state.Buttons[i] = native.buttons[i];
        }
        for (std::size_t i = 0; i < state.Axes.size(); ++i)
        {
            state.Axes[i] = native.axes[i];
        }
        return result;
    }

    std::optional<std::string> GetGamepadName(std::int32_t jid)
    {
        return ManagedString(::glfwGetGamepadName(jid));
    }

    std::optional<std::string> GetJoystickName(std::int32_t jid)
    {
        return ManagedString(::glfwGetJoystickName(jid));
    }

    std::optional<std::string> GetJoystickGUID(std::int32_t jid)
    {
        return ManagedString(::glfwGetJoystickGUID(jid));
    }

    std::vector<float> GetJoystickAxes(std::int32_t jid)
    {
        int count = 0;
        const float* values = ::glfwGetJoystickAxes(jid, &count);
        return ManagedArray(values, count);
    }

    std::vector<std::uint8_t> GetJoystickButtons(std::int32_t jid)
    {
        int count = 0;
        const unsigned char* values = ::glfwGetJoystickButtons(jid, &count);
        return ManagedArray<std::uint8_t>(values, count);
    }

    std::vector<std::uint8_t> GetJoystickHats(std::int32_t jid)
    {
        int count = 0;
        const unsigned char* values = ::glfwGetJoystickHats(jid, &count);
        return ManagedArray<std::uint8_t>(values, count);
    }

    bool UpdateGamepadMappings(const std::string& mappings)
    {
        return ::glfwUpdateGamepadMappings(mappings.c_str()) == GLFW_TRUE;
    }
#endif
}
