#include "GamepadProbe.hpp"

#include "GamepadAnalog.hpp"
#include "GamepadDesktop.hpp"
#include "GamepadInput.hpp"
#include "GamepadManager.hpp"
#include "GamepadMappings.hpp"
#include "GamepadOptions.hpp"
#include "PadBindings.hpp"
#include "../InputSettings.hpp"
#include "../../NativeRuntime/OpenTK/GLFW.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Number.hpp"
#include "../../NativeRuntime/System/Stopwatch.hpp"

#include <chrono>
#include <cmath>
#include <thread>

namespace MphRead::Mods::Input
{
    namespace Runtime = ::MphRead::NativeRuntime;
    namespace Glfw = ::OpenTK::Windowing::GraphicsLibraryFramework;

    namespace
    {
        [[nodiscard]] std::string Number(float value, std::string_view format)
        {
            return Runtime::ToString(value, format);
        }

        // $"{value,5:0.00}": right-aligned in `width`.
        [[nodiscard]] std::string Pad(const std::string& text, std::size_t width)
        {
            return text.size() >= width ? text : std::string(width - text.size(), ' ') + text;
        }

        [[nodiscard]] std::string Pair(std::pair<float, float> value)
        {
            // ValueTuple<float, float>.ToString(): "(x, y)".
            return "(" + Runtime::ToString(value.first) + ", " + Runtime::ToString(value.second) + ")";
        }

        template <typename T>
        [[nodiscard]] std::string Join(const std::vector<T>& values)
        {
            std::string text;
            for (std::size_t i = 0; i < values.size(); i++)
            {
                text += (i == 0 ? "" : ",") + Runtime::ToString(values[i]);
            }
            return text;
        }
    }

    std::int32_t GamepadProbe::Run(double seconds, bool verbose)
    {
        if (!Glfw::GLFW::Init())
        {
            Runtime::ConsoleWriteLine("[gamepad] GLFW would not start; no pads can be read here.");
            return 1;
        }
        std::int32_t result = 0;
        try
        {
            result = Watch(seconds, verbose);
        }
        catch (...)
        {
            Glfw::GLFW::Terminate();
            throw;
        }
        Glfw::GLFW::Terminate();
        return result;
    }

