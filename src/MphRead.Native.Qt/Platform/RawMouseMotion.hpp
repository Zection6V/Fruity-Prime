#pragma once

#include <cstdint>
#include <limits>
#include <utility>

namespace MphRead::Qt
{
    // GUI-thread owned. Counts are device units, never logical/physical pixels.
    class RawMouseMotion final
    {
    public:
        void SetCapture(bool enabled) noexcept
        {
            if (_capture != enabled) Discard();
            _capture = enabled;
        }
        [[nodiscard]] bool CaptureActive() const noexcept { return _capture; }
        void Add(std::int32_t x, std::int32_t y, bool absolute) noexcept
        {
            if (!_capture || absolute) return;
            _x = AddClamped(_x, x);
            _y = AddClamped(_y, y);
        }
        [[nodiscard]] std::pair<std::int64_t, std::int64_t> TakeDelta() noexcept
        {
            const auto result = std::pair{_x, _y};
            Discard();
            return result;
        }
        void Discard() noexcept { _x = _y = 0; }
        [[nodiscard]] static std::int64_t AddClamped(std::int64_t value, std::int64_t delta) noexcept
        {
            constexpr auto max = std::numeric_limits<std::int64_t>::max();
            constexpr auto min = std::numeric_limits<std::int64_t>::min();
            if (delta > 0 && value > max - delta) return max;
            if (delta < 0 && value < min - delta) return min;
            return value + delta;
        }
    private:
        bool _capture = false;
        std::int64_t _x = 0, _y = 0;
    };
}
