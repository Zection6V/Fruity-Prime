#include "PointerDevice.hpp"

#include "PointerInput.hpp"
#include "StylusZone.hpp"
#include "../../Entities/Players/PlayerInput.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"

#include <algorithm>

namespace MphRead::Mods::Input
{
    void PointerDevice::Update(const PointerSample& sample, std::int32_t width, std::int32_t height,
        bool independentPrimaryDown, bool acceptsInput)
    {
        const bool wasActive = _active && _acceptingInput;
        _active = PointerInput::StylusMode() && !::MphRead::NativeRuntime::IsAndroid();
        _acceptingInput = acceptsInput;
        const PointerSample previous = _current;
        _current = sample;
        StylusZone::AspectCorrection(static_cast<float>(width) / static_cast<float>(std::max(height, 1)));
        if (sample.Device != previous.Device || sample.Id != previous.Id)
        {
            StylusZone::Update(0, 0, false);
        }
        StylusZone::Update(sample.X / static_cast<float>(std::max(width, 1)), sample.Y / static_cast<float>(std::max(height, 1)),
            _active && acceptsInput && sample.InContact);
        _primaryDown = acceptsInput && ResolvePrimary(sample.PrimaryDown, independentPrimaryDown,
            StylusZone::CapturingPrimaryButton() || StylusZone::Placing());
        if (!_active || !acceptsInput || !wasActive || sample.Device != previous.Device || sample.Id != previous.Id)
        {
            _pendingX = _pendingY = 0;
            return;
        }
        if (StylusZone::Placing() || (StylusZone::Enabled() && !StylusZone::Aiming()))
        {
            _pendingX = _pendingY = 0;
            return;
        }
        if (sample.Device == PointerDeviceType::Pen && sample.InContact != previous.InContact)
        {
            _pendingX = _pendingY = 0;
            return;
        }
        const auto [x, y] = PointerInput::Filter(sample.X - previous.X, sample.Y - previous.Y);
        _pendingX += x;
        _pendingY += y;
    }

    bool PointerDevice::ResolvePrimary(bool tipDown, bool independentDown, bool captured) noexcept
    {
        return independentDown || (tipDown && !captured);
    }

    std::pair<float, float> PointerDevice::TakeDelta() noexcept
    {
        const std::pair<float, float> result{_pendingX, _pendingY};
        _pendingX = _pendingY = 0;
        return result;
    }

    void PointerDevice::Reset() noexcept
    {
        _active = false;
        _acceptingInput = false;
        _current = {};
        _primaryDown = false;
        _pendingX = _pendingY = 0;
        StylusZone::Reset();
    }

    void PointerBindings::Update(bool rawDown, bool captured, bool independentDown) noexcept
    {
        _previousDown = _down;
        _down = PointerDevice::ResolvePrimary(rawDown, independentDown, captured);
    }

    bool PointerBindings::Resolve(Entities::Keybind& control) const
    {
        if (control.Type() != Entities::ButtonType::Mouse
            || control.MouseButton() != ::OpenTK::Windowing::GraphicsLibraryFramework::MouseButton::Left)
        {
            return false;
        }
        control.SetIsDown(_down);
        control.SetIsPressed(_down && !_previousDown);
        control.SetIsReleased(!_down && _previousDown);
        return true;
    }
}
