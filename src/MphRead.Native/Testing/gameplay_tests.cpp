#include "Entities/gameplay.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Mods/Network/net_damage.hpp"
#include "Mods/Network/net_player_bridge.hpp"
#include "Mods/Network/net_player_setup.hpp"
#include "Mods/Network/net_slot_manager.hpp"
#include "Mods/Network/net_test_script.hpp"
#include "Mods/Network/player_entity_net_aim.hpp"
#include "Entities/room_catalog.hpp"
#include "Mods/spectator_mode.hpp"

#include "../Mods/MapGen/custom_rooms.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

bool net_setup_active = false;
int net_setup_local_slot = -1;

[[nodiscard]] bool net_setup_is_active() noexcept {
    return net_setup_active;
}

[[nodiscard]] int net_setup_slot() noexcept {
    return net_setup_local_slot;
}

} // namespace

int main() {
    const char* rom_value = std::getenv("FRUITY_PRIME_TEST_NDS");
    if (rom_value == nullptr || rom_value[0] == '\0') {
        std::cout << "real gameplay test skipped: set FRUITY_PRIME_TEST_NDS\n";
        return 0;
    }
    try {
        fruityprime::mapgen::custom_rooms::set_map_directory(
            std::filesystem::temp_directory_path()
                / "fruityprime-native-gameplay-no-custom-maps");
        const auto assets = fruityprime::assets::Store::from_rom(rom_value);
        const fruityprime::scene::RoomDefinition definition{
            "UNIT1_C0", "archives/unit1_C0.arc", "unit1_c0_model.bin",
            "levels/textures/unit1_c0_tex.bin",
            "unit1_c0_collision.bin", "levels/entities/Unit1_C0_Ent.bin",
            {}, {}, {}
        };
        const auto room = fruityprime::scene::Room::load(assets, definition);
        const auto* objective_entry = fruityprime::scene::find_multiplayer_room(
            "MP1 SANCTORUS");
        if (objective_entry == nullptr) {
            throw std::runtime_error("objective test room is not in catalog");
        }
        const auto objective_room = fruityprime::scene::Room::load(
            assets, objective_entry->definition);
        fruityprime::gameplay::Session session(room);
        fruityprime::players::PlayerEntity::Construct(session);
        static_cast<void>(session.add_player(0, 0));
        if (session.player_profile(0).hunter
            != fruityprime::metadata::Hunter::Samus) {
            throw std::runtime_error("player profile was not retained");
        }
        fruityprime::net::SlotManager slot_manager;
        fruityprime::game::State roster_state;
        roster_state.points[1] = 42;
        fruityprime::net::RosterPacket roster;
        roster.count = 2;
        roster.slots[0] = 0;
        roster.hunters[0] = 0;
        roster.slots[1] = 1;
        roster.hunters[1] = 2;
        const auto joined = slot_manager.sync(
            session, roster, 0, roster_state);
        if (joined.added != 1 || joined.removed != 0 || joined.active != 2
            || session.player_hunter(1) != 2) {
            throw std::runtime_error("network roster join was not applied");
        }
        auto player_table = fruityprime::players::PlayerEntity::Players();
        player_table[0]->IsBot(true);
        player_table[1]->IsBot(true);
        player_table[1]->BotLevel(2);
        fruityprime::net::detail::BindRuntime({
            &net_setup_is_active,
            &net_setup_slot
        });
        net_setup_local_slot = 0;
        fruityprime::net::NetPlayerSetup::Reset();
        fruityprime::net::NetPlayerSetup::ApplyOnce();
        if (!player_table[0]->IsBot() || !player_table[1]->IsBot()
            || player_table[1]->BotLevel() != 2) {
            throw std::runtime_error(
                "inactive network player setup was not a no-op");
        }
        net_setup_active = true;
        fruityprime::net::NetPlayerSetup::Reset();
        fruityprime::net::NetPlayerSetup::ApplyOnce();
        if (fruityprime::players::PlayerEntity::MainPlayerIndex() != 0
            || player_table[0]->IsBot() || player_table[1]->IsBot()
            || player_table[1]->BotLevel() != 0) {
            throw std::runtime_error("network player setup was not applied");
        }
        player_table[1]->IsBot(true);
        player_table[1]->BotLevel(2);
        fruityprime::net::NetPlayerSetup::ApplyOnce();
        if (!player_table[1]->IsBot() || player_table[1]->BotLevel() != 2) {
            throw std::runtime_error("network player setup applied twice");
        }
        fruityprime::net::NetPlayerSetup::Reset();
        net_setup_local_slot = 1;
        fruityprime::net::NetPlayerSetup::ApplyOnce();
        if (fruityprime::players::PlayerEntity::MainPlayerIndex() != 1
            || player_table[1]->IsBot() || player_table[1]->BotLevel() != 2
            || player_table[0]->IsBot() || player_table[0]->BotLevel() != 0) {
            throw std::runtime_error("network player setup reset was ignored");
        }
        roster.count = 1;
        const auto departed = slot_manager.sync(
            session, roster, 0, roster_state);
        if (departed.added != 0 || departed.removed != 1
            || session.has_player(1) || roster_state.points[1] != 0) {
            throw std::runtime_error("network roster departure was not applied");
        }

        // NetPlayerBridge keeps the managed intent/snapshot rules out of the
        // gameplay aggregate: packet history supplies a lost edge, reported
        // positions are corrected before the fixed tick, and a local
        // prediction is not overwritten by its own delayed snapshot.
        fruityprime::gameplay::Session bridge_session(room);
        static_cast<void>(bridge_session.add_player(0, 0));
        static_cast<void>(bridge_session.add_player(1, 1));
        bridge_session.inventory(1).ammo[0] = 20;
        fruityprime::net::NetPlayerBridge player_bridge;
        fruityprime::net::DamageBridge damage_bridge;
        fruityprime::net::IntentState first_intent;
        first_intent.frame = 100;
        first_intent.buttons = fruityprime::net::IntentButtons::InPlayState;
        first_intent.aim = {0.0F, 0.0F, 1.0F};
        first_intent.position = {1.0F, 1.0F, 1.0F};
        if (!player_bridge.apply_intent(bridge_session, 1, first_intent)) {
            throw std::runtime_error("network intent baseline was rejected");
        }
        auto edge_intent = first_intent;
        edge_intent.frame = 101;
        edge_intent.position = {20.0F, 1.0F, 1.0F};
        edge_intent.presses[0] = static_cast<std::uint32_t>(
            fruityprime::net::IntentButtons::Shoot);
        if (!player_bridge.apply_intent(bridge_session, 1, edge_intent)
            || player_bridge.metrics().snaps == 0) {
            throw std::runtime_error(
                "network intent edge or position correction was not applied");
        }
        bridge_session.tick();
        if (bridge_session.projectiles().empty()) {
            throw std::runtime_error(
                "network press history did not recover a lost shot edge");
        }

        fruityprime::gameplay::Session predicted_session(room);
        static_cast<void>(predicted_session.add_player(0, 0));
        fruityprime::net::NetPlayerBridge predicted_bridge;
        fruityprime::net::DamageBridge predicted_damage;
        auto placement = predicted_session.snapshot(1);
        placement.players[0].position = {2.0F, 1.0F, 2.0F};
        if (!predicted_bridge.apply_snapshot(
                predicted_session, placement, 0, false, predicted_damage)) {
            throw std::runtime_error("network placement snapshot was rejected");
        }
        predicted_session.mutable_player(0).position = {3.0F, 1.0F, 3.0F};
        auto delayed = placement;
        delayed.header.frame = 2;
        delayed.players[0].position = {40.0F, 1.0F, 40.0F};
        delayed.players[0].health = 77;
        if (!predicted_bridge.apply_snapshot(
                predicted_session, delayed, 0, false, predicted_damage)
            || std::fabs(predicted_session.player(0).position.x - 3.0F) > 0.001F
            || std::fabs(predicted_session.player(0).position.z - 3.0F) > 0.001F
            || predicted_session.player(0).health != 77) {
            throw std::runtime_error(
                "local prediction was overwritten by a delayed snapshot");
        }

        fruityprime::gameplay::Session damage_session(room);
        static_cast<void>(damage_session.add_player(0, 0));
        fruityprime::net::DamageBridge replay_damage;
        auto damage_state = damage_session.snapshot(1).players[0];
        replay_damage.replay(damage_session, damage_state);
        damage_state.damage_sequence = 1;
        damage_state.damage_beam = 1;
        damage_state.health = 90;
        damage_state.hit_direction = {0.0F, 0.0F, 1.0F};
        replay_damage.replay(damage_session, damage_state);
        const auto after_damage = damage_session.player(0).health;
        replay_damage.replay(damage_session, damage_state);
        if (after_damage != 90 || damage_session.player(0).health != after_damage
            || replay_damage.metrics().replayed[0] != 1) {
            throw std::runtime_error(
                "network damage replay was duplicated or not applied");
        }

        // PlayerEntityNetAim is the native counterpart of the large managed
        // partial PlayerEntity file: it owns the 120-frame position history,
        // absolute aim, state gates, test helpers, and transform repair.  It
        // must be exercised independently of NetPlayerBridge so the source
        // pairing cannot regress to a declaration-only file again.
        fruityprime::gameplay::Session aim_session(room);
        static_cast<void>(aim_session.add_player(0, 0));
        static_cast<void>(aim_session.add_player(1, 1));
        fruityprime::net::PlayerEntityNetAim player_aim;
        const auto first_position = aim_session.player(0).position;
        player_aim.record_position(aim_session, 0, 10);
        aim_session.mutable_player(0).position = {2.0F, 1.0F, 2.0F};
        player_aim.record_position(aim_session, 0, 20);
        fruityprime::net::Vec3 past_position;
        if (!player_aim.get_network_position(0, 15, past_position)
            || std::fabs(past_position.x - first_position.x) > 0.001F) {
            throw std::runtime_error(
                "network aim position history did not retain frame order");
        }
        if (!player_aim.set_aim(aim_session, 1, {1.0F, 0.0F, 1.0F})
            || std::fabs(player_aim.gun_vector(1).x
                             * player_aim.gun_vector(1).x
                         + player_aim.gun_vector(1).y
                             * player_aim.gun_vector(1).y
                         + player_aim.gun_vector(1).z
                             * player_aim.gun_vector(1).z
                         - 1.0F) > 0.001F) {
            throw std::runtime_error("network aim vector was not normalized");
        }
        if (player_aim.set_aim(aim_session, 1, {})
            || player_aim.metrics().rejected_values == 0) {
            throw std::runtime_error(
                "invalid network aim was not rejected");
        }
        if (!player_aim.set_facing(aim_session, 1, {0.0F, 0.0F, 2.0F})
            || !player_aim.set_aim(aim_session, 1, {0.0F, 0.0F, 1.0F})) {
            throw std::runtime_error("network facing setup failed");
        }
        const auto aim_origin = aim_session.player(1).position;
        const auto aim_delta = player_aim.aim_delta_towards(
            aim_session, 1,
            {aim_origin.x + 10.0F, aim_origin.y, aim_origin.z});
        if (aim_delta.x <= 0.0F) {
            throw std::runtime_error(
                "network aim delta did not turn toward the target");
        }
        player_aim.apply_script_aim(aim_session, 1, 90.0F, 0.0F);
        if (std::fabs(player_aim.gun_vector(1).x - 1.0F) > 0.01F) {
            throw std::runtime_error(
                "scripted network aim rotation did not use degrees");
        }
        if (!player_aim.set_weapon(aim_session, 1, 4)
            || !player_aim.can_zoom(aim_session, 1)
            || !player_aim.set_zoom(aim_session, 1, true)
            || (aim_session.player(1).flags
                & fruityprime::net::PlayerState::FlagZoomed) == 0) {
            throw std::runtime_error(
                "network weapon and zoom state did not converge");
        }
        player_aim.set_ammo(aim_session, 1, 37, 19);
        const auto carried = player_aim.ammo(aim_session, 1);
        if (carried.ua != 37 || carried.missiles != 19) {
            throw std::runtime_error(
                "network ammo state did not converge");
        }
        player_aim.set_spectating(aim_session, 1, true);
        if (player_aim.in_play(aim_session, 1)) {
            throw std::runtime_error(
                "spectating network player was still considered in play");
        }
        player_aim.set_spectating(aim_session, 1, false);
        if (!player_aim.network_spawn(aim_session, 1, {4.0F, 1.0F, 4.0F},
                                      {0.0F, 0.0F, 1.0F}, false)
            || !player_aim.is_in_play(aim_session, 1)) {
            throw std::runtime_error(
                "network spawn did not restore an in-play player");
        }
        static_cast<void>(player_aim.set_frozen(aim_session, 1, true));
        if (!player_aim.frozen(aim_session, 1)) {
            throw std::runtime_error("network frozen state was not applied");
        }
        static_cast<void>(player_aim.set_frozen(aim_session, 1, false));
        aim_session.mutable_player(1).position = {
            std::numeric_limits<float>::quiet_NaN(), 1.0F, 4.0F};
        player_aim.repair_vectors(aim_session, 1);
        if (!std::isfinite(aim_session.player(1).position.x)
            || player_aim.metrics().repaired_position == 0) {
            throw std::runtime_error(
                "network transform repair did not restore position");
        }
        if (!player_aim.net_die(aim_session, 1)
            || player_aim.is_in_play(aim_session, 1)
            || !player_aim.can_be_hurt(aim_session, 1)) {
            throw std::runtime_error(
                "network death or post-spawn hurtability state failed");
        }
        const auto board = fruityprime::net::PlayerEntityNetAim::scoreboard_size(
            8, false, false);
        if (board.rows != 8 || board.height < 8.0F * 19.0F) {
            throw std::runtime_error(
                "network scoreboard size did not apply the compact row rule");
        }

        // The fixed network tour is a real Session input producer, not a
        // marker translation unit. Check both the server-clock phase choice
        // and its offline audit helpers against the same gameplay surface.
        fruityprime::gameplay::Session script_session(room);
        static_cast<void>(script_session.add_player(0, 0));
        static_cast<void>(script_session.add_player(1, 1));
        fruityprime::net::NetTestScript script;
        if (script.phase(0.0) != fruityprime::net::TestPhase::Idle) {
            throw std::runtime_error("network test script idle phase failed");
        }
        script.set_phase_seconds(0.5);
        if (script.phase(0.6) != fruityprime::net::TestPhase::Walk) {
            throw std::runtime_error("network test script clock phase failed");
        }
        script.set_enabled(true);
        script.set_local_slot(0);
        script.set_network_active(true);
        script.apply(script_session, 0, 0.6);
        script_session.tick();
        const auto before_script_walk = script_session.player(0).position;
        script.walk_forward(script_session, 0);
        script_session.tick();
        if (script_session.player(0).position.z == before_script_walk.z) {
            throw std::runtime_error(
                "network test script did not write Session movement input");
        }
        script.hold_fire(script_session, 0, true);
        script_session.tick();
        script.hold_fire(script_session, 0, false);
        if (script.frame() <= 0) {
            throw std::runtime_error(
                "network test script frame state was not retained");
        }

        fruityprime::game::StorySave story_save;
        story_save.health = 77;
        story_save.health_max = 199;
        story_save.ammo = {321, 19};
        story_save.ammo_max = {400, 50};
        story_save.weapons = static_cast<std::uint16_t>(
            (std::uint16_t{1} << 0) | (std::uint16_t{1} << 4));
        story_save.weapon_slots = {4, 0, -1};
        fruityprime::gameplay::Session story_session(room);
        static_cast<void>(story_session.add_player(0, 0));
        story_session.apply_story_save(0, story_save);
        if (story_session.player(0).health != 77
            || story_session.player(0).current_weapon != 4
            || story_session.inventory(0).health_max != 199
            || story_session.inventory(0).ammo[0] != 321
            || story_session.inventory(0).ammo[1] != 19
            || !story_session.inventory(0).available_weapons[4]) {
            throw std::runtime_error("story save inventory was not applied");
        }
        const auto initial = session.player(0).position;
        session.set_input(0, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::MoveUp,
            {0.0F, 0.0F, 1.0F}
        });
        for (int i = 0; i < 60; ++i) {
            session.tick();
        }
        const auto final = session.player(0).position;
        if (session.tick_count() != 60 || final.z == initial.z) {
            throw std::runtime_error("fixed timestep did not move the player");
        }

        fruityprime::gameplay::Config item_config;
        item_config.gravity = 0.0F;
        item_config.walk_speed = 0.0F;
        item_config.air_acceleration = 0.0F;
        item_config.world_padding = 0.0F;
        fruityprime::gameplay::Session items(objective_room, item_config);
        static_cast<void>(items.add_player(0, 0));
        for (int i = 0; i < 600 && items.items().empty(); ++i) {
            items.set_input(0, fruityprime::gameplay::Input{});
            items.tick();
        }
        if (items.items().empty()) {
            throw std::runtime_error(
                "real room item spawner did not create an item");
        }
        const auto spawned_item = items.items().front();
        auto item_setup = items.snapshot(0);
        item_setup.players[0].position = spawned_item.position;
        items.apply_snapshot(item_setup);
        items.set_input(0, fruityprime::gameplay::Input{});
        items.tick();
        const bool item_still_present = std::find_if(
            items.items().begin(), items.items().end(),
            [&spawned_item](const fruityprime::gameplay::ItemState& item) {
                return item.owner_entity_id == spawned_item.owner_entity_id;
            }) != items.items().end();
        if (item_still_present) {
            throw std::runtime_error(
                "player did not pick up the spawned room item");
        }
        const auto item_pickup_sound = std::find_if(
            items.sound_events().begin(), items.sound_events().end(),
            [](const fruityprime::gameplay::SoundEvent& event) {
                return event.cue
                    == fruityprime::gameplay::SoundCue::ItemPickup;
            });
        if (item_pickup_sound == items.sound_events().end()) {
            throw std::runtime_error(
                "item pickup did not emit an item sound event");
        }

        fruityprime::gameplay::Config nodes_config;
        nodes_config.mode = 10; // GameMode.Nodes
        nodes_config.point_goal = 1;
        nodes_config.team_mode = true;
        nodes_config.gravity = 0.0F;
        fruityprime::gameplay::Session nodes(objective_room, nodes_config);
        static_cast<void>(nodes.add_player(0, 0));
        static_cast<void>(nodes.add_player(1, 1));
        if (nodes.objective_state().nodes.empty()) {
            throw std::runtime_error("real room has no node objectives");
        }
        const auto node_center = nodes.objective_state().nodes.front()
            .volume.center();
        auto node_setup = nodes.snapshot(0);
        node_setup.players[0].position = {
            node_center.x, node_center.y, node_center.z};
        node_setup.players[1].position = {
            node_center.x + 1000.0F, node_center.y, node_center.z};
        nodes.apply_snapshot(node_setup);
        nodes.set_input(0, fruityprime::gameplay::Input{});
        nodes.set_input(1, fruityprime::gameplay::Input{});
        for (int i = 0; i < 600; ++i) {
            nodes.tick();
        }
        if (nodes.objective_state().nodes.front().current_team != 0) {
            throw std::runtime_error("node capture did not change ownership");
        }
        node_setup = nodes.snapshot(0);
        node_setup.players[0].position = {
            node_center.x + 1000.0F, node_center.y, node_center.z};
        nodes.apply_snapshot(node_setup);
        nodes.tick();
        if (nodes.player(0).points == 0) {
            throw std::runtime_error("captured node did not award a point");
        }

        fruityprime::gameplay::Config defender_config;
        defender_config.mode = 12; // GameMode.Defender
        defender_config.gravity = 0.0F;
        fruityprime::gameplay::Session defender(objective_room, defender_config);
        static_cast<void>(defender.add_player(0, 0));
        auto defender_setup = defender.snapshot(0);
        const auto defender_center = defender.objective_state().nodes.front()
            .volume.center();
        defender_setup.players[0].position = {
            defender_center.x, defender_center.y, defender_center.z};
        defender.apply_snapshot(defender_setup);
        defender.set_input(0, fruityprime::gameplay::Input{});
        for (int i = 0; i < 60; ++i) {
            defender.tick();
        }
        if (defender.objective_state().team_time[0] <= 0.0F) {
            throw std::runtime_error("defender objective time did not advance");
        }

        fruityprime::gameplay::Config prime_config;
        prime_config.mode = 14; // GameMode.PrimeHunter
        prime_config.gravity = 0.0F;
        fruityprime::gameplay::Session prime_hunter(objective_room, prime_config);
        static_cast<void>(prime_hunter.add_player(0, 0));
        prime_hunter.set_input(0, fruityprime::gameplay::Input{});
        for (int i = 0; i < 60; ++i) {
            prime_hunter.tick();
        }
        if (prime_hunter.objective_state().prime_hunter != 0
            || prime_hunter.objective_state().player_time[0] <= 0.0F
            || prime_hunter.player(0).health != 97) {
            throw std::runtime_error("prime hunter objective time did not advance");
        }

        const auto* capture_entry = fruityprime::scene::find_multiplayer_room(
            "CTF1 FAULT LINE - EXPANDED");
        if (capture_entry == nullptr) {
            throw std::runtime_error("capture test room is not in catalog");
        }
        const auto capture_room = fruityprime::scene::Room::load(
            assets, capture_entry->definition);
        fruityprime::gameplay::Config capture_config;
        capture_config.mode = 7; // GameMode.Capture
        capture_config.point_goal = 5;
        capture_config.team_mode = true;
        capture_config.gravity = 0.0F;
        fruityprime::gameplay::Session capture(capture_room, capture_config);
        static_cast<void>(capture.add_player(0, 0));
        static_cast<void>(capture.add_player(1, 1));
        if (capture.objective_state().flags.size() < 2) {
            throw std::runtime_error("capture room has fewer than two flags");
        }
        const auto enemy_flag = std::find_if(
            capture.objective_state().flags.begin(),
            capture.objective_state().flags.end(),
            [](const fruityprime::gameplay::FlagObjectiveState& flag) {
                return flag.team_id == 1;
            });
        const auto own_flag = std::find_if(
            capture.objective_state().flags.begin(),
            capture.objective_state().flags.end(),
            [](const fruityprime::gameplay::FlagObjectiveState& flag) {
                return flag.team_id == 0;
            });
        if (enemy_flag == capture.objective_state().flags.end()
            || own_flag == capture.objective_state().flags.end()) {
            throw std::runtime_error("capture flags do not have two teams");
        }
        auto capture_setup = capture.snapshot(0);
        capture_setup.players[0].position = {
            enemy_flag->base_position.x, enemy_flag->base_position.y,
            enemy_flag->base_position.z};
        capture_setup.players[1].position = {
            enemy_flag->base_position.x + 1000.0F,
            enemy_flag->base_position.y, enemy_flag->base_position.z};
        capture.apply_snapshot(capture_setup);
        capture.set_input(0, fruityprime::gameplay::Input{});
        capture.set_input(1, fruityprime::gameplay::Input{});
        capture.tick();
        if (enemy_flag->carrier_slot != 0
            || enemy_flag->at_base) {
            throw std::runtime_error("capture flag was not picked up");
        }
        fruityprime::scene::VolumePoint own_base = own_flag->base_position;
        for (const auto& entity : capture_room.entities()) {
            if (entity.kind != fruityprime::scene::EntityKind::FlagBase) {
                continue;
            }
            const auto* base = std::get_if<fruityprime::scene::FlagBaseData>(
                &entity.typed_data);
            if (base != nullptr && (base->team_id & 0xffu) == 0) {
                own_base = base->volume.center();
                break;
            }
        }
        capture_setup = capture.snapshot(0);
        capture_setup.players[0].position = {
            own_base.x, own_base.y, own_base.z};
        capture.apply_snapshot(capture_setup);
        capture.tick();
        if (capture.player(0).points == 0 || !enemy_flag->at_base) {
            throw std::runtime_error("capture flag was not scored at own base");
        }

        fruityprime::gameplay::Session spectator(room);
        static_cast<void>(spectator.add_player(0, 0));
        const auto spectator_start = spectator.player(0).position;
        const auto spectator_buttons = static_cast<fruityprime::net::IntentButtons>(
            static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::SpectatingState)
            | static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::MoveUp)
            | static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::Shoot));
        spectator.set_input(0, fruityprime::gameplay::Input{
            spectator_buttons, {0.0F, 0.0F, 1.0F}
        });
        for (int i = 0; i < 30; ++i) {
            spectator.tick();
        }
        const auto spectator_state = spectator.player(0);
        if ((spectator_state.flags
                & fruityprime::net::PlayerState::FlagSpectating) == 0
            || spectator_state.position.x != spectator_start.x
            || spectator_state.position.y != spectator_start.y
            || spectator_state.position.z != spectator_start.z
            || !spectator.projectiles().empty()) {
            throw std::runtime_error(
                "spectating input moved, fired, or failed to publish its state");
        }
        spectator.set_input(0, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::MoveUp,
            {0.0F, 0.0F, 1.0F}
        });
        for (int i = 0; i < 30; ++i) {
            spectator.tick();
        }
        if ((spectator.player(0).flags
                & fruityprime::net::PlayerState::FlagSpectating) != 0
            || spectator.player(0).position.z == spectator_start.z) {
            throw std::runtime_error("rejoining input did not resume the player");
        }
        auto score_state = spectator.snapshot(0);
        score_state.players[0].flags |=
            fruityprime::net::PlayerState::FlagSpectating;
        score_state.players[0].points = 5;
        score_state.players[0].kills = 2;
        score_state.players[0].deaths = 3;
        spectator.apply_snapshot(score_state);
        spectator.rejoin_player(0);
        if (spectator.player(0).points != 0
            || spectator.player(0).kills != 0
            || spectator.player(0).deaths != 0
            || (spectator.player(0).flags
                & fruityprime::net::PlayerState::FlagSpectating) != 0) {
            throw std::runtime_error("positive spectator score did not reset");
        }
        score_state = spectator.snapshot(0);
        score_state.players[0].flags |=
            fruityprime::net::PlayerState::FlagSpectating;
        score_state.players[0].points = -2;
        score_state.players[0].kills = 4;
        score_state.players[0].deaths = 5;
        spectator.apply_snapshot(score_state);
        spectator.rejoin_player(0);
        if (spectator.player(0).points != -2
            || spectator.player(0).kills != 0
            || spectator.player(0).deaths != 0) {
            throw std::runtime_error("negative spectator penalty was reset");
        }

        fruityprime::mods::spectator::Controller spectator_controller;
        fruityprime::input::State spectator_input;
        static_cast<void>(spectator.add_player(1, 1));
        if (!spectator_controller.start(
                spectator, 0, true, false, spectator_input)
            || !spectator_controller.is_spectating()
            || !spectator_controller.free_camera()
            || (spectator.player(0).flags
                & fruityprime::net::PlayerState::FlagSpectating) == 0) {
            throw std::runtime_error("spectator controller did not start");
        }
        if (!spectator_controller.cycle_next(spectator, 0)
            || spectator_controller.free_camera()
            || spectator_controller.view_slot() != 1) {
            throw std::runtime_error("spectator controller did not switch view");
        }
        if (!spectator_controller.rejoin(spectator, 0)
            || spectator_controller.is_spectating()
            || spectator_controller.free_camera()
            || spectator_controller.view_slot() != 0
            || (spectator.player(0).flags
                & fruityprime::net::PlayerState::FlagSpectating) != 0) {
            throw std::runtime_error("spectator controller did not rejoin");
        }

        fruityprime::gameplay::Session observer(room);
        static_cast<void>(observer.add_player(0, 0));
        static_cast<void>(observer.add_player(1, 1));
        auto remote_state = observer.snapshot(0);
        remote_state.players[1].flags |=
            fruityprime::net::PlayerState::FlagSpectating;
        observer.apply_snapshot(remote_state);
        observer.tick();
        if ((observer.player(1).flags
                & fruityprime::net::PlayerState::FlagSpectating) == 0) {
            throw std::runtime_error(
                "snapshot-only spectator state was overwritten locally");
        }
        observer.set_input(1, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::MoveUp,
            {0.0F, 0.0F, 1.0F}
        });
        observer.tick();
        if ((observer.player(1).flags
                & fruityprime::net::PlayerState::FlagSpectating) != 0) {
            throw std::runtime_error(
                "authoritative rejoin input did not clear spectator state");
        }

        fruityprime::gameplay::Session frozen(room);
        static_cast<void>(frozen.add_player(0, 0));
        auto frozen_setup = frozen.snapshot(0);
        const auto frozen_start = frozen_setup.players[0].position;
        frozen_setup.players[0].flags |=
            fruityprime::net::PlayerState::FlagFrozen;
        frozen.apply_snapshot(frozen_setup);
        frozen.set_input(0, fruityprime::gameplay::Input{
            static_cast<fruityprime::net::IntentButtons>(
                static_cast<std::uint32_t>(
                    fruityprime::net::IntentButtons::MoveUp)
                | static_cast<std::uint32_t>(
                    fruityprime::net::IntentButtons::Shoot)),
            {0.0F, 0.0F, 1.0F}
        });
        frozen.tick();
        const auto frozen_state = frozen.player(0);
        if (frozen_state.position.x != frozen_start.x
            || frozen_state.position.y != frozen_start.y
            || frozen_state.position.z != frozen_start.z
            || !frozen.projectiles().empty()) {
            throw std::runtime_error(
                "frozen player moved or fired while state was replicated");
        }

        session.set_input(0, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::NextWeapon,
            {0.0F, 0.0F, 1.0F}
        });
        session.tick();
        if (session.player(0).current_weapon != 1) {
            throw std::runtime_error("next weapon input did not cycle weapon");
        }
        session.tick();
        if (session.player(0).current_weapon != 1) {
            throw std::runtime_error("held weapon input cycled repeatedly");
        }
        session.set_input(0, fruityprime::gameplay::Input{});
        session.tick();
        session.set_input(0, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::PrevWeapon,
            {0.0F, 0.0F, 1.0F}
        });
        session.tick();
        if (session.player(0).current_weapon != 0) {
            throw std::runtime_error("previous weapon input did not cycle back");
        }
        session.set_input(0, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::Zoom,
            {0.0F, 0.0F, 1.0F},
            4
        });
        session.tick();
        if (session.player(0).current_weapon != 4
            || (session.player(0).flags
                & fruityprime::net::PlayerState::FlagZoomed) == 0) {
            throw std::runtime_error(
                "direct weapon selection or zoom state failed");
        }
        session.set_input(0, fruityprime::gameplay::Input{});
        session.tick();
        if ((session.player(0).flags
             & fruityprime::net::PlayerState::FlagZoomed) != 0) {
            throw std::runtime_error("zoom state did not clear");
        }
        session.set_input(0, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::Shoot,
            {0.0F, 0.0F, 1.0F},
            0
        });
        session.tick();
        if (session.projectiles().empty()) {
            throw std::runtime_error("shoot input did not spawn a projectile");
        }
        const auto beam_shot_sound = std::find_if(
            session.sound_events().begin(), session.sound_events().end(),
            [](const fruityprime::gameplay::SoundEvent& event) {
                return event.cue == fruityprime::gameplay::SoundCue::BeamShot;
            });
        if (beam_shot_sound == session.sound_events().end()) {
            throw std::runtime_error(
                "shoot input did not emit a beam sound event");
        }
        const auto& visual_projectile = session.projectiles().back();
        if (visual_projectile.weapon != 0
            || visual_projectile.draw_func_id != 0
            || std::fabs(visual_projectile.color.x - 1.0F) > 0.0001F
            || std::fabs(visual_projectile.color.y - (26.0F / 31.0F))
                > 0.0001F
            || std::fabs(visual_projectile.color.z - (8.0F / 31.0F))
                > 0.0001F) {
            throw std::runtime_error(
                "projectile visual state did not initialize from weapon data: "
                + std::to_string(visual_projectile.draw_func_id) + " for weapon "
                + std::to_string(visual_projectile.weapon));
        }
        if (visual_projectile.collision_effect != 4
            || visual_projectile.muzzle_effect != 65
            || visual_projectile.damage_dir_type != 0
            || visual_projectile.damage_interpolation != 0) {
            throw std::runtime_error(
                "projectile effect metadata did not initialize from weapon data");
        }
        if (session.effects().empty()
            || session.effects().back().effect_id != 65
            || session.effects().back().source_entity_id != 0) {
            throw std::runtime_error(
                "projectile muzzle effect was not emitted from weapon data");
        }
        const auto fuzzball = std::find_if(
            session.single_particles().begin(),
            session.single_particles().end(),
            [](const fruityprime::gameplay::SingleParticleState& particle) {
                return particle.type
                    == fruityprime::gameplay::SingleParticleType::Fuzzball;
            });
        if (fuzzball == session.single_particles().end()
            || std::fabs(fuzzball->scale - 0.25F) > 0.0001F) {
            throw std::runtime_error(
                "in-flight projectile did not emit its single fuzzball particle");
        }
        for (const auto& point : visual_projectile.past_positions) {
            if (std::fabs(point.x - visual_projectile.back_position.x)
                    > 0.0001F
                || std::fabs(point.y - visual_projectile.back_position.y)
                    > 0.0001F
                || std::fabs(point.z - visual_projectile.back_position.z)
                    > 0.0001F) {
                throw std::runtime_error(
                    "projectile trail history did not retain the prior point");
            }
        }
        fruityprime::gameplay::Session ammo_session(room);
        static_cast<void>(ammo_session.add_player(0, 0));
        fruityprime::game::StorySave ammo_save;
        ammo_save.health = 100;
        ammo_save.health_max = 100;
        ammo_save.ammo = {0, 10};
        ammo_save.ammo_max = {400, 50};
        ammo_save.weapons = static_cast<std::uint16_t>(
            (std::uint16_t{1} << 0) | (std::uint16_t{1} << 2));
        ammo_save.weapon_slots = {2, 0, -1};
        ammo_session.apply_story_save(0, ammo_save);
        ammo_session.set_input(0, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::Shoot,
            {0.0F, 0.0F, 1.0F}
        });
        ammo_session.tick();
        if (ammo_session.inventory(0).ammo[1] != 0
            || ammo_session.projectiles().empty()) {
            throw std::runtime_error(
                "weapon ammo was not consumed before projectile spawn");
        }
        fruityprime::gameplay::Session empty_ammo_session(room);
        static_cast<void>(empty_ammo_session.add_player(0, 0));
        ammo_save.ammo[1] = 0;
        empty_ammo_session.apply_story_save(0, ammo_save);
        empty_ammo_session.set_input(0, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::Shoot,
            {0.0F, 0.0F, 1.0F}
        });
        empty_ammo_session.tick();
        if (!empty_ammo_session.projectiles().empty()) {
            throw std::runtime_error(
                "empty weapon ammo still spawned a projectile");
        }
        session.set_input(0, fruityprime::gameplay::Input{});
        for (int i = 0; i < 120; ++i) {
            session.tick();
        }
        if (!session.projectiles().empty()) {
            throw std::runtime_error("projectile lifetime did not expire");
        }

        fruityprime::gameplay::Session combat(room);
        static_cast<void>(combat.add_player(0, 0));
        static_cast<void>(combat.add_player(1, 1));
        auto combat_setup = combat.snapshot(0);
        combat_setup.players[1].position = {
            combat_setup.players[0].position.x,
            combat_setup.players[0].position.y + 0.9F,
            combat_setup.players[0].position.z + 0.1F
        };
        combat_setup.players[1].health = 6;
        combat.apply_snapshot(combat_setup);
        combat.set_input(0, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::Shoot,
            {0.0F, 0.0F, 1.0F}
        });
        combat.tick();
        if (combat.player(1).health != 0
            || combat.player(1).deaths != 1
            || combat.player(0).kills != 1
            || combat.player(0).points != 1
                || (combat.player(1).flags
                & fruityprime::net::PlayerState::FlagSpawned) != 0) {
            throw std::runtime_error("projectile damage/score path failed");
        }
        const auto player_damage_sound = std::find_if(
            combat.sound_events().begin(), combat.sound_events().end(),
            [](const fruityprime::gameplay::SoundEvent& event) {
                return event.cue
                    == fruityprime::gameplay::SoundCue::PlayerDamage;
            });
        const auto player_death_sound = std::find_if(
            combat.sound_events().begin(), combat.sound_events().end(),
            [](const fruityprime::gameplay::SoundEvent& event) {
                return event.cue
                    == fruityprime::gameplay::SoundCue::PlayerDeath;
            });
        if (player_damage_sound == combat.sound_events().end()
            || player_death_sound == combat.sound_events().end()) {
            throw std::runtime_error(
                "projectile damage path did not emit player sound events");
        }
        if (combat.respawn_ticks(1) == 0
            || combat.respawn_duration_ticks() == 0) {
            throw std::runtime_error(
                "dead player did not retain a renderable respawn countdown");
        }
        combat.set_input(0, fruityprime::gameplay::Input{});
        combat.set_input(1, fruityprime::gameplay::Input{});
        for (std::uint32_t i = 0; i < 90; ++i) {
            combat.tick();
        }
        if (combat.player(1).health != 100
            || (combat.player(1).flags
                & fruityprime::net::PlayerState::FlagSpawned) == 0) {
            throw std::runtime_error("dead player did not respawn");
        }
        fruityprime::gameplay::Config survival_config;
        survival_config.survival_mode = true;
        survival_config.survival_lives = 2;
        survival_config.respawn_ticks = 1;
        fruityprime::gameplay::Session survival(room, survival_config);
        static_cast<void>(survival.add_player(0, 0));
        static_cast<void>(survival.add_player(1, 1));
        auto eliminated = survival.snapshot(0);
        eliminated.players[1].health = 0;
        eliminated.players[1].deaths = 3;
        eliminated.players[1].flags &= static_cast<std::uint8_t>(
            ~fruityprime::net::PlayerState::FlagSpawned);
        survival.apply_snapshot(eliminated);
        for (int i = 0; i < 10; ++i) {
            survival.tick();
        }
        if (survival.player(1).health != 0
            || (survival.player(1).flags
                & fruityprime::net::PlayerState::FlagSpawned) != 0) {
            throw std::runtime_error("survival-eliminated player respawned");
        }
        fruityprime::gameplay::Session cleanup(room);
        static_cast<void>(cleanup.add_player(0, 0));
        cleanup.set_input(0, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::Shoot,
            {1.0F, 0.0F, 0.0F}
        });
        cleanup.tick();
        if (cleanup.projectiles().empty()) {
            throw std::runtime_error("projectile cleanup setup failed");
        }
        cleanup.remove_player(0);
        if (!cleanup.projectiles().empty()) {
            throw std::runtime_error("departed player's projectile survived");
        }
        fruityprime::gameplay::Config team_config;
        team_config.team_mode = true;
        fruityprime::gameplay::Session team_session(room, team_config);
        static_cast<void>(team_session.add_player(0, 0));
        static_cast<void>(team_session.add_player(1, 1));
        auto team_setup = team_session.snapshot(0);
        team_setup.players[1].position = {
            team_setup.players[0].position.x,
            team_setup.players[0].position.y + 0.9F,
            team_setup.players[0].position.z + 0.1F
        };
        team_setup.players[1].team = team_setup.players[0].team;
        team_setup.players[1].health = 6;
        team_session.apply_snapshot(team_setup);
        team_session.set_input(0, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::Shoot,
            {0.0F, 0.0F, 1.0F}
        });
        team_session.tick();
        if (team_session.player(1).health != 6) {
            throw std::runtime_error("friendly fire was not blocked");
        }
        team_session.set_friendly_fire(true);
        for (int i = 0; i < 5; ++i) {
            team_session.set_input(0, fruityprime::gameplay::Input{});
            team_session.tick();
        }
        team_session.set_input(0, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::Shoot,
            {0.0F, 0.0F, 1.0F}
        });
        team_session.tick();
        if (team_session.player(1).health != 0) {
            throw std::runtime_error("friendly fire was not enabled");
        }
        fruityprime::gameplay::Session individual_session(room);
        static_cast<void>(individual_session.add_player(0, 0));
        static_cast<void>(individual_session.add_player(1, 1));
        auto individual_setup = individual_session.snapshot(0);
        individual_setup.players[1].position = {
            individual_setup.players[0].position.x,
            individual_setup.players[0].position.y + 0.9F,
            individual_setup.players[0].position.z + 0.1F
        };
        individual_setup.players[1].team = individual_setup.players[0].team;
        individual_setup.players[1].health = 6;
        individual_session.apply_snapshot(individual_setup);
        individual_session.set_input(0, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::Shoot,
            {0.0F, 0.0F, 1.0F}
        });
        individual_session.tick();
        if (individual_session.player(1).health == 6) {
            throw std::runtime_error(
                "non-team mode incorrectly blocked damage");
        }
        bool jump_pad_tested = false;
        bool teleporter_tested = false;
        for (const auto& catalog_entry : fruityprime::scene::multiplayer_rooms()) {
            const auto environment_room = fruityprime::scene::Room::load(
                assets, catalog_entry.definition);
            for (const auto& entity : environment_room.entities()) {
                if (!jump_pad_tested
                    && entity.kind == fruityprime::scene::EntityKind::JumpPad) {
                    const auto* jump_pad = std::get_if<
                        fruityprime::scene::JumpPadData>(&entity.typed_data);
                    if (jump_pad == nullptr || !jump_pad->active
                        || jump_pad->volume.kind
                            == fruityprime::scene::VolumeKind::Invalid
                        || jump_pad->speed <= 0.0F
                        || (jump_pad->trigger_flags & ((1u << 9) | (1u << 10)))
                            == 0) {
                        continue;
                    }
                    fruityprime::gameplay::Config environment_config;
                    environment_config.gravity = 0.0F;
                    environment_config.walk_speed = 0.0F;
                    environment_config.air_acceleration = 0.0F;
                    environment_config.world_padding = 0.0F;
                    fruityprime::gameplay::Session environment(
                        environment_room, environment_config);
                    static_cast<void>(environment.add_player(0, 0));
                    auto setup = environment.snapshot(0);
                    const auto center = jump_pad->volume.center();
                    setup.players[0].position = {center.x, center.y, center.z};
                    const bool alt_form = (jump_pad->trigger_flags & (1u << 10))
                        != 0
                        && (jump_pad->trigger_flags & (1u << 9)) == 0;
                    if (alt_form) {
                        setup.players[0].flags |=
                            fruityprime::net::PlayerState::FlagAltForm;
                    }
                    environment.apply_snapshot(setup);
                    const auto buttons = alt_form
                        ? fruityprime::net::IntentButtons::AltFormState
                        : fruityprime::net::IntentButtons::None;
                    environment.set_input(0, fruityprime::gameplay::Input{
                        buttons, {0.0F, 0.0F, 1.0F}
                    });
                    environment.tick();
                    const auto launched_speed = environment.player(0).speed;
                    if (launched_speed.x * launched_speed.x
                            + launched_speed.y * launched_speed.y
                            + launched_speed.z * launched_speed.z
                        <= 0.01F) {
                        throw std::runtime_error(
                            "active jump pad did not apply its beam impulse");
                    }
                    jump_pad_tested = true;
                }
                if (!teleporter_tested
                    && entity.kind == fruityprime::scene::EntityKind::Teleporter) {
                    const auto* teleporter = std::get_if<
                        fruityprime::scene::TeleporterData>(&entity.typed_data);
                    if (teleporter == nullptr || !teleporter->active) {
                        continue;
                    }
                    fruityprime::gameplay::Config environment_config;
                    environment_config.gravity = 0.0F;
                    environment_config.walk_speed = 0.0F;
                    environment_config.air_acceleration = 0.0F;
                    environment_config.world_padding = 0.0F;
                    fruityprime::gameplay::Session environment(
                        environment_room, environment_config);
                    static_cast<void>(environment.add_player(0, 0));
                    const auto source = entity.position;
                    const auto target = teleporter->target_position;
                    auto setup = environment.snapshot(0);
                    setup.players[0].position = {
                        source.x.to_float() + 8.0F, source.y.to_float(),
                        source.z.to_float()};
                    environment.apply_snapshot(setup);
                    environment.set_input(0, fruityprime::gameplay::Input{});
                    environment.tick();
                    setup = environment.snapshot(0);
                    setup.players[0].position = {
                        source.x.to_float(), source.y.to_float(),
                        source.z.to_float()};
                    environment.apply_snapshot(setup);
                    environment.tick();
                    if (environment.room_transition().has_value()) {
                        const auto& transition =
                            *environment.room_transition();
                        if (transition.room_name.empty()
                            || transition.target_entity_id
                                != teleporter->target_index) {
                            throw std::runtime_error(
                                "cross-room teleporter produced an invalid transition");
                        }
                        teleporter_tested = true;
                        continue;
                    }
                    const auto teleported = environment.player(0).position;
                    if (std::fabs(teleported.x - target.x) > 0.001F
                        || std::fabs(teleported.y - (target.y + 0.5F)) > 0.001F
                        || std::fabs(teleported.z - target.z) > 0.001F) {
                        throw std::runtime_error(
                            "active teleporter did not move the player to its target");
                    }
                    teleporter_tested = true;
                }
            }
            if (jump_pad_tested && teleporter_tested) {
                break;
            }
        }
        if (!jump_pad_tested) {
            throw std::runtime_error(
                "real room catalog has no active testable jump pad");
        }

        const auto packet = session.snapshot(60);
        const auto encoded = packet.encode();
        const auto decoded = fruityprime::net::SnapshotPacket::decode(encoded);
        if (!decoded || decoded->players.size() != 1
            || decoded->players[0].slot_index != 0) {
            throw std::runtime_error("gameplay snapshot round trip failed");
        }
        std::cout << "real gameplay: ticks=" << session.tick_count()
                  << " start_z=" << initial.z << " final_z=" << final.z << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "gameplay test failed: " << error.what() << '\n';
        return 1;
    }
}
