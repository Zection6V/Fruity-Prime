#include "Entities/match_flow.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
    using fruityprime::match::Flow;
    using fruityprime::match::Phase;
    using fruityprime::match::Rules;
    using fruityprime::net::MatchStatePacket;
    using fruityprime::net::PlayerState;

    assert(fruityprime::match::is_team_mode(4));
    assert(fruityprime::match::is_team_mode(13));
    assert(!fruityprime::match::is_team_mode(3));
    const auto survival_defaults = fruityprime::match::defaults_for_mode(5);
    assert(std::fabs(survival_defaults.time_limit_seconds - 900.0F) < 0.001F);
    assert(survival_defaults.point_goal == 2);
    const auto defender_defaults = fruityprime::match::defaults_for_mode(12);
    assert(defender_defaults.point_goal == 0);
    assert(std::fabs(defender_defaults.time_goal_seconds - 90.0F) < 0.001F);
    assert(fruityprime::match::uses_point_goal(7));
    assert(!fruityprime::match::uses_point_goal(5));

    Rules clock_rules;
    clock_rules.time_limit_seconds = 0.05F;
    clock_rules.point_goal = 0;
    Flow clock(clock_rules);
    PlayerState player;
    player.flags = PlayerState::FlagActive | PlayerState::FlagSpawned;
    std::vector<PlayerState> players{player};
    clock.tick(0.02F, players, true, true);
    assert(clock.in_progress());
    assert(std::fabs(clock.state().time_remaining - 0.03F) < 0.001F);
    clock.tick(0.04F, players, true, true);
    assert(clock.phase() == Phase::GameOver);
    assert(clock.state().ending());
    assert(!clock.consume_end_report());

    Rules score_rules;
    score_rules.time_limit_seconds = 0.0F;
    score_rules.point_goal = 2;
    Flow score(score_rules);
    players[0].points = 2;
    score.tick(1.0F, players, true, true);
    assert(score.phase() == Phase::GameOver);
    assert(score.consume_end_report());
    assert(!score.consume_end_report());

    Rules survival_rules;
    survival_rules.mode = 5; // GameMode.Survival
    survival_rules.time_limit_seconds = 0.0F;
    survival_rules.point_goal = 2; // spare lives, not a score goal
    Flow survival(survival_rules);
    players[0].health = 100;
    PlayerState survival_opponent = players[0];
    survival_opponent.slot_index = 1;
    std::vector<PlayerState> survival_players{players[0], survival_opponent};
    survival.tick(1.0F, survival_players, true, true);
    assert(survival.in_progress());
    survival_players[1].health = 0;
    survival_players[1].deaths = 3;
    survival.tick(0.0F, survival_players, true, false);
    assert(survival.phase() == Phase::GameOver);
    assert(survival.consume_end_report());

    Rules defender_rules;
    defender_rules.mode = 12; // GameMode.Defender
    defender_rules.time_limit_seconds = 0.0F;
    defender_rules.point_goal = 0;
    Flow defender(defender_rules);
    assert(std::fabs(defender_rules.time_goal_seconds + 1.0F) < 0.001F);
    defender.set_team_objective_time(0, 90.0F);
    defender.tick(0.0F, survival_players, true, false);
    assert(defender.phase() == Phase::GameOver);
    assert(defender.consume_end_report());

    Rules prime_hunter_rules;
    prime_hunter_rules.mode = 14; // GameMode.PrimeHunter
    prime_hunter_rules.time_limit_seconds = 0.0F;
    prime_hunter_rules.point_goal = 0;
    Flow prime_hunter(prime_hunter_rules);
    prime_hunter.set_prime_hunter(0);
    prime_hunter.set_player_objective_time(0, 90.0F);
    prime_hunter.tick(0.0F, players, true, false);
    assert(prime_hunter.phase() == Phase::GameOver);
    assert(prime_hunter.consume_end_report());

    Rules forced_team_rules;
    forced_team_rules.mode = 3;
    forced_team_rules.team_mode = true;
    forced_team_rules.time_limit_seconds = 0.0F;
    forced_team_rules.point_goal = 3;
    Flow forced_teams(forced_team_rules);
    players[0].team = 0;
    players[0].points = 1;
    PlayerState forced_teammate = players[0];
    forced_teammate.team = 0;
    forced_teammate.points = 2;
    forced_teams.tick(
        0.0F, std::vector<PlayerState>{players[0], forced_teammate},
                      true, false);
    assert(forced_teams.phase() == Phase::GameOver);

    MatchStatePacket server_state;
    server_state.flags = MatchStatePacket::FlagEnding;
    server_state.time_remaining = 0.0F;
    server_state.point_goal = 7;
    score.apply_server_state(server_state);
    assert(score.phase() == Phase::GameOver);
    assert(!score.consume_end_report());

    server_state.flags = MatchStatePacket::FlagInProgress;
    server_state.time_remaining = 30.0F;
    score.apply_server_state(server_state);
    assert(score.in_progress());
    assert(std::fabs(score.state().time_remaining - 30.0F) < 0.001F);

    Rules team_rules;
    team_rules.mode = 4; // GameMode.BattleTeams
    team_rules.time_limit_seconds = 0.0F;
    team_rules.point_goal = 3;
    Flow teams(team_rules);
    PlayerState teammate = players[0];
    teammate.team = 0;
    teammate.points = 2;
    players[0].team = 0;
    players[0].points = 1;
    std::vector<PlayerState> team_players{players[0], teammate};
    teams.tick(0.0F, team_players, true, false);
    assert(teams.phase() == Phase::GameOver);
    assert(teams.consume_end_report());

    std::cout << "match flow: clock, score goal, server state passed\n";
    return 0;
}
