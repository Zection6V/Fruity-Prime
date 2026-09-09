// PlayerHud's off-screen markers and its scoreboard geometry.
//
// A locator inside the HUD box is drawn where the thing is.  Outside it, or
// behind the camera, it is pinned to the edge of the box and replaced by an
// arrow pointing at where the thing actually is -- and which edge it lands
// on is decided by where the line out to it leaves the box, not by which
// axis is larger.  That is the part worth pinning: getting it wrong puts
// markers in the corner whenever a target is off to one side.
//
// The scoreboard tightens its rows once a match holds more than the four
// players a DS could, and stops tightening at the height of a hunter icon.
#include "Entities/Players/PlayerHud.hpp"

#include <cmath>
#include <cstdio>

namespace {

constexpr float Width = 256.0F;
constexpr float Height = 192.0F;

bool near(float value, float expected, float tolerance = 0.001F) {
    return std::fabs(value - expected) <= tolerance;
}

} // namespace

int main() {
    using fruityprime::players::PlayerHud;

    int failures = 0;
    const auto check = [&failures](bool condition, const char* what) {
        if (!condition) {
            std::printf("native HUD locator tests: %s\n", what);
            ++failures;
        }
    };

    // Dead centre, in front of the camera: drawn where it is, no arrow.
    auto placed = PlayerHud::PlaceLocatorIcon({0.0F, 0.0F, -10.0F},
                                              128.0F / 256.0F,
                                              106.0F / 192.0F, Width, Height);
    check(!placed.arrow, "a marker in the middle of the box grew an arrow");
    check(near(placed.x, 0.5F), "the centre marker moved sideways");

    // Just inside the box on both axes is still the icon.
    placed = PlayerHud::PlaceLocatorIcon({0.0F, 0.0F, -10.0F},
                                         (128.0F + 99.0F) / 256.0F,
                                         (106.0F + 59.0F) / 192.0F,
                                         Width, Height);
    check(!placed.arrow, "a marker inside the box was pinned anyway");

    // Far off to the right and barely up: leaves through the right edge, so
    // it pins to x = 228 and the arrow points roughly along +x.
    placed = PlayerHud::PlaceLocatorIcon({0.0F, 0.0F, -10.0F},
                                         (128.0F + 400.0F) / 256.0F,
                                         (106.0F + 1.0F) / 192.0F,
                                         Width, Height);
    check(placed.arrow, "a marker off the right edge was not pinned");
    check(near(placed.x, 228.0F / 256.0F),
          "the right-edge marker did not pin to the right edge");
    check(std::fabs(placed.angle) < 30.0F,
          "the arrow off the right edge points the wrong way");

    // Far above and barely across: leaves through the top edge instead.
    placed = PlayerHud::PlaceLocatorIcon({0.0F, 0.0F, -10.0F},
                                         (128.0F - 1.0F) / 256.0F,
                                         (106.0F - 400.0F) / 192.0F,
                                         Width, Height);
    check(placed.arrow, "a marker above the box was not pinned");
    check(near(placed.y, 46.0F / 192.0F),
          "the top-edge marker did not pin to the top edge");
    // Screen y grows downwards, so a marker above the box has a positive
    // angle once the arrow's y is negated.
    check(placed.angle > 45.0F && placed.angle < 135.0F,
          "the arrow above the box points the wrong way");

    // Behind the camera: always pinned, whatever the projection says.
    placed = PlayerHud::PlaceLocatorIcon({8.0F, 0.0F, 1.0F}, 0.5F, 0.5F,
                                         Width, Height);
    check(placed.arrow, "a marker behind the camera was drawn in front");
    check(near(placed.x, 228.0F / 256.0F),
          "a marker behind and to the right pinned to the wrong side");

    // The scoreboard.  Four players keep the stock spacing; eight tighten,
    // but never past a hunter icon's height.
    fruityprime::game::State state;
    fruityprime::players::HudContext context;
    context.state = &state;
    state.active_players = 4;
    check(near(PlayerHud::GetScoreboardRowSpace(context),
               PlayerHud::ScorePlayerSpace),
          "a four player match did not use the stock row spacing");
    state.active_players = 8;
    const float eight = PlayerHud::GetScoreboardRowSpace(context);
    check(eight < PlayerHud::ScorePlayerSpace,
          "an eight player scoreboard did not tighten");
    check(eight >= PlayerHud::ScoreMinPlayerSpace,
          "the scoreboard tightened past a hunter icon");
    // GAME OVER takes a row above the list, and teams take two more, so
    // both leave less room per player.
    context.match_over = true;
    check(PlayerHud::GetScoreboardRowSpace(context) <= eight,
          "GAME OVER did not take a row from the list");
    context.match_over = false;
    state.teams = true;
    check(PlayerHud::GetScoreboardRowSpace(context) <= eight,
          "the team lines did not take rows from the list");

    if (failures == 0) {
        std::printf("native HUD locator tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
