// PlayerHud's mode HUDs: which markers each objective mode puts on screen
// and what colour they are.
//
// The colours carry information a player acts on, so the rules are worth
// pinning: a node you hold is blue, one somebody else holds is red, an
// unclaimed one is white, and one being taken blinks in the colour of
// whoever is taking it -- which is not the same as who holds it. In a team
// match the team's own colour replaces all of that, because "mine" and
// "theirs" is then a question about two teams rather than about you.
#include "Entities/Players/PlayerHud.hpp"

#include <cmath>
#include <cstdio>

namespace {

bool is_color(const fruityprime::players::HudBackend::Color& color,
              float red, float green, float blue) {
    const float tolerance = 0.01F;
    return std::fabs(color.red - red) < tolerance
        && std::fabs(color.green - green) < tolerance
        && std::fabs(color.blue - blue) < tolerance;
}

bool is_good(const fruityprime::players::HudBackend::Color& color) {
    return is_color(color, 15.0F / 31.0F, 15.0F / 31.0F, 1.0F);
}

bool is_hostile(const fruityprime::players::HudBackend::Color& color) {
    return is_color(color, 1.0F, 0.0F, 0.0F);
}

bool is_neutral(const fruityprime::players::HudBackend::Color& color) {
    return is_color(color, 1.0F, 1.0F, 1.0F);
}

} // namespace

int main() {
    using fruityprime::players::PlayerHud;

    int failures = 0;
    const auto check = [&failures](bool condition, const char* what) {
        if (!condition) {
            std::printf("native HUD mode tests: %s\n", what);
            ++failures;
        }
    };

    fruityprime::game::State state;
    state.active_players = 2;
    state.player_teams[0] = 0;
    state.player_teams[1] = 1;

    fruityprime::players::HudContext context;
    context.state = &state;
    context.local_slot = 0;

    PlayerHud::ModeHudFrame frame;
    PlayerHud::ModeHudState hud;

    // Defender: one node, and who holds it decides the colour.  A
    // free-for-all match asks "is it mine", a team match asks "whose".
    context.mode = fruityprime::game::Mode::Defender;
    fruityprime::gameplay::ObjectiveState objectives;
    fruityprime::gameplay::NodeObjectiveState node;
    node.current_team = fruityprime::gameplay::NeutralObjectiveTeam;
    objectives.nodes.push_back(node);

    static_cast<void>(PlayerHud::ProcessModeHud(context, objectives, frame, hud));
    check(hud.locators.size() == 1, "Defender did not mark its one node");
    check(is_neutral(hud.locators[0].color),
          "an unclaimed node was not white");

    objectives.nodes[0].current_team = 0;
    static_cast<void>(PlayerHud::ProcessModeHud(context, objectives, frame, hud));
    check(is_good(hud.locators[0].color), "a node I hold was not blue");

    objectives.nodes[0].current_team = 1;
    static_cast<void>(PlayerHud::ProcessModeHud(context, objectives, frame, hud));
    check(is_hostile(hud.locators[0].color),
          "a node somebody else holds was not red");

    // Nodes: a node being taken blinks in the taker's colour, which is not
    // who holds it.
    context.mode = fruityprime::game::Mode::Nodes;
    objectives.nodes[0].current_team = 0;
    objectives.nodes[0].occupying_team = 1;
    objectives.nodes[0].progress = 1.0F;
    static_cast<void>(PlayerHud::ProcessModeHud(context, objectives, frame, hud));
    check(is_hostile(hud.locators[0].color),
          "a node of mine being taken did not warn me");

    // Two nodes held with nobody taking them is a bonus, and the HUD says
    // whose.
    objectives.nodes[0].occupying_team =
        fruityprime::gameplay::NeutralObjectiveTeam;
    objectives.nodes[0].progress = 0.0F;
    objectives.nodes.push_back(objectives.nodes[0]);
    static_cast<void>(PlayerHud::ProcessModeHud(context, objectives, frame, hud));
    check(hud.main_node_bonus, "holding two nodes did not read as a bonus");
    check(hud.node_bonus_opponent == -1,
          "my own bonus was counted as an opponent's");

    objectives.nodes[0].current_team = 1;
    objectives.nodes[1].current_team = 1;
    static_cast<void>(PlayerHud::ProcessModeHud(context, objectives, frame, hud));
    check(!hud.main_node_bonus, "an opponent's bonus was reported as mine");
    check(hud.node_bonus_opponent == 1,
          "the opponent running a bonus was not named");

    // Bounty: an octolith nobody is carrying is white; one being carried
    // blinks between white and whose it is, on the frame counter.
    context.mode = fruityprime::game::Mode::Bounty;
    objectives.nodes.clear();
    objectives.flags.clear();
    fruityprime::gameplay::FlagObjectiveState flag;
    flag.team_id = 1;
    objectives.flags.push_back(flag);

    frame.frame_count = 0;
    static_cast<void>(PlayerHud::ProcessModeHud(context, objectives, frame, hud));
    check(hud.locators.size() == 1, "Bounty did not mark its octolith");
    check(is_neutral(hud.locators[0].color),
          "a loose octolith was not white");

    objectives.flags[0].carrier_slot = 1;
    frame.frame_count = 8;  // the blink is on
    static_cast<void>(PlayerHud::ProcessModeHud(context, objectives, frame, hud));
    check(is_hostile(hud.locators[0].color),
          "an octolith carried by an opponent did not blink red");
    frame.frame_count = 0;  // the blink is off
    static_cast<void>(PlayerHud::ProcessModeHud(context, objectives, frame, hud));
    check(is_neutral(hud.locators[0].color),
          "a carried octolith did not blink back to white");

    // Carrying one myself points me at where it has to go instead.
    objectives.flags[0].carrier_slot = 0;
    static_cast<void>(PlayerHud::ProcessModeHud(context, objectives, frame, hud));
    check(hud.locators.size() == 1 && is_good(hud.locators[0].color),
          "carrying an octolith did not point at the base");

    // Prime Hunter: whoever is it gets marked for everybody else.
    context.mode = fruityprime::game::Mode::PrimeHunter;
    objectives.flags.clear();
    objectives.prime_hunter = 1;
    static_cast<void>(PlayerHud::ProcessModeHud(context, objectives, frame, hud));
    // Marking where they are needs a session to read a position from, which
    // this test does not build; that somebody else is wearing the crown is
    // decided here either way.
    check(!hud.is_prime_hunter, "somebody else's crown was worn locally");

    objectives.prime_hunter = 0;
    static_cast<void>(PlayerHud::ProcessModeHud(context, objectives, frame, hud));
    check(hud.locators.empty(), "the prime hunter marked themselves");
    check(hud.is_prime_hunter, "being the prime hunter was not noticed");
    check(hud.restart_prime_hunter_animation,
          "becoming the prime hunter did not start the announcement");
    check(hud.prime_hunter_text_timer > 0.0F,
          "the prime hunter announcement had no time on it");

    if (failures == 0) {
        std::printf("native HUD mode tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
