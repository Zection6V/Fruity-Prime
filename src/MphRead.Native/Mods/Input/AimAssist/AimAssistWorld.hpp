#pragma once

#include "AimAssistState.hpp"
#include "AimAssistTarget.hpp"
#include "../../../NativeRuntime/System/Numerics.hpp"

#include <cstdint>

#define MPHREAD_PLAYER_ENTITY_AIM_ASSIST_MEMBERS \
private: \
    ::MphRead::Mods::Input::AimAssist::AimAssistState _controllerAssist{}; \
    std::int64_t _assistDeviceRevision = -1; \
    std::int64_t _assistContextRevision = -1; \
    std::int64_t _aimSourceRevision = -1; \
    const void* _assistRoom = nullptr; \
    [[nodiscard]] ::System::Numerics::Vector2 AssistAngles(::OpenTK::Mathematics::Vector3 point) const; \
    [[nodiscard]] bool AssistVisible(::OpenTK::Mathematics::Vector3 point) const; \
    [[nodiscard]] ::MphRead::Mods::Input::AimAssist::AimAssistResult ApplyControllerAssist(float x, float y);

#ifndef MPHREAD_PLAYER_ENTITY_CANONICAL_HEADER
#include "../../../Entities/Players/PlayerEntity.hpp"
#endif
