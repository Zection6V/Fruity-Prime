#include "HunterSuits.hpp"

#include "../Formats/Model.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Read.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace MphRead::Mods
{
    namespace
    {
        constexpr std::int32_t PlayerColorCount = 4;
    }

    const ColorRgba HunterSuits::_unknown(150, 150, 155, 255);

    std::unordered_map<Hunter, std::shared_ptr<ManagedArray<ColorRgba>>> HunterSuits::_cache{};

    ColorRgba HunterSuits::Color(Hunter hunter, std::int32_t suit)
    {
        const std::shared_ptr<ManagedArray<ColorRgba>> colors = Colors(hunter);
        if (suit < 0 || static_cast<std::size_t>(suit) >= colors->Length())
        {
            return _unknown;
        }
        return (*colors)[static_cast<std::size_t>(suit)];
    }

    std::shared_ptr<ManagedArray<ColorRgba>> HunterSuits::Colors(Hunter hunter)
    {
        const auto cached = _cache.find(hunter);
        if (cached != _cache.end())
        {
            return cached->second;
        }

        auto colors = std::make_shared<ManagedArray<ColorRgba>>(PlayerColorCount);
        for (std::size_t i = 0; i < colors->Length(); ++i)
        {
            (*colors)[i] = _unknown;
        }

        try
        {
            const auto models = Metadata::HunterModels.find(hunter);
            if (models != Metadata::HunterModels.end() && !models->second.empty())
            {
                const std::shared_ptr<ModelInstance> instance = Read::GetModelInstance(models->second[0]);
                if (!instance)
                {
                    throw System::NullReferenceException();
                }
                const std::shared_ptr<Model> model = instance->Model();
                if (!model || !model->Recolors)
                {
                    throw System::NullReferenceException();
                }

                for (std::int32_t i = 0;
                    static_cast<std::size_t>(i) < colors->Length()
                        && static_cast<std::size_t>(i) < model->Recolors->size();
                    ++i)
                {
                    const std::shared_ptr<Recolor>& recolor
                        = model->Recolors->at(static_cast<std::size_t>(i));
                    if (!recolor)
                    {
                        throw System::NullReferenceException();
                    }
                    (*colors)[static_cast<std::size_t>(i)] = Sample(*recolor);
                }
            }
        }
        catch (...)
        {
            // A suit swatch is not worth a match. The neutral color is
            // already in place.
        }

        _cache[hunter] = colors;
        return colors;
    }

    ColorRgba HunterSuits::Sample(const Recolor& recolor)
    {
        double red = 0.0;
        double green = 0.0;
        double blue = 0.0;
        double weight = 0.0;

        if (!recolor.PaletteData)
        {
            throw System::NullReferenceException();
        }

        for (std::int32_t p = 0;
            static_cast<std::size_t>(p) < recolor.PaletteData->size();
            ++p)
        {
            const std::vector<ColorRgba> pixels = recolor.GetPalettePixels(p);
            for (std::int32_t i = 0;
                static_cast<std::size_t>(i) < pixels.size();
                ++i)
            {
                const ColorRgba color = pixels[static_cast<std::size_t>(i)];
                const std::int32_t max = std::max(
                    static_cast<std::int32_t>(color.Red),
                    std::max(
                        static_cast<std::int32_t>(color.Green),
                        static_cast<std::int32_t>(color.Blue)));
                const std::int32_t min = std::min(
                    static_cast<std::int32_t>(color.Red),
                    std::min(
                        static_cast<std::int32_t>(color.Green),
                        static_cast<std::int32_t>(color.Blue)));
                if (max == 0)
                {
                    continue;
                }

                const double saturation
                    = static_cast<double>(max - min) / static_cast<double>(max);
                const double w
                    = saturation * saturation * (static_cast<double>(max) / 255.0);
                if (w <= 0.0)
                {
                    continue;
                }

                red += static_cast<double>(color.Red) * w;
                green += static_cast<double>(color.Green) * w;
                blue += static_cast<double>(color.Blue) * w;
                weight += w;
            }
        }

        if (weight <= 0.0)
        {
            return _unknown;
        }
        return Brighten(red / weight, green / weight, blue / weight);
    }

    ColorRgba HunterSuits::Brighten(double red, double green, double blue)
    {
        const double max = std::max(red, std::max(green, blue));
        if (max <= 0.0)
        {
            return _unknown;
        }

        const double gain = std::min(235.0 / max, 2.2);
        return ColorRgba(
            static_cast<std::uint8_t>(std::clamp(red * gain, 0.0, 255.0)),
            static_cast<std::uint8_t>(std::clamp(green * gain, 0.0, 255.0)),
            static_cast<std::uint8_t>(std::clamp(blue * gain, 0.0, 255.0)),
            255);
    }

    std::string HunterSuits::Name(ColorRgba color)
    {
        const double r = static_cast<double>(color.Red) / 255.0;
        const double g = static_cast<double>(color.Green) / 255.0;
        const double b = static_cast<double>(color.Blue) / 255.0;
        const double max = std::max(r, std::max(g, b));
        const double min = std::min(r, std::min(g, b));
        const double delta = max - min;
        if (max <= 0.001 || delta / max < 0.18)
        {
            return max > 0.7 ? "WHITE" : max > 0.3 ? "GREY" : "BLACK";
        }

        double hue;
        if (max == r)
        {
            hue = 60.0 * std::fmod((g - b) / delta, 6.0);
        }
        else if (max == g)
        {
            hue = 60.0 * ((b - r) / delta + 2.0);
        }
        else
        {
            hue = 60.0 * ((r - g) / delta + 4.0);
        }

        if (hue < 0.0)
        {
            hue += 360.0;
        }
        if (hue < 15.0 || hue >= 330.0)
        {
            return "RED";
        }
        if (hue < 45.0)
        {
            return "ORANGE";
        }
        if (hue < 70.0)
        {
            return "YELLOW";
        }
        if (hue < 160.0)
        {
            return "GREEN";
        }
        if (hue < 200.0)
        {
            return "CYAN";
        }
        if (hue < 260.0)
        {
            return "BLUE";
        }
        if (hue < 300.0)
        {
            return "PURPLE";
        }
        return "PINK";
    }
}
