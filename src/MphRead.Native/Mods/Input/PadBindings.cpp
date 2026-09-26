#include "PadBindings.hpp"

#include "ControllerLayoutState.hpp"
#include "GamepadRuntimeConfig.hpp"
#include "PadBindingState.hpp"

namespace MphRead::Mods::Input
{
    namespace
    {
        [[nodiscard]] std::shared_ptr<PadBindingState> State()
        {
            return GamepadRuntimeConfig::Current()->Bindings();
        }
    }

    std::string PadBindings::Preset() { return State()->Preset(); }
    void PadBindings::Preset(std::string value) { State()->Preset(std::move(value)); }
    std::int64_t PadBindings::Revision() { return State()->Revision(); }
    const std::array<PadAction, PadActionCount>& PadBindings::Actions() { return State()->Actions(); }
    GamepadButtons PadBindings::Get(PadAction action) { return State()->Get(action); }
    void PadBindings::Set(PadAction action, GamepadButtons buttons) { State()->Set(action, buttons); }
    GamepadButtons PadBindings::Default(PadAction action) { return State()->Default(action); }
    GamepadButtons PadBindings::Slot(PadAction action, std::int32_t slot) { return State()->Slot(action, slot); }

    void PadBindings::SetSlot(PadAction action, std::int32_t slot, GamepadButtons button, GamepadButtons modifier)
    {
        State()->SetSlot(action, slot, button, modifier);
    }

    bool PadBindings::Single(GamepadButtons button) { return State()->Single(button); }
    GamepadButtons PadBindings::Modifier(PadAction action, std::int32_t slot) { return State()->Modifier(action, slot); }
    std::string PadBindings::DescribeSlot(PadAction action, std::int32_t slot) { return State()->DescribeSlot(action, slot); }

    std::uint64_t PadBindings::Evaluate(GamepadButtons buttons, GamepadButtons suppressed)
    {
        return State()->Evaluate(buttons, suppressed);
    }

    GamepadButtons PadBindings::ChordButtons(GamepadButtons buttons) { return State()->ChordButtons(buttons); }
    void PadBindings::Write(std::vector<std::string>& lines) { State()->Write(lines); }
    void PadBindings::LoadSlots(const std::vector<std::string>& lines) { State()->LoadSlots(lines); }

    std::vector<PadAction> PadBindings::Conflicts(PadAction action, GamepadButtons button, GamepadButtons modifier)
    {
        return State()->Conflicts(action, button, modifier);
    }

    void PadBindings::Assign(PadAction action, std::int32_t slot, GamepadButtons button, std::string_view resolution,
        GamepadButtons modifier)
    {
        State()->Assign(action, slot, button, resolution, modifier);
    }

    void PadBindings::ApplyPreset(const std::string& name) { GamepadRuntimeConfig::Current()->Layout()->Apply(name); }
    void PadBindings::Reset() { State()->Reset(); }
    std::string PadBindings::Name(PadAction action) { return State()->Name(action); }
    std::string PadBindings::Describe(GamepadButtons buttons) { return State()->Describe(buttons); }
    std::string PadBindings::ButtonName(GamepadButtons button) { return State()->ButtonName(button); }
    std::string PadBindings::SettingKey(PadAction action) { return State()->SettingKey(action); }
    bool PadBindings::TryLoad(std::string_view key, std::string_view value) { return State()->TryLoad(key, value); }
}
