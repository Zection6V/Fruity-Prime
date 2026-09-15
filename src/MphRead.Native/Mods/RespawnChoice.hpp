#pragma once

#include "../Formats/Enums.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace MphRead::Entities
{
    class PlayerEntity;
}

namespace MphRead::Mods
{
    class RespawnChoice final
    {
    public:
        [[nodiscard]] static MphRead::Hunter Hunter();
        [[nodiscard]] static std::int32_t Color();

        static void Reset();
        static void Request(MphRead::Hunter hunter, std::int32_t color);
        static void ApplyOnSpawn(const std::shared_ptr<Entities::PlayerEntity>& player);

    private:
        RespawnChoice() = delete;

        static std::optional<MphRead::Hunter> _hunter;
        static std::optional<std::int32_t> _color;
    };
}
