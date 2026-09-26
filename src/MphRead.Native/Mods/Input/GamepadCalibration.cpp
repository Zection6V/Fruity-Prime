#include "GamepadCalibration.hpp"

#include "GamepadAnalog.hpp"
#include "GamepadOptions.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/Number.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Input
{
    void GamepadCalibration::Sample(const GamepadState& state, bool resting)
    {
        std::vector<GamepadState>& samples = resting ? _rest : _range;
        if (samples.size() < 2048)
        {
            samples.push_back(state);
            _dirty = true;
        }
    }

    float GamepadCalibration::Percentile(const std::vector<GamepadState>& samples,
        const std::function<float(const GamepadState&)>& value, float quantile)
    {
        std::vector<float> values(samples.size());
        for (std::size_t i = 0; i < values.size(); i++)
        {
            values[i] = value(samples[i]);
        }
        // Array.Sort on float: NaN first, then ascending.
        std::sort(values.begin(), values.end(), [](float a, float b)
        {
            if (std::isnan(a))
            {
                return !std::isnan(b);
            }
            return !std::isnan(b) && a < b;
        });
        const auto last = static_cast<float>(values.size()) - 1;
        const auto index = static_cast<std::int32_t>(std::clamp(std::floor(last * quantile), 0.0F, last));
        return values.at(static_cast<std::size_t>(index));
    }

    bool GamepadCalibration::Measure(bool left, StickCalibration& calibration, float& dead)
    {
        const auto x = [left](const GamepadState& s) { return left ? s.LeftX : s.RightX; };
        const auto y = [left](const GamepadState& s) { return left ? s.LeftY : s.RightY; };
        const float cx = Percentile(_rest, x, .5F);
        const float cy = Percentile(_rest, y, .5F);
        const float radius = Percentile(_rest, [&](const GamepadState& s)
        {
            return std::sqrt(std::pow(x(s) - cx, 2.0F) + std::pow(y(s) - cy, 2.0F));
        }, .99F);
        calibration = StickCalibration{cx, cy, Percentile(_range, x, .02F), Percentile(_range, x, .98F),
            Percentile(_range, y, .02F), Percentile(_range, y, .98F)};
        dead = std::clamp(radius + .04F, .04F, .3F);
        return std::isfinite(radius) && radius < .25F && std::abs(cx) < .3F && std::abs(cy) < .3F
            && calibration.MinX < -.5F && calibration.MaxX > .5F && calibration.MinY < -.5F && calibration.MaxY > .5F;
    }

    void GamepadCalibration::Measure()
    {
        if (!_dirty)
        {
            return;
        }
        _dirty = false;
        _valid = false;
        if (_rest.size() < 10 || _range.size() < 10)
        {
            return;
        }
        const bool left = Measure(true, _left, _leftDead);
        const bool right = Measure(false, _right, _rightDead);
        _ltMin = Percentile(_rest, [](const GamepadState& s) { return s.LeftTrigger; }, .99F);
        _rtMin = Percentile(_rest, [](const GamepadState& s) { return s.RightTrigger; }, .99F);
        _ltMax = Percentile(_range, [](const GamepadState& s) { return s.LeftTrigger; }, .98F);
        _rtMax = Percentile(_range, [](const GamepadState& s) { return s.RightTrigger; }, .98F);
        _valid = left && right;
    }

    bool GamepadCalibration::Valid()
    {
        Measure();
        return _valid;
    }

    std::string GamepadCalibration::Summary()
    {
        return !Valid() ? "Insufficient range or movement during rest. Rotate both sticks in every direction and retry."
            : "Center offsets measured. Dead zones: left " + ::MphRead::NativeRuntime::ToString(_leftDead, "0.00")
                + ", right " + ::MphRead::NativeRuntime::ToString(_rightDead, "0.00")
                + ". Apply to use these measurements. Triggers without enough travel retain their current calibration.";
    }

    void GamepadCalibration::Apply()
    {
        if (!Valid())
        {
            throw System::InvalidOperationException("Calibration is incomplete.");
        }
        GamepadOptions::LeftCalibration(_left);
        GamepadOptions::RightCalibration(_right);
        GamepadOptions::LeftInner(_leftDead);
        GamepadOptions::RightInner(_rightDead);
        GamepadOptions::RightOuter(0);
        GamepadOptions::LeftOuter(GamepadOptions::RightOuter());
        if (_ltMax - _ltMin >= .4F)
        {
            GamepadOptions::LeftTriggerMin(_ltMin);
            GamepadOptions::LeftTriggerMax(_ltMax);
        }
        if (_rtMax - _rtMin >= .4F)
        {
            GamepadOptions::RightTriggerMin(_rtMin);
            GamepadOptions::RightTriggerMax(_rtMax);
        }
    }

    float GamepadCalibration::Trigger(float value, float min, float max) noexcept
    {
        return std::clamp((GamepadAnalog::Finite(value, 0, 1) - min) / std::max(.1F, max - min), 0.0F, 1.0F);
    }
}
