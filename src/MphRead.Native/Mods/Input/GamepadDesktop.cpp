#include "GamepadDesktop.hpp"

#include "GamepadInput.hpp"
#include "GamepadLayout.hpp"
#include "GamepadMappings.hpp"
#include "../../NativeRuntime/OpenTK/GLFW.hpp"
#include "../../NativeRuntime/System/Encoding.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

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

using ::MphRead::NativeRuntime::MathClamp;

namespace Glfw = ::OpenTK::Windowing::GraphicsLibraryFramework;

namespace
{
    [[nodiscard]] MphRead::Mods::Input::GamepadButtons Or(
        MphRead::Mods::Input::GamepadButtons left,
        MphRead::Mods::Input::GamepadButtons right) noexcept
    {
        return static_cast<MphRead::Mods::Input::GamepadButtons>(
            static_cast<std::int32_t>(left) | static_cast<std::int32_t>(right));
    }

}

namespace MphRead::Mods::Input::GamepadLayoutAdapters
{
    std::optional<std::vector<float>> GetJoystickAxes(std::int32_t slot)
    {
        try
        {
            return Glfw::GLFW::GetJoystickAxes(slot);
        }
        catch (const Glfw::GlfwUnavailableException&)
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
                (void)Glfw::GLFW::Init();
                _initialised = true;
            }
            Glfw::GLFW::PollEvents();
        }
        catch (const Glfw::GlfwUnavailableException&)
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
        catch (const Glfw::GlfwUnavailableException&)
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
        if (!Glfw::GLFW::JoystickIsGamepad(slot))
        {
            return false;
        }
        Glfw::GamepadState raw{};
        if (!Glfw::GLFW::GetGamepadState(slot, raw))
        {
            return false;
        }

        GamepadState state{};
        state.Connected = true;
        state.Name = Glfw::GLFW::GetGamepadName(slot).value_or("gamepad");
        state.LeftX = raw.Axes[0];
        state.LeftY = -raw.Axes[1];
        state.RightX = raw.Axes[2];
        state.RightY = -raw.Axes[3];
        state.LeftTrigger = (raw.Axes[4] + 1.0F) / 2.0F;
        state.RightTrigger = (raw.Axes[5] + 1.0F) / 2.0F;

        GamepadButtons buttons = GamepadButtons::None;
        Add(buttons, raw.Buttons.data(), 0, GamepadButtons::A);
        Add(buttons, raw.Buttons.data(), 1, GamepadButtons::B);
        Add(buttons, raw.Buttons.data(), 2, GamepadButtons::X);
        Add(buttons, raw.Buttons.data(), 3, GamepadButtons::Y);
        Add(buttons, raw.Buttons.data(), 4, GamepadButtons::LeftBumper);
        Add(buttons, raw.Buttons.data(), 5, GamepadButtons::RightBumper);
        Add(buttons, raw.Buttons.data(), 6, GamepadButtons::Back);
        Add(buttons, raw.Buttons.data(), 7, GamepadButtons::Start);
        Add(buttons, raw.Buttons.data(), 9, GamepadButtons::LeftThumb);
        Add(buttons, raw.Buttons.data(), 10, GamepadButtons::RightThumb);
        Add(buttons, raw.Buttons.data(), 11, GamepadButtons::DpadUp);
        Add(buttons, raw.Buttons.data(), 12, GamepadButtons::DpadRight);
        Add(buttons, raw.Buttons.data(), 13, GamepadButtons::DpadDown);
        Add(buttons, raw.Buttons.data(), 14, GamepadButtons::DpadLeft);
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
        if (!Glfw::GLFW::JoystickPresent(slot) || Glfw::GLFW::JoystickIsGamepad(slot))
        {
            return false;
        }
        const std::vector<float> axes = Glfw::GLFW::GetJoystickAxes(slot);
        const std::vector<std::uint8_t> buttons = Glfw::GLFW::GetJoystickButtons(slot);
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
        state.Name = Glfw::GLFW::GetJoystickName(slot).value_or("gamepad")
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

        const std::vector<std::uint8_t> hats = Glfw::GLFW::GetJoystickHats(slot);
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
        return span <= 0.0F ? 0.0F : MathClamp((value - floor) / span, 0.0F, 1.0F);
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
