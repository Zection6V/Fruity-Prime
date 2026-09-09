#include "Entities/Players/PlayerAi.hpp"

#include "Entities/gameplay.hpp"
#include "Entities/room_catalog.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

std::string environment_value(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}

} // namespace

int main() {
    const std::string rom_path = environment_value("FRUITY_PRIME_TEST_NDS");
    if (rom_path.empty()) {
        std::cout << "real player AI test skipped: set FRUITY_PRIME_TEST_NDS\n";
        return 0;
    }
    try {
        const auto assets = fruityprime::assets::Store::from_rom(rom_path);
        const auto* entry = fruityprime::scene::find_multiplayer_room(
            "MP1 SANCTORUS");
        if (entry == nullptr) {
            throw std::runtime_error("AI test room is not in catalog");
        }
        const auto room = fruityprime::scene::Room::load(
            assets, entry->definition);
        fruityprime::gameplay::Session session(room);
        static_cast<void>(session.add_player(0, 0));
        static_cast<void>(session.add_player(1, 1));
        auto setup = session.snapshot(0);
        setup.players[0].position = {0.0F, 0.0F, 0.0F};
        setup.players[0].facing = {0.0F, 0.0F, 1.0F};
        setup.players[1].position = {0.0F, 0.0F, 12.0F};
        session.apply_snapshot(setup);
        const auto decision = fruityprime::players::BotAi::decide(
            session, 0, 2);
        assert(decision.target_slot == 1);
        assert(decision.target_distance > 11.9F);
        assert(decision.input.weapon_select == 1);
        assert((static_cast<std::uint32_t>(decision.input.buttons)
                & static_cast<std::uint32_t>(
                    fruityprime::net::IntentButtons::Shoot)) != 0);
        assert(decision.input.aim.z > 0.0F);

        // Exercise the stateful PlayerAiData boundary as well as the legacy
        // stateless decision helper.  A two-child tree with a 100000 weight
        // follows the same immediate-switch path as UpdatePathWeights in the
        // managed implementation.
        auto root = std::make_shared<fruityprime::ai::Data1>();
        auto first_child = std::make_shared<fruityprime::ai::Data1>();
        auto second_child = std::make_shared<fruityprime::ai::Data1>();
        root->func24_id = 7;
        first_child->func24_id = 8;
        second_child->func24_id = 9;
        first_child->data2.push_back({1, 100000, {}, 0, {}});
        root->data1 = {first_child, second_child};
        first_child->parent = root;
        second_child->parent = root;
        fruityprime::ai::Personality personality{root, 3};
        fruityprime::players::PlayerAiData ai_state;
        ai_state.initialize(&personality);
        fruityprime::gameplay::Input held_input;
        held_input.buttons = static_cast<fruityprime::net::IntentButtons>(
            static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::Shoot)
            | static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::Jump));
        ai_state.process_input(held_input);
        assert(ai_state.buttons()[fruityprime::players::AiButtonId::Shoot]
                   .frames_down == 1);
        assert(ai_state.buttons()[fruityprime::players::AiButtonId::Jump]
                   .frames_down == 1);
        ai_state.notify_damage(1, 12);
        ai_state.process(session, 0);
        assert(ai_state.target_slot() == 1);
        assert(fruityprime::players::has_flag(
            ai_state.flags2(), fruityprime::players::AiFlags2::TargetPlayer));
        assert(ai_state.aggro_count() >= 2);
        assert(ai_state.execution_tree()[1].data1 == second_child.get());
        assert(ai_state.unknown_func3_calls() == 0);
        ai_state.process_input({});
        assert(ai_state.buttons()[fruityprime::players::AiButtonId::Shoot]
                   .frames_up == 1);

        fruityprime::gameplay::Config team_config;
        team_config.team_mode = true;
        fruityprime::gameplay::Session team_session(room, team_config);
        static_cast<void>(team_session.add_player(0, 0));
        static_cast<void>(team_session.add_player(1, 1));
        static_cast<void>(team_session.add_player(2, 2));
        setup = team_session.snapshot(0);
        setup.players[0].position = {0.0F, 0.0F, 0.0F};
        setup.players[1].position = {0.0F, 0.0F, 8.0F};
        setup.players[2].position = {0.0F, 0.0F, 1.0F};
        setup.players[2].team = setup.players[0].team;
        team_session.apply_snapshot(setup);
        const auto team_decision = fruityprime::players::BotAi::decide(
            team_session, 0, 1);
        assert(team_decision.target_slot == 1);

        fruityprime::gameplay::Session empty_session(room);
        static_cast<void>(empty_session.add_player(0, 0));
        const auto no_target = fruityprime::players::BotAi::decide(
            empty_session, 0, 1);
        assert(no_target.target_slot == 0xff);
        assert((static_cast<std::uint32_t>(no_target.input.buttons)
                & static_cast<std::uint32_t>(
                    fruityprime::net::IntentButtons::MoveUp)) != 0);

        std::cout << "native player AI tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "player AI test failed: " << error.what() << '\n';
        return 1;
    }
}