    std::int32_t GamepadProbe::Watch(double seconds, bool verbose)
    {
        Runtime::ConsoleWriteLine("[gamepad] watching for " + Number(static_cast<float>(seconds), "0") + " s. "
            + "dead zone " + Number(InputSettings::GamepadDeadZone(), "0.00") + ", "
            + "look " + Number(InputSettings::GamepadLookSensitivity(), "0.00") + ", "
            + "invert y " + (InputSettings::GamepadInvertY() ? "on" : "off"));
        std::string bindings;
        for (const PadAction action : PadBindings::Actions())
        {
            bindings += (bindings.empty() ? "" : ", ") + PadBindings::Name(action) + " "
                + PadBindings::Describe(PadBindings::Get(action));
        }
        Runtime::ConsoleWriteLine("[gamepad] buttons: " + bindings);
        ReportPresence();
        const std::int64_t clock = Runtime::StopwatchGetTimestamp();
        const auto elapsed = [clock]()
        {
            return static_cast<double>(Runtime::StopwatchGetElapsedTicks(clock)) / 10000000.0;
        };
        std::string last;
        bool everConnected = false;
        bool everMoved = false;
        while (elapsed() < seconds)
        {
            Glfw::GLFW::PollEvents();
            GamepadDesktop::Poll();
            GamepadInput::BeginFrame();
            const GamepadState state = GamepadInput::State();
            everConnected |= state.Connected;
            std::string line = Describe(state);
            if (verbose)
            {
                const std::optional<GamepadDeviceSnapshot> active = GamepadManager::ActiveDevice();
                for (const GamepadDeviceSnapshot& device : GamepadManager::Devices())
                {
                    const auto left = GamepadAnalog::ApplyRadialDeadZone(device.State.LeftX, device.State.LeftY,
                        GamepadOptions::LeftInner(), GamepadOptions::LeftOuter());
                    const auto right = GamepadAnalog::ApplyRadialDeadZone(device.State.RightX, device.State.RightY,
                        GamepadOptions::RightInner(), GamepadOptions::RightOuter());
                    line += "\n    " + device.DeviceId + " " + ToString(device.Family)
                        + " mapped=" + (device.IsMapped ? "True" : "False")
                        + " active=" + (active.has_value() && device == *active ? "True" : "False")
                        + " mapping=" + device.Mapping + " capabilities=" + ToString(device.Capabilities)
                        + " " + Describe(device.State) + " processed L" + Pair(left) + " R" + Pair(right);
                }
                for (std::int32_t slot = 0; slot < 16; slot++)
                {
                    if (!Glfw::GLFW::JoystickPresent(slot))
                    {
                        continue;
                    }
                    std::vector<std::int32_t> buttons;
                    for (const std::uint8_t button : Glfw::GLFW::GetJoystickButtons(slot))
                    {
                        buttons.push_back(button);
                    }
                    std::vector<std::int32_t> hats;
                    for (const std::uint8_t hat : Glfw::GLFW::GetJoystickHats(slot))
                    {
                        hats.push_back(hat);
                    }
                    line += "\n    raw slot " + std::to_string(slot) + " axes=[" + Join(Glfw::GLFW::GetJoystickAxes(slot)) + "]"
                        + " buttons=[" + Join(buttons) + "]" + " hats=[" + Join(hats) + "]";
                }
            }
            if (line != last)
            {
                last = line;
                Runtime::ConsoleWriteLine("  " + Pad(Number(static_cast<float>(elapsed()), "0.0"), 5) + "s " + line);
                everMoved |= state.Connected
                    && (state.Buttons != GamepadButtons::None
                        || std::abs(state.LeftX) > 0.5F || std::abs(state.LeftY) > 0.5F
                        || std::abs(state.RightX) > 0.5F || std::abs(state.RightY) > 0.5F);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        Runtime::ConsoleWriteLine();
        if (!everConnected)
        {
            Runtime::ConsoleWriteLine("[gamepad] FAIL: no pad was seen -- nothing is plugged in or "
                "paired that reports itself as a joystick at all. A pad GLFW has no "
                "mapping for would still have been listed above and read raw.");
            return 1;
        }
        if (!everMoved)
        {
            Runtime::ConsoleWriteLine("[gamepad] a pad is connected, but nothing was pressed or "
                "moved far enough to read.");
            return 1;
        }
        Runtime::ConsoleWriteLine("[gamepad] PASS: a pad is connected and its input arrives.");
        return 0;
    }

    void GamepadProbe::ReportPresence()
    {
        GamepadMappings::EnsureLoaded();
        Runtime::ConsoleWriteLine("[gamepad] " + GamepadMappings::Summary());
        std::int32_t found = 0;
        for (std::int32_t i = 0; i < 16; i++)
        {
            if (!Glfw::GLFW::JoystickPresent(i))
            {
                continue;
            }
            found++;
            const std::string name = Glfw::GLFW::GetJoystickName(i).value_or("?");
            if (Glfw::GLFW::JoystickIsGamepad(i))
            {
                Runtime::ConsoleWriteLine("  slot " + std::to_string(i) + ": " + name + " -- mapped, usable");
                continue;
            }
            Runtime::ConsoleWriteLine("  slot " + std::to_string(i) + ": " + name + " -- no mapping for this device; "
                "read raw, on a guessed layout");
            Runtime::ConsoleWriteLine("    axes " + std::to_string(Glfw::GLFW::GetJoystickAxes(i).size()) + ", "
                + "buttons " + std::to_string(Glfw::GLFW::GetJoystickButtons(i).size()) + ", "
                + "hats " + std::to_string(Glfw::GLFW::GetJoystickHats(i).size()));
            Runtime::ConsoleWriteLine("    if any button below is in the wrong place, correct this "
                "line and put it in " + std::string(GamepadMappings::FileName) + ", beside the game or "
                "with your settings:");
            Runtime::ConsoleWriteLine("    " + GamepadMappings::Suggest(i));
        }
        if (found == 0)
        {
            Runtime::ConsoleWriteLine("  no joystick in any slot");
        }
    }

    std::string GamepadProbe::Describe(const GamepadState& state)
    {
        if (!state.Connected)
        {
            return "no pad";
        }
        std::string text = state.Name.value_or("") + "  L(" + Pad(Number(state.LeftX, "0.00"), 5) + ","
            + Pad(Number(state.LeftY, "0.00"), 5) + ")";
        text += " R(" + Pad(Number(state.RightX, "0.00"), 5) + "," + Pad(Number(state.RightY, "0.00"), 5) + ")";
        text += " LT" + Number(state.LeftTrigger, "0.00") + " RT" + Number(state.RightTrigger, "0.00");
        text += "  aim(" + Pad(Number(GamepadInput::AimDeltaX(), "0.00"), 6) + ","
            + Pad(Number(GamepadInput::AimDeltaY(), "0.00"), 6) + ")";
        if (state.Buttons != GamepadButtons::None)
        {
            text += "  " + ToString(state.Buttons);
            text += "  -> " + Actions(state.Buttons);
        }
        return text;
    }

    std::string GamepadProbe::Actions(GamepadButtons buttons)
    {
        std::string text;
        const std::uint64_t active = PadBindings::Evaluate(buttons);
        for (const PadAction action : PadBindings::Actions())
        {
            if ((active & (1ULL << static_cast<std::int32_t>(action))) != 0)
            {
                text += (text.empty() ? "" : ", ") + PadBindings::Name(action);
            }
        }
        return text.empty() ? "(nothing bound)" : text;
    }

    void GamepadProbe::Name(std::string& text, GamepadButtons buttons, GamepadButtons match, std::string_view action)
    {
        if (Any(buttons & match))
        {
            text += text.empty() ? "" : ", ";
            text += action;
        }
    }
}
