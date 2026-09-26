#include "StylusZone.hpp"

#include "PointerInput.hpp"
#include "../DebugLog.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Input
{
    const char* ToString(StylusRegion value) noexcept
    {
        switch (value)
        {
        case StylusRegion::None: return "None";
        case StylusRegion::Aim: return "Aim";
        case StylusRegion::PowerBeam: return "PowerBeam";
        case StylusRegion::Missile: return "Missile";
        case StylusRegion::Weapons: return "Weapons";
        case StylusRegion::WeaponSelect: return "WeaponSelect";
        case StylusRegion::AltForm: return "AltForm";
        }
        return "";
    }

    const std::array<StylusZone::Button, 5> StylusZone::Buttons{{
        {StylusRegion::PowerBeam, 26, 26, 22, "BEAM"},
        {StylusRegion::Missile, 80, 24, 20, "MSL"},
        {StylusRegion::Weapons, 150, 28, 30, "WPN"},
        {StylusRegion::WeaponSelect, 222, 28, 26, "SEL"},
        {StylusRegion::AltForm, 228, 166, 22, "ALT"}}};

    bool StylusZone::Enabled() noexcept
    {
        return _wanted && PointerInput::StylusMode();
    }

    void StylusZone::SetRect(float left, float top, float width) noexcept
    {
        _width = std::clamp(width, 0.10F, 1.0F);
        _left = std::clamp(left, 0.0F, 1 - _width);
        _top = std::clamp(top, 0.0F, std::max(0.0F, 1 - Height()));
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
        _placeAnchorX = std::clamp(x, 0.0F, 1.0F);
        _placeAnchorY = std::clamp(y, 0.0F, 1.0F);
        _placeAnchored = true;
    }

    void StylusZone::PlacementDrag(float x, float y) noexcept
    {
        if (!_placing || !_placeAnchored)
        {
            return;
        }
        x = std::clamp(x, 0.0F, 1.0F);
        y = std::clamp(y, 0.0F, 1.0F);
        _width = std::clamp(std::abs(x - _placeAnchorX), 0.10F, 1.0F);
        const float height = Height();
        const float left = std::min(_placeAnchorX, x);
        const float top = y >= _placeAnchorY ? _placeAnchorY : _placeAnchorY - height;
        _left = std::clamp(left, 0.0F, std::max(0.0F, 1 - _width));
        _top = std::clamp(top, 0.0F, std::max(0.0F, 1 - height));
    }

    void StylusZone::Nudge(float dx, float dy) noexcept
    {
        if (!_placing)
        {
            return;
        }
        _left = std::clamp(_left + dx, 0.0F, std::max(0.0F, 1 - _width));
        _top = std::clamp(_top + dy, 0.0F, std::max(0.0F, 1 - Height()));
    }

    void StylusZone::Resize(float by) noexcept
    {
        if (!_placing)
        {
            return;
        }
        SetRect(_left, _top, _width + by);
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
            Enabled(true);
        }
        _placeAnchored = false;
    }

    void StylusZone::CommitPlacement() noexcept
    {
        if (!_placing)
        {
            return;
        }
        _placing = false;
        _placeAnchored = false;
        Enabled(true);
    }

    StylusRegion StylusZone::TakePressed() noexcept
    {
        const StylusRegion pressed = _pressed;
        _pressed = StylusRegion::None;
        return pressed;
    }

    bool StylusZone::Aiming() noexcept
    {
        return Enabled() && !_placing && _contact && _held == StylusRegion::Aim && _aimReady;
    }

    bool StylusZone::MenuHeld() noexcept
    {
        return Enabled() && !_placing && _contact && _held == StylusRegion::WeaponSelect;
    }

    bool StylusZone::CapturingPointer() noexcept
    {
        return _placing || CapturingPrimaryButton();
    }

    bool StylusZone::CapturingPrimaryButton() noexcept
    {
        return Enabled() && _contact && _held != StylusRegion::None;
    }

    void StylusZone::LogTransitions()
    {
        if (DebugLog::Active())
        {
            if (_contact != _loggedContact)
            {
                DebugLog::Line("input", _contact ? std::string("stylus contact began region=") + ToString(_region)
                    : std::string("stylus contact ended"));
            }
            if (CapturingPointer() != _loggedCapture)
            {
                const auto b = [](bool value) { return value ? "True" : "False"; };
                DebugLog::Line("input", std::string("stylus primary button captured=") + b(CapturingPrimaryButton())
                    + " pointer=" + b(CapturingPointer()) + " contact=" + b(_contact) + " region=" + ToString(_region)
                    + " held=" + ToString(_held) + " aiming=" + b(Aiming())
                    + " jumps=" + std::to_string(PointerInput::JumpsIgnored()));
            }
        }
        _loggedContact = _contact;
        _loggedCapture = CapturingPointer();
    }

    void StylusZone::Update(float x, float y, bool contact)
    {
        if (_placing)
        {
            _region = StylusRegion::None;
            _held = StylusRegion::None;
            _pressed = StylusRegion::None;
            _contact = contact;
            _lastContact = contact;
            _aimReady = false;
            LogTransitions();
            return;
        }
        if (!Enabled())
        {
            _region = StylusRegion::None;
            _held = StylusRegion::None;
            _pressed = StylusRegion::None;
            _contact = false;
            _lastContact = false;
            _aimReady = false;
            LogTransitions();
            return;
        }
        const StylusRegion under = RegionAt(x, y);
        if (!contact)
        {
            _held = StylusRegion::None;
            _pressed = StylusRegion::None;
            _aimReady = false;
        }
        else if (!_lastContact)
        {
            _held = under;
            _aimReady = false;
            if (under != StylusRegion::None && under != StylusRegion::Aim && under != StylusRegion::WeaponSelect)
            {
                _pressed = under;
            }
        }
        else
        {
            if (_held != StylusRegion::None && _held != StylusRegion::Aim && _held != StylusRegion::WeaponSelect
                && under != _held && under != StylusRegion::None && under != StylusRegion::Aim)
            {
                _held = under;
                if (under != StylusRegion::WeaponSelect)
                {
                    _pressed = under;
                }
            }
            _aimReady = true;
        }
        _region = contact ? _held : under;
        _contact = contact;
        _lastContact = contact;
        LogTransitions();
    }

    StylusRegion StylusZone::RegionAt(float x, float y) noexcept
    {
        const float height = Height();
        if (height <= 0 || x < _left || x >= _left + _width || y < _top || y >= _top + height)
        {
            return StylusRegion::None;
        }
        const float dsX = (x - _left) / _width * DsWidth;
        const float dsY = (y - _top) / height * DsHeight;
        for (const Button& button : Buttons)
        {
            const float dx = dsX - button.X;
            const float dy = dsY - button.Y;
            if (dx * dx + dy * dy <= button.Radius * button.Radius)
            {
                return button.Region;
            }
        }
        return StylusRegion::Aim;
    }

    void StylusZone::Reset() noexcept
    {
        _region = StylusRegion::None;
        _held = StylusRegion::None;
        _pressed = StylusRegion::None;
        _contact = false;
        _lastContact = false;
        _aimReady = false;
        _placing = false;
        _placeAnchored = false;
        _loggedContact = false;
        _loggedCapture = false;
    }
}
