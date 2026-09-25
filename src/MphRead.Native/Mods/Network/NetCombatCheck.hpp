#pragma once

#include "NetProtocol.hpp"

#include <cstdint>
#include <string>

namespace MphRead::Entities
{
    class PlayerEntity;
}

namespace MphRead::Mods::Network
{
    // Combat on a headless authority, asserted: ghost shots from a dead held
    // trigger, projectile lifecycles across a death, hit-claim arbitration and
    // the continuous-weapon cadence every peer has to agree on.
    class NetCombatCheck final
    {
    public:
        NetCombatCheck() = delete;

        [[nodiscard]] static std::int32_t Run(const std::string& room);

    private:
        static void Check(bool ok, const std::string& name);
        [[nodiscard]] static IntentPacket Intent(Entities::PlayerEntity& player, std::uint32_t frame, bool playing, bool shoot);
        static void PrepareClaims();
        [[nodiscard]] static HitClaimPacket Claim(std::int32_t shooter, std::uint32_t world);
        static void Receive(std::int32_t shooter, const HitClaimPacket& claim);
        static void InvalidHitClaimIsRefused();
        static void MutualKillOrdering();
        static void InvulnerableClaimIsRefused();
        static void ClaimArbitrationHasDeadline();
        static void ContinuousPhaseAgreesAcrossPeers();

        inline static std::int32_t _checks = 0;
    };
}
