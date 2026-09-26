#pragma once

#include "GamepadState.hpp"
#include "PadAction.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Input
{
    class PadBindingState final
    {
    public:
        PadBindingState();

        [[nodiscard]] const std::string& Preset() const noexcept { return _preset; }
        void Preset(std::string value) { _preset = std::move(value); }
        [[nodiscard]] std::int64_t Revision() const noexcept { return _revision; }
        [[nodiscard]] const std::array<PadAction, PadActionCount>& Actions() const noexcept { return ActionOrder; }

        [[nodiscard]] GamepadButtons Get(PadAction action) const;
        void Set(PadAction action, GamepadButtons buttons);
        [[nodiscard]] GamepadButtons Default(PadAction action) const;
        [[nodiscard]] GamepadButtons Slot(PadAction action, std::int32_t slot) const;
        void SetSlot(PadAction action, std::int32_t slot, GamepadButtons button,
            GamepadButtons modifier = GamepadButtons::None);
        [[nodiscard]] bool Single(GamepadButtons button) const noexcept;
        [[nodiscard]] GamepadButtons Modifier(PadAction action, std::int32_t slot) const;
        [[nodiscard]] std::string DescribeSlot(PadAction action, std::int32_t slot) const;
        [[nodiscard]] std::uint64_t Evaluate(GamepadButtons buttons,
            GamepadButtons suppressed = GamepadButtons::None) const;
        [[nodiscard]] GamepadButtons ChordButtons(GamepadButtons buttons) const;
        void Write(std::vector<std::string>& lines) const;
        void LoadSlots(const std::vector<std::string>& lines);
        [[nodiscard]] std::vector<PadAction> Conflicts(PadAction action, GamepadButtons button,
            GamepadButtons modifier = GamepadButtons::None) const;
        void Assign(PadAction action, std::int32_t slot, GamepadButtons button, std::string_view resolution,
            GamepadButtons modifier = GamepadButtons::None);
        void ApplyPreset(const std::string& name);
        [[nodiscard]] std::shared_ptr<PadBindingState> Clone() const;
        void Reset();
        [[nodiscard]] std::string Name(PadAction action) const;
        [[nodiscard]] std::string Describe(GamepadButtons buttons) const;
        [[nodiscard]] std::string ButtonName(GamepadButtons button) const;
        [[nodiscard]] std::string SettingKey(PadAction action) const;
        [[nodiscard]] bool TryLoad(std::string_view key, std::string_view value);

    private:
        static const std::array<GamepadButtons, PadActionCount> _defaults;
        static const std::array<PadAction, PadActionCount> ActionOrder;

        std::string _preset = "Default";
        std::int64_t _revision = 0;
        std::array<GamepadButtons, PadActionCount> _current = _defaults;
        std::array<GamepadButtons, PadActionCount> Primary{};
        std::array<GamepadButtons, PadActionCount> Secondary{};
        std::array<std::array<GamepadButtons, 2>, PadActionCount> Modifiers{};
    };
}
