#pragma once

#include "../../Formats/Types.hpp"

#define MPHREAD_PLAYER_STYLUS_HUD_MEMBERS \
public: \
    void ModDrawStylusZone(); \
    [[nodiscard]] float ModPlaceWeaponSelect(); \
private: \
    static const ::OpenTK::Mathematics::Vector4 _stylusInk; \
    static const ::OpenTK::Mathematics::Vector4 _stylusFill; \
    static const ::OpenTK::Mathematics::Vector4 _stylusLit; \
    void DrawStylusCircle(float centreX, float centreY, float radiusX, float radiusY, \
        ::OpenTK::Mathematics::Vector4 colour);

#ifndef MPHREAD_PLAYER_ENTITY_CANONICAL_HEADER
#include "../../Entities/Players/PlayerEntity.hpp"
#endif
