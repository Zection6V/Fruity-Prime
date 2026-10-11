#pragma once

#include "NetProtocol.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>

namespace MphRead::Entities
{
    class ItemSpawnEntity;
    class PlayerEntity;
}

namespace MphRead::Mods::Network
{
    // A client's word that its own player touched a power-up, and the
    // authority's ruling on it -- the hit claim's shape for an item. The
    // client sends and resends until answered; the authority checks the
    // claim against the spawn it names and the path it saw that player take,
    // hands the item to whoever it reaches first, and grants the power-up
    // from the moment of contact (NetPowerUps). A contact the authority sees
    // for itself goes through the same grant without waiting for a claim.
    class NetPickupClaims final
    {
    public:
        NetPickupClaims() = delete;

        using VerdictWriter = std::function<void(std::int32_t slot, std::span<const PickupVerdictPacket::Entry>)>;

        static constexpr std::int32_t MaxSends = 6;
        // Frames an unanswered claim is kept before it is given up as refused.
        static constexpr std::int32_t MaxAge = 120;
        // How old a contact the authority still rules on.
        static constexpr std::uint32_t MaxContactAge = 120;
        // How near its own record of the claimant the authority must have
        // had them to where they say they touched it.
        static constexpr float CorroborationRadius = 2.0F;

        // Client: this machine's player touched what `spawner` put out.
        // Returns the claim's id, 0 when no claim can be made.
        [[nodiscard]] static std::uint16_t Declare(Entities::PlayerEntity& player,
            const Entities::ItemSpawnEntity& spawner);
        // Client: the claims due a send into `dest`; its length, 0 for none.
        [[nodiscard]] static std::int32_t Compose(std::span<std::uint8_t> dest);
        static void ApplyVerdicts(std::span<const std::uint8_t> payload);

        // Authority.
        static void Receive(std::int32_t slot, std::span<const std::uint8_t> payload);
        static void VerdictSink(VerdictWriter value) { _verdictSink = std::move(value); }

        static void Tick();
        static void Reset();
        [[nodiscard]] static std::string Describe();

    private:
        struct Outgoing
        {
            bool Live = false;
            PickupClaimPacket Claim{};
            std::int32_t Age = 0;
            std::int32_t Sends = 0;
            std::int32_t LastSentAge = 0;
        };

        struct Remembered
        {
            std::uint16_t ClaimId = 0;
            PickupVerdictPacket::Entry Verdict{};
        };

        static constexpr std::size_t OutboxCapacity = 8;
        static constexpr std::size_t RememberedPerSlot = 16;
        static constexpr std::size_t Slots = Entities::PlayerEntity::SlotCapacity;

        [[nodiscard]] static bool Claiming();
        [[nodiscard]] static PickupVerdictPacket::Entry Judge(std::int32_t slot, const PickupClaimPacket& claim);
        [[nodiscard]] static bool Reachable(std::int32_t slot, const PickupClaimPacket& claim,
            const Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 item);

        static std::array<Outgoing, OutboxCapacity> _outbox;
        inline static std::uint16_t _nextId = 0;
        static std::array<std::array<Remembered, RememberedPerSlot>, Slots> _remembered;
        inline static VerdictWriter _verdictSink{};
        inline static std::array<std::int64_t, 7> _results{};
        inline static std::int64_t _declared = 0;
        inline static std::int64_t _unanswered = 0;
    };
}
