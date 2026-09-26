#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace MphRead::Mods::Render
{
    // The launcher's moving backdrop: domain-warped value noise at a few
    // hundred cells across, refilled at most every Gap milliseconds.
    class NoiseField final
    {
    public:
        NoiseField();

        static constexpr std::int32_t Cell = 6;
        static constexpr std::int32_t MaxCells = 320;
        static constexpr double Gap = 33;

        [[nodiscard]] const std::vector<std::uint8_t>& Pixels() const noexcept { return _pixels; }
        [[nodiscard]] std::int32_t Width() const noexcept { return _width; }
        [[nodiscard]] std::int32_t Height() const noexcept { return _height; }
        [[nodiscard]] static std::int32_t CellsFor(double points) noexcept;
        bool Step(double windowWidth, double windowHeight, bool still = false);

    private:
        void Resize(std::int32_t w, std::int32_t h);
        [[nodiscard]] static double Smooth(double t) noexcept;
        [[nodiscard]] static double Noise(double x, double y) noexcept;
        void Fill(double seconds);

        static constexpr std::int32_t GridSize = 64;
        inline static std::array<float, GridSize * GridSize> _grid{};
        inline static bool _seeded = false;
        static constexpr std::array<double, 3> _hot{196, 96, 88};
        static constexpr std::array<double, 3> _cold{48, 112, 186};
        static constexpr std::array<double, 3> _floor{14, 20, 30};

        std::int64_t _clock;
        std::int32_t _width = 0;
        std::int32_t _height = 0;
        std::vector<std::uint8_t> _pixels{};
        std::vector<float> _fall{};
        double _filledAt = -std::numeric_limits<double>::infinity();
    };
}
