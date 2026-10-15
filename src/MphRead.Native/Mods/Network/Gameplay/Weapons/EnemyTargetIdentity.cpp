#include "Mods/Network/Gameplay/Weapons/EnemyTargetIdentity.hpp"

#include "Entities/EnemyInstanceEntity.hpp"
#include "Entities/EntityBase.hpp"
#include "Mods/Network/Diagnostics/NetLog.hpp"
#include "Mods/Network/Session/NetSession.hpp"

namespace MphRead::Mods::Network
{
    namespace
    {
        // splitmix64's finaliser: spreads close inputs far apart.
        [[nodiscard]] constexpr std::uint64_t Mix(std::uint64_t value) noexcept
        {
            value ^= value >> 30U;
            value *= 0xBF58476D1CE4E5B9ULL;
            value ^= value >> 27U;
            value *= 0x94D049BB133111EBULL;
            value ^= value >> 31U;
            return value;
        }
    }

    std::uint64_t EnemyTargetIdentity::ForSpawn(Entities::EntityBase* spawner, EnemyType type) noexcept
    {
        if (spawner == nullptr)
        {
            return 0;
        }
        std::uint64_t parent = 0;
        if (spawner->Type == EntityType::EnemyInstance)
        {
            parent = static_cast<const Entities::EnemyInstanceEntity*>(spawner)->ModStableTargetId();
        }
        else if (spawner->Id >= 0)
        {
            parent = (static_cast<std::uint64_t>(spawner->Type) << 32U) | static_cast<std::uint32_t>(spawner->Id);
        }
        if (parent == 0)
        {
            return 0;
        }
        const std::uint32_t ordinal = spawner->ModNextSpawnOrdinal();
        const std::uint64_t stableId = ForSpawn(parent, ordinal, type);
        if (NetSession::Active() && NetLog::Enabled())
        {
            NetLog::Event("[enemy-target] frame=" + std::to_string(NetSession::NetFrame())
                + " spawnerType=" + std::to_string(static_cast<std::uint32_t>(spawner->Type))
                + " spawnerId=" + std::to_string(spawner->Id)
                + " root=" + std::to_string(parent)
                + " ordinal=" + std::to_string(ordinal)
                + " enemyType=" + std::to_string(static_cast<std::uint32_t>(type))
                + " stableId=" + std::to_string(stableId));
        }
        return stableId;
    }

    std::uint64_t EnemyTargetIdentity::ForSpawn(
        std::uint64_t parent, std::uint32_t ordinal, EnemyType type) noexcept
    {
        if (parent == 0)
        {
            return 0;
        }
        const std::uint64_t value = Mix(parent
            ^ (static_cast<std::uint64_t>(ordinal) * 0x9E3779B97F4A7C15ULL)
            ^ (static_cast<std::uint64_t>(type) * 0xBF58476D1CE4E5B9ULL));
        const std::uint64_t stableId = value == 0 ? 1 : value;
        return stableId;
    }
}
