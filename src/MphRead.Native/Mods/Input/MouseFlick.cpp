#include "MouseFlick.hpp"

#include "../DebugLog.hpp"
#include "../InputSettings.hpp"
#include "../../NativeRuntime/System/Number.hpp"

#include <cmath>
#include <numbers>

namespace MphRead::Mods::Input
{
    void MouseFlick::Reset() noexcept
    {
        ClearSamples();
        _fed = false;
        _armed = false;
    }

    void MouseFlick::ClearSamples() noexcept
    {
        _count = 0;
        _newest = -1;
    }

    bool MouseFlick::Check(float deltaX, float deltaY, std::uint64_t frame, float& dirX, float& dirY)
    {
        dirX = 0;
        dirY = 0;
        if (!_fed || frame != _lastFrame + 1)
        {
            Reset();
        }
        _fed = true;
        _lastFrame = frame;
        _newest = (_newest + 1) % Burst;
        _deltaX[static_cast<std::size_t>(_newest)] = deltaX;
        _deltaY[static_cast<std::size_t>(_newest)] = deltaY;
        if (_count < Burst)
        {
            _count++;
        }
        const float sensitivity = InputSettings::MouseSensitivity();
        if (sensitivity <= 0)
        {
            return false;
        }
        const float rest = RestDegrees * 4 / sensitivity;
        const float threshold = TurnDegrees * 4 / sensitivity;
        float sumX = deltaX;
        float sumY = deltaY;
        float magnitude = std::sqrt(sumX * sumX + sumY * sumY);
        if (magnitude < rest)
        {
            _armed = true;
            return false;
        }
        float dirSumX = sumX * magnitude;
        float dirSumY = sumY * magnitude;
        if (!_armed || frame < _cooldownUntil)
        {
            return false;
        }
        for (std::int32_t i = 1; i < _count; i++)
        {
            const auto index = static_cast<std::size_t>((_newest - i + Burst) % Burst);
            const float olderX = _deltaX[index];
            const float olderY = _deltaY[index];
            const float older = std::sqrt(olderX * olderX + olderY * olderY);
            if (older < rest)
            {
                break;
            }
            if (olderX * sumX + olderY * sumY < Coherence * older * magnitude)
            {
                break;
            }
            sumX += olderX;
            sumY += olderY;
            dirSumX += olderX * older;
            dirSumY += olderY * older;
            magnitude = std::sqrt(sumX * sumX + sumY * sumY);
        }
        if (magnitude < threshold)
        {
            return false;
        }
        const float dirMag = std::sqrt(dirSumX * dirSumX + dirSumY * dirSumY);
        if (dirMag > 0)
        {
            dirX = dirSumX / dirMag;
            dirY = dirSumY / dirMag;
        }
        else
        {
            dirX = sumX / magnitude;
            dirY = sumY / magnitude;
        }
        _cooldownUntil = frame + Cooldown;
        _fired++;
        if (DebugLog::Active())
        {
            namespace Runtime = ::MphRead::NativeRuntime;
            DebugLog::Line("input", "mouse flick read as a boost: " + Runtime::ToString(magnitude, "0") + " px, "
                + Runtime::ToString(magnitude / 4 * sensitivity, "0") + " degrees of turn, direction "
                + "(" + Runtime::ToString(dirX, "0.00") + ", " + Runtime::ToString(dirY, "0.00") + "), "
                + Runtime::ToString(std::atan2(-dirY, dirX) * 180 / std::numbers::pi_v<float>, "0") + " deg "
                + "anticlockwise from screen right");
        }
        ClearSamples();
        _armed = false;
        return true;
    }
}
