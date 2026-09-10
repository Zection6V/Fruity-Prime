#include "NetScoreboard.hpp"

namespace MphRead::Mods::Network::Detail
{
    std::int32_t NetScoreboardPlayerSlotCapacity();

    std::int32_t& NetScoreboardPoints(std::int32_t slot);
    std::int32_t& NetScoreboardKills(std::int32_t slot);
    std::int32_t& NetScoreboardDeaths(std::int32_t slot);
    float& NetScoreboardTime(std::int32_t slot);
    std::int32_t& NetScoreboardSuicides(std::int32_t slot);
    std::int32_t& NetScoreboardFriendlyKills(std::int32_t slot);
    std::int32_t& NetScoreboardHeadshotKills(std::int32_t slot);
    std::int32_t& NetScoreboardDamageCount(std::int32_t slot);
    std::int32_t& NetScoreboardAltDamageCount(std::int32_t slot);
    std::int32_t& NetScoreboardBeamDamageDealt(std::int32_t slot);
    std::int32_t& NetScoreboardBeamDamageMax(std::int32_t slot);
    std::int32_t& NetScoreboardOctolithScores(std::int32_t slot);
    std::int32_t& NetScoreboardOctolithDrops(std::int32_t slot);
    std::int32_t& NetScoreboardOctolithStops(std::int32_t slot);
    std::int32_t& NetScoreboardNodesCaptured(std::int32_t slot);
    std::int32_t& NetScoreboardNodesLost(std::int32_t slot);
    std::int32_t& NetScoreboardKillsAsPrime(std::int32_t slot);
    std::int32_t& NetScoreboardPrimesKilled(std::int32_t slot);
    std::int32_t& NetScoreboardBeamKills(std::int32_t slot, std::int32_t beam);
}

namespace MphRead::Mods::Network
{
    void NetScoreboard::ForgetSlot(std::int32_t slot)
    {
        if (slot < 0 || slot >= Detail::NetScoreboardPlayerSlotCapacity())
        {
            return;
        }

        Detail::NetScoreboardPoints(slot) = 0;
        Detail::NetScoreboardKills(slot) = 0;
        Detail::NetScoreboardDeaths(slot) = 0;
        Detail::NetScoreboardTime(slot) = 0.0f;
        Detail::NetScoreboardSuicides(slot) = 0;
        Detail::NetScoreboardFriendlyKills(slot) = 0;
        Detail::NetScoreboardHeadshotKills(slot) = 0;
        Detail::NetScoreboardDamageCount(slot) = 0;
        Detail::NetScoreboardAltDamageCount(slot) = 0;
        Detail::NetScoreboardBeamDamageDealt(slot) = 0;
        Detail::NetScoreboardBeamDamageMax(slot) = 0;
        Detail::NetScoreboardOctolithScores(slot) = 0;
        Detail::NetScoreboardOctolithDrops(slot) = 0;
        Detail::NetScoreboardOctolithStops(slot) = 0;
        Detail::NetScoreboardNodesCaptured(slot) = 0;
        Detail::NetScoreboardNodesLost(slot) = 0;
        Detail::NetScoreboardKillsAsPrime(slot) = 0;
        Detail::NetScoreboardPrimesKilled(slot) = 0;
        for (std::int32_t beam = 0; beam < 9; beam++)
        {
            Detail::NetScoreboardBeamKills(slot, beam) = 0;
        }
    }
}
