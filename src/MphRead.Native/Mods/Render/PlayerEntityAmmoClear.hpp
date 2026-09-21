#pragma once

#include <cstdint>
#include <string_view>

namespace MphRead::Hud
{
    enum class Align : std::int32_t;
    class HudObjectInstance;
    class HudObjects;
}

namespace MphRead::Text
{
    class Font;
}

#define MPHREAD_PLAYER_ENTITY_AMMO_CLEAR_MEMBERS \
public: \
    float ModAmmoTextX(float x, float y, MphRead::Hud::Align align, std::u16string_view text); \
private: \
    static constexpr float AmmoIconGap = 2.0F; \
    static constexpr float FontLineHeight = 12.0F; \
    float ModTextWidth(std::u16string_view text);

#ifndef MPHREAD_PLAYER_ENTITY_CANONICAL_HEADER
#include "../../Entities/Players/PlayerEntity.hpp"
#endif
