#pragma once

#include "Mods/Network/Protocol/NetProtocol.hpp"
#include "Mods/Network/Protocol/SessionProtocol.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace MphRead::Mods::Network
{
    // A recording contains exactly one authoritative match identity. Identify
    // boundaries from accepted control packets, not the map name: rotations
    // can repeat a room and a single map can host successive matches.
    class DemoMatchBoundary final
    {
    public:
        struct Identity final
        {
            std::uint16_t MatchId = 0;
            std::uint64_t Epoch = 0;
            std::string Room{};
        };

        [[nodiscard]] static std::optional<Identity> Decode(std::span<const std::uint8_t> bytes)
        {
            if (bytes.empty()) return std::nullopt;
            const auto type = static_cast<PacketType>(bytes[0]);
            const auto payload = bytes.subspan(1);
            if ((type == PacketType::MatchState || type == PacketType::MapChange)
                && payload.size() >= MatchStatePacket::Size)
            {
                const auto match = MatchStatePacket::Read(payload);
                return Identity{match.MatchId, match.AuthorityEpoch, match.RoomKey.value_or("")};
            }
            if (type == PacketType::SessionState)
            {
                SessionStatePacket state{};
                if (SessionStatePacket::TryRead(payload, state))
                {
                    return Identity{state.MatchId, state.AuthorityEpoch,
                        state.Match.RoomKey.value_or("")};
                }
            }
            return std::nullopt;
        }

        // True iff a new, non-stale match began; identity is advanced exactly
        // once. A reordered control packet from the previous match is ignored.
        [[nodiscard]] bool Observe(std::uint16_t matchId, std::uint64_t epoch) noexcept
        {
            if (matchId == 0 || epoch == 0) return false;
            if (_matchId == 0)
            {
                _matchId = matchId;
                _epoch = epoch;
                return false;
            }
            if (epoch == _epoch)
            {
                if (matchId == _matchId) return false;
                const auto delta = static_cast<std::uint16_t>(matchId - _matchId);
                if (delta >= 0x8000U) return false;
            }
            else
            {
                const auto delta = epoch - _epoch;
                if (delta >= (std::uint64_t{1} << 63)) return false;
            }
            _matchId = matchId;
            _epoch = epoch;
            return true;
        }

        [[nodiscard]] bool HasIdentity() const noexcept { return _matchId != 0; }
        [[nodiscard]] std::uint16_t MatchId() const noexcept { return _matchId; }
        [[nodiscard]] std::uint64_t Epoch() const noexcept { return _epoch; }
        void Reset() noexcept { _matchId = 0; _epoch = 0; }

    private:
        std::uint16_t _matchId = 0;
        std::uint64_t _epoch = 0;
    };
}
