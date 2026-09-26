#include "GamepadPlatformChecks.hpp"

#include "GamepadChecks.hpp"
#include "GamepadLayout.hpp"
#include "GamepadManager.hpp"
#include "GamepadMappings.hpp"
#include "GamepadOptions.hpp"
#include "PadBindings.hpp"

#include <array>
#include <utility>
#include <vector>

namespace MphRead::Mods::Input
{
    void GamepadPlatformChecks::Run()
    {
        const std::string guid = "030000005e040000130b000099090000";
        const GamepadLayout profile = GamepadLayout::Select(guid, 6, 19, 1, true);
        float leftFloor = 0;
        float rightFloor = 0;
        std::vector<float> axes{0, 0, 0, 0, -1, -1};
        std::vector<std::uint8_t> buttons(19);
        std::vector<std::uint8_t> hats{0};
        const auto read = [&]() { return profile.Read(axes, buttons, hats, leftFloor, rightFloor); };
        GamepadState state = read();
        GamepadChecks::Check(state.RightX == 0 && state.RightY == 0 && state.RightTrigger == 0 && state.LeftTrigger == 0,
            "macOS Xbox Bluetooth rests with neutral sticks and released triggers");
        axes[4] = 1;
        state = read();
        GamepadChecks::Check(state.RightTrigger == 1 && state.LeftTrigger == 0 && state.RightX == 0 && state.RightY == 0,
            "macOS Xbox right trigger cannot move the camera");
        GamepadManager::UpdateDevice("mac-fixture", state, false);
        GamepadChecks::Check(GamepadManager::ActiveState().Down(GamepadButtons::RightTrigger), "raw macOS trigger reaches bindable RT flag");
        axes[4] = -1;
        axes[5] = 1;
        state = read();
        GamepadChecks::Check(state.LeftTrigger == 1 && state.RightTrigger == 0 && state.RightX == 0 && state.RightY == 0,
            "macOS Xbox left trigger cannot move either stick");
        axes[5] = -1;
        axes[2] = .75F;
        axes[3] = -.5F;
        state = read();
        GamepadChecks::Check(state.RightX == .75F && state.RightY == .5F && state.LeftTrigger == 0 && state.RightTrigger == 0,
            "macOS Xbox right stick does not actuate triggers");
        axes[2] = axes[3] = 0;
        const std::array<std::pair<std::size_t, GamepadButtons>, 10> physical{{
            {0, GamepadButtons::A}, {1, GamepadButtons::B}, {3, GamepadButtons::X}, {4, GamepadButtons::Y},
            {6, GamepadButtons::LeftBumper}, {7, GamepadButtons::RightBumper}, {10, GamepadButtons::Back},
            {11, GamepadButtons::Start}, {13, GamepadButtons::LeftThumb}, {14, GamepadButtons::RightThumb}}};
        for (const auto& [index, expected] : physical)
        {
            buttons[index] = 1;
            GamepadChecks::Check(read().Buttons == expected, "macOS Xbox physical " + ToString(expected));
            buttons[index] = 0;
        }
        hats[0] = 1 | 2; // JoystickHats.RightUp
        GamepadChecks::Check(read().Buttons == (GamepadButtons::DpadUp | GamepadButtons::DpadRight), "macOS Xbox diagonal hat");
        GamepadManager::RemoveDevice("mac-fixture");
        const std::optional<std::string> mapping = GamepadMappings::CompatibleMacXboxMapping(guid, "Xbox Wireless Controller", 6, 19, 1, true);
        GamepadChecks::Check(mapping.has_value() && mapping->starts_with(guid + ",") && mapping->find("righttrigger:a4,") != std::string::npos
            && mapping->find("lefttrigger:a5,") != std::string::npos && mapping->find("rightx:a2,righty:a3,") != std::string::npos,
            "unknown Xbox firmware receives correct macOS mapping");
        GamepadChecks::Check(!GamepadMappings::CompatibleMacXboxMapping(guid, "Xbox", 6, 19, 1, false).has_value(),
            "macOS firmware compatibility never overrides Windows/Linux layouts");
        GamepadChecks::Check(!GamepadMappings::CompatibleMacXboxMapping(guid, "Xbox", 7, 19, 1, true).has_value()
            && !GamepadMappings::CompatibleMacXboxMapping(guid, "Xbox", 6, 10, 0, true).has_value(),
            "unrecognized HID shapes are not silently assigned a Bluetooth mapping");
        GamepadChecks::Check(GamepadLayout::Select(guid, 6, 19, 1, false).AxisRightX == 3,
            "existing Linux raw profile stays interleaved");
        GamepadChecks::Check(GamepadLayout::Select("generic", 4, 12, 1, false).ButtonRightTrigger == 7,
            "generic four-axis digital-trigger profile remains available");
        GamepadOptions::LookX(1.75F);
        for (const std::string preset : {"Default", "Bumper Jumper", "Southpaw", "Classic"})
        {
            PadBindings::ApplyPreset(preset);
            GamepadChecks::Check(PadBindings::Get(PadAction::Shoot) == GamepadButtons::RightTrigger
                && PadBindings::Get(PadAction::Zoom) == GamepadButtons::LeftTrigger, preset + " preserves trigger actions");
            GamepadChecks::Check(GamepadOptions::Southpaw() == (preset == "Southpaw"), preset + " selects correct stick roles");
            GamepadChecks::Check(GamepadOptions::LookX() == 1.75F, preset + " preserves personal calibration");
        }
        PadBindings::Reset();
        PadBindings::SetSlot(PadAction::Jump, 0, GamepadButtons::None);
        PadBindings::SetSlot(PadAction::Jump, 1, GamepadButtons::A);
        PadBindings::Assign(PadAction::Shoot, 0, GamepadButtons::A, "Swap");
        GamepadChecks::Check(PadBindings::Slot(PadAction::Jump, 0) == GamepadButtons::None
            && PadBindings::Slot(PadAction::Jump, 1) == GamepadButtons::RightTrigger, "conflict swaps preserve primary/secondary positions");
        PadBindings::Reset();
        GamepadOptions::Reset();
    }
}
