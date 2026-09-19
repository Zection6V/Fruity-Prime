#include "GamepadProbe.hpp"

#include "GamepadDesktop.hpp"
#include "GamepadInput.hpp"
#include "GamepadMappings.hpp"
#include "PadBindings.hpp"
#include "../InputSettings.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <locale>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if !defined(_WIN32)
#if defined(__APPLE__)
#define MPHREAD_GLFW_WEAK __attribute__((weak_import))
#elif defined(__GNUC__) || defined(__clang__)
#define MPHREAD_GLFW_WEAK __attribute__((weak))
#else
#define MPHREAD_GLFW_WEAK
#endif
extern "C"
{
    MPHREAD_GLFW_WEAK int glfwInit();
    MPHREAD_GLFW_WEAK void glfwTerminate();
    MPHREAD_GLFW_WEAK void glfwPollEvents();
    MPHREAD_GLFW_WEAK int glfwJoystickPresent(int);
    MPHREAD_GLFW_WEAK const char* glfwGetJoystickName(int);
    MPHREAD_GLFW_WEAK int glfwJoystickIsGamepad(int);
    MPHREAD_GLFW_WEAK const float* glfwGetJoystickAxes(int, int*);
    MPHREAD_GLFW_WEAK const unsigned char* glfwGetJoystickButtons(int, int*);
    MPHREAD_GLFW_WEAK const unsigned char* glfwGetJoystickHats(int, int*);
}
#undef MPHREAD_GLFW_WEAK
#endif

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

    using Init = int (*)();
    using Terminate = void (*)();
    using PollEvents = void (*)();
    using JoystickBool = int (*)(int);
    using JoystickString = const char* (*)(int);
    using JoystickFloats = const float* (*)(int, int*);
    using JoystickBytes = const unsigned char* (*)(int, int*);

