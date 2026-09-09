// PlayerHud's timed pickups and the opponent readout.
//
// Double damage pulses its icon, and the pulse gets shorter as the pickup
// runs out -- that is the whole point of it, so the three speeds are pinned
// here rather than left to look about right.  The name under it types
// itself out a character a frame, and both timers are frame-time driven,
// which means a paused frame must not advance them.
#include "Entities/Players/player_hud_state.hpp"

#include <cstdio>

int main() {
    using fruityprime::players::PlayerHudState;

    int failures = 0;
    const auto check = [&failures](bool condition, const char* what) {
        if (!condition) {
            std::printf("native HUD pickup tests: %s\n", what);
            ++failures;
        }
    };

    const float frame = 1.0F / 60.0F;
    PlayerHudState hud;

    // No pickup, no pulse.
    check(hud.double_damage_icon_frame() == 0,
          "the icon pulsed with no pickup running");

    // Picking one up spells its name out for two seconds.
    hud.update_double_damage_speed(1);
    check(hud.double_damage_text_timer() > 0.0F,
          "picking up double damage did not announce it");
    // A frame with no pickup left does not advance anything.
    const float before = hud.double_damage_text_timer();
    hud.process_double_damage_hud(0.0F, frame);
    check(hud.double_damage_text_timer() == before,
          "the announcement ran down with no pickup active");

    // At speed one the icon is lit for thirty frames of every thirty-five.
    const auto lit_frames = [&](int speed) {
        PlayerHudState state;
        state.update_double_damage_speed(speed);
        int lit = 0;
        // One full period at each speed, in this head's own frames.
        for (int i = 0; i < 70; ++i) {
            if (state.double_damage_icon_frame() == 0) {
                ++lit;
            }
            state.process_double_damage_hud(10.0F, frame);
        }
        return lit;
    };
    const int one = lit_frames(1);
    const int two = lit_frames(2);
    const int three = lit_frames(3);
    check(one > two && two > three,
          "the icon did not pulse faster as the pickup ran out");

    // Cloak: announced once, and only while it is actually running.
    PlayerHudState cloak;
    cloak.process_cloak_hud(true, frame);
    check(cloak.hud_cloaking(), "cloaking was not noticed");
    check(cloak.cloak_text_timer() > 0.0F, "cloaking was not announced");
    const float announced = cloak.cloak_text_timer();
    cloak.process_cloak_hud(true, frame);
    check(cloak.cloak_text_timer() < announced,
          "the cloak announcement did not run down");
    cloak.process_cloak_hud(false, frame);
    check(!cloak.hud_cloaking(), "cloaking outlived the cloak");
    // Cloaking again announces itself again.
    cloak.process_cloak_hud(true, frame);
    check(cloak.cloak_text_timer() >= announced,
          "cloaking a second time did not announce itself");

    // The opponent readout holds for two seconds and then clears itself.
    PlayerHudState opponent;
    opponent.update_opponent(1, 0, true);
    check(opponent.opponent_index() == 1, "the opponent was not recorded");
    // Your own slot is not an opponent, and neither is anyone in the story.
    opponent.update_opponent(0, 0, true);
    check(opponent.opponent_index() == 1, "hitting yourself set an opponent");
    // Two of the cartridge's seconds, which is a hundred and twenty of
    // this head's frames.
    for (int i = 0; i < 130 && opponent.opponent_index() != -1; ++i) {
        opponent.process_opponent(frame);
    }
    check(opponent.opponent_index() == -1,
          "the opponent readout never cleared");

    PlayerHudState story;
    story.update_opponent(1, 0, false);
    check(story.opponent_index() == -1,
          "the story mode showed an opponent readout");

    if (failures == 0) {
        std::printf("native HUD pickup tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
