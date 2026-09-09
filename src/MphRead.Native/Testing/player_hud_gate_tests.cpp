#include "../Entities/Players/PlayerAi.hpp"
#include "../Entities/Players/PlayerHud.hpp"

#include "Formats/ai_personality.hpp"

#include <cassert>
#include <iostream>
#include <string>

namespace {

using fruityprime::game::MatchState;
using fruityprime::game::Mode;
using fruityprime::players::PlayerAiData;
using fruityprime::players::PlayerHud;

// Pausing freezes the readouts and hands the visor layer to the pause
// background; a dialog freezes them without drawing anything of its own.
void test_update_hud() {
    PlayerHud::HudFrame frame;
    frame.single_player = true;
    auto update = PlayerHud::update_hud(frame);
    assert(!update.draw_pause_background);
    assert(update.update_readouts);
    assert(update.update_dialogs);

    // Dialogs are a story thing only.
    frame.single_player = false;
    update = PlayerHud::update_hud(frame);
    assert(update.update_readouts);
    assert(!update.update_dialogs);

    frame.dialog_pause = true;
    update = PlayerHud::update_hud(frame);
    assert(!update.update_readouts);
    assert(!update.draw_pause_background);

    // The menu wins over a dialog: pausing while a dialog is up still draws
    // the pause background.
    frame.menu_pause = true;
    update = PlayerHud::update_hud(frame);
    assert(update.draw_pause_background);
    assert(!update.update_readouts);

    frame = {};
    frame.weapon_menu_open = true;
    update = PlayerHud::update_hud(frame);
    assert(update.update_weapon_menu);
}

// Only the team-scoring modes and Prime Hunter have a HUD of their own; the
// two-slot modes share one between their team and free-for-all forms.
void test_process_mode_hud() {
    assert(PlayerHud::process_mode_hud(Mode::Survival)
           == PlayerHud::ModeHud::Survival);
    assert(PlayerHud::process_mode_hud(Mode::SurvivalTeams)
           == PlayerHud::ModeHud::Survival);
    assert(PlayerHud::process_mode_hud(Mode::Bounty)
           == PlayerHud::ModeHud::Bounty);
    assert(PlayerHud::process_mode_hud(Mode::BountyTeams)
           == PlayerHud::ModeHud::Bounty);
    // Capture has no team variant: it is a team mode already.
    assert(PlayerHud::process_mode_hud(Mode::Capture)
           == PlayerHud::ModeHud::Capture);
    assert(PlayerHud::process_mode_hud(Mode::Defender)
           == PlayerHud::ModeHud::Defender);
    assert(PlayerHud::process_mode_hud(Mode::DefenderTeams)
           == PlayerHud::ModeHud::Defender);
    assert(PlayerHud::process_mode_hud(Mode::Nodes)
           == PlayerHud::ModeHud::Nodes);
    assert(PlayerHud::process_mode_hud(Mode::NodesTeams)
           == PlayerHud::ModeHud::Nodes);
    assert(PlayerHud::process_mode_hud(Mode::PrimeHunter)
           == PlayerHud::ModeHud::PrimeHunter);
    // Battle and the story have none.
    assert(PlayerHud::process_mode_hud(Mode::Battle)
           == PlayerHud::ModeHud::None);
    assert(PlayerHud::process_mode_hud(Mode::BattleTeams)
           == PlayerHud::ModeHud::None);
    assert(PlayerHud::process_mode_hud(Mode::SinglePlayer)
           == PlayerHud::ModeHud::None);
    assert(PlayerHud::process_mode_hud(Mode::None)
           == PlayerHud::ModeHud::None);
}

// The frame counter and the chat log are drawn ahead of the pause and
// spectator gates, on purpose.
void test_draw_hud_objects() {
    PlayerHud::DrawFrame frame;
    auto draw = PlayerHud::draw_hud_objects(frame);
    assert(draw.chat);
    assert(draw.player_hud);
    assert(!draw.fps);
    assert(!draw.scoreboard);

    frame.show_fps = true;
    frame.menu_pause = true;
    draw = PlayerHud::draw_hud_objects(frame);
    // Paused: the readouts are gone but the counter and the chat log are not.
    assert(draw.fps);
    assert(draw.chat);
    assert(!draw.player_hud);
    assert(!draw.scoreboard);

    // Watching from the map: no player readouts, but the scoreboard belongs to
    // the match rather than to a player and still answers.
    frame = {};
    frame.free_camera = true;
    frame.show_scoreboard = true;
    draw = PlayerHud::draw_hud_objects(frame);
    assert(!draw.player_hud);
    assert(draw.scoreboard);
    assert(draw.match_time);
    assert(draw.chat);

    // ...unless the menu is up over it.
    frame.menu_pause = true;
    draw = PlayerHud::draw_hud_objects(frame);
    assert(!draw.scoreboard);

    // A thumbnail capture draws nothing at all, counter and chat included.
    frame = {};
    frame.thumbnail_mode = true;
    frame.show_fps = true;
    draw = PlayerHud::draw_hud_objects(frame);
    assert(!draw.fps && !draw.chat && !draw.player_hud);

    frame = {};
    frame.match_state = MatchState::GameOver;
    draw = PlayerHud::draw_hud_objects(frame);
    assert(draw.game_over_text);
    assert(draw.player_hud);
}

// A bot's reaction delays are rolled when it is created and not again, so
// respawning does not change how sharp it is.
void test_ai_initialize() {
    PlayerAiData ai;
    // Samus at the lowest level waits 45 frames, doubled.
    ai.initialize_at_load(nullptr, /*hunter=*/0, /*bot_level=*/0,
                          /*single_player=*/false, /*in_encounter=*/false);
    assert(ai.aim_delay() == 90);
    assert(ai.decision_delay() == 300);

    // Trace is the slowest hunter at the lowest level and the same as the rest
    // at the highest.
    ai.initialize_at_load(nullptr, /*hunter=*/2, 0, false, false);
    assert(ai.aim_delay() == 180);
    ai.initialize_at_load(nullptr, /*hunter=*/2, 2, false, false);
    assert(ai.aim_delay() == 4);

    // An out-of-range level plays like a middling bot, not like the best one.
    ai.initialize_at_load(nullptr, 0, 9, false, false);
    assert(ai.aim_delay() == 30);
    ai.initialize_at_load(nullptr, 0, -1, false, false);
    assert(ai.aim_delay() == 30);

    // A story bot already in a fight reacts instantly.
    ai.initialize_at_load(nullptr, 0, 0, /*single_player=*/true,
                          /*in_encounter=*/true);
    assert(ai.aim_delay() == 0);
    assert(ai.decision_delay() == 0);

    // Respawning must not re-roll the delays.
    ai.initialize_at_load(nullptr, 0, 0, false, false);
    const std::uint32_t aim = ai.aim_delay();
    ai.initialize_at_spawn(nullptr);
    assert(ai.aim_delay() == aim);

    // An out-of-range hunter falls back to the first column rather than
    // reading past the table.
    ai.initialize_at_load(nullptr, /*hunter=*/200, 0, false, false);
    assert(ai.aim_delay() == 90);
}

// The execution-path dump is empty for a bot with no personality, and names
// the path once it has one.
void test_ai_output() {
    PlayerAiData ai;
    ai.initialize_at_spawn(nullptr);
    assert(ai.get_ouptut().empty());

    fruityprime::ai::Personality personality;
    personality.root = std::make_shared<fruityprime::ai::Data1>();
    personality.root->label = "root";
    auto child = std::make_shared<fruityprime::ai::Data1>();
    child->label = "child";
    child->parent = personality.root;
    personality.root->data1.push_back(child);

    ai.initialize_at_spawn(&personality);
    const std::string output = ai.get_ouptut();
    assert(output.find("root") != std::string::npos);
}

} // namespace

int main() {
    test_update_hud();
    test_process_mode_hud();
    test_draw_hud_objects();
    test_ai_initialize();
    test_ai_output();
    std::cout << "player hud gate tests passed\n";
    return 0;
}
