#include "StylusZone.hpp"

#include <bit>
#include <cmath>
#include <utility>

namespace
{
    float DotNetAbs(float value) noexcept
    {
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(value) & 0x7FFFFFFFU;
        return std::bit_cast<float>(bits);
    }

    float DotNetClamp(float value, float minimum, float maximum) noexcept
    {
        if (value < minimum)
        {
            return minimum;
        }
        if (value > maximum)
        {
            return maximum;
        }
        return value;
    }

    float DotNetMax(float val1, float val2) noexcept
    {
        if (val1 != val2)
        {
            if (!std::isnan(val1))
            {
                return val2 < val1 ? val1 : val2;
            }
            return val1;
        }
        return std::signbit(val2) ? val1 : val2;
    }

    float DotNetMin(float val1, float val2) noexcept
    {
        if (val1 != val2)
        {
            if (!std::isnan(val1))
            {
                return val1 < val2 ? val1 : val2;
            }
            return val1;
        }
        return std::signbit(val1) ? val1 : val2;
    }
}

namespace MphRead::Mods::Input
{
    StylusZone::Button::Button(StylusRegion region, float x, float y, float radius,
        std::optional<std::string> label)
        : Region(region), X(x), Y(y), Radius(radius), Label(std::move(label))
    {
    }

    StylusZone::Button StylusZone::_buttons[5] =
    {
        { StylusRegion::PowerBeam, 26.0F, 26.0F, 22.0F, std::string("BEAM") },
        { StylusRegion::Missile, 80.0F, 24.0F, 20.0F, std::string("MSL") },
        { StylusRegion::Weapons, 150.0F, 28.0F, 30.0F, std::string("WPN") },
        { StylusRegion::WeaponSelect, 222.0F, 28.0F, 26.0F, std::string("SEL") },
        { StylusRegion::AltForm, 228.0F, 166.0F, 22.0F, std::string("ALT") }
    };

    StylusZone::Button (&StylusZone::Buttons)[5] = StylusZone::_buttons;

    bool StylusZone::_enabled = false;
    float StylusZone::_left = 0.62F;
    float StylusZone::_top = 0.60F;
    float StylusZone::_width = 0.34F;
    float StylusZone::_aspectCorrection = 16.0F / 9.0F;
    float StylusZone::_opacity = 0.22F;

    bool StylusZone::_placing = false;
    float StylusZone::_placeAnchorX = 0.0F;
    float StylusZone::_placeAnchorY = 0.0F;
    bool StylusZone::_placeAnchored = false;

    StylusRegion StylusZone::_region = StylusRegion::None;
    bool StylusZone::_contact = false;
    StylusRegion StylusZone::_pressed = StylusRegion::None;
    StylusRegion StylusZone::_lastRegion = StylusRegion::None;
    bool StylusZone::_lastContact = false;

    bool StylusZone::Enabled() noexcept
    {
        return _enabled;
    }

    void StylusZone::Enabled(bool value) noexcept
    {
        _enabled = value;
    }

    float StylusZone::Left() noexcept
    {
        return _left;
    }

    float StylusZone::Top() noexcept
    {
        return _top;
    }

    float StylusZone::Width() noexcept
    {
        return _width;
    }

    float StylusZone::Height() noexcept
    {
        return Width() * (DsHeight / DsWidth) * AspectCorrection();
    }

    float StylusZone::AspectCorrection() noexcept
    {
        return _aspectCorrection;
    }

    void StylusZone::AspectCorrection(float value) noexcept
    {
        _aspectCorrection = value;
    }

    float StylusZone::Opacity() noexcept
    {
        return _opacity;
    }

    void StylusZone::Opacity(float value) noexcept
    {
        _opacity = value;
    }

    void StylusZone::SetRect(float left, float top, float width) noexcept
    {
        _width = DotNetClamp(width, 0.10F, 1.0F);
        _left = DotNetClamp(left, 0.0F, 1.0F - _width);
        _top = DotNetClamp(top, 0.0F, DotNetMax(0.0F, 1.0F - Height()));
    }

    bool StylusZone::Placing() noexcept
    {
        return _placing;
    }

    void StylusZone::BeginPlacement() noexcept
    {
        _placing = true;
        _placeAnchored = false;
    }

    void StylusZone::CancelPlacement() noexcept
    {
        _placing = false;
        _placeAnchored = false;
    }

    void StylusZone::PlacementDown(float x, float y) noexcept
    {
        if (!_placing)
        {
            return;
        }
        _placeAnchorX = DotNetClamp(x, 0.0F, 1.0F);
        _placeAnchorY = DotNetClamp(y, 0.0F, 1.0F);
        _placeAnchored = true;
    }

    void StylusZone::PlacementDrag(float x, float y) noexcept
    {
        if (!_placing || !_placeAnchored)
        {
            return;
        }
        const float width = DotNetAbs(DotNetClamp(x, 0.0F, 1.0F) - _placeAnchorX);
        SetRect(DotNetMin(_placeAnchorX, x), DotNetMin(_placeAnchorY, y), width);
    }

    void StylusZone::PlacementUp() noexcept
    {
        if (!_placing)
        {
            return;
        }
        _placing = false;
        if (_placeAnchored)
        {
            _enabled = true;
        }
        _placeAnchored = false;
    }

    StylusRegion StylusZone::Region() noexcept
    {
        return _region;
    }

    bool StylusZone::Contact() noexcept
    {
        return _contact;
    }

    StylusRegion StylusZone::Pressed() noexcept
    {
        return _pressed;
    }

    void StylusZone::Update(float x, float y, bool contact) noexcept
    {
        _pressed = StylusRegion::None;
        if (_placing)
        {
            _region = StylusRegion::None;
            _contact = contact;
            _lastContact = contact;
            return;
        }
        if (!_enabled)
        {
            _region = StylusRegion::None;
            _contact = false;
            _lastRegion = StylusRegion::None;
            _lastContact = false;
            return;
        }
        _region = RegionAt(x, y);
        _contact = contact;
        if (contact && _region != StylusRegion::Aim && _region != StylusRegion::None
            && (!_lastContact || _region != _lastRegion))
        {
            _pressed = _region;
        }
        _lastRegion = _region;
        _lastContact = contact;
    }

    StylusRegion StylusZone::RegionAt(float x, float y) noexcept
    {
        const float height = Height();
        if (height <= 0.0F || x < _left || x >= _left + _width
            || y < _top || y >= _top + height)
        {
            return StylusRegion::None;
        }
        const float dsX = (x - _left) / _width * DsWidth;
        const float dsY = (y - _top) / height * DsHeight;
        for (std::int32_t i = 0; i < 5; ++i)
        {
            const Button button = Buttons[i];
            const float dx = dsX - button.X;
            const float dy = dsY - button.Y;
            if (dx * dx + dy * dy <= button.Radius * button.Radius)
            {
                return button.Region;
            }
        }
        return StylusRegion::Aim;
    }

    bool StylusZone::OnButton() noexcept
    {
        return _enabled && !_placing
            && _region != StylusRegion::None && _region != StylusRegion::Aim;
    }

    void StylusZone::Reset() noexcept
    {
        _region = StylusRegion::None;
        _pressed = StylusRegion::None;
        _contact = false;
        _lastRegion = StylusRegion::None;
        _lastContact = false;
        _placing = false;
        _placeAnchored = false;
    }
}
