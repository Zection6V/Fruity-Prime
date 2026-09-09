#include "Assets/game_assets.hpp"
#include "Entities/gameplay.hpp"
#include "Mods/Launcher/Portable/launch_plan.hpp"
#include "Mods/Launcher/Portable/match_start.hpp"
#include "Entities/room_catalog.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

int main() {
    using fruityprime::launcher::LaunchKind;
    using fruityprime::launcher::LaunchPlan;
    using fruityprime::launcher::MatchStart;
    using fruityprime::launcher::MatchStartOptions;
    using fruityprime::metadata::Hunter;

    LaunchPlan offline;
    offline.kind = LaunchKind::Offline;
    offline.hunter = Hunter::Kanden;
    offline.room_key = "UNIT1_C0";
    offline.mode = 4;
    offline.bots = 7;
    offline.bot_level = 2;
    MatchStartOptions options;
    options.time_limit_seconds = 123.0F;
    options.point_goal = 11;
    options.friendly_fire = true;
    options.objective_authority = false;
    const auto prepared = MatchStart::prepare(offline, options);
    assert(prepared.ok);
    assert(prepared.kind == LaunchKind::Offline);
    assert(prepared.room_key == "UNIT1_C0");
    assert(prepared.mode == 4);
    assert(prepared.rules.team_mode);
    assert(prepared.rules.time_limit_seconds == 123.0F);
    assert(prepared.rules.point_goal == 11);
    assert(prepared.gameplay.friendly_fire);
    assert(!prepared.gameplay.objective_authority);
    assert(prepared.players.size() == 8);
    assert(prepared.players[0].hunter == Hunter::Kanden);
    assert(prepared.players[0].slot == 0);
    assert(prepared.players[7].bot && prepared.players[7].bot_level == 2);

    LaunchPlan survival = offline;
    survival.mode = 5;
    survival.bots = 0;
    const auto survival_defaults = MatchStart::prepare(survival);
    assert(survival_defaults.ok);
    assert(survival_defaults.rules.time_limit_seconds == 900.0F);
    assert(survival_defaults.rules.point_goal == 2);

    LaunchPlan online;
    online.kind = LaunchKind::Online;
    online.hunter = Hunter::Samus;
    online.room_key = "LOCAL_MENU_ROOM";
    online.mode = 3;
    online.bots = 7;
    const auto authoritative = MatchStart::prepare(
        online, {}, "SERVER_ROOM", static_cast<std::uint8_t>(14));
    assert(authoritative.ok);
    assert(authoritative.room_key == "SERVER_ROOM");
    assert(authoritative.mode == 14);
    assert(authoritative.players.size() == 1);

    MatchStartOptions remote_slot_options;
    remote_slot_options.local_slot = 3;
    const auto remote_slot = MatchStart::prepare(
        online, remote_slot_options);
    assert(remote_slot.ok && remote_slot.players.front().slot == 3);

    LaunchPlan adventure;
    adventure.kind = LaunchKind::Adventure;
    adventure.hunter = Hunter::Trace;
    adventure.room_key = "UNIT2_RM4";
    adventure.mode = 3;
    adventure.bots = 7;
    const auto story = MatchStart::prepare(adventure);
    assert(story.ok);
    assert(story.kind == LaunchKind::Adventure);
    assert(story.room_key == "UNIT2_RM4");
    assert(story.mode == 2);
    assert(!story.rules.team_mode);
    assert(story.players.size() == 1);
    assert(!story.players.front().bot);

    LaunchPlan random;
    random.kind = LaunchKind::Offline;
    random.hunter = Hunter::Random;
    random.room_key = "UNIT1_C0";
    const auto rejected = MatchStart::prepare(random);
    assert(!rejected.ok);
    assert(!rejected.error.empty());

    const char* rom_value = std::getenv("FRUITY_PRIME_TEST_NDS");
    if (rom_value != nullptr && rom_value[0] != '\0') {
        const auto assets = fruityprime::assets::Store::from_rom(rom_value);
        const auto* entry = fruityprime::scene::find_room("UNIT1_C0");
        assert(entry != nullptr);
        const auto room = fruityprime::scene::Room::load(
            assets, entry->definition);
        fruityprime::gameplay::Session session(room, prepared.gameplay);
        std::vector<std::uint8_t> bots;
        std::string error;
        assert(MatchStart::populate(session, prepared, &bots, &error));
        assert(error.empty());
        assert(session.players().size() == prepared.players.size());
        assert(bots.size() == 7);
    }

    std::cout << "native match start tests passed\n";
    return 0;
}
