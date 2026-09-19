#include "GamepadDesktop.hpp"

#include "GamepadInput.hpp"
#include "GamepadLayout.hpp"
#include "GamepadMappings.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#include <dlfcn.h>
#include <unistd.h>
#endif

struct MphReadGlfwGamepadState
{
    std::uint8_t buttons[15];
    float axes[6];
};

namespace
{
    class GlfwBindingUnavailable final : public std::runtime_error
    {
    public:
        explicit GlfwBindingUnavailable(const char* procedure)
            : std::runtime_error(std::string("GLFW binding unavailable: ") + procedure)
        {
        }
    };

    void AppendUtf8(std::string& output, std::uint32_t value)
    {
        if (value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU))
        {
            value = 0xFFFDU;
        }
        if (value <= 0x7FU)
        {
            output.push_back(static_cast<char>(value));
        }
        else if (value <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (value >> 6)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else if (value <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (value >> 12)));
            output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (value >> 18)));
            output.push_back(static_cast<char>(0x80U | ((value >> 12) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
    }

    [[nodiscard]] std::string DecodeUtf8(std::string_view input)
    {
        std::string output;
        output.reserve(input.size());
        for (std::size_t index = 0; index < input.size();)
        {
            const auto first = static_cast<unsigned char>(input[index]);
            if (first <= 0x7FU)
            {
                output.push_back(static_cast<char>(first));
                ++index;
                continue;
            }

            std::size_t length = 0;
            if (first >= 0xC2U && first <= 0xDFU)
            {
                length = 2;
            }
            else if (first >= 0xE0U && first <= 0xEFU)
            {
                length = 3;
            }
            else if (first >= 0xF0U && first <= 0xF4U)
            {
                length = 4;
            }
            else
            {
                AppendUtf8(output, 0xFFFDU);
                ++index;
                continue;
            }

            std::size_t available = 1;
            while (available < length && index + available < input.size()
                && (static_cast<unsigned char>(input[index + available]) & 0xC0U) == 0x80U)
            {
                ++available;
            }
            if (available < length)
            {
                AppendUtf8(output, 0xFFFDU);
                index += available;
                continue;
            }

            const auto second = static_cast<unsigned char>(input[index + 1]);
            if ((first == 0xE0U && second < 0xA0U)
                || (first == 0xEDU && second >= 0xA0U)
                || (first == 0xF0U && second < 0x90U)
                || (first == 0xF4U && second > 0x8FU))
            {
                AppendUtf8(output, 0xFFFDU);
                ++index;
                continue;
            }

            output.append(input.substr(index, length));
            index += length;
        }
        return output;
    }

    using Init = int (*)();
    using PollEvents = void (*)();
    using JoystickBool = int (*)(int);
    using GetGamepadState = int (*)(int, MphReadGlfwGamepadState*);
    using JoystickString = const char* (*)(int);
    using JoystickFloats = const float* (*)(int, int*);
    using JoystickBytes = const unsigned char* (*)(int, int*);

#if defined(_WIN32)
    [[nodiscard]] HMODULE GlfwModule() noexcept
    {
        static HMODULE module = []() noexcept -> HMODULE
        {
            for (const wchar_t* name : {L"glfw3.3.dll", L"glfw3.dll", L"glfw.dll"})
            {
                if (HMODULE handle = GetModuleHandleW(name))
                {
                    return handle;
                }
                if (HMODULE handle = LoadLibraryW(name))
                {
                    return handle;
                }
            }
            return nullptr;
        }();
        return module;
    }

    template <typename T>
    [[nodiscard]] T GlfwProc(const char* name) noexcept
    {
        const HMODULE module = GlfwModule();
        return module == nullptr
            ? nullptr
            : reinterpret_cast<T>(GetProcAddress(module, name));
    }
#elif defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__))
    [[nodiscard]] std::optional<std::filesystem::path> ExecutableDirectory() noexcept
    {
        try
        {
#if defined(__APPLE__)
            std::uint32_t size = 1;
            char probe = 0;
            if (_NSGetExecutablePath(&probe, &size) == 0 || size == 0)
            {
                return std::nullopt;
            }
            std::vector<char> buffer(size);
            if (_NSGetExecutablePath(buffer.data(), &size) != 0)
            {
                return std::nullopt;
            }
            return std::filesystem::path(buffer.data()).parent_path();
#else
            std::vector<char> buffer(256);
            for (;;)
            {
                const ssize_t length = readlink(
                    "/proc/self/exe", buffer.data(), buffer.size());
                if (length < 0)
                {
                    return std::nullopt;
                }
                if (static_cast<std::size_t>(length) < buffer.size())
                {
                    return std::filesystem::path(std::string(
                        buffer.data(), static_cast<std::size_t>(length))).parent_path();
                }
                buffer.resize(buffer.size() * 2U);
            }
#endif
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    [[nodiscard]] void* GlfwModule() noexcept
    {
        static void* module = []() noexcept -> void*
        {
#if defined(__APPLE__)
            constexpr const char* names[] = {
                "glfw.3.3.dylib", "libglfw.3.3.dylib",
                "glfw.3.dylib", "libglfw.3.dylib",
                "glfw.dylib", "libglfw.dylib", "glfw"};
#else
            constexpr const char* names[] = {
                "glfw.so.3.3", "libglfw.so.3.3",
                "glfw.so.3", "libglfw.so.3",
                "glfw.so", "libglfw.so", "glfw"};
#endif
            if (const auto directory = ExecutableDirectory())
            {
                for (const char* name : names)
                {
                    try
                    {
                        const std::string local = (*directory / name).string();
                        if (void* handle = dlopen(
                            local.c_str(), RTLD_LAZY | RTLD_LOCAL))
                        {
                            return handle;
                        }
                    }
                    catch (...)
                    {
                    }
                }
            }
            for (const char* name : names)
            {
                if (void* handle = dlopen(name, RTLD_LAZY | RTLD_LOCAL))
                {
                    return handle;
                }
            }
            return nullptr;
        }();
        return module;
    }

    template <typename T>
    [[nodiscard]] T GlfwProc(const char* name) noexcept
    {
        void* module = GlfwModule();
        return module == nullptr
            ? nullptr
            : reinterpret_cast<T>(dlsym(module, name));
    }
#else
    template <typename T>
    [[nodiscard]] T GlfwProc(const char*) noexcept
    {
        return nullptr;
    }
#endif

#define MPHREAD_GLFW_API(name, type, symbol) \
    [[nodiscard]] type name() noexcept \
    { \
        static const auto value = GlfwProc<type>(symbol); \
        return value; \
    }

    MPHREAD_GLFW_API(InitApi, Init, "glfwInit")
    MPHREAD_GLFW_API(PollEventsApi, PollEvents, "glfwPollEvents")
    MPHREAD_GLFW_API(IsGamepadApi, JoystickBool, "glfwJoystickIsGamepad")
    MPHREAD_GLFW_API(GamepadStateApi, GetGamepadState, "glfwGetGamepadState")
    MPHREAD_GLFW_API(GamepadNameApi, JoystickString, "glfwGetGamepadName")
    MPHREAD_GLFW_API(JoystickPresentApi, JoystickBool, "glfwJoystickPresent")
    MPHREAD_GLFW_API(JoystickAxesApi, JoystickFloats, "glfwGetJoystickAxes")
    MPHREAD_GLFW_API(JoystickButtonsApi, JoystickBytes, "glfwGetJoystickButtons")
    MPHREAD_GLFW_API(JoystickHatsApi, JoystickBytes, "glfwGetJoystickHats")
    MPHREAD_GLFW_API(JoystickNameApi, JoystickString, "glfwGetJoystickName")
#undef MPHREAD_GLFW_API

    template <typename T>
    [[nodiscard]] T Require(T function, const char* procedure)
    {
        if (function == nullptr)
        {
            throw GlfwBindingUnavailable(procedure);
        }
        return function;
    }

    [[nodiscard]] bool JoystickIsGamepad(std::int32_t slot)
    {
        return Require(IsGamepadApi(), "glfwJoystickIsGamepad")(slot) == 1;
    }

    [[nodiscard]] bool ReadGamepadState(
        std::int32_t slot, MphReadGlfwGamepadState& state)
    {
        return Require(GamepadStateApi(), "glfwGetGamepadState")(slot, &state) == 1;
    }

    [[nodiscard]] bool JoystickPresent(std::int32_t slot)
    {
        return Require(JoystickPresentApi(), "glfwJoystickPresent")(slot) == 1;
    }

    [[nodiscard]] std::optional<std::string> ReadJoystickString(
        JoystickString function, std::int32_t slot, const char* procedure)
    {
        const char* value = Require(function, procedure)(slot);
        return value == nullptr
            ? std::nullopt
            : std::optional<std::string>{DecodeUtf8(value)};
    }

    [[nodiscard]] std::vector<float> ReadJoystickAxes(std::int32_t slot)
    {
        int count = 0;
        const float* values = Require(
            JoystickAxesApi(), "glfwGetJoystickAxes")(slot, &count);
        if (values == nullptr)
        {
            return {};
        }
        if (count < 0)
        {
            throw std::out_of_range("GLFW joystick axis count was negative.");
        }
        return std::vector<float>(values, values + count);
    }

    [[nodiscard]] std::vector<std::uint8_t> ReadJoystickBytes(
        JoystickBytes function, std::int32_t slot, const char* procedure)
    {
        int count = 0;
        const unsigned char* values = Require(function, procedure)(slot, &count);
        if (values == nullptr)
        {
            return {};
        }
        if (count < 0)
        {
            throw std::out_of_range("GLFW joystick item count was negative.");
        }
        return std::vector<std::uint8_t>(values, values + count);
    }

    [[nodiscard]] MphRead::Mods::Input::GamepadButtons Or(
        MphRead::Mods::Input::GamepadButtons left,
        MphRead::Mods::Input::GamepadButtons right) noexcept
    {
        return static_cast<MphRead::Mods::Input::GamepadButtons>(
            static_cast<std::int32_t>(left) | static_cast<std::int32_t>(right));
    }

    [[nodiscard]] float Clamp01(float value) noexcept
    {
        if (value < 0.0F)
        {
            return 0.0F;
        }
        if (value > 1.0F)
        {
            return 1.0F;
        }
        return value;
    }
}

namespace MphRead::Mods::Input::GamepadLayoutAdapters
{
    std::optional<std::vector<float>> GetJoystickAxes(std::int32_t slot)
    {
        try
        {
            return ReadJoystickAxes(slot);
        }
        catch (const GlfwBindingUnavailable&)
        {
            return std::nullopt;
        }
    }
}

namespace MphRead::Mods::Input
{
    std::int32_t GamepadDesktop::_slot = -1;
    std::int32_t GamepadDesktop::_rescanCountdown = 0;
    bool GamepadDesktop::_rawSlot = false;
    bool GamepadDesktop::_initialised = false;
    std::int32_t GamepadDesktop::_floorSlot = -1;
    float GamepadDesktop::_leftFloor = 0.0F;
    float GamepadDesktop::_rightFloor = 0.0F;

    void GamepadDesktop::PollForMenu()
    {
#if defined(__ANDROID__)
        return;
#else
        try
        {
            if (!_initialised)
            {
                (void)Require(InitApi(), "glfwInit")();
                _initialised = true;
            }
            Require(PollEventsApi(), "glfwPollEvents")();
        }
        catch (const GlfwBindingUnavailable&)
        {
            _slot = -2;
            return;
        }
        Poll();
#endif
    }

    void GamepadDesktop::Poll()
    {
#if defined(__ANDROID__)
        return;
#else
        try
        {
            PollUnsafe();
        }
        catch (const GlfwBindingUnavailable&)
        {
            GamepadInput::State = GamepadState{};
            _slot = -2;
        }
#endif
    }

    void GamepadDesktop::PollUnsafe()
    {
        if (_slot == -2)
        {
            return;
        }
        GamepadMappings::EnsureLoaded();
        if (_slot >= 0 && (_rawSlot ? TryReadRaw(_slot) : TryRead(_slot)))
        {
            return;
        }
        _slot = -1;
        GamepadInput::State = GamepadState{};
        if (_rescanCountdown-- > 0)
        {
            return;
        }
        _rescanCountdown = RescanFrames;

        for (std::int32_t i = 0; i < 16; ++i)
        {
            if (TryRead(i))
            {
                _slot = i;
                _rawSlot = false;
                std::cout << "[input] gamepad: "
                    << GamepadInput::State.Name.value_or("gamepad") << '\n';
                return;
            }
        }
        for (std::int32_t i = 0; i < 16; ++i)
        {
            if (TryReadRaw(i))
            {
                _slot = i;
                _rawSlot = true;
                std::cout << "[input] gamepad: "
                    << GamepadInput::State.Name.value_or("gamepad")
                    << " -- no mapping for this device, reading it raw."
                    << " Run -gamepad to check the buttons, and rebind in"
                    << " Settings, Controls if any are in the wrong place.\n";
                return;
            }
        }
    }

    bool GamepadDesktop::TryRead(std::int32_t slot)
    {
        if (!JoystickIsGamepad(slot))
        {
            return false;
        }
        MphReadGlfwGamepadState raw{};
        if (!ReadGamepadState(slot, raw))
        {
            return false;
        }

        GamepadState state{};
        state.Connected = true;
        state.Name = ReadJoystickString(
            GamepadNameApi(), slot, "glfwGetGamepadName").value_or("gamepad");
        state.LeftX = raw.axes[0];
        state.LeftY = -raw.axes[1];
        state.RightX = raw.axes[2];
        state.RightY = -raw.axes[3];
        state.LeftTrigger = (raw.axes[4] + 1.0F) / 2.0F;
        state.RightTrigger = (raw.axes[5] + 1.0F) / 2.0F;

        GamepadButtons buttons = GamepadButtons::None;
        Add(buttons, raw.buttons, 0, GamepadButtons::A);
        Add(buttons, raw.buttons, 1, GamepadButtons::B);
        Add(buttons, raw.buttons, 2, GamepadButtons::X);
        Add(buttons, raw.buttons, 3, GamepadButtons::Y);
        Add(buttons, raw.buttons, 4, GamepadButtons::LeftBumper);
        Add(buttons, raw.buttons, 5, GamepadButtons::RightBumper);
        Add(buttons, raw.buttons, 6, GamepadButtons::Back);
        Add(buttons, raw.buttons, 7, GamepadButtons::Start);
        Add(buttons, raw.buttons, 9, GamepadButtons::LeftThumb);
        Add(buttons, raw.buttons, 10, GamepadButtons::RightThumb);
        Add(buttons, raw.buttons, 11, GamepadButtons::DpadUp);
        Add(buttons, raw.buttons, 12, GamepadButtons::DpadRight);
        Add(buttons, raw.buttons, 13, GamepadButtons::DpadDown);
        Add(buttons, raw.buttons, 14, GamepadButtons::DpadLeft);
        if (state.LeftTrigger > TriggerPress)
        {
            buttons = Or(buttons, GamepadButtons::LeftTrigger);
        }
        if (state.RightTrigger > TriggerPress)
        {
            buttons = Or(buttons, GamepadButtons::RightTrigger);
        }
        state.Buttons = buttons;
        GamepadInput::State = std::move(state);
        return true;
    }

    bool GamepadDesktop::TryReadRaw(std::int32_t slot)
    {
        if (!JoystickPresent(slot) || JoystickIsGamepad(slot))
        {
            return false;
        }
        const std::vector<float> axes = ReadJoystickAxes(slot);
        const std::vector<std::uint8_t> buttons = ReadJoystickBytes(
            JoystickButtonsApi(), slot, "glfwGetJoystickButtons");
        if (axes.size() < 2 || buttons.size() < 4)
        {
            return false;
        }
        if (_floorSlot != slot)
        {
            _floorSlot = slot;
            _leftFloor = 0.0F;
            _rightFloor = 0.0F;
        }
        const GamepadLayout layout = GamepadLayout::For(slot);

        GamepadState state{};
        state.Connected = true;
        state.Name = ReadJoystickString(
            JoystickNameApi(), slot, "glfwGetJoystickName").value_or("gamepad")
            + " (unmapped)";
        state.LeftX = Axis(axes, layout.AxisLeftX);
        state.LeftY = -Axis(axes, layout.AxisLeftY);
        state.RightX = Axis(axes, layout.AxisRightX);
        state.RightY = -Axis(axes, layout.AxisRightY);
        state.LeftTrigger = Trigger(axes, layout.AxisLeftTrigger, _leftFloor);
        state.RightTrigger = Trigger(axes, layout.AxisRightTrigger, _rightFloor);

        GamepadButtons flags = GamepadButtons::None;
        AddRaw(flags, buttons, layout.ButtonA, GamepadButtons::A);
        AddRaw(flags, buttons, layout.ButtonB, GamepadButtons::B);
        AddRaw(flags, buttons, layout.ButtonX, GamepadButtons::X);
        AddRaw(flags, buttons, layout.ButtonY, GamepadButtons::Y);
        AddRaw(flags, buttons, layout.ButtonLeftBumper, GamepadButtons::LeftBumper);
        AddRaw(flags, buttons, layout.ButtonRightBumper, GamepadButtons::RightBumper);
        AddRaw(flags, buttons, layout.ButtonBack, GamepadButtons::Back);
        AddRaw(flags, buttons, layout.ButtonStart, GamepadButtons::Start);
        AddRaw(flags, buttons, layout.ButtonLeftThumb, GamepadButtons::LeftThumb);
        AddRaw(flags, buttons, layout.ButtonRightThumb, GamepadButtons::RightThumb);
        AddRaw(flags, buttons, layout.ButtonLeftTrigger, GamepadButtons::LeftTrigger);
        AddRaw(flags, buttons, layout.ButtonRightTrigger, GamepadButtons::RightTrigger);

        const std::vector<std::uint8_t> hats = ReadJoystickBytes(
            JoystickHatsApi(), slot, "glfwGetJoystickHats");
        if (!hats.empty())
        {
            const std::uint8_t hat = hats[0];
            AddHat(flags, hat, 1, GamepadButtons::DpadUp);
            AddHat(flags, hat, 2, GamepadButtons::DpadRight);
            AddHat(flags, hat, 4, GamepadButtons::DpadDown);
            AddHat(flags, hat, 8, GamepadButtons::DpadLeft);
        }
        if (state.LeftTrigger > TriggerPress)
        {
            flags = Or(flags, GamepadButtons::LeftTrigger);
        }
        if (state.RightTrigger > TriggerPress)
        {
            flags = Or(flags, GamepadButtons::RightTrigger);
        }
        state.Buttons = flags;
        GamepadInput::State = std::move(state);
        return true;
    }

    float GamepadDesktop::Axis(
        const std::vector<float>& axes, std::int32_t index) noexcept
    {
        return index >= 0 && static_cast<std::size_t>(index) < axes.size()
            ? axes[static_cast<std::size_t>(index)]
            : 0.0F;
    }

    float GamepadDesktop::Trigger(
        const std::vector<float>& axes, std::int32_t index, float& floor) noexcept
    {
        if (index < 0 || static_cast<std::size_t>(index) >= axes.size())
        {
            return 0.0F;
        }
        const float value = axes[static_cast<std::size_t>(index)];
        if (value < floor)
        {
            floor = value;
        }
        const float span = 1.0F - floor;
        return span <= 0.0F ? 0.0F : Clamp01((value - floor) / span);
    }

    void GamepadDesktop::AddRaw(GamepadButtons& into,
        const std::vector<std::uint8_t>& buttons,
        std::int32_t index, GamepadButtons flag) noexcept
    {
        if (index >= 0 && static_cast<std::size_t>(index) < buttons.size()
            && buttons[static_cast<std::size_t>(index)] == 1)
        {
            into = Or(into, flag);
        }
    }

    void GamepadDesktop::AddHat(GamepadButtons& into,
        std::uint8_t hat, std::uint8_t match, GamepadButtons flag) noexcept
    {
        if ((hat & match) != 0)
        {
            into = Or(into, flag);
        }
    }

    void GamepadDesktop::Add(GamepadButtons& into,
        const std::uint8_t* buttons, std::int32_t index,
        GamepadButtons flag) noexcept
    {
        if (buttons[index] == 1)
        {
            into = Or(into, flag);
        }
    }
}
