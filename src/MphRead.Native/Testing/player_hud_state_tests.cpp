// PlayerHud's whiteout and disruption state machines.
//
// The whiteout sweep follows a cubic in time rather than a linear ramp, and
// its last state hands the renderer a factor of -1 to mean "use the table
// value directly".  Both are easy to get subtly wrong and produce something
// that still looks like a flash, so they are pinned here.
#include "Entities/Players/player_hud_state.hpp"
#include <cstdio>
int main() {
    using fruityprime::players::PlayerHudState;
    PlayerHudState h;
    if (h.whiteout_state() != -1) { std::printf("not idle\n"); return 1; }
    h.begin_whiteout(0.0F);
    // State 0 ramps the factor in; 2.8125 * (1/60) per frame reaches 1 in ~22.
    int frames = 0;
    while (h.whiteout_state() == 0 && frames < 100) {
        h.update_whiteout_state(frames / 60.0F, 1.0F / 60.0F);
        ++frames;
    }
    const int ramp_frames = frames;
    // State 1 sweeps the band out to 96 and then hands over to the fade.
    while (h.whiteout_state() == 1 && frames < 2000) {
        h.update_whiteout_state(frames / 60.0F, 1.0F / 60.0F);
        ++frames;
    }
    const bool reached_fade = h.whiteout_state() == 2;
    h.update_whiteout_state(frames / 60.0F, 1.0F / 60.0F);
    const bool direct = h.whiteout_factor() == -1.0F;
    // Disruption: ramp in, hold, ramp out, back to idle.
    h.begin_disrupted();
    int guard = 0;
    while (h.disrupted_state() == 1 && guard++ < 100) { h.update_disrupted_state(); }
    const bool held = h.disrupted_state() == 2 && h.disruption_factor() == 1.0F;
    h.hud_end_disrupted();
    std::printf("native player HUD state: ramp_frames=%d fade=%d direct=%d held=%d ended=%d table=%zu\n",
        ramp_frames, reached_fade, direct, held, h.disrupted_state() == 0,
        h.whiteout_table().size());
    return (ramp_frames > 1 && reached_fade && direct && held
            && h.disrupted_state() == 0 && h.disruption_factor() == 0.0F) ? 0 : 1;
}
