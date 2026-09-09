#include "../Features.hpp"
#include "../GameState.hpp"
#include "../Memory.hpp"
#include "../MemoryArrays.hpp"
#include "../MemoryClasses.hpp"
#include "../Messaging.hpp"
#include "../Read.hpp"
#include "../Renderer.hpp"
#include "../SceneSetup.hpp"
#include "../Shaders.hpp"
#include "../Menu.hpp"
#include "../Selection.hpp"
#include "../Strings.hpp"
#include "../Program.hpp"
#include "../Test.hpp"
#include "../Metadata/metadata.hpp"
#include "../Formats/Types.hpp"
#include "../Utility/utility.hpp"
#include "Mods/Network/net_scoreboard.hpp"

#include <cmath>
#include <array>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    try {
        MphReadNative::Features::Registry registry;
        registry.load({{"ProHud", "true"}, {"ReticleOpacity", "0.75"},
                       {"NoDoubleEnemyDeath", "false"}});
        require(registry.features.pro_hud
                    && std::abs(registry.features.reticle_opacity - 0.75F)
                        < 0.001F,
                "feature settings were not loaded");
        require(registry.features.effective_helmet_opacity() == 0.0F
                    && registry.features.effective_fixed_weapon(),
                "pro HUD effective values mismatch");
        require(registry.bugfixes.no_double_enemy_death == false,
                "bugfix settings were not loaded");
        require(registry.commit().at("ProHud") == "true",
                "feature settings were not committed");
        require(!MphReadNative::Features::parse_bool("1", false)
                    && MphReadNative::Features::parse_bool(" true ", false),
                "Features.cs Boolean.TryParse grammar mismatch");
        MphReadNative::Features::CheatSettings cheat_commit;
        cheat_commit.no_random_encounters = true;
        cheat_commit.free_weapon_select = true;
        require(cheat_commit.commit().at("FreeWeaponSelect") == "true"
                    && cheat_commit.commit().at("NoRandomEncounters") == "True",
                "Cheats.cs Boolean.ToString casing mismatch");

        fruityprime::features::Features::SetHelmetOpacity(0.6F);
        fruityprime::features::Features::Load(
            {{"ProHud", "true"}, {"ReticleOpacity", "0.25"},
             {"HudSway", "false"}});
        require(fruityprime::features::Features::ProHud
                    && fruityprime::features::Features::HelmetOpacity() == 0.0F
                    && fruityprime::features::Features::FixedWeapon()
                    && fruityprime::features::Features::HudSway
                    && fruityprime::features::Features::Commit().size() == 2,
                "Features static class did not preserve managed global semantics");
        fruityprime::features::Bugfixes::Load(
            {{"NoDoubleEnemyDeath", "false"}});
        fruityprime::features::Cheats::Load(
            {{"UnlimitedJumps", "true"}});
        require(!fruityprime::features::Bugfixes::NoDoubleEnemyDeath
                    && fruityprime::features::Cheats::UnlimitedJumps,
                "Bugfixes/Cheats static classes did not share process state");
        fruityprime::features::Features::ProHud = false;
        fruityprime::features::Features::ReticleOpacity = 1.0F;
        fruityprime::features::Features::SetHelmetOpacity(1.0F);
        fruityprime::features::Bugfixes::NoDoubleEnemyDeath = true;
        fruityprime::features::Cheats::UnlimitedJumps = false;

        MphReadNative::GameState state;
        state.begin_room("TEST ARENA", 7, true,
                         MphReadNative::Game::Mode::Battle);
        state.tick();
        state.set_trigger(17);
        require(state.multiplayer() && state.frame_count == 1
                    && state.trigger_set(17),
                "game state transition mismatch");

        MphReadNative::Game::State scoreboard_state;
        scoreboard_state.points[2] = 10;
        scoreboard_state.kills[2] = 11;
        scoreboard_state.deaths[2] = 12;
        scoreboard_state.player_time[2] = 13.0F;
        scoreboard_state.suicides[2] = 14;
        scoreboard_state.friendly_kills[2] = 15;
        scoreboard_state.headshot_kills[2] = 16;
        scoreboard_state.damage_count[2] = 17;
        scoreboard_state.alt_damage_count[2] = 18;
        scoreboard_state.beam_damage_dealt[2] = 19;
        scoreboard_state.beam_damage_max[2] = 20;
        scoreboard_state.octolith_scores[2] = 21;
        scoreboard_state.octolith_drops[2] = 22;
        scoreboard_state.octolith_stops[2] = 23;
        scoreboard_state.nodes_captured[2] = 24;
        scoreboard_state.nodes_lost[2] = 25;
        scoreboard_state.kills_as_prime[2] = 26;
        scoreboard_state.primes_killed[2] = 27;
        scoreboard_state.beam_kills[2][8] = 28;
        scoreboard_state.team_points[2] = 29;
        scoreboard_state.kill_streak[2] = 30;
        scoreboard_state.points[3] = 31;
        fruityprime::net::forget_scoreboard_slot(scoreboard_state, 2);
        require(scoreboard_state.points[2] == 0
                    && scoreboard_state.kills[2] == 0
                    && scoreboard_state.deaths[2] == 0
                    && scoreboard_state.player_time[2] == 0.0F
                    && scoreboard_state.suicides[2] == 0
                    && scoreboard_state.friendly_kills[2] == 0
                    && scoreboard_state.headshot_kills[2] == 0
                    && scoreboard_state.damage_count[2] == 0
                    && scoreboard_state.alt_damage_count[2] == 0
                    && scoreboard_state.beam_damage_dealt[2] == 0
                    && scoreboard_state.beam_damage_max[2] == 0
                    && scoreboard_state.octolith_scores[2] == 0
                    && scoreboard_state.octolith_drops[2] == 0
                    && scoreboard_state.octolith_stops[2] == 0
                    && scoreboard_state.nodes_captured[2] == 0
                    && scoreboard_state.nodes_lost[2] == 0
                    && scoreboard_state.kills_as_prime[2] == 0
                    && scoreboard_state.primes_killed[2] == 0
                    && scoreboard_state.beam_kills[2][8] == 0
                    && scoreboard_state.team_points[2] == 29
                    && scoreboard_state.kill_streak[2] == 30
                    && scoreboard_state.points[3] == 31,
                "network scoreboard slot was not cleared exactly");
        fruityprime::net::forget_scoreboard_slot(scoreboard_state, -1);
        fruityprime::net::forget_scoreboard_slot(
            scoreboard_state, static_cast<int>(MphReadNative::Game::SlotCapacity));
        require(scoreboard_state.points[3] == 31,
                "invalid network scoreboard slot changed another player");
        scoreboard_state.points[3] = 44;
        fruityprime::net::NetScoreboard::BindRuntime(&scoreboard_state);
        fruityprime::net::NetScoreboard::ForgetSlot(3);
        require(scoreboard_state.points[3] == 0,
                "static NetScoreboard did not clear the bound game state");
        state.paused = true;
        state.tick();
        require(state.frame_count == 1, "paused state advanced a frame");
        state.pause_prevented = false;
        state.unpause_menu();
        state.apply_pause();
        state.pause_menu();
        require(!state.paused && state.menu_pause,
                "game state menu pause was applied before ApplyPause");
        state.apply_pause();
        require(state.paused && state.menu_pause,
                "game state menu pause mismatch");
        state.unpause_menu();
        state.apply_pause();
        require(!state.paused && !state.menu_pause,
                "game state menu unpause mismatch");

        state.pause_dialog();
        require(!state.dialog_pause,
                "game state dialog pause was applied before ApplyPause");
        state.apply_pause();
        require(state.dialog_pause && state.paused,
                "game state dialog pause mismatch");
        state.unpause_dialog();
        require(state.dialog_pause,
                "game state dialog unpause was applied before ApplyPause");
        state.apply_pause();
        require(!state.dialog_pause && !state.paused,
                "game state dialog unpause mismatch");

        state.mode = MphReadNative::Game::Mode::Capture;
        state.setup();
        require(state.teams && state.is_octolith_mode()
                    && MphReadNative::GameState::is_team_mode(
                        MphReadNative::Game::Mode::Capture)
                    && state.mode_state
                        == MphReadNative::Game::ModeStateKind::Capture
                    && state.point_goal == 5
                    && std::abs(state.match_time - 900.0F) < 0.001F,
                "game state mode setup mismatch");
        state.active_players = 2;
        state.update_time(1.5F);
        require(std::abs(state.match_time - 898.5F) < 0.001F
                    && std::abs(state.player_time[0]) < 0.001F
                    && std::abs(state.player_time[1]) < 0.001F,
                "game state match countdown mismatch");
        state.story_save.boss_flags = 0U;
        require(state.area_state(0) == MphReadNative::Game::AreaState::None,
                "game state area state mismatch");
        state.update_boss_flags(0);
        require(state.area_state(0) == MphReadNative::Game::AreaState::Escape,
                "game state boss state transition mismatch");
        state.complete_random_encounter(39);
        require(state.completed_random_encounter_rooms[12],
                "game state encounter completion mismatch");
        state.complete_random_encounter(12);
        require(!state.completed_random_encounter_rooms[0],
                "invalid encounter room changed completion state");

        state.mode = MphReadNative::Game::Mode::BattleTeams;
        state.teams = true;
        std::array<fruityprime::net::PlayerState, 4> result_players{};
        result_players[0].slot_index = 0;
        result_players[0].flags = fruityprime::net::PlayerState::FlagActive;
        result_players[0].team = 0;
        result_players[0].points = 1;
        result_players[1].slot_index = 1;
        result_players[1].flags = fruityprime::net::PlayerState::FlagActive;
        result_players[1].team = 1;
        result_players[1].points = 3;
        result_players[2].slot_index = 2;
        result_players[2].flags = fruityprime::net::PlayerState::FlagActive;
        result_players[2].team = 0;
        result_players[2].points = 3;
        state.sync_players(result_players);
        require(state.active_players == 3
                    && state.result_slots[0] == 2
                    && state.result_slots[1] == 0
                    && state.result_slots[2] == 1
                    && state.team_points[0] == 4
                    && state.team_points[1] == 3
                    && state.standings[2] == 0
                    && state.standings[1] == 1
                    && state.team_standings[2] == 0
                    && state.team_standings[0] == 1,
                "game state result ordering mismatch");

        state.begin_room("TEST ARENA", 7, true,
                         MphReadNative::Game::Mode::Battle);
        state.point_goal = 0;
        state.match_time = 0.5F;
        state.match_clock_enabled = true;
        for (auto& player : result_players) {
            player.points = 0;
        }
        state.process_match_frame(0.25F, result_players, true);
        require(state.match_state == MphReadNative::Game::MatchState::InProgress
                    && std::abs(state.match_time - 0.25F) < 0.001F,
                "game state process frame advanced incorrectly");
        state.process_match_frame(0.30F, result_players, true);
        require(state.match_state == MphReadNative::Game::MatchState::GameOver,
                "game state timeout was not processed");

        MphReadNative::GameState survival_state;
        survival_state.begin_room("TEST ARENA", 7, true,
                                  MphReadNative::Game::Mode::SurvivalTeams);
        survival_state.match_clock_enabled = false;
        std::array<fruityprime::net::PlayerState, 3> survival_state_players{};
        survival_state_players[0].slot_index = 0;
        survival_state_players[0].flags =
            fruityprime::net::PlayerState::FlagActive;
        survival_state_players[0].team = 0;
        survival_state_players[0].health = 100;
        survival_state_players[1].slot_index = 1;
        survival_state_players[1].flags =
            fruityprime::net::PlayerState::FlagActive;
        survival_state_players[1].team = 1;
        survival_state_players[1].deaths = 2;
        survival_state_players[2].slot_index = 2;
        survival_state_players[2].flags =
            fruityprime::net::PlayerState::FlagActive;
        survival_state_players[2].team = 1;
        survival_state_players[2].deaths = 2;
        survival_state.process_match_frame(0.0F, survival_state_players, true);
        require(survival_state.match_state
                    == MphReadNative::Game::MatchState::GameOver
                    && survival_state.player_time[0] == -1.0F
                    && survival_state.team_time[0] == -1.0F,
                "survival team-death completion mismatch");

        MphReadNative::GameState story_message_state;
        story_message_state.begin_room("UNIT1_LAND", 45, false,
                                       MphReadNative::Game::Mode::Story);
        MphReadNative::MessageInfo story_trigger;
        story_trigger.cartridge_message = static_cast<std::uint32_t>(
            fruityprime::formats::Message::SetTriggerState);
        story_trigger.parameter1 = 10;
        story_message_state.process_message(story_trigger);
        require((story_message_state.story_save.trigger_state[1] & 0x04)
                    != 0,
                "cartridge trigger-set message was not applied");
        story_trigger.cartridge_message = static_cast<std::uint32_t>(
            fruityprime::formats::Message::ClearTriggerState);
        story_message_state.process_message(story_trigger);
        require((story_message_state.story_save.trigger_state[1] & 0x04)
                    == 0,
                "cartridge trigger-clear message was not applied");

        MphReadNative::MessageInfo checkpoint;
        checkpoint.cartridge_message = static_cast<std::uint32_t>(
            fruityprime::formats::Message::Checkpoint);
        checkpoint.sender = 123;
        story_message_state.process_message(checkpoint);
        require(story_message_state.story_save.checkpoint_entity_id == 123
                    && story_message_state.story_save.checkpoint_room_id == 45,
                "story checkpoint message was not recorded");

        MphReadNative::MessageInfo escape;
        escape.cartridge_message = static_cast<std::uint32_t>(
            fruityprime::formats::Message::EscapeUpdate1);
        escape.parameter1 = 60;
        escape.parameter2 = static_cast<std::int32_t>(
            MphReadNative::Game::EscapeState::Escape);
        story_message_state.process_message(escape);
        require(story_message_state.escape_state
                    == MphReadNative::Game::EscapeState::Escape
                    && std::abs(story_message_state.escape_timer - 60.0F)
                        < 0.001F
                    && (story_message_state.story_save.trigger_state[2]
                        & 0x80) != 0,
                "story escape message was not applied");
        story_message_state.process_story_frame(1.0F);
        require(std::abs(story_message_state.escape_timer - 59.0F) < 0.001F,
                "story escape timer did not advance");
        escape.cartridge_message = static_cast<std::uint32_t>(
            fruityprime::formats::Message::EscapeUpdate2);
        escape.parameter1 = 0;
        story_message_state.process_message(escape);
        require(story_message_state.escape_paused,
                "story escape pause toggle was not applied");
        escape.parameter1 = 1;
        escape.parameter2 = static_cast<std::int32_t>(
            MphReadNative::Game::EscapeState::None);
        story_message_state.process_message(escape);
        require(story_message_state.escape_state
                    == MphReadNative::Game::EscapeState::None
                    && story_message_state.escape_timer == -1.0F,
                "story escape reset message was not applied");

        story_message_state.story_save.current_octoliths = 0xff;
        MphReadNative::MessageInfo unlock;
        unlock.cartridge_message = static_cast<std::uint32_t>(
            fruityprime::formats::Message::UnlockOubliette);
        story_message_state.process_message(unlock);
        require(story_message_state.pause_prevented
                    && story_message_state.queued_oubliette_unlock_message,
                "Oubliette unlock message was not queued");
        unlock.cartridge_message = static_cast<std::uint32_t>(
            fruityprime::formats::Message::LoadOubliette);
        story_message_state.process_message(unlock);
        require(story_message_state.transition_room_id == 91
                    && story_message_state.transition_state
                        == MphReadNative::Game::TransitionState::Start,
                "Oubliette load message did not request a transition");
        unlock.cartridge_message = static_cast<std::uint32_t>(
            fruityprime::formats::Message::Complete);
        story_message_state.match_time = 30.0F;
        story_message_state.process_message(unlock);
        require(story_message_state.match_time == 0.0F,
                "global complete message did not stop the timer");

        story_message_state.story_save.trigger_state[2] = 0xff;
        story_message_state.story_save.boss_flags =
            static_cast<std::uint32_t>(MphReadNative::Game::BossFlags::Unit1B1Kill)
            | static_cast<std::uint32_t>(MphReadNative::Game::BossFlags::Unit4B2Kill);
        story_message_state.enter_ship();
        const auto enter_ship_flags = story_message_state.story_save.boss_flags;
        require((story_message_state.story_save.trigger_state[2] & 0x80) == 0
                    && (enter_ship_flags
                        & static_cast<std::uint32_t>(MphReadNative::Game::BossFlags::Unit1B1Kill)) == 0
                    && (enter_ship_flags
                        & static_cast<std::uint32_t>(MphReadNative::Game::BossFlags::Unit1B1Done)) != 0
                    && (enter_ship_flags
                        & static_cast<std::uint32_t>(MphReadNative::Game::BossFlags::Unit4B2Done)) != 0,
                "enter-ship story flag transition was not applied");

        story_message_state.escape_state =
            MphReadNative::Game::EscapeState::Escape;
        story_message_state.escape_timer = 12.0F;
        story_message_state.escape_paused = true;
        story_message_state.reset_escape_state();
        require(story_message_state.escape_state
                    == MphReadNative::Game::EscapeState::None
                    && story_message_state.escape_timer == -1.0F
                    && !story_message_state.escape_paused,
                "story escape state reset did not clear all fields");

        MphReadNative::Game::StorySave save;
        require(save.init_room_state(27, 4, true) == 2
                    && save.room_state_value(27, 4) == 2,
                "story room state initialization mismatch");
        save.set_room_state(27, 4, 1);
        save.set_visited_room(27);
        save.set_visited_connector(33, 0);
        save.update_found_octolith(2);
        save.update_found_artifact(1, 2);
        require(save.visited_room(27) && save.visited_connector(33, 0)
                    && save.found_octolith(2)
                    && save.found_artifact(1, 2)
                    && save.found_octolith_count() == 1,
                "story save collection state mismatch");
        const auto scans_before = save.scan_count;
        save.set_scan_category(300, 'L');
        save.update_logbook(300);
        require(save.logbook_found(300)
                    && save.scan_count == scans_before + 1,
                "story save scan was not added to the logbook");
        save.update_logbook(300);
        require(save.scan_count == scans_before + 1,
                "story save scan was counted more than once");
        MphReadNative::Game::StorySave save_copy;
        save.copy_to(save_copy);
        require(save_copy.room_state_value(27, 4) == 0
                    && save_copy.completion_percentage() >= 0,
                "story save copy mismatch");

        MphReadNative::Game::StorySave default_save;
        require(default_save.weapons == 0x0005
                    && default_save.weapon_slots[0] == 0
                    && default_save.weapon_slots[1] == 2,
                "story save weapon ordinals do not match the cartridge");
        MphReadNative::Game::State hunter_state;
        hunter_state.reset();
        default_save.boss_flags = 2; // area 0 is clear
        default_save.defeated_hunters = 1u << 1;
        MphReadNative::Rng::Rng hunter_rng;
        MphReadNative::SceneSetup::update_area_hunters(
            hunter_state, default_save, hunter_rng, true);
        require((default_save.area_hunters[0] & (1u << 1)) != 0
                    && !hunter_state.completed_random_encounter_rooms[0],
                "area hunter placement did not follow the story save");

        MphReadNative::Features::Registry hunter_settings;
        MphReadNative::Game::StorySave encounter_save;
        encounter_save.boss_flags = 2;
        encounter_save.area_hunters[0] = (1u << 1) | (1u << 2);
        std::array<MphReadNative::SceneSetup::StoryHunterSpawner, 2>
            hunter_spawners{{
                {11, {}, {}, {}, 1, 2, 255, 60, 75, 20, 0, 100},
                {12, {}, {}, {}, 8, 1, 255, 80, 90, 25, 2, 100}
            }};
        MphReadNative::Rng::Rng encounter_rng;
        const auto hunter_plan = MphReadNative::SceneSetup::select_hunter_spawns(
            hunter_spawners, 27, 0, hunter_state, encounter_save,
            hunter_settings.features, hunter_settings.cheats, encounter_rng,
            MphReadNative::Metadata::Hunter::Kanden, 0);
        require(hunter_plan.spawns.size() == 2
                    && hunter_plan.spawns[0].hunter
                        == MphReadNative::Metadata::Hunter::Kanden
                    && hunter_plan.spawns[0].color == 1
                    && hunter_plan.spawns[0].health == 60,
                "story hunter spawner selection mismatch");

        hunter_state.completed_random_encounter_rooms[0] = true;
        hunter_settings.features.no_repeat_encounters = true;
        const auto blocked_plan = MphReadNative::SceneSetup::select_hunter_spawns(
            std::span<const MphReadNative::SceneSetup::StoryHunterSpawner>(
                hunter_spawners).subspan(1),
            27, 0, hunter_state, encounter_save, hunter_settings.features,
            hunter_settings.cheats, encounter_rng,
            MphReadNative::Metadata::Hunter::Samus, 0);
        require(blocked_plan.spawns.empty()
                    && blocked_plan.blocked_by_completed_random_encounter,
                "completed random encounter was not blocked");

        const MphReadNative::Types::Vector3 forward{0.0F, 0.0F, 1.0F};
        const auto transform = MphReadNative::Types::MatrixOps::get_transform4(
            forward, {0.0F, 1.0F, 0.0F}, {3.0F, 4.0F, 5.0F});
        const auto transformed = MphReadNative::Types::MatrixOps::vec3_mult_mtx4(
            {}, transform);
        require(std::abs(transformed.x - 3.0F) < 0.001F
                    && std::abs(transformed.y - 4.0F) < 0.001F
                    && std::abs(transformed.z - 5.0F) < 0.001F,
                "native format matrix transform mismatch");

        MphReadNative::Rng::Rng rng;
        const auto first = rng.random1(100);
        const auto second = rng.random1(100);
        require(first < 100 && second < 100 && first != second,
                "native RNG sequence mismatch");
        const auto shake = rng.damage_shake(100);
        require(shake.frames > 0 && shake.calls == shake.frames * 3
                    && shake.before != shake.after,
                "native camera shake mismatch");

        MphReadNative::Messaging::Queue queue;
        require(queue.send_delayed(
                    MphReadNative::Messaging::Message::Activate, 1, 2, 3, 4,
                    10, 2),
                "message was not queued");
        MphReadNative::Messaging::Queue overload_queue;
        require(overload_queue.send(
                    MphReadNative::Messaging::Message::Activate, 1, -1, 3, 4,
                    10)
                    && overload_queue.entries().front().execute_frame == 11
                    && overload_queue.send_delayed(
                           MphReadNative::Messaging::Message::Activate, 1, -1,
                           3, 4, 10, 0)
                    && overload_queue.entries().back().execute_frame == 10,
                "Messaging.cs overload scheduling mismatch");
        std::int32_t observed = 0;
        require(queue.dispatch_due(11, [&](const MphReadNative::MessageInfo& info) {
                    observed = info.parameter1;
                }) == 0,
                "message dispatched before its frame");
        require(queue.dispatch_due(12, [&](const MphReadNative::MessageInfo& info) {
                    observed = info.parameter1;
                }) == 1
                    && observed == 3,
                "message dispatch mismatch");

        MphReadNative::MemoryBuffer memory(64);
        memory.write_u32_le(4, 0x12345678U);
        require(memory.read_u32_le(4) == 0x12345678U,
                "memory endian access mismatch");
        MphReadNative::MemoryArray array(memory, 4, 4);
        require(array.get(0) == 0x78, "memory array view mismatch");
        MphReadNative::MemoryLayout layout;
        require(layout.add("value", 4, 4), "memory layout field was rejected");
        MphReadNative::MemoryObject object(memory, layout);
        require(object.read_u32("value") == 0x12345678U,
                "memory class read mismatch");
        object.write_u32("value", 9);
        require(memory.read_u32_le(4) == 9, "memory class write mismatch");

        MphReadNative::MemoryClass typed_memory(memory, 8, 0x8000);
        typed_memory.write_i16(0, -1234);
        typed_memory.write_u32(2, 0xdeadbeefU);
        typed_memory.write_vec3(6, {1.25F, -2.5F, 3.75F});
        typed_memory.write_vec4(18, {4.0F, 5.0F, -6.0F, 7.0F});
        require(typed_memory.address() == 0x8000
                    && typed_memory.read_i16(0) == -1234
                    && typed_memory.read_u32(2) == 0xdeadbeefU,
                "typed memory primitive access mismatch");
        const auto vector = typed_memory.read_vec3(6);
        const auto vector4 = typed_memory.read_vec4(18);
        require(std::abs(vector.x - 1.25F) < 0.001F
                    && std::abs(vector.y + 2.5F) < 0.001F
                    && std::abs(vector.z - 3.75F) < 0.001F
                    && std::abs(vector4.w - 7.0F) < 0.001F,
                "fixed-point memory vector access mismatch");
        MphReadNative::UInt16Array words(memory, 0, 2);
        words.set(0, 0xabcdU);
        words.set(1, 0x1234U);
        require(words.get(0) == 0xabcdU && words.get(1) == 0x1234U,
                "typed memory array access mismatch");

        const std::array<std::uint8_t, 8> text{
            'N', 'a', 't', 'i', 'v', 'e', 0, 0};
        require(MphReadNative::Read::string(text, 0, text.size()) == "Native",
                "read string mismatch");

        MphReadNative::Renderer::NullRenderer renderer;
        require(renderer.begin({1280, 720}), "renderer did not begin");
        renderer.draw({42, MphReadNative::Renderer::Layer::Hud, 1, 2, 3,
                       1.0F, true});
        renderer.end();
        require(renderer.frame().items().size() == 1
                    && renderer.frame().viewport().aspect() > 1.7F,
                "renderer frame mismatch");
        require(MphReadNative::Shaders::has_required_entry_points(
                    MphReadNative::Shaders::Program::Hud),
                "shader source contract mismatch");

        MphReadNative::MenuModel menu;
        menu.set_game_files_ready(true);
        menu.open(MphReadNative::Menu::Page::Home);
        require(menu.items().size() == 6 && menu.items()[0].enabled,
                "menu model mismatch");
        menu.move_selection(2);
        require(menu.activate() == MphReadNative::Menu::Action::OpenSettings,
                "menu selection mismatch");

        MphReadNative::MenuConfiguration menu_configuration;
        menu_configuration.load(MphReadNative::MenuSettings{});
        menu_configuration.update_settings();
        require(menu_configuration.mode() == "auto-select"
                    && menu_configuration.goal_type() == "Point Goal"
                    && menu_configuration.point_goal() == 7
                    && menu_configuration.time_limit() == 420
                    && !menu_configuration.teams(),
                "Menu.cs default settings mismatch");
        auto menu_defaults = menu_configuration.commit();
        require(menu_defaults.Language == "English"
                    && menu_defaults.MusicVolume == "0.50"
                    && menu_defaults.Planets == "CA"
                    && menu_defaults.Player1 == "Samus 0",
                "Menu.cs default commit mismatch");

        MphReadNative::MenuSettings numeric_language;
        numeric_language.Language = "1";
        MphReadNative::MenuConfiguration numeric_language_menu(
            numeric_language);
        require(numeric_language_menu.language()
                    == fruityprime::formats::Language::Japanese,
                "Menu.cs numeric Language parsing mismatch");
        numeric_language.Language = "6";
        MphReadNative::MenuConfiguration undefined_language_menu(
            numeric_language);
        require(undefined_language_menu.commit().Language == "6",
                "Menu.cs undefined numeric Language formatting mismatch");
        numeric_language.Language = " English ";
        MphReadNative::MenuConfiguration spaced_language_menu(
            numeric_language);
        require(spaced_language_menu.commit().Language == "English",
                "Menu.cs spaced Language enum parsing mismatch");
        numeric_language.Language = "English,Japanese";
        MphReadNative::MenuConfiguration combined_language_menu(
            numeric_language);
        require(combined_language_menu.commit().Language == "Japanese",
                "Menu.cs combined Language enum parsing mismatch");
        numeric_language.Language = "-1";
        MphReadNative::MenuConfiguration negative_language_menu(
            numeric_language);
        require(negative_language_menu.commit().Language == "-1",
                "Menu.cs negative Language enum parsing mismatch");

        MphReadNative::MenuSettings numeric_save_when;
        numeric_save_when.SaveFromExit = "10";
        MphReadNative::MenuConfiguration numeric_save_when_menu(
            numeric_save_when);
        require(numeric_save_when_menu.SaveFromExit
                    == static_cast<fruityprime::formats::SaveWhen>(10)
                    && numeric_save_when_menu.commit().SaveFromExit == "10",
                "Menu.cs numeric SaveWhen parsing mismatch");

        fruityprime::settings::MenuSettings feature_settings;
        feature_settings.features_json =
            R"({"ReticleOpacity":"0.25","ProHud":"TRUE"})";
        MphReadNative::MenuConfiguration feature_menu;
        feature_menu.load_native(feature_settings);
        require(feature_menu.feature_registry().features.pro_hud
                    && feature_menu.feature_registry().features.reticle_opacity
                        == 0.25F,
                "Menu.cs feature settings parsing mismatch");

        require(menu_configuration.read_mode("Survival Teams"),
                "Menu.cs mode parser rejected a known mode");
        menu_configuration.update_settings();
        require(menu_configuration.goal_type() == "Extra Lives"
                    && menu_configuration.point_goal() == 2
                    && menu_configuration.time_limit() == 900
                    && menu_configuration.teams(),
                "Menu.cs mode defaults mismatch");

        menu_configuration.set_mode("Battle");
        menu_configuration.update_settings();
        require(menu_configuration.read_player("trace 99", 0),
                "Menu.cs player parser rejected a hunter");
        require(menu_configuration.commit().Player1 == "Trace 5",
                "Menu.cs player recolor clamp mismatch");
        menu_configuration.set_mode("Battle Teams");
        menu_configuration.update_settings();
        require(menu_configuration.read_player("trace red", 0)
                    && menu_configuration.commit().Player1 == "Trace orange",
                "Menu.cs team parser mismatch");

        MphReadNative::MenuConfiguration numeric_hunter_menu;
        numeric_hunter_menu.set_mode("Battle");
        numeric_hunter_menu.update_settings();
        require(numeric_hunter_menu.read_player("2 0", 0)
                    && numeric_hunter_menu.commit().Player1 == "Trace 0",
                "Menu.cs numeric Hunter enum order mismatch");

        MphReadNative::MenuSettings custom;
        custom.Planets = "ca, alinos";
        custom.Models = "KandenGun 99, unknown_model 3, Crate01 99";
        custom.Weapons = "power beam, omega cannon";
        custom.Octoliths = "ca1, arcterra2";
        MphReadNative::MenuConfiguration custom_menu(custom);
        const auto custom_commit = custom_menu.commit();
        require(custom_menu.planets()[0] == 1 && custom_menu.planets()[1] == 1
                    && custom_menu.planets()[2] == 0
                    && custom_commit.Models == "KandenGun 5, Crate01 0"
                    && custom_commit.Weapons == "Power Beam,Omega Cannon"
                    && custom_commit.Octoliths == "CA1,Arcterra2",
                "Menu.cs model/story list parsing mismatch");

        const std::array<MphReadNative::Strings::TableEntry, 8> logbook_table{{
            {"L000", 'S', "lore title", "lore text", 0, 'L', "lore title", "lore text"},
            {"B001", 'S', "bio title", "bio text", 0, 'B', "bio title", "bio text"},
            {"O002", 'S', "object title", "object text", 0, 'O', "object title", "object text"},
            {"E003", 'S', "equip title", "equip text", 0, 'E', "equip title", "equip text"},
            {"X004", 'S', "ignored 1", "ignored 1", 0, 'X', "ignored 1", "ignored 1"},
            {"X005", 'S', "ignored 2", "ignored 2", 0, 'X', "ignored 2", "ignored 2"},
            {"X006", 'S', "ignored 3", "ignored 3", 0, 'X', "ignored 3", "ignored 3"},
            {"L007", 'S', "locked title", "locked text", 0, 'L', "locked title", "locked text"},
        }};
        const auto logbook_entries =
            MphReadNative::MenuConfiguration::logbook_entries(
                MphReadNative::Game::StorySave{}, logbook_table);
        require(logbook_entries[0].size() == 2
                    && logbook_entries[0][0].title == "lore title"
                    && logbook_entries[0][1].title == "-"
                    && logbook_entries[1][0].log == "bio text"
                    && logbook_entries[2][0].title == "object title"
                    && logbook_entries[3][0].title == "equip title",
                "Menu.cs logbook entry filtering mismatch");

        MphReadNative::MenuSettings model_separator_settings;
        model_separator_settings.Models = "Crate01\t99";
        MphReadNative::MenuConfiguration model_separator_menu(
            model_separator_settings);
        require(model_separator_menu.commit().Models.empty(),
                "Menu.cs model parser accepted a tab as a field separator");

        MphReadNative::MenuSettings model_directory_settings;
        model_directory_settings.Models = "big_kanden 0 CharSelect";
        MphReadNative::MenuConfiguration model_directory_menu(
            model_directory_settings);
        require(model_directory_menu.commit().Models == "big_kanden 0",
                "Menu.cs MetaDir parsing mismatch");
        model_directory_settings.Models = "big_kanden 0 CharSelect invalid";
        MphReadNative::MenuConfiguration model_directory_reset_menu(
            model_directory_settings);
        require(model_directory_reset_menu.commit().Models.empty(),
                "Menu.cs failed MetaDir parse did not reset to Models");
        model_directory_settings.Models = "KandenGun 0 CharSelect";
        MphReadNative::MenuConfiguration model_unknown_directory_menu(
            model_directory_settings);
        require(model_unknown_directory_menu.commit().Models.empty(),
                "Menu.cs non-Models MetaDir was treated as Models");

        MphReadNative::MenuSettings decimal_settings;
        decimal_settings.SfxVolume = "1e2";
        decimal_settings.MusicVolume = "0x1p2";
        MphReadNative::MenuConfiguration decimal_menu(decimal_settings);
        require(decimal_menu.commit().SfxVolume == "0.35"
                    && decimal_menu.commit().MusicVolume == "0.50",
                "Menu.cs decimal parser accepted a non-decimal literal");

        decimal_settings.SfxVolume = "1,00";
        MphReadNative::MenuConfiguration grouped_decimal_menu(
            decimal_settings);
        require(grouped_decimal_menu.commit().SfxVolume == "100",
                "Menu.cs decimal group separator mismatch");
        decimal_settings.SfxVolume = "1,000.50";
        MphReadNative::MenuConfiguration scaled_grouped_decimal_menu(
            decimal_settings);
        require(scaled_grouped_decimal_menu.commit().SfxVolume == "1000.50",
                "Menu.cs decimal group scale mismatch");
        decimal_settings.SfxVolume = "-0.00";
        MphReadNative::MenuConfiguration negative_zero_decimal_menu(
            decimal_settings);
        require(negative_zero_decimal_menu.commit().SfxVolume == "0.00",
                "Menu.cs decimal negative-zero formatting mismatch");

        custom_menu.set_apply_settings(true);
        MphReadNative::Game::State menu_state;
        custom_menu.set_point_goal(17);
        custom_menu.set_time_goal(90);
        custom_menu.set_time_limit(600);
        custom_menu.apply_multiplayer_settings(menu_state);
        require(menu_state.point_goal == 17
                    && menu_state.time_goal == 90.0F
                    && menu_state.match_time == 600.0F,
                "Menu.cs multiplayer application mismatch");

        custom_menu.feature_registry().features.pro_hud = true;
        custom_menu.feature_registry().features.fixed_crosshair = true;
        custom_menu.feature_registry().features.reticle_opacity = 0.25F;
        custom_menu.reset_features();
        require(custom_menu.feature_registry().features.pro_hud
                    && custom_menu.feature_registry().features.fixed_crosshair
                    && custom_menu.feature_registry().features.reticle_opacity
                        == 1.0F,
                "Menu.cs feature reset scope mismatch");
        custom_menu.set_planet(1, 1);
        custom_menu.set_boss_state(0, 2);
        custom_menu.set_checkpoint_id(123);
        custom_menu.set_health_max(799);
        custom_menu.set_missile_max(950);
        custom_menu.set_ua_max(4000);
        custom_menu.set_weapon(8, 1);
        custom_menu.set_octolith(7, 1);
        custom_menu.reset_adventure_settings();
        require(custom_menu.planets()[0] == 1
                    && custom_menu.planets()[1] == 0
                    && custom_menu.boss_states()[0] == 0
                    && custom_menu.checkpoint_id() == -1
                    && custom_menu.health_max() == 99
                    && custom_menu.missile_max() == 50
                    && custom_menu.ua_max() == 400
                    && custom_menu.weapons()[0] == 1
                    && custom_menu.weapons()[2] == 1
                    && custom_menu.weapons()[8] == 0
                    && custom_menu.octoliths()[7] == 0,
                "Menu.cs adventure reset mismatch");
        custom_menu.set_mode("Battle Teams");
        custom_menu.update_settings();
        custom_menu.set_radar_players(true);
        custom_menu.set_damage_level(2);
        custom_menu.set_friendly_fire(true);
        custom_menu.set_affinity_weapons(true);
        custom_menu.set_point_goal(100);
        custom_menu.set_time_limit(3600);
        custom_menu.reset_match_settings();
        require(custom_menu.teams()
                    && custom_menu.point_goal() == 7
                    && custom_menu.time_limit() == 420
                    && !custom_menu.radar_players()
                    && custom_menu.damage_level() == 1
                    && !custom_menu.friendly_fire()
                    && !custom_menu.affinity_weapons(),
                "Menu.cs match reset mismatch");
        double parsed_time = 0;
        require(MphReadNative::Menu::parse_time("+1:30", parsed_time)
                    && parsed_time == 90
                    && MphReadNative::Menu::format_time(parsed_time) == "1:30"
                    && MphReadNative::Menu::format_time(89.9996) == "1:30"
                    && MphReadNative::Menu::format_time(24.0 * 60.0 * 60.0)
                        == "0:00",
                "Menu.cs time parser/formatter mismatch");

        MphReadNative::SelectionState selection;
        selection.select({MphReadNative::Selection::Type::Entity, 5});
        require(selection.is_selected(
                    {MphReadNative::Selection::Type::Entity, 5})
                    && MphReadNative::Selection::State::color(
                           MphReadNative::Selection::Type::Parent).red == 1.0F,
                "selection state mismatch");
        selection.select_path({5, 8, 13, 21});
        require(selection.check_volume(5)
                    && selection.check_volume(6)
                    && selection.check_selection({5, 8, 13, 21}, 4, 6)
                           == MphReadNative::Selection::SelectionType::Selected,
                "selection hierarchy mismatch");
        selection.toggle_hide_unselected_volumes();
        require(selection.check_volume(5) && !selection.check_volume(6),
                "selection volume filter mismatch");
        require(selection.selection_color(
                            MphReadNative::Selection::SelectionType::Selected, 0)
                        ->blue == 1.0F
                    && selection.selection_color(
                           MphReadNative::Selection::SelectionType::Parent, 500)
                           ->green == 0.0F,
                "selection pulse color mismatch");

        const auto strings = MphReadNative::StringCatalog::builtins();
        require(strings.contains(MphReadNative::Strings::Language::Japanese,
                                 "play")
                    && strings.lookup(MphReadNative::Strings::Language::German,
                                      "settings")
                        == "Einstellungen",
                "string catalog mismatch");

        MphReadNative::ProgramArguments arguments(
            std::vector<std::string>{"FruityPrime", "-mapgen"});
        require(arguments.action() == MphReadNative::Program::Action::MapGen,
                "program argument dispatch mismatch");
        MphReadNative::ProgramArguments movie_arguments(
            std::vector<std::string>{"FruityPrime", "-export", "movie"});
        require(movie_arguments.action()
                    == MphReadNative::Program::Action::MovieInfo,
                "movie export argument dispatch mismatch");

        const std::vector<std::string> managed_args{
            "noise", "-alpha", "one", "two", "-beta", "-gamma",
            "-delta", "last", "-", "--double"};
        const auto parsed_args =
            MphReadNative::Program::parse_arguments(managed_args);
        require(parsed_args.size() == 5
                    && parsed_args[0].Name == "alpha"
                    && parsed_args[0].ValueOne == "one"
                    && parsed_args[0].ValueTwo == "two"
                    && parsed_args[1].Name == "beta"
                    && !parsed_args[1].ValueOne.has_value()
                    && parsed_args[2].Name == "gamma"
                    && !parsed_args[2].ValueOne.has_value()
                    && parsed_args[3].Name == "delta"
                    && parsed_args[3].ValueOne == "last"
                    && parsed_args[4].Name == "-double",
                "Program.cs argument scan mismatch");

        const std::vector<std::string> duplicate_args{
            "-room", "-next", "-room", "  +2147483647 ",
            "-model", "Ship", "  -2147483648 ", "-model", "Crate",
            "overflow"};
        const auto duplicate_parsed =
            MphReadNative::Program::parse_arguments(duplicate_args);
        require(!MphReadNative::Program::try_get_string(
                    duplicate_parsed, "room", "r")
                    .has_value()
                && MphReadNative::Program::try_get_int(
                       duplicate_parsed, "room", "r")
                       == std::nullopt,
                "Program.cs first-match argument behavior mismatch");
        require(MphReadNative::Program::try_get_int(
                    duplicate_parsed, "room", "r")
                    == std::nullopt,
                "Program.cs Int32.TryParse null behavior mismatch");
        const auto models = MphReadNative::Program::get_pairs(
            duplicate_parsed, "model", "m");
        require(models.size() == 2 && models[0].first == "Ship"
                    && models[0].second == -2147483648
                    && models[1].first == "Crate"
                    && models[1].second == 0,
                "Program.cs model pair parsing mismatch");
        require(MphReadNative::Program::check_version("0.19.0.0")
                    && MphReadNative::Program::check_version("0.35.1.0")
                    && !MphReadNative::Program::check_version("0.18.9.0")
                    && MphReadNative::Program::check_version("0.35")
                    && MphReadNative::Program::parse_version("0.35.1.0")
                           == MphReadNative::Program::CurrentVersion,
                "program setup version check mismatch");
        require(MphReadNative::SceneSetup::find_room("MP1 SANCTORUS") != nullptr,
                "scene setup did not resolve the room catalog");
        require(MphReadNative::SceneSetup::find_room("UNIT1_LAND") != nullptr,
                "scene setup did not resolve a story room catalog entry");
        require(MphReadNative::SceneSetup::resolve_node_data_path(
                    "default.bin", 28,
                    MphReadNative::Game::Mode::SinglePlayer, true)
                    == R"(levels\nodeData\unit1_C0_Boss_Node.bin)"
                && MphReadNative::SceneSetup::resolve_node_data_path(
                    "default.bin", 93,
                    MphReadNative::Game::Mode::Capture)
                    == R"(levels\nodeData\mp1_CTF_node.bi))"
                && MphReadNative::SceneSetup::resolve_node_data_path(
                    "default.bin", 107,
                    MphReadNative::Game::Mode::Nodes)
                    == R"(levels\nodeData\mp14_KOTH_node.bin)"
                && MphReadNative::SceneSetup::resolve_node_data_path(
                    "default.bin", 28,
                    MphReadNative::Game::Mode::SinglePlayer, false)
                    == "default.bin",
                "scene setup node-data override rules changed");

        MphReadNative::TestRunner runner;
        runner.add("pass", [] {});
        require(runner.all_passed(), "test runner did not pass its case");

        std::cout << "native core source tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native core source tests failed: " << error.what()
                  << '\n';
        return 1;
    }
}
