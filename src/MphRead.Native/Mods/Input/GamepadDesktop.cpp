#include "GamepadDesktop.hpp"

#include "GamepadGlyphs.hpp"
#include "GamepadHaptics.hpp"
#include "GamepadManager.hpp"
#include "GamepadMappingWizard.hpp"
#include "GamepadMappings.hpp"
#include "WindowsGamepadHaptics.hpp"
#include "../../NativeRuntime/OpenTK/GLFW.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"

namespace MphRead::Mods::Input
{
    namespace Glfw = ::OpenTK::Windowing::GraphicsLibraryFramework;

    std::array<GamepadDesktop::Slot, 16> GamepadDesktop::Slots{};

    void GamepadDesktop::DeviceChanged(std::int32_t index)
    {
        if (static_cast<std::uint32_t>(index) >= Slots.size())
        {
            return;
        }
        Slot& slot = Slots[static_cast<std::size_t>(index)];
        if (slot.Id.has_value())
        {
            GamepadHaptics::Unregister(*slot.Id);
            GamepadManager::RemoveDevice(*slot.Id);
        }
        slot.Id = std::nullopt;
    }

    void GamepadDesktop::Poll()
    {
        if (::MphRead::NativeRuntime::IsAndroid())
        {
            return;
        }
        try
        {
            PollUnsafe();
        }
        catch (const Glfw::GlfwUnavailableException&)
        {
            for (const Slot& slot : Slots)
            {
                if (slot.Id.has_value())
                {
                    GamepadManager::RemoveDevice(*slot.Id);
                }
            }
            _unavailable = true;
        }
    }

