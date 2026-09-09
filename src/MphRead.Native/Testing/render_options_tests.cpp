#include "Mods/render_options.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main() {
    fruityprime::mods::render::Options options;
    options.set_resolution_scale(10);
    assert(options.resolution_scale() == 25);
    options.set_resolution_scale(75);
    assert(options.scaled(101) == 75 && options.scaled(0) == 1
           && options.scaled(-100) == 1);
    options.set_resolution_scale(100);
    assert(options.scaled(101) == 101);

    options.set_cel_bands(1);
    assert(options.cel_bands() == 2);
    options.set_cel_bands(99);
    assert(options.cel_bands() == 8);
    options.set_cel_edge(-1.0F);
    assert(options.cel_edge() == 0.0F);
    options.set_cel_edge(std::numeric_limits<float>::quiet_NaN());
    assert(std::isnan(options.cel_edge()));

    assert(fruityprime::mods::render::Options::parse_on_off(" YES ", false));
    assert(!fruityprime::mods::render::Options::parse_on_off("off", true));
    assert(fruityprime::mods::render::Options::parse_on_off("unknown", true));
    assert(fruityprime::mods::render::Options::parse_scale("75%", 100) == 75);
    assert(fruityprime::mods::render::Options::parse_scale("+75%%", 100) == 75);
    assert(fruityprime::mods::render::Options::parse_int(" 12 ", 0) == 12);
    assert(fruityprime::mods::render::Options::on_off(false) == "off");

    fruityprime::mods::RenderOptions::SetResolutionScale(50);
    fruityprime::mods::RenderOptions::SetLighting(false);
    fruityprime::mods::RenderOptions::SetCelBands(3);
    assert(fruityprime::mods::RenderOptions::ResolutionScale() == 50);
    assert(!fruityprime::mods::RenderOptions::Lighting());
    assert(fruityprime::mods::RenderOptions::CelBands() == 3);
    assert(fruityprime::mods::RenderOptions::Scaled(101) == 50);
    assert(fruityprime::mods::RenderOptions::ParseInt("+12%%", 0) == 12);

    std::cout << "native render options tests passed\n";
    return 0;
}
