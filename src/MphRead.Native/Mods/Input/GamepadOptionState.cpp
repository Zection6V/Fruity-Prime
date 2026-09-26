#include "GamepadOptionState.hpp"

#include "GamepadManager.hpp"
#include "PadBindings.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Number.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <string_view>

namespace MphRead::Mods::Input
{
    namespace Runtime = ::MphRead::NativeRuntime;

    GamepadOptionState::GamepadOptionState() : GlyphStyle(GamepadFamily::Unknown)
    {
    }

    void GamepadOptionState::SetWheelSlot(std::int32_t position, std::int32_t slot)
    {
        const auto found = std::find(_wheelOrder.begin(), _wheelOrder.end(), slot);
        const std::int32_t previous = found == _wheelOrder.end() ? -1 : static_cast<std::int32_t>(found - _wheelOrder.begin());
        if (position < 0 || position >= 6 || previous < 0)
        {
            return;
        }
        std::swap(_wheelOrder[static_cast<std::size_t>(position)], _wheelOrder[static_cast<std::size_t>(previous)]);
    }

    void GamepadOptionState::Load(const std::vector<std::string>& lines)
    {
        std::map<std::string, std::string, std::less<>> values;
        for (const std::string& line : lines)
        {
            const std::size_t split = line.find('=');
            if (split != std::string::npos && split > 0)
            {
                values[Runtime::StringTrim(line.substr(0, split))] = Runtime::StringTrim(line.substr(split + 1));
            }
        }
        const auto text = [&values](std::string_view key) -> const std::string*
        {
            const auto found = values.find(key);
            return found == values.end() ? nullptr : &found->second;
        };
        const auto number = [&text](const std::string& key, float fallback, float min, float max)
        {
            const std::string* value = text(key);
            float n = 0;
            return value != nullptr && Runtime::SingleTryParseInvariant(*value, n) && std::isfinite(n)
                ? std::clamp(n, min, max) : fallback;
        };
        const auto flag = [&text](std::string_view key, bool fallback)
        {
            const std::string* value = text(key);
            bool b = false;
            return value != nullptr && Runtime::BooleanTryParse(*value, b) ? b : fallback;
        };
        const auto calibration = [&number](const std::string& side)
        {
            return StickCalibration{
                number("gamepad_" + side + "_center_x", 0, -.3F, .3F), number("gamepad_" + side + "_center_y", 0, -.3F, .3F),
                number("gamepad_" + side + "_min_x", -1, -1, -.4F), number("gamepad_" + side + "_max_x", 1, .4F, 1),
                number("gamepad_" + side + "_min_y", -1, -1, -.4F), number("gamepad_" + side + "_max_y", 1, .4F, 1)};
        };
        LeftCalibration = calibration("left");
        RightCalibration = calibration("right");
        ScopedX = number("gamepad_scoped_x", 1, .1F, 3);
        ScopedY = number("gamepad_scoped_y", 1, .1F, 3);
        WheelThreshold = number("gamepad_wheel_threshold", .45F, .1F, .95F);
        WheelToggle = flag("gamepad_wheel_toggle", false);
        GamepadButtons modifier = GamepadButtons::None;
        const std::string* m = text("gamepad_binding_modifier");
        BindingModifier = m != nullptr && TryParse(*m, modifier) && PadBindings::Single(modifier)
            ? modifier : GamepadButtons::None;
        LeftTriggerMin = number("gamepad_lt_min", 0, 0, .8F);
        RightTriggerMin = number("gamepad_rt_min", 0, 0, .8F);
        LeftTriggerMax = number("gamepad_lt_max", 1, LeftTriggerMin + .1F, 1);
        RightTriggerMax = number("gamepad_rt_max", 1, RightTriggerMin + .1F, 1);
        for (std::int32_t i = 0; i < 6; i++)
        {
            _wheelOrder[static_cast<std::size_t>(i)] = i;
        }
        if (const std::string* order = text("gamepad_wheel_order"))
        {
            const std::vector<std::string> parts = Runtime::StringSplit(*order, ',');
            std::array<std::int32_t, 6> parsed{};
            std::int32_t used = 0;
            if (parts.size() == 6)
            {
                for (std::size_t i = 0; i < 6; i++)
                {
                    std::int32_t n = 0;
                    if (Runtime::Int32TryParseCurrentCulture(parts[i], n) && n >= 0 && n < 6 && (used & (1 << n)) == 0)
                    {
                        parsed[i] = n;
                        used |= 1 << n;
                    }
                    else
                    {
                        break;
                    }
                }
                if (used == 63)
                {
                    _wheelOrder = parsed;
                }
            }
        }
        const float legacyDead = number("gamepad_deadzone", 0.2F, 0, 0.9F);
        const float legacyLook = number("gamepad_look", 1, 0.1F, 5);
        LeftInner = number("gamepad_left_inner_deadzone", legacyDead, 0, 0.9F);
        RightInner = number("gamepad_right_inner_deadzone", legacyDead, 0, 0.9F);
        LeftOuter = number("gamepad_left_outer_deadzone", 0, 0, 0.5F);
        RightOuter = number("gamepad_right_outer_deadzone", 0, 0, 0.5F);
        LookX = number("gamepad_look_x", legacyLook, 0.1F, 5);
        LookY = number("gamepad_look_y", legacyLook, 0.1F, 5);
        TriggerThreshold = number("gamepad_trigger_threshold", 0.60F, 0.05F, 0.95F);
        ActivityThreshold = number("gamepad_activity_threshold", 0.35F, 0.2F, 0.95F);
        VibrationStrength = number("gamepad_vibration_strength", 0.65F, 0, 1);
        InvertX = flag("gamepad_invert_x", false);
        InvertY = flag("gamepad_invert_y", false);
        Southpaw = flag("gamepad_southpaw", false);
        Vibration = flag("gamepad_vibration", true);
        GamepadCurve curve = GamepadCurve::Classic;
        const std::string* c = text("gamepad_curve");
        Curve = c != nullptr && TryParse(*c, curve) && IsDefined(curve) ? curve : GamepadCurve::Classic;
        GamepadFamily glyph = GamepadFamily::Unknown;
        const std::string* g = text("gamepad_glyph_style");
        GlyphStyle = g != nullptr && TryParse(*g, glyph) && IsDefined(glyph) ? glyph : GamepadFamily::Unknown;
    }

