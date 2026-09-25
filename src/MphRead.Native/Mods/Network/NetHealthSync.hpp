#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <vector>

namespace MphRead::Entities
{
    class ItemInstanceEntity;
    class ItemSpawnEntity;
}

namespace MphRead::Mods::Network
{
    struct HealthSpawnState final
    {
        bool Available = false;
        bool Active = false;
        std::uint16_t Cooldown = 0;
        std::uint16_t SpawnCount = 0;
        std::int8_t PickerSlot = 0;

        [[nodiscard]] friend bool operator==(const HealthSpawnState&, const HealthSpawnState&) = default;
    };

    // A bounded tail on the normal authoritative snapshot, not a second
    // channel. Full state repeats so loss and joining late repair themselves.
    class NetHealthSync final
    {
    public:
        NetHealthSync() = delete;

        static constexpr std::int32_t MaxSpawns = 56;
        static constexpr std::int32_t HeaderSize = 3;
        static constexpr std::int32_t EntrySize = 7;

        [[nodiscard]] static const std::vector<std::shared_ptr<::MphRead::Entities::ItemSpawnEntity>>&
            RegisteredSpawns() noexcept;
        [[nodiscard]] static bool IsReplica();
        [[nodiscard]] static bool OwnsPickup(const ::MphRead::Entities::ItemInstanceEntity& item);

        static void BeginRoom();
        static void Register(::MphRead::Entities::ItemSpawnEntity& spawn);

        [[nodiscard]] static bool TryGet(std::int16_t id, HealthSpawnState& state);
        [[nodiscard]] static bool IsCurrentMatch(std::span<const std::uint8_t> src);

        [[nodiscard]] static std::int32_t Write(std::span<std::uint8_t> dest);
        [[nodiscard]] static bool Validate(std::span<const std::uint8_t> src);
        static void Receive(std::span<const std::uint8_t> src);

    private:
        static constexpr std::int32_t PickerShift = 2;
        static constexpr std::int32_t PickerMask = 0xF;
        static constexpr std::int32_t ReservedMask = 0xC0;

        static std::vector<std::shared_ptr<::MphRead::Entities::ItemSpawnEntity>> _spawns;
        static std::map<std::int16_t, HealthSpawnState> _states;
    };
}
