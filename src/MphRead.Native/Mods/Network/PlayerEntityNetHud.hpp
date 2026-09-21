#pragma once

#include "../../Formats/Types.hpp"

#include <cstdint>

#define MPHREAD_PLAYER_ENTITY_NET_HUD_MEMBERS \
public: \
    [[nodiscard]] float ModScoreColumn1() const; \
    [[nodiscard]] float ModScoreColumn2() const; \
 \
    void ModDrawPingHeader(float posY); \
    void ModDrawPingRow(float posY, ColorRgba rowColor, std::int32_t slot); \
 \
private: \
    static constexpr float _pingColumnX = 236.0F; \
 \
    [[nodiscard]] static ColorRgba PingColor(std::int32_t ping);

#ifndef MPHREAD_PLAYER_ENTITY_CANONICAL_HEADER
#include "../../Entities/Players/PlayerEntity.hpp"
#endif