    void GamepadOptionState::Write(std::vector<std::string>& lines) const
    {
        const auto number = [&lines](const std::string& key, float n)
        {
            lines.push_back(key + "=" + Runtime::ToStringInvariant(n));
        };
        const auto boolean = [](bool value) { return std::string(value ? "True" : "False"); };
        const auto calibration = [&number](const std::string& side, const StickCalibration& c)
        {
            number("gamepad_" + side + "_center_x", c.CenterX);
            number("gamepad_" + side + "_center_y", c.CenterY);
            number("gamepad_" + side + "_min_x", c.MinX);
            number("gamepad_" + side + "_max_x", c.MaxX);
            number("gamepad_" + side + "_min_y", c.MinY);
            number("gamepad_" + side + "_max_y", c.MaxY);
        };
        calibration("left", LeftCalibration);
        calibration("right", RightCalibration);
        number("gamepad_scoped_x", ScopedX);
        number("gamepad_scoped_y", ScopedY);
        number("gamepad_wheel_threshold", WheelThreshold);
        number("gamepad_lt_min", LeftTriggerMin);
        number("gamepad_lt_max", LeftTriggerMax);
        number("gamepad_rt_min", RightTriggerMin);
        number("gamepad_rt_max", RightTriggerMax);
        lines.push_back("gamepad_invert_y=" + boolean(InvertY));
        lines.push_back("gamepad_wheel_toggle=" + boolean(WheelToggle));
        lines.push_back("gamepad_binding_modifier=" + ToString(BindingModifier));
        std::string order;
        for (std::size_t i = 0; i < _wheelOrder.size(); i++)
        {
            order += (i == 0 ? "" : ",") + std::to_string(_wheelOrder[i]);
        }
        lines.push_back("gamepad_wheel_order=" + order);
        number("gamepad_left_inner_deadzone", LeftInner);
        number("gamepad_right_inner_deadzone", RightInner);
        number("gamepad_left_outer_deadzone", LeftOuter);
        number("gamepad_right_outer_deadzone", RightOuter);
        number("gamepad_look_x", LookX);
        number("gamepad_look_y", LookY);
        number("gamepad_trigger_threshold", TriggerThreshold);
        number("gamepad_activity_threshold", ActivityThreshold);
        number("gamepad_vibration_strength", VibrationStrength);
        lines.push_back("gamepad_invert_x=" + boolean(InvertX));
        lines.push_back("gamepad_southpaw=" + boolean(Southpaw));
        lines.push_back("gamepad_vibration=" + boolean(Vibration));
        lines.push_back("gamepad_curve=" + ToString(Curve));
        lines.push_back("gamepad_glyph_style=" + ToString(GlyphStyle));
    }

    std::shared_ptr<GamepadOptionState> GamepadOptionState::Clone() const
    {
        auto copy = std::make_shared<GamepadOptionState>();
        std::vector<std::string> lines;
        Write(lines);
        copy->Load(lines);
        return copy;
    }

    void GamepadOptionState::Reset()
    {
        Load({});
    }
}
