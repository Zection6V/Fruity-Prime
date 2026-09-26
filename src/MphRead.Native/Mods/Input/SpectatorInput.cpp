#include "SpectatorInput.hpp"

#include "GamepadAnalog.hpp"
#include "GamepadInput.hpp"
#include "GamepadOptions.hpp"
#include "GamepadUiRouter.hpp"
#include "../SpectatorMode.hpp"

namespace MphRead::Mods::Input
{
    SpectatorInput SpectatorInput::ReadController(bool replay)
    {
        static_cast<void>(replay);
        if (!GamepadContexts::Focused() || GamepadContexts::Current() != GamepadContext::Gameplay)
        {
            return {};
        }
        const GamepadState pad = GamepadInput::State();
        const auto move = GamepadAnalog::ApplyRadialDeadZone(pad.LeftX, pad.LeftY, GamepadOptions::LeftInner(), GamepadOptions::LeftOuter());
        SpectatorInput input{};
        input.NextPlayer = GamepadInput::TakePress(GamepadButtons::RightBumper);
        input.PreviousPlayer = GamepadInput::TakePress(GamepadButtons::LeftBumper);
        input.ToggleView = GamepadInput::TakePress(GamepadButtons::Y);
        input.Scoreboard = pad.Down(GamepadButtons::Back);
        input.OpenMenu = GamepadInput::TakePress(GamepadButtons::Start);
        input.MoveX = move.first;
        input.MoveY = move.second;
        input.LookX = GamepadInput::AimDeltaX();
        input.LookY = GamepadInput::AimDeltaY();
        input.Ascend = pad.RightTrigger;
        input.Descend = pad.LeftTrigger;
        return input;
    }

    void SpectatorInput::ApplyView(bool replay) const
    {
        static_cast<void>(replay);
        if (NextPlayer)
        {
            SpectatorMode::CycleNext();
        }
        if (PreviousPlayer)
        {
            SpectatorMode::CyclePrevious();
        }
        if (ToggleView)
        {
            SpectatorMode::ToggleView();
        }
    }
}
