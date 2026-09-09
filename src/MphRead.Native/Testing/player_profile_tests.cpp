#include "../Entities/Players/player_profile.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using fruityprime::metadata::Hunter;
    using fruityprime::players::AvailableArray;
    using fruityprime::players::ProfileCount;
    using fruityprime::players::profile;
    using fruityprime::players::profiles;

    assert(profiles().size() == ProfileCount);
    assert(profiles()[0].hunter == Hunter::Samus);
    assert(profiles()[7].hunter == Hunter::Guardian);
    assert(profiles()[0].walk_speed_cap == 983);
    assert(profiles()[2].alt_collision_y == 3072);
    assert(profiles()[6].alt_collision_radius == 1638);
    assert(profiles()[7].normal_fov == 159744);
    assert(profiles()[3].alt_form_strafe == 1);
    assert(profiles()[0].alt_form_strafe == 0);
    assert(profiles()[4].alt_attack_damage == 42);
    assert(profile(255).hunter == Hunter::Guardian);
    assert(std::fabs(profile(0).fixed_to_float(4096) - 1.0F) < 0.0001F);
    assert(profile(2).alt_speed_scale() > profile(6).alt_speed_scale());

    AvailableArray available;
    available.set(0b01010101);
    assert(available[0] && !available[1] && available[2] && !available[3]);
    assert(available[4] && !available[5] && available[6] && !available[7]);
    assert(!available[8]);
    available.set_all();
    assert(available[8]);
    AvailableArray copy;
    copy.copy_from(available);
    assert(copy[0] && copy[8]);
    copy.clear_all();
    assert(!copy[0] && !copy[8]);

    std::cout << "native player profile tests passed\n";
    return 0;
}
