#pragma once

#include "GamepadHaptics.hpp"

#include <cstdint>

#define MPHREAD_PLAYER_ENTITY_HAPTICS_MEMBERS \
private: \
    void ModControllerFeedback(::MphRead::Mods::Input::GamepadFeedback feedback); \
    [[nodiscard]] std::int32_t ModControllerWeaponSelection(); \
    [[nodiscard]] std::int32_t ModResolveWeaponSlot(std::int32_t slot);

#ifndef MPHREAD_PLAYER_ENTITY_CANONICAL_HEADER
#include "../../Entities/Players/PlayerEntity.hpp"
#endif
