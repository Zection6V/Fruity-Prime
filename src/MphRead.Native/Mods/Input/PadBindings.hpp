#pragma once

#include "GamepadState.hpp"
#include "PadAction.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Input
{
    // The bindings of whichever controller is in force.
    class PadBindings final
    {
    public:
        PadBindings() = delete;

        [[nodiscard]] static std::string Preset();
        static void Preset(std::string value);
        [[nodiscard]] static std::int64_t Revision();
        [[nodiscard]] static const std::array<PadAction, PadActionCount>& Actions();
        [[nodiscard]] static GamepadButtons Get(PadAction action);
        static void Set(PadAction action, GamepadButtons buttons);
        [[nodiscard]] static GamepadButtons Default(PadAction action);
        [[nodiscard]] static GamepadButtons Slot(PadAction action, std::int32_t slot);
        static void SetSlot(PadAction action, std::int32_t slot, GamepadButtons button,
            GamepadButtons modifier = GamepadButtons::None);
        [[nodiscard]] static bool Single(GamepadButtons button);
        [[nodiscard]] static GamepadButtons Modifier(PadAction action, std::int32_t slot);
        [[nodiscard]] static std::string DescribeSlot(PadAction action, std::int32_t slot);
        [[nodiscard]] static std::uint64_t Evaluate(GamepadButtons buttons,
            GamepadButtons suppressed = GamepadButtons::None);
        [[nodiscard]] static GamepadButtons ChordButtons(GamepadButtons buttons);
        static void Write(std::vector<std::string>& lines);
        static void LoadSlots(const std::vector<std::string>& lines);
        [[nodiscard]] static std::vector<PadAction> Conflicts(PadAction action, GamepadButtons button,
            GamepadButtons modifier = GamepadButtons::None);
        static void Assign(PadAction action, std::int32_t slot, GamepadButtons button, std::string_view resolution,
            GamepadButtons modifier = GamepadButtons::None);
        static void ApplyPreset(const std::string& name);
        static void Reset();
        [[nodiscard]] static std::string Name(PadAction action);
        [[nodiscard]] static std::string Describe(GamepadButtons buttons);
        [[nodiscard]] static std::string ButtonName(GamepadButtons button);
        [[nodiscard]] static std::string SettingKey(PadAction action);
        [[nodiscard]] static bool TryLoad(std::string_view key, std::string_view value);
    };
}
