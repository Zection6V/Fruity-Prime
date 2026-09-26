#include "GamepadLayout.hpp"

#include "GamepadAnalog.hpp"
#include "GamepadManager.hpp"
#include "../../NativeRuntime/OpenTK/GLFW.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"

#include <algorithm>

namespace MphRead::Mods::Input
{
    namespace Glfw = ::OpenTK::Windowing::GraphicsLibraryFramework;

    namespace
    {
        constexpr std::uint8_t Press = 1;
        constexpr std::uint8_t HatUp = 1;
        constexpr std::uint8_t HatRight = 2;
        constexpr std::uint8_t HatDown = 4;
        constexpr std::uint8_t HatLeft = 8;

        [[nodiscard]] bool Span(const std::string& guid, std::size_t start, std::string_view text)
        {
            return ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase(std::string_view(guid).substr(start, 8), text);
        }
    }

    // axisLeftX, axisLeftY, axisRightX, axisRightY, axisLeftTrigger,
    // axisRightTrigger, buttonA, buttonB, buttonX, buttonY, buttonLeftBumper,
    // buttonRightBumper, buttonLeftTrigger, buttonRightTrigger, buttonBack,
    // buttonStart, buttonLeftThumb, buttonRightThumb.
    const GamepadLayout GamepadLayout::Triggers{0, 1, 3, 4, 2, 5, 0, 1, 2, 3, 4, 5, -1, -1, 6, 7, 9, 10};
    const GamepadLayout GamepadLayout::Buttons{0, 1, 2, 3, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    const GamepadLayout GamepadLayout::MacXboxBluetooth{0, 1, 2, 3, 5, 4, 0, 1, 3, 4, 6, 7, -1, -1, 10, 11, 13, 14};

    GamepadCapabilities GamepadLayout::Capabilities(std::int32_t axes) const
    {
        const auto has = [axes](std::int32_t axis) { return axis >= 0 && axis < axes; };
        return (has(AxisLeftX) && has(AxisLeftY) ? GamepadCapabilities::AnalogLeftStick : GamepadCapabilities::None)
            | (has(AxisRightX) && has(AxisRightY) ? GamepadCapabilities::AnalogRightStick : GamepadCapabilities::None)
            | (has(AxisLeftTrigger) && has(AxisRightTrigger) ? GamepadCapabilities::AnalogTriggers : GamepadCapabilities::None);
    }

    bool GamepadLayout::IsMacXboxBluetooth(const std::string& guid, std::int32_t axes, std::int32_t buttons,
        std::int32_t hats, bool macOS)
    {
        return macOS && axes == 6 && buttons >= 15 && hats == 1 && guid.size() == 32
            && Span(guid, 0, "03000000") && Span(guid, 8, "5e040000")
            && (Span(guid, 16, "130b0000") || Span(guid, 16, "200b0000"));
    }

    GamepadLayout GamepadLayout::Select(const std::string& guid, std::int32_t axes, std::int32_t buttons,
        std::int32_t hats, bool macOS)
    {
        return IsMacXboxBluetooth(guid, axes, buttons, hats, macOS) ? MacXboxBluetooth
            : axes >= 6 ? Triggers : Buttons;
    }

    GamepadLayout GamepadLayout::For(std::int32_t slot)
    {
        return Select(Glfw::GLFW::GetJoystickGUID(slot).value_or(""),
            static_cast<std::int32_t>(Glfw::GLFW::GetJoystickAxes(slot).size()),
            static_cast<std::int32_t>(Glfw::GLFW::GetJoystickButtons(slot).size()),
            static_cast<std::int32_t>(Glfw::GLFW::GetJoystickHats(slot).size()),
            ::MphRead::NativeRuntime::IsMacOS());
    }

    GamepadState GamepadLayout::Read(const std::vector<float>& axes, const std::vector<std::uint8_t>& buttons,
        const std::vector<std::uint8_t>& hats, float& leftFloor, float& rightFloor) const
    {
        GamepadState state{};
        state.Connected = true;
        state.LeftX = Axis(axes, AxisLeftX);
        state.LeftY = -Axis(axes, AxisLeftY);
        state.RightX = Axis(axes, AxisRightX);
        state.RightY = -Axis(axes, AxisRightY);
        state.LeftTrigger = Trigger(axes, AxisLeftTrigger, leftFloor);
        state.RightTrigger = Trigger(axes, AxisRightTrigger, rightFloor);
        GamepadButtons flags = GamepadButtons::None;
        Add(flags, buttons, ButtonA, GamepadButtons::A);
        Add(flags, buttons, ButtonB, GamepadButtons::B);
        Add(flags, buttons, ButtonX, GamepadButtons::X);
        Add(flags, buttons, ButtonY, GamepadButtons::Y);
        Add(flags, buttons, ButtonLeftBumper, GamepadButtons::LeftBumper);
        Add(flags, buttons, ButtonRightBumper, GamepadButtons::RightBumper);
        Add(flags, buttons, ButtonBack, GamepadButtons::Back);
        Add(flags, buttons, ButtonStart, GamepadButtons::Start);
        Add(flags, buttons, ButtonLeftThumb, GamepadButtons::LeftThumb);
        Add(flags, buttons, ButtonRightThumb, GamepadButtons::RightThumb);
        Add(flags, buttons, ButtonLeftTrigger, GamepadButtons::LeftTrigger);
        Add(flags, buttons, ButtonRightTrigger, GamepadButtons::RightTrigger);
        if (!hats.empty())
        {
            if ((hats[0] & HatUp) != 0) { flags |= GamepadButtons::DpadUp; }
            if ((hats[0] & HatDown) != 0) { flags |= GamepadButtons::DpadDown; }
            if ((hats[0] & HatLeft) != 0) { flags |= GamepadButtons::DpadLeft; }
            if ((hats[0] & HatRight) != 0) { flags |= GamepadButtons::DpadRight; }
        }
        state.Buttons = flags;
        return state;
    }

    float GamepadLayout::Axis(const std::vector<float>& axes, std::int32_t index)
    {
        return index >= 0 && index < static_cast<std::int32_t>(axes.size())
            ? GamepadAnalog::Finite(axes[static_cast<std::size_t>(index)]) : 0;
    }

    float GamepadLayout::Trigger(const std::vector<float>& axes, std::int32_t index, float& floor)
    {
        if (index < 0 || index >= static_cast<std::int32_t>(axes.size()))
        {
            return 0;
        }
        const float value = GamepadAnalog::Finite(axes[static_cast<std::size_t>(index)]);
        floor = std::min(floor, value);
        return std::clamp((value - floor) / (1 - floor), 0.0F, 1.0F);
    }

    void GamepadLayout::Add(GamepadButtons& flags, const std::vector<std::uint8_t>& buttons, std::int32_t index,
        GamepadButtons flag)
    {
        if (index >= 0 && index < static_cast<std::int32_t>(buttons.size()) && buttons[static_cast<std::size_t>(index)] == Press)
        {
            flags |= flag;
        }
    }
}
