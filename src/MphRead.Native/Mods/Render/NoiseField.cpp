#include "NoiseField.hpp"

#include "../../NativeRuntime/System/Random.hpp"
#include "../../NativeRuntime/System/Stopwatch.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Render
{
    NoiseField::NoiseField() : _clock(::MphRead::NativeRuntime::StopwatchGetTimestamp())
    {
    }

    std::int32_t NoiseField::CellsFor(double points) noexcept
    {
        return std::clamp(static_cast<std::int32_t>(points / Cell), 32, MaxCells);
    }

    bool NoiseField::Step(double windowWidth, double windowHeight, bool still)
    {
        if (windowWidth <= 0 || windowHeight <= 0)
        {
            return false;
        }
        Resize(CellsFor(windowWidth), CellsFor(windowHeight));
        if (_pixels.empty())
        {
            return false;
        }
        const double now = still ? 0
            : static_cast<double>(::MphRead::NativeRuntime::StopwatchGetElapsedTicks(_clock)) / 10000.0;
        if (_filledAt > -std::numeric_limits<double>::infinity() && std::abs(now - _filledAt) < Gap)
        {
            return true;
        }
        _filledAt = now;
        Fill(now / 1000.0);
        return true;
    }

    void NoiseField::Resize(std::int32_t w, std::int32_t h)
    {
        if (!_seeded)
        {
            ::MphRead::NativeRuntime::Random random(0x46505250);
            for (float& value : _grid)
            {
                value = static_cast<float>(random.NextDouble());
            }
            _seeded = true;
        }
        if (w == _width && h == _height && !_pixels.empty())
        {
            return;
        }
        _width = w;
        _height = h;
        _pixels.assign(static_cast<std::size_t>(w * h * 3), 0);
        _fall.assign(static_cast<std::size_t>(w * h), 0);
        for (std::int32_t y = 0; y < h; y++)
        {
            for (std::int32_t x = 0; x < w; x++)
            {
                const double dx = (x / static_cast<double>(w) - 0.5) * 2;
                const double dy = (y / static_cast<double>(h) - 0.5) * 2;
                const double d = std::min(1.0, std::sqrt(dx * dx * 0.78 + dy * dy) / 1.18);
                _fall[static_cast<std::size_t>(y * w + x)] = static_cast<float>(1 - d * d * 0.45);
            }
        }
        _filledAt = -std::numeric_limits<double>::infinity();
    }

    double NoiseField::Smooth(double t) noexcept
    {
        return t * t * (3 - 2 * t);
    }

    double NoiseField::Noise(double x, double y) noexcept
    {
        const auto xi = static_cast<std::int32_t>(std::floor(x));
        const auto yi = static_cast<std::int32_t>(std::floor(y));
        const double xf = Smooth(x - xi);
        const double yf = Smooth(y - yi);
        const std::int32_t y0 = (yi & 63) * GridSize;
        const std::int32_t y1 = ((yi + 1) & 63) * GridSize;
        const std::int32_t x0 = xi & 63;
        const std::int32_t x1 = (xi + 1) & 63;
        const double a = _grid[static_cast<std::size_t>(y0 + x0)];
        const double b = _grid[static_cast<std::size_t>(y0 + x1)];
        const double c = _grid[static_cast<std::size_t>(y1 + x0)];
        const double d = _grid[static_cast<std::size_t>(y1 + x1)];
        const double t = a + (b - a) * xf;
        return t + ((c + (d - c) * xf) - t) * yf;
    }

    void NoiseField::Fill(double seconds)
    {
        const double time = seconds * 0.26;
        const std::int32_t w = _width;
        const std::int32_t h = _height;
        std::size_t p = 0;
        std::size_t q = 0;
        for (std::int32_t y = 0; y < h; y++)
        {
            const double v = y * 0.055;
            for (std::int32_t x = 0; x < w; x++)
            {
                const double u = x * 0.055;
                const double wx = Noise(u + time * 2.0, v) * 4.4;
                const double wy = Noise(u, v - time * 1.6) * 4.4;
                double n = Noise(u + wx, v + wy);
                n = n * n * (3 - 2 * n);
                const double shade = (0.18 + n * 0.95) * _fall[q++];
                for (std::size_t c = 0; c < 3; c++)
                {
                    _pixels[p++] = static_cast<std::uint8_t>(std::clamp(
                        _floor[c] + (_hot[c] * n + _cold[c] * (1 - n)) * shade, 0.0, 255.0));
                }
            }
        }
    }
}
