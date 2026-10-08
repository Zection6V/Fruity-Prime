#pragma once

#include <cstdint>
#include <string>

namespace MphRead { class Scene; }
namespace MphRead::Entities { class PlayerEntity; class HalfturretEntity; }

namespace MphRead::Mods::Diagnostics
{
    class WeavelAltFormCheck final
    {
    public:
        static std::int32_t Run(const std::string& room);
    private:
        static void CheckEnemyExtension(Scene& scene, Entities::PlayerEntity& owner,
            Entities::HalfturretEntity& turret);
    };
}