#if defined(_WIN32)
    [[nodiscard]] HMODULE GlfwModule() noexcept
    {
        static HMODULE module = []() noexcept -> HMODULE
        {
            for (const wchar_t* name : {L"glfw3.dll", L"glfw.dll"})
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

#define MPHREAD_GLFW_API(name, type, symbol) \
    [[nodiscard]] type name() noexcept \
    { \
        static const auto value = GlfwProc<type>(symbol); \
        return value; \
    }

    MPHREAD_GLFW_API(InitApi, Init, "glfwInit")
    MPHREAD_GLFW_API(TerminateApi, Terminate, "glfwTerminate")
    MPHREAD_GLFW_API(PollEventsApi, PollEvents, "glfwPollEvents")
    MPHREAD_GLFW_API(JoystickPresentApi, JoystickBool, "glfwJoystickPresent")
    MPHREAD_GLFW_API(JoystickNameApi, JoystickString, "glfwGetJoystickName")
    MPHREAD_GLFW_API(IsGamepadApi, JoystickBool, "glfwJoystickIsGamepad")
    MPHREAD_GLFW_API(JoystickAxesApi, JoystickFloats, "glfwGetJoystickAxes")
    MPHREAD_GLFW_API(JoystickButtonsApi, JoystickBytes, "glfwGetJoystickButtons")
    MPHREAD_GLFW_API(JoystickHatsApi, JoystickBytes, "glfwGetJoystickHats")
#undef MPHREAD_GLFW_API
#else
    [[nodiscard]] Init InitApi() noexcept
    {
        return glfwInit == nullptr ? nullptr : &glfwInit;
    }

    [[nodiscard]] Terminate TerminateApi() noexcept
    {
        return glfwTerminate == nullptr ? nullptr : &glfwTerminate;
    }

    [[nodiscard]] PollEvents PollEventsApi() noexcept
    {
        return glfwPollEvents == nullptr ? nullptr : &glfwPollEvents;
    }

    [[nodiscard]] JoystickBool JoystickPresentApi() noexcept
    {
        return glfwJoystickPresent == nullptr ? nullptr : &glfwJoystickPresent;
    }

    [[nodiscard]] JoystickString JoystickNameApi() noexcept
    {
        return glfwGetJoystickName == nullptr ? nullptr : &glfwGetJoystickName;
    }

    [[nodiscard]] JoystickBool IsGamepadApi() noexcept
    {
        return glfwJoystickIsGamepad == nullptr ? nullptr : &glfwJoystickIsGamepad;
    }

    [[nodiscard]] JoystickFloats JoystickAxesApi() noexcept
    {
        return glfwGetJoystickAxes == nullptr ? nullptr : &glfwGetJoystickAxes;
    }

    [[nodiscard]] JoystickBytes JoystickButtonsApi() noexcept
    {
        return glfwGetJoystickButtons == nullptr ? nullptr : &glfwGetJoystickButtons;
    }

    [[nodiscard]] JoystickBytes JoystickHatsApi() noexcept
    {
        return glfwGetJoystickHats == nullptr ? nullptr : &glfwGetJoystickHats;
    }
#endif

    template <typename T>
    [[nodiscard]] T Require(T function, const char* procedure)
    {
        if (function == nullptr)
        {
            throw GlfwBindingUnavailable(procedure);
        }
        return function;
    }

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

    [[nodiscard]] bool GlfwInit()
    {
        return Require(InitApi(), "glfwInit")() != 0;
    }

    void GlfwTerminate()
    {
        Require(TerminateApi(), "glfwTerminate")();
    }

    void GlfwPollEvents()
    {
        Require(PollEventsApi(), "glfwPollEvents")();
    }

    [[nodiscard]] bool JoystickPresent(std::int32_t slot)
    {
        return Require(JoystickPresentApi(), "glfwJoystickPresent")(slot) != 0;
    }

    [[nodiscard]] std::optional<std::string> JoystickName(std::int32_t slot)
    {
        const char* value = Require(
            JoystickNameApi(), "glfwGetJoystickName")(slot);
        return value == nullptr
            ? std::nullopt
            : std::optional<std::string>{DecodeUtf8(value)};
    }

    [[nodiscard]] bool JoystickIsGamepad(std::int32_t slot)
    {
        return Require(IsGamepadApi(), "glfwJoystickIsGamepad")(slot) != 0;
    }

    template <typename T>
    [[nodiscard]] std::int32_t JoystickLength(
        T function, std::int32_t slot, const char* procedure)
    {
        int count = 0;
        (void)Require(function, procedure)(slot, &count);
        return static_cast<std::int32_t>(count);
    }

    [[nodiscard]] std::string FormatFixed(double value, std::int32_t decimals)
    {
        if (std::isnan(value))
        {
            return "NaN";
        }
        if (std::isinf(value))
        {
            return std::signbit(value) ? "-Infinity" : "Infinity";
        }
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::fixed << std::setprecision(decimals) << value;
        return stream.str();
    }

    [[nodiscard]] std::string AlignRight(std::string value, std::size_t width)
    {
        if (value.size() < width)
        {
            value.insert(value.begin(), width - value.size(), ' ');
        }
        return value;
    }

    [[nodiscard]] std::int32_t Bits(MphRead::Mods::Input::GamepadButtons value) noexcept
    {
        return static_cast<std::int32_t>(value);
    }

    [[nodiscard]] MphRead::Mods::Input::GamepadButtons Or(
        MphRead::Mods::Input::GamepadButtons left,
        MphRead::Mods::Input::GamepadButtons right) noexcept
    {
        return static_cast<MphRead::Mods::Input::GamepadButtons>(
            Bits(left) | Bits(right));
    }

    [[nodiscard]] bool HasAny(
        MphRead::Mods::Input::GamepadButtons buttons,
        MphRead::Mods::Input::GamepadButtons match) noexcept
    {
        return (Bits(buttons) & Bits(match)) != 0;
    }

    [[nodiscard]] std::string ButtonsToString(
        MphRead::Mods::Input::GamepadButtons buttons)
    {
        using MphRead::Mods::Input::GamepadButtons;
        constexpr GamepadButtons values[] = {
            GamepadButtons::A,
            GamepadButtons::B,
            GamepadButtons::X,
            GamepadButtons::Y,
            GamepadButtons::LeftBumper,
            GamepadButtons::RightBumper,
            GamepadButtons::Back,
            GamepadButtons::Start,
            GamepadButtons::LeftThumb,
            GamepadButtons::RightThumb,
            GamepadButtons::DpadUp,
            GamepadButtons::DpadRight,
            GamepadButtons::DpadDown,
            GamepadButtons::DpadLeft,
            GamepadButtons::LeftTrigger,
            GamepadButtons::RightTrigger
        };
        constexpr std::string_view names[] = {
            "A", "B", "X", "Y", "LeftBumper", "RightBumper", "Back", "Start",
            "LeftThumb", "RightThumb", "DpadUp", "DpadRight", "DpadDown", "DpadLeft",
            "LeftTrigger", "RightTrigger"
        };
        static_assert(std::size(values) == std::size(names));

        if (buttons == GamepadButtons::None)
        {
            return "None";
        }
        constexpr std::int32_t knownMask = 0xFFFF;
        if ((Bits(buttons) & ~knownMask) != 0)
        {
            return std::to_string(Bits(buttons));
        }

        std::string result;
        for (std::size_t index = 0; index < std::size(values); ++index)
        {
            if (HasAny(buttons, values[index]))
            {
                if (!result.empty())
                {
                    result.append(", ");
                }
                result.append(names[index]);
            }
        }
        return result.empty() ? std::to_string(Bits(buttons)) : result;
    }
}

namespace MphRead::Mods::Input
{
    std::int32_t GamepadProbe::Run(double seconds)
    {
        if (!GlfwInit())
        {
            std::cout << "[gamepad] GLFW would not start; no pads can be read here.\n";
            return 1;
        }
        try
        {
            const std::int32_t result = Watch(seconds);
            GlfwTerminate();
            return result;
        }
        catch (...)
        {
            GlfwTerminate();
            throw;
        }
    }

    std::int32_t GamepadProbe::Watch(double seconds)
    {
        const std::string formattedSeconds = FormatFixed(seconds, 0);
        const std::string formattedDeadZone
            = FormatFixed(InputSettings::GamepadDeadZone(), 2);
        const std::string formattedLook
            = FormatFixed(InputSettings::GamepadLookSensitivity(), 2);
        const bool invertY = InputSettings::GamepadInvertY();
        std::string watchLine = "[gamepad] watching for ";
        watchLine.append(formattedSeconds);
        watchLine.append(" s. dead zone ");
        watchLine.append(formattedDeadZone);
        watchLine.append(", look ");
        watchLine.append(formattedLook);
        watchLine.append(", invert y ");
        watchLine.append(invertY ? "on" : "off");
        std::cout << watchLine << '\n';

        std::string bindingLine = "[gamepad] buttons: ";
        bool firstBinding = true;
        for (const PadAction action : PadBindings::Actions())
        {
            if (!firstBinding)
            {
                bindingLine.append(", ");
            }
            firstBinding = false;
            bindingLine.append(PadBindings::Name(action));
            bindingLine.push_back(' ');
            bindingLine.append(PadBindings::Describe(PadBindings::Get(action)));
        }
        std::cout << bindingLine << '\n';

        ReportPresence();
        const auto start = std::chrono::steady_clock::now();
        const auto elapsedSeconds = [&]() -> double
        {
            return std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start).count();
        };

        std::string last;
        bool everConnected = false;
        bool everMoved = false;
        while (elapsedSeconds() < seconds)
        {
            GlfwPollEvents();
            GamepadDesktop::Poll();
            GamepadInput::BeginFrame();
            GamepadState state = GamepadInput::State;
            everConnected |= state.Connected;
            std::string line = Describe(state);
            if (line != last)
            {
                last = line;
                const std::string elapsed
                    = AlignRight(FormatFixed(elapsedSeconds(), 1), 5);
                std::string output = "  ";
                output.append(elapsed);
                output.append("s ");
                output.append(line);
                std::cout << output << '\n';
                everMoved |= state.Connected
                    && (state.Buttons != GamepadButtons::None
                        || std::fabs(state.LeftX) > 0.5F || std::fabs(state.LeftY) > 0.5F
                        || std::fabs(state.RightX) > 0.5F || std::fabs(state.RightY) > 0.5F);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        std::cout << '\n';
        if (!everConnected)
        {
            std::cout << "[gamepad] FAIL: no pad was seen -- nothing is plugged in or "
                << "paired that reports itself as a joystick at all. A pad GLFW has no "
                << "mapping for would still have been listed above and read raw.\n";
            return 1;
        }
        if (!everMoved)
        {
            std::cout << "[gamepad] a pad is connected, but nothing was pressed or "
                << "moved far enough to read.\n";
            return 1;
        }
        std::cout << "[gamepad] PASS: a pad is connected and its input arrives.\n";
        return 0;
    }

    void GamepadProbe::ReportPresence()
    {
        GamepadMappings::EnsureLoaded();
        std::cout << "[gamepad] " << GamepadMappings::Summary() << '\n';
        std::int32_t found = 0;
        for (std::int32_t i = 0; i < 16; ++i)
        {
            if (!JoystickPresent(i))
            {
                continue;
            }
            ++found;
            const std::string name = JoystickName(i).value_or("?");
            if (JoystickIsGamepad(i))
            {
                std::cout << "  slot " << i << ": " << name << " -- mapped, usable\n";
                continue;
            }

            std::cout << "  slot " << i << ": " << name
                << " -- no mapping for this device; read raw, on a guessed layout\n";
            const std::int32_t axisCount
                = JoystickLength(JoystickAxesApi(), i, "glfwGetJoystickAxes");
            const std::int32_t buttonCount
                = JoystickLength(JoystickButtonsApi(), i, "glfwGetJoystickButtons");
            const std::int32_t hatCount
                = JoystickLength(JoystickHatsApi(), i, "glfwGetJoystickHats");
            std::cout << "    axes " << axisCount
                << ", buttons " << buttonCount
                << ", hats " << hatCount << '\n';
            std::cout << "    if any button below is in the wrong place, correct this "
                << "line and put it in " << GamepadMappings::FileName
                << ", beside the game or with your settings:\n";
            const std::string suggestion = GamepadMappings::Suggest(i);
            std::cout << "    " << suggestion << '\n';
        }
        if (found == 0)
        {
            std::cout << "  no joystick in any slot\n";
        }
    }

    std::string GamepadProbe::Describe(GamepadState state)
    {
        if (!state.Connected)
        {
            return "no pad";
        }

        std::string text = state.Name.value_or("");
        text.append("  L(");
        text.append(AlignRight(FormatFixed(state.LeftX, 2), 5));
        text.push_back(',');
        text.append(AlignRight(FormatFixed(state.LeftY, 2), 5));
        text.append(") R(");
        text.append(AlignRight(FormatFixed(state.RightX, 2), 5));
        text.push_back(',');
        text.append(AlignRight(FormatFixed(state.RightY, 2), 5));
        text.append(") LT");
        text.append(FormatFixed(state.LeftTrigger, 2));
        text.append(" RT");
        text.append(FormatFixed(state.RightTrigger, 2));
        text.append("  aim(");
        text.append(AlignRight(FormatFixed(GamepadInput::AimDeltaX(), 2), 6));
        text.push_back(',');
        text.append(AlignRight(FormatFixed(GamepadInput::AimDeltaY(), 2), 6));
        text.push_back(')');
        if (state.Buttons != GamepadButtons::None)
        {
            text.append("  ");
            text.append(ButtonsToString(state.Buttons));
            text.append("  -> ");
            text.append(Actions(state.Buttons));
        }
        return text;
    }

    std::string GamepadProbe::Actions(GamepadButtons buttons)
    {
        std::string text;
        Name(text, buttons, GamepadButtons::RightTrigger, "shoot/alt-attack");
        Name(text, buttons, GamepadButtons::LeftTrigger, "zoom");
        Name(text, buttons, GamepadButtons::A, "jump/boost");
        Name(text, buttons, GamepadButtons::B, "morph");
        Name(text, buttons, GamepadButtons::X, "scan");
        Name(text, buttons, GamepadButtons::Y, "scan visor");
        Name(text, buttons, GamepadButtons::Back, "scoreboard");
        Name(text, buttons, Or(GamepadButtons::RightBumper, GamepadButtons::DpadRight),
            "next weapon");
        Name(text, buttons, Or(GamepadButtons::LeftBumper, GamepadButtons::DpadLeft),
            "previous weapon");
        Name(text, buttons, GamepadButtons::DpadUp, "missile");
        Name(text, buttons, GamepadButtons::DpadDown, "power beam");
        Name(text, buttons, GamepadButtons::Start, "pause menu");
        return text.empty() ? "(nothing bound)" : text;
    }

    void GamepadProbe::Name(std::string& text, GamepadButtons buttons,
        GamepadButtons match, std::string_view action)
    {
        if (HasAny(buttons, match))
        {
            if (!text.empty())
            {
                text.append(", ");
            }
            text.append(action);
        }
    }
}