    void GamepadDesktop::PollUnsafe()
    {
        if (_unavailable)
        {
            return;
        }
        if (GamepadMappings::ReloadRequested)
        {
            GamepadMappings::ReloadRequested = false;
            for (std::int32_t slot = 0; slot < static_cast<std::int32_t>(Slots.size()); slot++)
            {
                DeviceChanged(slot);
            }
        }
        GamepadMappings::EnsureLoaded();
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(Slots.size()); i++)
        {
            Slot& slot = Slots[static_cast<std::size_t>(i)];
            if (!Glfw::GLFW::JoystickPresent(i))
            {
                if (slot.Id.has_value())
                {
                    GamepadManager::RemoveDevice(*slot.Id);
                }
                slot.Id = std::nullopt;
                continue;
            }
            if (!slot.Id.has_value())
            {
                slot.Generation++;
                const std::string guid = Glfw::GLFW::GetJoystickGUID(i).value_or("");
                slot.XInput = ::MphRead::NativeRuntime::StringStartsWithOrdinalIgnoreCase(guid, "78696e707574");
                slot.Id = "glfw:" + guid + ":" + std::to_string(i) + ":" + std::to_string(slot.Generation);
                const bool compatible = GamepadMappings::TryMapMacXbox(i);
                slot.Mapped = Glfw::GLFW::JoystickIsGamepad(i);
                slot.Mapping = compatible ? "Xbox Bluetooth compatibility" : slot.Mapped ? "GLFW mapping" : "Unmapped fallback";
                slot.Name = (slot.Mapped ? Glfw::GLFW::GetGamepadName(i) : Glfw::GLFW::GetJoystickName(i)).value_or("gamepad");
                if (!slot.Mapped)
                {
                    slot.Name += " (unmapped)";
                }
                slot.Family = GamepadGlyphs::Detect(slot.Name, guid);
                slot.Layout = GamepadLayout::For(i);
                slot.Capabilities = GamepadMappings::Capabilities(guid,
                    slot.Layout.Capabilities(static_cast<std::int32_t>(Glfw::GLFW::GetJoystickAxes(i).size())));
                slot.LeftFloor = slot.RightFloor = 0;
            }
            if (GamepadMappingWizard::RequestedDevice == slot.Id)
            {
                auto sample = std::make_shared<GamepadRawSample>();
                sample->DeviceId = *slot.Id;
                sample->Guid = Glfw::GLFW::GetJoystickGUID(i).value_or("");
                sample->Name = slot.Name;
                sample->Axes = Glfw::GLFW::GetJoystickAxes(i);
                for (const std::uint8_t button : Glfw::GLFW::GetJoystickButtons(i))
                {
                    sample->Buttons.push_back(button == 1);
                }
                sample->Hats = Glfw::GLFW::GetJoystickHats(i);
                GamepadMappingWizard::Latest = sample;
            }
            if (!(slot.Mapped ? TryRead(i) : TryReadRaw(i)))
            {
                GamepadManager::RemoveDevice(*slot.Id);
                slot.Id = std::nullopt;
            }
        }
        std::optional<std::string> uniqueId;
        std::int32_t xinputCount = 0;
        for (const Slot& slot : Slots)
        {
            if (slot.Id.has_value() && slot.XInput)
            {
                uniqueId = slot.Id;
                xinputCount++;
            }
        }
        WindowsGamepadHaptics::Synchronize(xinputCount == 1 ? uniqueId : std::nullopt);
    }

    bool GamepadDesktop::TryRead(std::int32_t index)
    {
        Glfw::GamepadState raw{};
        if (!Glfw::GLFW::JoystickIsGamepad(index) || !Glfw::GLFW::GetGamepadState(index, raw))
        {
            return false;
        }
        Slot& slot = Slots[static_cast<std::size_t>(index)];
        GamepadState state{};
        state.Connected = true;
        state.Name = slot.Name;
        state.LeftX = raw.Axes[AxisLeftX];
        state.LeftY = -raw.Axes[AxisLeftY];
        state.RightX = raw.Axes[AxisRightX];
        state.RightY = -raw.Axes[AxisRightY];
        state.LeftTrigger = (raw.Axes[AxisLeftTrigger] + 1) / 2;
        state.RightTrigger = (raw.Axes[AxisRightTrigger] + 1) / 2;
        GamepadButtons buttons = GamepadButtons::None;
        Add(buttons, raw.Buttons, ButtonA, GamepadButtons::A);
        Add(buttons, raw.Buttons, ButtonB, GamepadButtons::B);
        Add(buttons, raw.Buttons, ButtonX, GamepadButtons::X);
        Add(buttons, raw.Buttons, ButtonY, GamepadButtons::Y);
        Add(buttons, raw.Buttons, ButtonLeftBumper, GamepadButtons::LeftBumper);
        Add(buttons, raw.Buttons, ButtonRightBumper, GamepadButtons::RightBumper);
        Add(buttons, raw.Buttons, ButtonBack, GamepadButtons::Back);
        Add(buttons, raw.Buttons, ButtonStart, GamepadButtons::Start);
        Add(buttons, raw.Buttons, ButtonLeftThumb, GamepadButtons::LeftThumb);
        Add(buttons, raw.Buttons, ButtonRightThumb, GamepadButtons::RightThumb);
        Add(buttons, raw.Buttons, ButtonDpadUp, GamepadButtons::DpadUp);
        Add(buttons, raw.Buttons, ButtonDpadRight, GamepadButtons::DpadRight);
        Add(buttons, raw.Buttons, ButtonDpadDown, GamepadButtons::DpadDown);
        Add(buttons, raw.Buttons, ButtonDpadLeft, GamepadButtons::DpadLeft);
        state.Buttons = buttons;
        GamepadManager::UpdateDevice(*slot.Id, state, slot.Mapped, slot.Family,
            slot.Capabilities | (GamepadHaptics::Available(*slot.Id) ? GamepadCapabilities::Rumble : GamepadCapabilities::None),
            slot.Mapping);
        return true;
    }

    bool GamepadDesktop::TryReadRaw(std::int32_t index)
    {
        if (!Glfw::GLFW::JoystickPresent(index) || Glfw::GLFW::JoystickIsGamepad(index))
        {
            return false;
        }
        const std::vector<float> axes = Glfw::GLFW::GetJoystickAxes(index);
        const std::vector<std::uint8_t> buttons = Glfw::GLFW::GetJoystickButtons(index);
        if (axes.size() < 2 || buttons.size() < 4)
        {
            return false;
        }
        Slot& slot = Slots[static_cast<std::size_t>(index)];
        GamepadState state = slot.Layout.Read(axes, buttons, Glfw::GLFW::GetJoystickHats(index),
            slot.LeftFloor, slot.RightFloor);
        state.Name = slot.Name;
        GamepadManager::UpdateDevice(*slot.Id, state, slot.Mapped, slot.Family,
            slot.Layout.Capabilities(static_cast<std::int32_t>(axes.size()))
                | (GamepadHaptics::Available(*slot.Id) ? GamepadCapabilities::Rumble : GamepadCapabilities::None),
            slot.Mapping);
        return true;
    }

    void GamepadDesktop::Add(GamepadButtons& into, const std::array<std::uint8_t, 15>& buttons, std::int32_t index,
        GamepadButtons flag) noexcept
    {
        if (buttons[static_cast<std::size_t>(index)] == 1)
        {
            into |= flag;
        }
    }
}
