#pragma once

#include "../../Formats/Types.hpp"

#include <cstdint>

namespace MphRead
{
    class Scene;
}

namespace MphRead::Entities
{
    class PlayerEntity;
}

namespace MphRead::Mods::Network
{
    class NetHooks final
    {
    public:
        NetHooks() = delete;

        [[nodiscard]] static std::int32_t LocalSlot();
        [[nodiscard]] static bool IsPuppet(Entities::PlayerEntity& player);
        [[nodiscard]] static bool KeepSlotAlive(Entities::PlayerEntity& player);
        [[nodiscard]] static bool PinPuppetsOnClients() noexcept { return _pinPuppetsOnClients; }
        static void PinPuppetsOnClients(bool value) noexcept { _pinPuppetsOnClients = value; }
        [[nodiscard]] static bool SnapshotOwnsPuppets() noexcept { return _snapshotOwnsPuppets; }
        static void SnapshotOwnsPuppets(bool value) noexcept { _snapshotOwnsPuppets = value; }
        static void AfterRemoteMovement(Entities::PlayerEntity& player);
        [[nodiscard]] static OpenTK::Mathematics::Vector3 RemoteShotOrigin(
            Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 current);
        [[nodiscard]] static OpenTK::Mathematics::Vector3 RemoteShotDirection(
            Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 current);
        [[nodiscard]] static bool TryApplyRemoteInput(
            Entities::PlayerEntity& player, std::int32_t slot);
        [[nodiscard]] static bool ForceSpawn(Entities::PlayerEntity& player);
        static void AfterInput(MphRead::Scene& scene);
        static void AfterSimulation();

    private:
        static constexpr std::uint32_t SnapshotStaleFrames = 12U;
        static constexpr std::uint32_t StaleIntentFrames = 30U;

        inline static bool _pinPuppetsOnClients = false;
        inline static bool _snapshotOwnsPuppets = true;

        [[nodiscard]] static bool SnapshotPositions();

        static void ApplyRemoteStates();
    };
}
