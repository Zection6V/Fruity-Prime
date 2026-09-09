#include "Testing/test_effects.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

int main() {
    using namespace fruityprime::testing::effects;

    assert(fx_div(4096, 2) == 8388608);
    assert(fx_div(-4096, 2) == -8388608);
    bool rejected = false;
    try {
        (void)fx_div(1, 0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    constexpr std::array<std::int32_t, 6> parameters{
        0, 0, 4096, 4096, std::numeric_limits<std::int32_t>::min(), 0};
    assert(interpolate_fx41(parameters, -1) == 0);
    assert(interpolate_fx41(parameters, 2048) == 2048);
    assert(interpolate_fx41(parameters, 4096) == 4096);
    assert(interpolate_fx41(parameters, 8192) == 4096);

    const AngleLookup angles = effect_angle_lookup(0x80000U);
    assert(angles.index1 % 2 == 0);
    assert(angles.index3 % 2 == 0);
    assert(angles.index2 == angles.index1 + 1);
    assert(angles.index4 == angles.index3 + 1);
    assert(angles.index3 == angles.index1 + 2048);
    assert(angles.angle1 >= 0.0F && angles.angle1 < 360.0F);
    assert(angles.angle2 >= 0.0F && angles.angle2 < 360.0F);
    assert(std::fabs((angles.angle1 + 90.0F >= 360.0F
                         ? angles.angle1 + 90.0F - 360.0F
                         : angles.angle1 + 90.0F) - angles.angle2) < 0.001F);
    return 0;
}
