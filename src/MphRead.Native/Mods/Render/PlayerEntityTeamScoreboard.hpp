#pragma once

#include <string>

#define MPHREAD_PLAYER_TEAM_SCOREBOARD_MEMBERS \
private: \
    void ModDrawTeamScoreboard(); \
    [[nodiscard]] std::string TeamScoreValue(bool timed, float time, std::int32_t points) const;

#ifndef MPHREAD_PLAYER_ENTITY_CANONICAL_HEADER
#include "../../Entities/Players/PlayerEntity.hpp"
#endif
