#include "TouchControls.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace
{
    std::int64_t TickCount64() noexcept
    {
        using Clock = std::chrono::steady_clock;
        using Milliseconds = std::chrono::milliseconds;
        const auto count = std::chrono::duration_cast<Milliseconds>(
            Clock::now().time_since_epoch()
        ).count();
        return static_cast<std::int64_t>(count);
    }

    std::int64_t UncheckedSubtract(std::int64_t left, std::int64_t right) noexcept
    {
        const std::uint64_t bits
            = static_cast<std::uint64_t>(left) - static_cast<std::uint64_t>(right);
        return std::bit_cast<std::int64_t>(bits);
    }
}

namespace MphRead::Droid
{
    TouchButton::TouchButton(TouchAction action, std::string label)
        : _action(action),
          _label(std::move(label)),
          _defaultLabel(_label)
    {
    }

    TouchAction TouchButton::Action() const noexcept
    {
        return _action;
    }

    std::string TouchButton::Label() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _label;
    }

    void TouchButton::Relabel(std::optional<std::string_view> label)
    {
        std::lock_guard<std::mutex> guard(_lock);
        _label = label.has_value() ? std::string(*label) : _defaultLabel;
    }

    float TouchButton::CentreX() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _centreX;
    }

    void TouchButton::CentreX(float value)
    {
        std::lock_guard<std::mutex> guard(_lock);
        _centreX = value;
    }

    float TouchButton::CentreY() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _centreY;
    }

    void TouchButton::CentreY(float value)
    {
        std::lock_guard<std::mutex> guard(_lock);
        _centreY = value;
    }

    float TouchButton::Radius() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _radius;
    }

    void TouchButton::Radius(float value)
    {
        std::lock_guard<std::mutex> guard(_lock);
        _radius = value;
    }

    bool TouchButton::Visible() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _visible;
    }

    void TouchButton::Visible(bool value)
    {
        std::lock_guard<std::mutex> guard(_lock);
        _visible = value;
    }

    bool TouchButton::Contains(float x, float y) const
    {
        std::lock_guard<std::mutex> guard(_lock);
        const float dx = x - _centreX;
        const float dy = y - _centreY;
        const float reach = _radius * 1.15F;
        return dx * dx + dy * dy <= reach * reach;
    }

    TouchControls::TouchControls()
    {
        _buttons.reserve(11);

        auto scan = std::make_shared<TouchButton>(TouchAction::Scan, "SCAN");
        scan->Visible(false);
        _buttons.push_back(std::move(scan));

        _buttons.push_back(std::make_shared<TouchButton>(TouchAction::Shoot, "FIRE"));
        _buttons.push_back(std::make_shared<TouchButton>(TouchAction::Jump, "JUMP"));
        _buttons.push_back(std::make_shared<TouchButton>(TouchAction::Morph, "MORPH"));
        _buttons.push_back(std::make_shared<TouchButton>(TouchAction::ScanVisor, "VISOR"));
        _buttons.push_back(std::make_shared<TouchButton>(TouchAction::Missile, "MSSL"));
        _buttons.push_back(std::make_shared<TouchButton>(TouchAction::WeaponMenu, "WEAPON"));
        _buttons.push_back(std::make_shared<TouchButton>(TouchAction::Zoom, "ZOOM"));
        _buttons.push_back(std::make_shared<TouchButton>(TouchAction::Pause, "MENU"));
        _buttons.push_back(std::make_shared<TouchButton>(TouchAction::Scoreboard, "SCORE"));

        auto chat = std::make_shared<TouchButton>(TouchAction::Chat, "CHAT");
        chat->Visible(false);
        _buttons.push_back(std::move(chat));
    }

    const std::vector<std::shared_ptr<TouchButton>>& TouchControls::Buttons() const noexcept
    {
        return _buttons;
    }

    float TouchControls::Width() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _width;
    }

    float TouchControls::Height() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _height;
    }

    float TouchControls::Density() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _density;
    }

    bool TouchControls::StickActive() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _stickActive;
    }

    float TouchControls::StickX() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _stickX;
    }

    float TouchControls::StickY() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _stickY;
    }

    float TouchControls::StickKnobX() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _stickKnobX;
    }

    float TouchControls::StickKnobY() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _stickKnobY;
    }

    float TouchControls::StickRadius() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _stickRadius;
    }

    float TouchControls::StickKnobRadius() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _stickKnobRadius;
    }

    bool TouchControls::SwipeBoostEnabled() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _swipeBoostEnabled;
    }

    void TouchControls::SwipeBoostEnabled(bool value)
    {
        std::lock_guard<std::mutex> guard(_lock);
        _swipeBoostEnabled = value;
    }

    bool TouchControls::ScanVisorActive() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _scanVisorActive;
    }

    void TouchControls::ScanVisorActive(bool value)
    {
        Change([this, value]()
        {
            if (_scanVisorActive == value)
            {
                return false;
            }
            _scanVisorActive = value;
            return true;
        });
    }

    bool TouchControls::ChatEnabled() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _chatEnabled;
    }

    void TouchControls::ChatEnabled(bool value)
    {
        Change([this, value]()
        {
            if (_chatEnabled == value)
            {
                return false;
            }
            _chatEnabled = value;
            return true;
        });
    }

    void TouchControls::SetSpectator(bool spectating, bool freeCamera)
    {
        Change([this, spectating, freeCamera]()
        {
            if (_spectating == spectating && _spectatorFreeCam == freeCamera)
            {
                return false;
            }
            _spectating = spectating;
            _spectatorFreeCam = freeCamera;
            return true;
        });
    }

    void TouchControls::SetEndScreen(bool active)
    {
        Change([this, active]()
        {
            if (_endScreen == active)
            {
                return false;
            }
            _endScreen = active;
            if (!active)
            {
                _tapPending = false;
            }
            return true;
        });
    }

    void TouchControls::SetTapTargets(std::shared_ptr<std::vector<float>> targets)
    {
        std::lock_guard<std::mutex> guard(_lock);
        _tapTargets = std::move(targets);
    }

    void TouchControls::NoteTapLocked(float x, float y) noexcept
    {
        _tapPending = true;
        _tapX = x / std::max(_width, 1.0F);
        _tapY = y / std::max(_height, 1.0F);
    }

    TouchControls::TapResult TouchControls::TakeTap()
    {
        std::lock_guard<std::mutex> guard(_lock);
        if (!_tapPending)
        {
            return { false, 0.0F, 0.0F };
        }
        _tapPending = false;
        return { true, _tapX, _tapY };
    }

    bool TouchControls::TapHitsTargetLocked(float x, float y) const
    {
        if (_width <= 0.0F || _height <= 0.0F)
        {
            return false;
        }

        if (_tapTargets == nullptr)
        {
            throw std::runtime_error("Object reference not set to an instance of an object.");
        }

        const float fx = x / _width;
        const float fy = y / _height;
        const std::vector<float>& targets = *_tapTargets;
        for (std::size_t i = 0; i + 3 < targets.size(); i += 4)
        {
            if (fx >= targets[i] && fx < targets[i + 2]
                && fy >= targets[i + 1] && fy < targets[i + 3])
            {
                return true;
            }
        }
        return false;
    }

    bool TouchControls::TapIsForHudLocked(float x, float y) const noexcept
    {
        static_cast<void>(x);
        static_cast<void>(y);
        if (_width <= 0.0F || _height <= 0.0F)
        {
            return false;
        }
        return _endScreen;
    }

    void TouchControls::Change(const std::function<bool()>& mutate)
    {
        {
            std::lock_guard<std::mutex> guard(_lock);
            if (!mutate())
            {
                return;
            }
            ApplyLayoutLocked();
        }
        InvokeInvalidated();
    }

    void TouchControls::ApplyLayoutLocked()
    {
        for (const std::shared_ptr<TouchButton>& button : _buttons)
        {
            const TouchAction action = button->Action();
            bool visible = false;
            std::optional<std::string_view> label;

            if (_endScreen)
            {
                visible = action == TouchAction::Pause
                    || action == TouchAction::Scoreboard
                    || (action == TouchAction::Chat && _chatEnabled);
            }
            else if (_spectating)
            {
                switch (action)
                {
                case TouchAction::Shoot:
                    visible = true;
                    label = "NEXT";
                    break;
                case TouchAction::ScanVisor:
                    visible = true;
                    label = "VIEW";
                    break;
                case TouchAction::Jump:
                case TouchAction::Morph:
                    visible = _spectatorFreeCam;
                    label = action == TouchAction::Jump ? "UP" : "DOWN";
                    break;
                case TouchAction::Scoreboard:
                case TouchAction::Pause:
                    visible = true;
                    break;
                case TouchAction::Chat:
                    visible = _chatEnabled;
                    break;
                default:
                    visible = false;
                    break;
                }
            }
            else
            {
                switch (action)
                {
                case TouchAction::Scan:
                    visible = _scanVisorActive;
                    break;
                case TouchAction::Shoot:
                    visible = !_scanVisorActive;
                    break;
                case TouchAction::Chat:
                    visible = _chatEnabled;
                    break;
                default:
                    visible = true;
                    break;
                }
            }

            button->Visible(
                visible && Mods::Input::TouchSettings::Shown(SettingOf(action))
            );
            button->Relabel(label);
        }
    }

    Mods::Input::TouchControl TouchControls::SettingOf(TouchAction action) noexcept
    {
        using Mods::Input::TouchControl;
        switch (action)
        {
        case TouchAction::Shoot:
            return TouchControl::Shoot;
        case TouchAction::Jump:
            return TouchControl::Jump;
        case TouchAction::Morph:
            return TouchControl::Morph;
        case TouchAction::ScanVisor:
            return TouchControl::ScanVisor;
        case TouchAction::Scan:
            return TouchControl::Scan;
        case TouchAction::Missile:
            return TouchControl::Missile;
        case TouchAction::WeaponMenu:
            return TouchControl::WeaponMenu;
        case TouchAction::Zoom:
            return TouchControl::Zoom;
        case TouchAction::Pause:
            return TouchControl::Pause;
        case TouchAction::Scoreboard:
            return TouchControl::Scoreboard;
        default:
            return TouchControl::Chat;
        }
    }

    void TouchControls::ReloadSettings()
    {
        {
            std::lock_guard<std::mutex> guard(_lock);
            ApplyLayoutLocked();
        }
        InvokeInvalidated();
    }

    std::function<void()> TouchControls::Invalidated() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _invalidated;
    }

    void TouchControls::Invalidated(std::function<void()> value)
    {
        std::lock_guard<std::mutex> guard(_lock);
        _invalidated = std::move(value);
    }

    bool TouchControls::PadDriving() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return HiddenLocked();
    }

    bool TouchControls::HiddenLocked() const noexcept
    {
        return _padDriving && !_forceVisible;
    }

    bool TouchControls::ForceVisible() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _forceVisible;
    }

    void TouchControls::ForceVisible(bool value)
    {
        bool before;
        bool after;
        {
            std::lock_guard<std::mutex> guard(_lock);
            before = HiddenLocked();
            _forceVisible = value;
            after = HiddenLocked();
        }
        Settle(before, after);
    }

    void TouchControls::NotePadActivity()
    {
        bool before;
        bool after;
        {
            std::lock_guard<std::mutex> guard(_lock);
            before = HiddenLocked();
            _padDriving = true;
            after = HiddenLocked();
        }
        Settle(before, after);
    }

    void TouchControls::Settle(bool wasHidden, bool isHidden)
    {
        if (wasHidden == isHidden)
        {
            return;
        }
        InvokeInvalidated();
    }

    void TouchControls::InvokeInvalidated()
    {
        std::function<void()> invalidated;
        {
            std::lock_guard<std::mutex> guard(_lock);
            invalidated = _invalidated;
        }
        if (invalidated)
        {
            invalidated();
        }
    }

    void TouchControls::Layout(float width, float height, float density)
    {
        std::lock_guard<std::mutex> guard(_lock);
        _width = width;
        _height = height;
        _density = density <= 0.0F ? 1.0F : density;

        const float h = height;
        _stickRadius = 0.15F * h;
        _stickKnobRadius = 0.06F * h;

        Place(TouchAction::Shoot, width - 0.17F * h, h - 0.19F * h, 0.105F * h);
        Place(TouchAction::Scan, width - 0.17F * h, h - 0.19F * h, 0.105F * h);
        Place(TouchAction::Jump, width - 0.40F * h, h - 0.15F * h, 0.085F * h);
        Place(TouchAction::Morph, width - 0.15F * h, h - 0.47F * h, 0.080F * h);
        Place(TouchAction::ScanVisor, width - 0.38F * h, h - 0.42F * h, 0.075F * h);
        Place(TouchAction::Missile, width - 0.62F * h, h - 0.28F * h, 0.075F * h);
        Place(TouchAction::WeaponMenu, width - 0.12F * h, 0.15F * h, 0.075F * h);
        Place(TouchAction::Zoom, width - 0.33F * h, 0.12F * h, 0.065F * h);
        Place(TouchAction::Pause, 0.11F * h, 0.12F * h, 0.060F * h);
        Place(TouchAction::Scoreboard, 0.28F * h, 0.12F * h, 0.060F * h);
        Place(TouchAction::Chat, 0.45F * h, 0.12F * h, 0.060F * h);
    }

    void TouchControls::Place(TouchAction action, float x, float y, float radius)
    {
        for (const std::shared_ptr<TouchButton>& button : _buttons)
        {
            if (button->Action() == action)
            {
                button->CentreX(x);
                button->CentreY(y);
                button->Radius(radius);
                return;
            }
        }
    }

    bool TouchControls::IsHeld(TouchAction action) const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _held.find(action) != _held.end();
    }

    TouchControls::AimDelta TouchControls::TakeAimDelta()
    {
        std::lock_guard<std::mutex> guard(_lock);
        const float x = _aimDeltaX;
        const float y = _aimDeltaY;
        _aimDeltaX = 0.0F;
        _aimDeltaY = 0.0F;
        return { x / _density, y / _density };
    }

    TouchControls::SwipeBoostResult TouchControls::TakeSwipeBoost()
    {
        std::lock_guard<std::mutex> guard(_lock);
        const bool pending = _swipeBoostPending;
        _swipeBoostPending = false;
        return { pending, _swipeBoostX, _swipeBoostY };
    }

    bool TouchControls::TakeDoubleTapJump()
    {
        std::lock_guard<std::mutex> guard(_lock);
        const bool pending = _doubleTapJumpPending;
        _doubleTapJumpPending = false;
        return pending;
    }

    bool TouchControls::PointerIsAbsolute() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _pointerIsAbsolute;
    }

    void TouchControls::PointerIsAbsolute(bool value)
    {
        std::lock_guard<std::mutex> guard(_lock);
        _pointerIsAbsolute = value;
    }

    TouchControls::PositionResult TouchControls::AimPosition() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return { _aimDown, _aimAbsX, _aimAbsY };
    }

    TouchControls::PositionResult TouchControls::WeaponWheelPosition() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        if (_aimDown)
        {
            return { true, _aimAbsX, _aimAbsY };
        }
        if (_wheelPointer != -1)
        {
            return { true, _wheelX, _wheelY };
        }
        return { false, _aimAbsX, _aimAbsY };
    }

    TouchControls::Dir TouchControls::Direction() const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _direction;
    }

    void TouchControls::PointerDown(std::int32_t pointerId, float x, float y)
    {
        bool revealed = false;
        {
            std::lock_guard<std::mutex> guard(_lock);
            revealed = HiddenLocked();
            _padDriving = false;
            PointerDownLocked(pointerId, x, y);
        }
        if (revealed)
        {
            InvokeInvalidated();
        }
    }

    void TouchControls::PointerDownLocked(std::int32_t pointerId, float x, float y)
    {
        if (TapHitsTargetLocked(x, y))
        {
            NoteTapLocked(x, y);
            return;
        }

        for (const std::shared_ptr<TouchButton>& button : _buttons)
        {
            if (button->Visible() && button->Contains(x, y))
            {
                const TouchAction action = button->Action();
                _buttonPointers[pointerId] = action;
                _held.insert(action);

                if (action == TouchAction::Shoot || action == TouchAction::Scan)
                {
                    _fireAimPointer = pointerId;
                    _fireAimLastX = x;
                    _fireAimLastY = y;
                    _fireAimSwipe.Reset();
                }
                else if (action == TouchAction::WeaponMenu)
                {
                    _wheelPointer = pointerId;
                    _wheelX = x;
                    _wheelY = y;
                }
                return;
            }
        }

        if (TapIsForHudLocked(x, y))
        {
            NoteTapLocked(x, y);
            return;
        }

        if (!_pointerIsAbsolute && x < _width / 2.0F && _stickPointer == -1)
        {
            _stickPointer = pointerId;
            _stickActive = true;
            _stickX = x;
            _stickY = y;
            _stickKnobX = x;
            _stickKnobY = y;
            _direction = Dir::None;
            return;
        }

        if (_aimPointer == -1)
        {
            _aimPointer = pointerId;
            _aimLastX = x;
            _aimLastY = y;
            _aimAbsX = x;
            _aimAbsY = y;
            _aimDown = true;
            _aimSwipe.Reset();
            _tapDownTime = TickCount64();
            _tapDownX = x;
            _tapDownY = y;
            _tapMoved = false;
        }
    }

    void TouchControls::SwipeTracker::Reset() noexcept
    {
        _count = 0;
        _newest = -1;
    }

    void TouchControls::SwipeTracker::Add(
        float x,
        float y,
        std::int64_t now
    ) noexcept
    {
        _newest = (_newest + 1) % Capacity;
        _x[static_cast<std::size_t>(_newest)] = x;
        _y[static_cast<std::size_t>(_newest)] = y;
        _time[static_cast<std::size_t>(_newest)] = now;
        if (_count < Capacity)
        {
            ++_count;
        }
    }

    TouchControls::DisplacementResult TouchControls::SwipeTracker::Displacement(
        std::int64_t now,
        std::int64_t windowMs
    ) const
    {
        if (_count < 2)
        {
            return { 0.0F, 0.0F, 0.0F };
        }

        const std::size_t newest = static_cast<std::size_t>(_newest);
        const float newestX = _x[newest];
        const float newestY = _y[newest];
        float best = 0.0F;
        float bestX = 0.0F;
        float bestY = 0.0F;

        for (std::int32_t i = 1; i < _count; ++i)
        {
            const std::int32_t indexValue = (_newest - i + Capacity) % Capacity;
            const std::size_t index = static_cast<std::size_t>(indexValue);
            if (i > 1 && UncheckedSubtract(now, _time[index]) > windowMs)
            {
                break;
            }

            const float dx = newestX - _x[index];
            const float dy = newestY - _y[index];
            const float distance = dx * dx + dy * dy;
            if (distance > best)
            {
                best = distance;
                bestX = dx;
                bestY = dy;
            }
        }

        return { std::sqrt(best), bestX, bestY };
    }

    void TouchControls::CheckSwipeBoost(SwipeTracker& tracker, float x, float y)
    {
        const std::int64_t now = TickCount64();
        tracker.Add(x, y, now);

        if (!_swipeBoostEnabled
            || UncheckedSubtract(now, _lastSwipeBoostTime) < SwipeBoostCooldownMs)
        {
            return;
        }

        const float threshold = SwipeBoostDistanceDp * _density;
        const DisplacementResult displacement
            = tracker.Displacement(now, SwipeBoostWindowMs);

        if (displacement.Distance > threshold)
        {
            _swipeBoostPending = true;
            _swipeBoostX = displacement.X / displacement.Distance;
            _swipeBoostY = displacement.Y / displacement.Distance;
            _lastSwipeBoostTime = now;
            tracker.Reset();
            _aimDeltaX = 0.0F;
            _aimDeltaY = 0.0F;
        }
    }

    void TouchControls::PointerMove(std::int32_t pointerId, float x, float y)
    {
        std::lock_guard<std::mutex> guard(_lock);

        if (pointerId == _stickPointer)
        {
            float dx = x - _stickX;
            float dy = y - _stickY;
            float length = std::sqrt(dx * dx + dy * dy);
            if (length > _stickRadius)
            {
                _stickX += dx * (1.0F - _stickRadius / length);
                _stickY += dy * (1.0F - _stickRadius / length);
                dx = x - _stickX;
                dy = y - _stickY;
                length = _stickRadius;
            }

            _stickKnobX = x;
            _stickKnobY = y;
            _direction = Dir::None;

            const float deadzone = _stickRadius * 0.28F;
            if (length > deadzone)
            {
                float angle = std::atan2(-dy, dx)
                    * (180.0F / std::numbers::pi_v<float>);
                if (angle < 0.0F)
                {
                    angle += 360.0F;
                }
                if (angle > 22.5F && angle < 157.5F)
                {
                    _direction |= Dir::Up;
                }
                if (angle > 202.5F && angle < 337.5F)
                {
                    _direction |= Dir::Down;
                }
                if (angle > 112.5F && angle < 247.5F)
                {
                    _direction |= Dir::Left;
                }
                if (angle < 67.5F || angle > 292.5F)
                {
                    _direction |= Dir::Right;
                }
            }
            return;
        }

        if (pointerId == _aimPointer)
        {
            CheckSwipeBoost(_aimSwipe, x, y);
            if (!_tapMoved)
            {
                const float tapDx = x - _tapDownX;
                const float tapDy = y - _tapDownY;
                const float slop = TapSlopDp * _density;
                _tapMoved = tapDx * tapDx + tapDy * tapDy > slop * slop;
            }
            _aimDeltaX += x - _aimLastX;
            _aimDeltaY += y - _aimLastY;
            _aimLastX = x;
            _aimLastY = y;
            _aimAbsX = x;
            _aimAbsY = y;
            return;
        }

        if (pointerId == _fireAimPointer)
        {
            CheckSwipeBoost(_fireAimSwipe, x, y);
            _aimDeltaX += x - _fireAimLastX;
            _aimDeltaY += y - _fireAimLastY;
            _fireAimLastX = x;
            _fireAimLastY = y;
            return;
        }

        if (pointerId == _wheelPointer)
        {
            _wheelX = x;
            _wheelY = y;
            return;
        }

        const auto found = _buttonPointers.find(pointerId);
        if (found != _buttonPointers.end())
        {
            const TouchAction action = found->second;
            for (const std::shared_ptr<TouchButton>& button : _buttons)
            {
                if (button->Action() == action)
                {
                    if (!button->Contains(x, y))
                    {
                        _buttonPointers.erase(pointerId);
                        ReleaseAction(action);
                    }
                    return;
                }
            }
        }
    }

    void TouchControls::PointerUp(std::int32_t pointerId)
    {
        std::lock_guard<std::mutex> guard(_lock);

        if (pointerId == _stickPointer)
        {
            _stickPointer = -1;
            _stickActive = false;
            _direction = Dir::None;
            return;
        }

        if (pointerId == _aimPointer)
        {
            _aimPointer = -1;
            _aimDown = false;
            _aimSwipe.Reset();

            const std::int64_t up = TickCount64();
            if (!_tapMoved
                && UncheckedSubtract(up, _tapDownTime) <= TapMaxMs)
            {
                const float spread = DoubleTapSpreadDp * _density;
                const float sinceX = _tapDownX - _lastTapX;
                const float sinceY = _tapDownY - _lastTapY;
                if (UncheckedSubtract(up, _lastTapTime) <= DoubleTapGapMs
                    && sinceX * sinceX + sinceY * sinceY <= spread * spread)
                {
                    _doubleTapJumpPending = true;
                    _lastTapTime = 0;
                }
                else
                {
                    _lastTapTime = up;
                    _lastTapX = _tapDownX;
                    _lastTapY = _tapDownY;
                }
            }
            return;
        }

        if (pointerId == _fireAimPointer)
        {
            _fireAimPointer = -1;
            _fireAimSwipe.Reset();
        }

        if (pointerId == _wheelPointer)
        {
            _wheelPointer = -1;
        }

        const auto found = _buttonPointers.find(pointerId);
        if (found != _buttonPointers.end())
        {
            const TouchAction action = found->second;
            _buttonPointers.erase(found);
            ReleaseAction(action);
        }
    }

    void TouchControls::ReleaseEverything()
    {
        std::lock_guard<std::mutex> guard(_lock);

        _buttonPointers.clear();
        _held.clear();
        _stickPointer = -1;
        _aimPointer = -1;
        _aimDown = false;
        _fireAimPointer = -1;
        _wheelPointer = -1;
        _swipeBoostPending = false;
        _doubleTapJumpPending = false;
        _lastTapTime = 0;
        _aimSwipe.Reset();
        _fireAimSwipe.Reset();
        _stickActive = false;
        _direction = Dir::None;
        _aimDeltaX = 0.0F;
        _aimDeltaY = 0.0F;
    }

    void TouchControls::ReleaseAction(TouchAction action)
    {
        for (const auto& pair : _buttonPointers)
        {
            if (pair.second == action)
            {
                return;
            }
        }
        _held.erase(action);
    }
}
