#include "Mods/Network/net_feature_check.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <sstream>
#include <string>

int main() {
    using fruityprime::metadata::Hunter;
    using fruityprime::net::NetConfig;
    using fruityprime::net::NetFeatureCheck;
    using fruityprime::net::PlayerState;
    using fruityprime::net::TestPhase;

    NetFeatureCheck check;
    std::array<std::string, NetConfig::SlotCapacity> names{};
    names[0] = "local";
    names[1] = "remote";

    for (int frame = 0; frame < 140; ++frame) {
        NetFeatureCheck::Frame observation;
        observation.local_slot = 0;
        observation.net_frame = static_cast<std::uint32_t>(frame + 1);
        observation.phase = frame < 70 ? TestPhase::MorphA : TestPhase::Zoom;
        observation.items_now = frame < 80 ? 3 : 2;
        observation.beams[0] = 1;
        observation.beams[1] = 1;
        observation.halfturrets[0] = 1;
        observation.halfturrets[1] = 1;
        observation.fired_totals[0] = 20;
        observation.fired_totals[1] = 20;

        for (std::size_t slot = 0; slot < 2; ++slot) {
            auto& player = observation.players[slot];
            player.present = true;
            player.active = true;
            player.spawned = true;
            player.alt_form = frame < 70;
            player.zoomed = frame >= 70;
            player.double_damage = frame >= 70;
            player.alt_attack_pressed = frame < 3;
            player.hunter = static_cast<std::uint8_t>(Hunter::Weavel);
            player.current_weapon = static_cast<std::uint8_t>(
                frame < 10 ? 0 : frame < 20 ? 1 : 2);
            player.health = frame < 20 ? 100 : 99;
            player.position = {static_cast<float>(frame) * 0.1F,
                               frame < 70 ? static_cast<float>(frame) * 0.03F
                                          : 2.1F,
                               static_cast<float>(slot)};
            player.facing = {0.0F, 0.0F, 1.0F};

            observation.remote_state_valid[slot] = slot == 1;
            observation.remote_states[slot].slot_index =
                static_cast<std::uint8_t>(slot);
            observation.remote_states[slot].flags = PlayerState::FlagSpawned
                | (player.alt_form ? PlayerState::FlagAltForm : 0);
            observation.remote_states[slot].position = player.position;
        }
        observation.bombs[0] = 1;
        observation.bombs[1] = 1;
        check.observe(observation);
    }

    const auto& records = check.records();
    assert(records[0].spawned_frames == 140);
    assert(records[1].spawned_frames == 140);
    assert(records[0].shots_fired == 20);
    assert(records[1].shots_fired == 20);
    assert(records[0].weapon_changes == 2);
    assert(records[1].weapon_changes == 2);
    assert(records[0].alt_form_in_morph_phase == 70);
    assert(records[1].alt_form_in_morph_phase == 70);
    assert(records[0].biped_in_unmorph_phase == 70);
    assert(records[1].biped_in_unmorph_phase == 70);
    assert(records[0].damage_events == 1);
    assert(records[1].damage_in_alt_form == 1);
    assert(records[1].ever_compared);
    assert(records[1].worst_position_gap == 0.0);
    assert(check.items_now() == 2);
    assert(check.items_picked_up() == 1);
    assert(check.item_samples() == 140);

    std::array<bool, NetConfig::SlotCapacity> present{};
    present[0] = true;
    present[1] = true;
    std::array<std::int32_t, NetConfig::SlotCapacity> kills{};
    std::array<std::int32_t, NetConfig::SlotCapacity> deaths{};
    std::array<std::int32_t, NetConfig::SlotCapacity> points{};
    kills[0] = 2;
    points[0] = 3;
    check.sample_scoreboard(30, names, present, kills, deaths, points);
    assert(check.boards().at(30).find("local 2k/0d/3p") != std::string::npos);

    const auto result = check.report(names);
    assert(result.any_remote);
    assert(result.failures == 0);
    assert(result.pairs.size() == 1);
    assert(result.pairs[0].slot == 1);
    bool saw_n_a_bomb = false;
    bool saw_pairwise_hit = false;
    for (const auto& feature : result.pairs[0].features) {
        if (feature.feature == "bombs") {
            saw_n_a_bomb = feature.verdict == "n/a";
        }
        if (feature.feature == "hit-in-alt-form") {
            saw_pairwise_hit = feature.tested && feature.pairwise
                && feature.verdict == "ok";
        }
    }
    assert(saw_n_a_bomb);
    assert(saw_pairwise_hit);

    std::ostringstream printed;
    check.print_report(printed, names);
    assert(printed.str().find("feature coverage") != std::string::npos);
    assert(printed.str().find("netcheck local saw remote shots 20")
           != std::string::npos);
    assert(printed.str().find("scoreboard at t=30s") != std::string::npos);

    check.reset();
    assert(check.records()[0].spawned_frames == 0);
    assert(check.items_now() == 0);
    assert(check.report(names).failures == 1);
    return 0;
}
