#include "GamepadProbe.hpp"

#include "GamepadDesktop.hpp"
#include "GamepadInput.hpp"
#include "GamepadMappings.hpp"
#include "PadBindings.hpp"
#include "../InputSettings.hpp"
#include "../../NativeRuntime/OpenTK/GLFW.hpp"
#include "../../NativeRuntime/System/Encoding.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <vector>

namespace Glfw = ::OpenTK::Windowing::GraphicsLibraryFramework;

namespace
{
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
        if (!Glfw::GLFW::Init())
        {
            std::cout << "[gamepad] GLFW would not start; no pads can be read here.\n";
            return 1;
        }

        std::int32_t result = 0;
        try
        {
            result = Watch(seconds);
        }
        catch (...)
        {
            Glfw::GLFW::Terminate();
            throw;
        }
        Glfw::GLFW::Terminate();
        return result;
    }

    std::int32_t GamepadProbe::Watch(double seconds)
    {
        const std::string formattedSeconds = ::MphRead::NativeRuntime::ToString(seconds, "0");
        const std::string formattedDeadZone
            = ::MphRead::NativeRuntime::ToString(InputSettings::GamepadDeadZone(), "0.00");
        const std::string formattedLook
            = ::MphRead::NativeRuntime::ToString(InputSettings::GamepadLookSensitivity(), "0.00");
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
            Glfw::GLFW::PollEvents();
            GamepadDesktop::Poll();
            GamepadInput::BeginFrame();
            GamepadState state = GamepadInput::State;
            everConnected |= state.Connected;
            std::string line = Describe(state);
            if (line != last)
            {
                last = line;
                const std::string elapsed
                    = ::MphRead::NativeRuntime::StringPadLeft(::MphRead::NativeRuntime::ToString(elapsedSeconds(), "0.0"), 5);
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
            if (!Glfw::GLFW::JoystickPresent(i))
            {
                continue;
            }
            ++found;
            const std::string name = Glfw::GLFW::GetJoystickName(i).value_or("?");
            if (Glfw::GLFW::JoystickIsGamepad(i))
            {
                std::cout << "  slot " << i << ": " << name << " -- mapped, usable\n";
                continue;
            }

            std::cout << "  slot " << i << ": " << name
                << " -- no mapping for this device; read raw, on a guessed layout\n";
            const std::int32_t axisCount
                = static_cast<std::int32_t>(Glfw::GLFW::GetJoystickAxes(i).size());
            const std::int32_t buttonCount
                = static_cast<std::int32_t>(Glfw::GLFW::GetJoystickButtons(i).size());
            const std::int32_t hatCount
                = static_cast<std::int32_t>(Glfw::GLFW::GetJoystickHats(i).size());
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
        text.append(::MphRead::NativeRuntime::StringPadLeft(::MphRead::NativeRuntime::ToString(state.LeftX, "0.00"), 5));
        text.push_back(',');
        text.append(::MphRead::NativeRuntime::StringPadLeft(::MphRead::NativeRuntime::ToString(state.LeftY, "0.00"), 5));
        text.append(") R(");
        text.append(::MphRead::NativeRuntime::StringPadLeft(::MphRead::NativeRuntime::ToString(state.RightX, "0.00"), 5));
        text.push_back(',');
        text.append(::MphRead::NativeRuntime::StringPadLeft(::MphRead::NativeRuntime::ToString(state.RightY, "0.00"), 5));
        text.append(") LT");
        text.append(::MphRead::NativeRuntime::ToString(state.LeftTrigger, "0.00"));
        text.append(" RT");
        text.append(::MphRead::NativeRuntime::ToString(state.RightTrigger, "0.00"));
        text.append("  aim(");
        text.append(::MphRead::NativeRuntime::StringPadLeft(::MphRead::NativeRuntime::ToString(GamepadInput::AimDeltaX(), "0.00"), 6));
        text.push_back(',');
        text.append(::MphRead::NativeRuntime::StringPadLeft(::MphRead::NativeRuntime::ToString(GamepadInput::AimDeltaY(), "0.00"), 6));
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
