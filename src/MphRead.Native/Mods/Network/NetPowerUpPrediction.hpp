#pragma once

#include "PowerUps.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace MphRead::Entities
{
    class ItemInstanceEntity;
    class PlayerEntity;
}

namespace MphRead::Mods::Network
{
    // A client's own pickups of a power-up, shown before the authority has
    // said so: the effect starts and the item goes the frame it is touched,
    // a claim goes out (NetPickupClaims), and the answer either confirms it
    // or takes it back. The effect a prediction starts only ever changes
    // what this machine shows -- a doubled hit is still judged against the
    // authority's grant (NetPowerUps::RanAt).
    class NetPowerUpPrediction final
    {
    public:
        NetPowerUpPrediction() = delete;

        static constexpr std::size_t Capacity = 4;
        // A granted pickup waits this long for a snapshot to carry its grant.
        static constexpr std::uint32_t UnseenFrames = 120;
        // And a confirmed one this long for the item sync to show it gone.
        static constexpr std::uint32_t HoldFrames = 600;

        // This machine's player touched a power-up a spawner put out.
        static void Predict(Entities::PlayerEntity& player, Entities::ItemInstanceEntity& item);
        // The item that spawn put out is shown taken here: not drawn again
        // on the strength of an item sync that has not caught up.
        [[nodiscard]] static bool Holds(std::int16_t spawnId, std::uint16_t spawnCount) noexcept;
        // A prediction of `kind` the snapshots have not caught up with yet,
        // which a snapshot saying it is not running must not end.
        [[nodiscard]] static bool Awaiting(PowerUp kind) noexcept;
        // The authority's answer to a claim: granted under `sequence`, or not.
        static void Settle(std::uint16_t claimId, bool granted, std::uint8_t sequence);
        // A snapshot says this machine's player holds grant `sequence`.
        static void Seen(PowerUp kind, std::uint8_t sequence);
        static void Tick();
        static void Reset();
        [[nodiscard]] static std::string Describe();

    private:
        struct Entry
        {
            bool Live = false;
            std::uint16_t ClaimId = 0;
            PowerUp Kind = PowerUp::DoubleDamage;
            std::int16_t SpawnId = 0;
            std::uint16_t SpawnCount = 0;
            std::uint32_t Since = 0;
            bool Granted = false;
            // Granted, and a snapshot has shown the grant.
            bool Confirmed = false;
            std::uint8_t Sequence = 0;
            // The grant this machine's player held when it was predicted, and
            // what that grant still had: put back if this pickup is refused.
            std::uint8_t SeenBefore = 0;
            std::uint16_t RemainingBefore = 0;
        };

        static void Undo(const Entry& entry);

        static std::array<Entry, Capacity> _entries;
        inline static std::array<std::uint8_t, PowerUpCount> _seen{};
        inline static std::int64_t _predicted = 0;
        inline static std::int64_t _confirmed = 0;
        inline static std::int64_t _refused = 0;
        inline static std::int64_t _undone = 0;
    };
}
