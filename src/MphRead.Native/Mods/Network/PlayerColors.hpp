#pragma once

#include "../../Entities/Players/PlayerEntity.hpp"

#include <array>
#include <cstdint>

namespace MphRead::Mods::Network
{
    class PlayerColors final
    {
    public:
        PlayerColors() = delete;

        static constexpr std::int32_t Count = 4;

        static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> Choice;

        static void Reset();
        [[nodiscard]] static std::int32_t Clamp(std::int32_t color);
        static void Resolve();

    private:
        [[nodiscard]] static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity>
            CreateApplied();
        [[nodiscard]] static bool TakenBefore(
            std::int32_t slot, MphRead::Hunter hunter, std::int32_t color);

        static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> _applied;
    };
}
