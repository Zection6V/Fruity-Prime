#pragma once

#include "../Formats/Types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace MphRead
{
    class Recolor;

    namespace Mods
    {
        class HunterSuits final
        {
        public:
            HunterSuits() = delete;
            HunterSuits(const HunterSuits&) = delete;
            HunterSuits& operator=(const HunterSuits&) = delete;

            [[nodiscard]] static ColorRgba Color(Hunter hunter, std::int32_t suit);
            [[nodiscard]] static std::shared_ptr<ManagedArray<ColorRgba>> Colors(Hunter hunter);
            [[nodiscard]] static std::string Name(ColorRgba color);

        private:
            static const ColorRgba _unknown;
            static std::unordered_map<Hunter, std::shared_ptr<ManagedArray<ColorRgba>>> _cache;

            [[nodiscard]] static ColorRgba Sample(const Recolor& recolor);
            [[nodiscard]] static ColorRgba Brighten(double red, double green, double blue);
        };
    }
}
