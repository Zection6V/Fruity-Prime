#include "InputPrompt.hpp"

#include "GamepadGlyphs.hpp"
#include "GamepadUiRouter.hpp"
#include "PadBindings.hpp"

namespace MphRead::Mods::Input
{
    InputPrompt InputPrompt::For(UiAction action)
    {
        GamepadButtons button = GamepadButtons::DpadRight;
        switch (action)
        {
        case UiAction::Accept: button = GamepadButtons::A; break;
        case UiAction::Back: button = GamepadButtons::B; break;
        case UiAction::PreviousTab: button = GamepadButtons::LeftBumper; break;
        case UiAction::NextTab: button = GamepadButtons::RightBumper; break;
        case UiAction::PageUp: button = GamepadButtons::LeftTrigger; break;
        case UiAction::PageDown: button = GamepadButtons::RightTrigger; break;
        case UiAction::Up: button = GamepadButtons::DpadUp; break;
        case UiAction::Down: button = GamepadButtons::DpadDown; break;
        case UiAction::Left: button = GamepadButtons::DpadLeft; break;
        default: break;
        }
        return {button, Input::ToString(action)};
    }

    InputPrompt InputPrompt::For(PadAction action)
    {
        const std::int32_t slot = PadBindings::Slot(action, 0) == GamepadButtons::None ? 1 : 0;
        return {PadBindings::Slot(action, slot), PadBindings::Name(action), PadBindings::Modifier(action, slot)};
    }

    std::string InputPrompt::Glyph() const
    {
        return (Modifier == GamepadButtons::None ? std::string() : GamepadGlyphs::Resolve(Modifier) + " + ")
            + GamepadGlyphs::Resolve(Button);
    }

    std::string InputPrompt::ToString() const
    {
        return Glyph() + ": " + Label;
    }
}
