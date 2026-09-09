// PlayerHud's weapon wheel, and the two readouts that move with the ball.
//
// The wheel is six wedges around a hub near the top right of the touch
// screen, chosen by the slope of the offset from the hub -- so the wedges
// are equal in angle rather than in area, and an off-by-one in the
// boundaries hands the player the wrong gun under fire. That is worth
// pinning, and so is the rule that a beam you do not have is simply not
// selectable rather than selecting its neighbour.
#include "Entities/Players/player_hud_state.hpp"

#include <array>
#include <cmath>
#include <cstdio>

int main() {
    using fruityprime::players::PlayerHudState;

    int failures = 0;
    const auto check = [&failures](bool condition, const char* what) {
        if (!condition) {
            std::printf("native HUD wheel tests: %s\n", what);
            ++failures;
        }
    };

    constexpr float width = 256.0F;
    constexpr float height = 192.0F;
    const std::array<bool, 6> all{{true, true, true, true, true, true}};
    const std::array<bool, 6> none{{false, false, false, false, false, false}};

    const auto pick = [&](float x, float y, const std::array<bool, 6>& have) {
        return PlayerHudState::weapon_wheel_selection(
            x, y, width, height, std::span<const bool, 6>(have));
    };

    // The hub sits at 224, 38, and the wheel occupies the quadrant below
    // and left of it -- strictly, so a pointer exactly below or exactly
    // left of the hub selects nothing at all.  Just inside that boundary
    // is the first wedge at one end and the last at the other.
    check(pick(224.0F, 130.0F, all) == -1,
          "exactly below the hub selected a weapon");
    check(pick(220.0F, 130.0F, all) == 0,
          "just inside the bottom edge is not the first wedge");
    check(pick(120.0F, 42.0F, all) == 5,
          "just inside the left edge is not the last wedge");

    // Walking a quarter circle from below to left passes through every
    // wedge in order and never goes backwards.
    int previous = -1;
    int seen = 0;
    for (int step = 1; step < 90; ++step) {
        const float radians = static_cast<float>(step) * 3.14159265F / 180.0F;
        const float x = 224.0F - 80.0F * std::sin(radians);
        const float y = 38.0F + 80.0F * std::cos(radians);
        const int wedge = pick(x, y, all);
        if (wedge < 0) {
            continue;
        }
        check(wedge >= previous, "the wheel went backwards round the arc");
        if (wedge != previous) {
            ++seen;
            previous = wedge;
        }
    }
    check(seen == 6, "the arc did not pass through all six wedges");

    // The hub itself selects nothing, and so does anywhere above or right
    // of it -- the wheel only occupies one quadrant.
    check(pick(224.0F, 38.0F, all) == -1, "the hub selected a weapon");
    check(pick(224.0F, 45.0F, all) == -1,
          "inside the hub's dead zone selected a weapon");
    check(pick(240.0F, 130.0F, all) == -1,
          "right of the hub selected a weapon");
    check(pick(120.0F, 20.0F, all) == -1,
          "above the hub selected a weapon");

    // A beam the player does not have is not selectable, and does not hand
    // them the one beside it instead.
    check(pick(224.0F, 130.0F, none) == -1,
          "a weapon the player does not have was selected");

    // The click happens on a change, not on every frame.
    PlayerHudState hud;
    check(hud.note_weapon_selection(2), "the first selection was silent");
    check(!hud.note_weapon_selection(2), "holding on one wedge kept clicking");
    check(hud.note_weapon_selection(-1), "letting go was silent");

    // The bomb and boost row slides up as the player morphs and back down
    // as they stand up, and stops at each end.
    PlayerHudState bombs;
    const float standing = bombs.boost_bombs_y_offset();
    for (int i = 0; i < 200; ++i) {
        bombs.update_boost_bombs(true, false);
    }
    const float morphed = bombs.boost_bombs_y_offset();
    check(morphed < standing, "the bomb row did not slide up");
    bombs.update_boost_bombs(true, false);
    check(bombs.boost_bombs_y_offset() == morphed,
          "the bomb row slid past its resting place");
    for (int i = 0; i < 200; ++i) {
        bombs.update_boost_bombs(false, false);
    }
    check(bombs.boost_bombs_y_offset() == standing,
          "the bomb row did not slide back off the bottom");

    // A damage arrow blinks while its timer runs and goes out when it ends.
    PlayerHudState damage;
    damage.set_damage_indicator(3, 40);
    bool blinked_on = false;
    bool blinked_off = false;
    for (int i = 0; i < 40; ++i) {
        damage.update_damage_indicators();
        if (damage.damage_indicator_shown(3)) {
            blinked_on = true;
        } else {
            blinked_off = true;
        }
    }
    check(blinked_on && blinked_off, "the damage arrow did not blink");
    damage.update_damage_indicators();
    check(!damage.damage_indicator_shown(3),
          "the damage arrow outlived its timer");

    // The aspect fix is one at the DS's own shape and shrinks as the window
    // gets wider, which is what keeps a round icon round.
    check(PlayerHudState::hud_aspect_fix(256.0F, 192.0F) == 1.0F,
          "the aspect fix is not one at the DS's own shape");
    check(PlayerHudState::hud_aspect_fix(512.0F, 192.0F) < 1.0F,
          "a wider window did not shrink the aspect fix");
    check(PlayerHudState::hud_aspect_fix(0.0F, 0.0F) == 1.0F,
          "a window with no size did not fall back to one");

    if (failures == 0) {
        std::printf("native HUD wheel tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
