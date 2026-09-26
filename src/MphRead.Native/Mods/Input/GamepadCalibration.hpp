#pragma once

#include "GamepadState.hpp"
#include "StickCalibration.hpp"

#include <functional>
#include <string>
#include <vector>

namespace MphRead::Mods::Input
{
    class GamepadCalibration final
    {
    public:
        void Sample(const GamepadState& state, bool resting);
        [[nodiscard]] bool Valid();
        [[nodiscard]] std::string Summary();
        void Apply();
        [[nodiscard]] static float Trigger(float value, float min, float max) noexcept;

    private:
        [[nodiscard]] static float Percentile(const std::vector<GamepadState>& samples,
            const std::function<float(const GamepadState&)>& value, float quantile);
        [[nodiscard]] bool Measure(bool left, StickCalibration& calibration, float& dead);
        void Measure();

        std::vector<GamepadState> _rest{};
        std::vector<GamepadState> _range{};
        bool _dirty = true;
        bool _valid = false;
        StickCalibration _left{};
        StickCalibration _right{};
        float _leftDead = 0;
        float _rightDead = 0;
        float _ltMin = 0;
        float _rtMin = 0;
        float _ltMax = 0;
        float _rtMax = 0;
    };
}
