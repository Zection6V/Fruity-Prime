// PlayerEntity's per-process statics and the PlayerValues table they read.
//
// The pickup spheres are placed by their edges, not their centres, so a
// transliteration error anywhere in the 104-field table shows up here as a
// reach that no longer matches the cartridge's own -0.5 and 1.1.
//
// MainPlayerIndex resetting is the other check: nothing used to move it back,
// so a client that joined on slot 1 kept pointing at slot 1 and the next
// offline match made Main a bot.
#include "Entities/Players/player_statics.hpp"
#include "Metadata/player_values.hpp"
#include <cmath>
#include <cstdio>
int main() {
    using namespace fruityprime;
    players::PlayerStatics s;
    s.generate_player_volumes();
    using V = players::PlayerStatics::Volume;
    const auto& samus = metadata::PlayerValuesTable[0];
    const auto& low = s.player_volume(0, V::PickupLow);
    const auto& high = s.player_volume(0, V::PickupHigh);
    const auto& alt = s.player_volume(0, V::AltForm);
    const float min_h = samus.MinPickupHeight / 4096.0F;
    const float max_h = samus.MaxPickupHeight / 4096.0F;
    // The bottom of the low sphere is the minimum pickup height; the top of
    // the high sphere is the maximum.
    const float low_bottom = low.SpherePosition.y - low.SphereRadius;
    const float high_top = high.SpherePosition.y + high.SphereRadius;
    // The slot cap must not silently reset when references are cleared.
    s.set_main_player_index(1);
    const bool created = s.create_player(4) && s.create_player(4);
    s.reset_references();
    std::printf("native player statics: low_bottom=%.4f min=%.4f high_top=%.4f max=%.4f alt_r=%.3f "
                "created=%d reset_main=%d reset_created=%d hunters=%zu\n",
        low_bottom, min_h, high_top, max_h, alt.SphereRadius, created,
        s.main_player_index(), s.players_created(),
        metadata::PlayerValuesTable.size());
    return (std::fabs(low_bottom - min_h) < 1e-4F
            && std::fabs(high_top - max_h) < 1e-4F
            && alt.SphereRadius > 0.0F && created
            && s.main_player_index() == 0 && s.players_created() == 0
            && metadata::PlayerValuesTable.size() == 8) ? 0 : 1;
}
