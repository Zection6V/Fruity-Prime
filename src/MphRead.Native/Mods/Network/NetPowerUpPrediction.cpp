#include "NetPowerUpPrediction.hpp"

#include "NetItemState.hpp"
#include "NetLog.hpp"
#include "NetPickupClaims.hpp"
#include "NetSession.hpp"
#include "../../Entities/ItemInstanceEntity.hpp"
#include "../../Entities/ItemSpawnEntity.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"

#include <algorithm>
#include <optional>

namespace MphRead::Mods::Network
{
    namespace
    {
        [[nodiscard]] Entities::PlayerEntity* LocalPlayer()
        {
            const std::int32_t slot = NetSession::LocalSlot();
            const auto& players = Entities::PlayerEntity::Players();
            return slot >= 0 && static_cast<std::size_t>(slot) < players.size()
                ? players[static_cast<std::size_t>(slot)].get() : nullptr;
        }

        // The item sync no longer shows the predicted spawn out.
        [[nodiscard]] bool SpawnSettled(std::int16_t spawnId, std::uint16_t spawnCount)
        {
            ItemSpawnState state{};
            return NetItemState::TryGet(spawnId, state) && (!state.Available || state.SpawnCount != spawnCount);
        }
    }

    void NetPowerUpPrediction::Predict(Entities::PlayerEntity& player, Entities::ItemInstanceEntity& item)
    {
        Entities::ItemSpawnEntity* spawner = item.Owner();
        const std::optional<PowerUp> kind = PowerUpOf(item.ItemType());
        if (spawner == nullptr || !kind.has_value())
        {
            return;
        }
        const auto spawnId = static_cast<std::int16_t>(spawner->Id);
        const std::uint16_t spawnCount = spawner->SpawnCount();
        if (Holds(spawnId, spawnCount))
        {
            return;
        }
        const auto free = std::find_if(_entries.begin(), _entries.end(), [](const Entry& entry) { return !entry.Live; });
        if (free == _entries.end())
        {
            // Left to the authority, which sees the same contact a trip later.
            return;
        }
        const std::uint16_t claimId = NetPickupClaims::Declare(player, *spawner);
        if (claimId == 0)
        {
            return;
        }
        *free = Entry{ true, claimId, *kind, spawnId, spawnCount, std::max(1U, NetSession::NetFrame()), false, false, 0,
            _seen[IndexOf(*kind)] };
        player.GrantItemEffect(item.ItemType(), PowerUpFrames);
        item.SetDespawnTimer(0);
        _predicted++;
        if (NetLog::Enabled())
        {
            NetLog::Event("[powerup] predicted " + std::string(NameOf(*kind)) + " spawn="
                + std::to_string(spawnId) + "/" + std::to_string(spawnCount) + " claim=" + std::to_string(claimId));
        }
    }

    bool NetPowerUpPrediction::Holds(std::int16_t spawnId, std::uint16_t spawnCount) noexcept
    {
        return std::any_of(_entries.begin(), _entries.end(), [&](const Entry& entry)
        {
            return entry.Live && entry.SpawnId == spawnId && entry.SpawnCount == spawnCount;
        });
    }

    bool NetPowerUpPrediction::Awaiting(PowerUp kind) noexcept
    {
        return std::any_of(_entries.begin(), _entries.end(), [&](const Entry& entry)
        {
            return entry.Live && !entry.Confirmed && entry.Kind == kind;
        });
    }

    void NetPowerUpPrediction::Settle(std::uint16_t claimId, bool granted, std::uint8_t sequence)
    {
        for (Entry& entry : _entries)
        {
            if (!entry.Live || entry.ClaimId != claimId || entry.Granted)
            {
                continue;
            }
            if (granted)
            {
                entry.Granted = true;
                entry.Sequence = sequence;
                entry.Confirmed = _seen[IndexOf(entry.Kind)] == sequence;
                _confirmed++;
            }
            else
            {
                Undo(entry);
                entry.Live = false;
                _refused++;
            }
            return;
        }
    }

    void NetPowerUpPrediction::Seen(PowerUp kind, std::uint8_t sequence)
    {
        _seen[IndexOf(kind)] = sequence;
        for (Entry& entry : _entries)
        {
            if (entry.Live && entry.Granted && entry.Kind == kind && entry.Sequence == sequence)
            {
                entry.Confirmed = true;
            }
        }
    }

    void NetPowerUpPrediction::Undo(const Entry& entry)
    {
        // Only while no grant has been shown since: one that has is the
        // authority's, and the snapshots already rule on it.
        Entities::PlayerEntity* player = LocalPlayer();
        if (player == nullptr || _seen[IndexOf(entry.Kind)] != entry.SeenBefore)
        {
            return;
        }
        player->SetItemEffectFrames(ItemTypeOf(entry.Kind), 0);
        _undone++;
        if (NetLog::Enabled())
        {
            NetLog::Event("[powerup] prediction undone " + std::string(NameOf(entry.Kind))
                + " claim=" + std::to_string(entry.ClaimId));
        }
    }

    void NetPowerUpPrediction::Tick()
    {
        const std::uint32_t now = NetSession::NetFrame();
        for (Entry& entry : _entries)
        {
            if (!entry.Live)
            {
                continue;
            }
            const std::uint32_t age = now - entry.Since;
            if (entry.Granted && !entry.Confirmed && age > UnseenFrames)
            {
                entry.Confirmed = true;
            }
            if (entry.Confirmed && (SpawnSettled(entry.SpawnId, entry.SpawnCount) || age > HoldFrames))
            {
                entry.Live = false;
            }
        }
    }

    void NetPowerUpPrediction::Reset()
    {
        _entries = {};
        _seen = {};
        _predicted = 0;
        _confirmed = 0;
        _refused = 0;
        _undone = 0;
    }

    std::string NetPowerUpPrediction::Describe()
    {
        return "power-up pickups: " + std::to_string(_predicted) + " predicted, " + std::to_string(_confirmed)
            + " granted, " + std::to_string(_refused) + " refused (" + std::to_string(_undone) + " taken back)";
    }
}
