#include "GamepadUiRouter.hpp"

#include "AimInputSourceTracker.hpp"
#include "GamepadHaptics.hpp"

#include <cmath>

namespace MphRead::Mods::Input
{
    std::string ToString(UiAction value)
    {
        switch (value)
        {
        case UiAction::Up: return "Up";
        case UiAction::Down: return "Down";
        case UiAction::Left: return "Left";
        case UiAction::Right: return "Right";
        case UiAction::Accept: return "Accept";
        case UiAction::Back: return "Back";
        case UiAction::PreviousTab: return "PreviousTab";
        case UiAction::NextTab: return "NextTab";
        case UiAction::PageUp: return "PageUp";
        case UiAction::PageDown: return "PageDown";
        }
        return std::to_string(static_cast<std::int32_t>(value));
    }

    void GamepadContexts::MenuVisible(bool value)
    {
        if (_menu.load() != value)
        {
            _menu.store(value);
            _revision++;
            if (value)
            {
                GamepadHaptics::Stop();
            }
        }
    }

    void GamepadContexts::Capturing(bool value)
    {
        if (_capture.load() != value)
        {
            _capture.store(value);
            _revision++;
        }
    }

    void GamepadContexts::Focused(bool value)
    {
        if (_focused.load() == value)
        {
            return;
        }
        _focused.store(value);
        if (!value)
        {
            GamepadHaptics::Stop();
            AimInputSourceTracker::Reset();
        }
        _revision++;
    }

    GamepadContext GamepadContexts::Resolve(bool textEntry, bool results)
    {
        return Capturing() ? GamepadContext::BindingCapture : MenuVisible() ? GamepadContext::Menu
            : textEntry ? GamepadContext::TextEntry : results ? GamepadContext::Results : GamepadContext::Gameplay;
    }

    GamepadButtons GamepadEdges::Update(const GamepadSnapshot& snapshot)
    {
        const GamepadButtons pressed = snapshot.Revision == _revision
            ? snapshot.State.Buttons & ~_previous : GamepadButtons::None;
        _previous = snapshot.State.Buttons;
        _revision = snapshot.Revision;
        return pressed;
    }

    void GamepadUiRouter::Reset()
    {
        _direction = std::nullopt;
        _neutralRequired = true;
    }

    void GamepadUiRouter::Update(GamepadSnapshot snapshot, GamepadContext context, std::int64_t milliseconds)
    {
        GamepadState menuState = snapshot.State;
        const std::optional<GamepadDeviceSnapshot> device = GamepadManager::ActiveDevice();
        if (((device.has_value() ? device->Capabilities : GamepadCapabilities::None)
            & GamepadCapabilities::AnalogTriggers) != GamepadCapabilities::None)
        {
            _leftTrigger = menuState.LeftTrigger >= (_leftTrigger ? .30F : .45F);
            _rightTrigger = menuState.RightTrigger >= (_rightTrigger ? .30F : .45F);
            menuState.Buttons &= ~(GamepadButtons::LeftTrigger | GamepadButtons::RightTrigger);
            if (_leftTrigger)
            {
                menuState.Buttons |= GamepadButtons::LeftTrigger;
            }
            if (_rightTrigger)
            {
                menuState.Buttons |= GamepadButtons::RightTrigger;
            }
            snapshot.State = menuState;
        }
        GamepadButtons pressed = _edges.Update(snapshot);
        const std::int64_t contextRevision = GamepadContexts::Revision();
        const bool changed = _revision != snapshot.Revision || _context != context || _contextRevision != contextRevision;
        _revision = snapshot.Revision;
        _context = context;
        _contextRevision = contextRevision;
        if (changed)
        {
            Reset();
            pressed = GamepadButtons::None;
        }
        if (context != GamepadContext::Menu && context != GamepadContext::Results && context != GamepadContext::TextEntry)
        {
            return;
        }
        const GamepadState& state = snapshot.State;
        if (!state.Connected)
        {
            Reset();
            return;
        }
        if (_neutralRequired)
        {
            if (state.Buttons != GamepadButtons::None || std::abs(state.LeftX) > .35F || std::abs(state.LeftY) > .35F)
            {
                return;
            }
            _neutralRequired = false;
        }
        std::optional<UiAction> direction;
        if (state.Down(GamepadButtons::DpadUp) || state.LeftY > .55F)
        {
            direction = UiAction::Up;
        }
        else if (state.Down(GamepadButtons::DpadDown) || state.LeftY < -.55F)
        {
            direction = UiAction::Down;
        }
        else if (state.Down(GamepadButtons::DpadLeft) || state.LeftX < -.55F)
        {
            direction = UiAction::Left;
        }
        else if (state.Down(GamepadButtons::DpadRight) || state.LeftX > .55F)
        {
            direction = UiAction::Right;
        }
        if (direction != _direction)
        {
            _direction = direction;
            _started = milliseconds;
            _next = milliseconds + 300;
            if (direction.has_value())
            {
                Action.Invoke(*direction);
            }
        }
        else if (direction.has_value() && milliseconds >= _next)
        {
            _next = milliseconds + (milliseconds - _started >= 1500 ? 55 : 90);
            Action.Invoke(*direction);
        }
        if (Any(pressed & GamepadButtons::A))
        {
            Action.Invoke(UiAction::Accept);
        }
        else if (Any(pressed & (GamepadButtons::B | GamepadButtons::Start)))
        {
            Action.Invoke(UiAction::Back);
        }
        else if (Any(pressed & GamepadButtons::LeftBumper))
        {
            Action.Invoke(UiAction::PreviousTab);
        }
        else if (Any(pressed & GamepadButtons::RightBumper))
        {
            Action.Invoke(UiAction::NextTab);
        }
        else if (Any(pressed & GamepadButtons::LeftTrigger))
        {
            Action.Invoke(UiAction::PageUp);
        }
        else if (Any(pressed & GamepadButtons::RightTrigger))
        {
            Action.Invoke(UiAction::PageDown);
        }
    }
}
