#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace fruityprime::game {
struct State;
}

namespace fruityprime::runtime {
class HalfturretEntity;
}

namespace fruityprime::players {
class PlayerEntity;
}

namespace fruityprime::net {

class DamageBridge;
class MatchEnd;
class NetLog;
class NetPlayerBridge;
class SlotManager;

// Native counterpart of MphRead/Mods/Network/NetRoomChange.cs.
//
// NetSession, Scene, and GameState are process-wide objects in the managed
// build.  The Win32 host owns those objects explicitly, so the equivalent
// native entry points receive the host's live objects through these small
// contexts.  The state below remains process-wide, exactly like the four
// private static fields in the C# class.
class NetRoomChange final {
public:
    struct SyncContext {
        bool active;
        bool in_room_transition;
        std::string_view current_room;
        const MatchStatePacket& server_match;
        std::uint32_t net_frame;
        game::State& game_state;
        void (*set_fade)() noexcept;
        NetLog& log;
    };

    struct RebuildContext {
        game::State& game_state;
        const RosterPacket& roster;
        int local_slot;
        std::uint8_t local_hunter;
        const std::array<std::uint8_t, NetConfig::SlotCapacity>& slot_hunters;
        int local_recolor;
        SlotManager& slot_manager;
        DamageBridge& damage;
        MatchEnd& match_end;
        NetLog& log;
    };

    struct AfterRebuildContext {
        std::uint32_t net_frame;
        NetPlayerBridge& player_bridge;
        NetLog& log;
        // These four callbacks are the native Scene boundary for the four
        // calls made by NetRoomChange.AfterRebuild.  They are required: a
        // caller must provide the room's insertion, PlayerEntity.Initialize,
        // and both Scene.InitEntity operations in the same order.
        void (*insert_entity)(players::PlayerEntity&);
        void (*initialize)(players::PlayerEntity&);
        void (*init_entity)(players::PlayerEntity&);
        void (*init_halfturret)(runtime::HalfturretEntity&);
    };

    static void Reset() noexcept;

    // Cheap enough to call from every network update.  This is the native
    // Scene argument boundary for NetRoomChange.Sync(Scene scene).
    static void Sync(const SyncContext& context) noexcept;

    [[nodiscard]] static bool Settling(std::uint32_t net_frame) noexcept;
    [[nodiscard]] static int RoomPlayerCount() noexcept;
    [[nodiscard]] static bool Rebuilding() noexcept;

    // RoomEntity.LoadRoom calls this after PlayerEntity.Construct and before
    // the new room's normal fixed-step processing begins.
    [[nodiscard]] static players::PlayerEntity* RebuildPlayers(
        const RebuildContext& context);

    // RoomEntity.LoadRoom calls this after the room transition has inserted
    // the main player.  Native gameplay::Session already owns the other
    // active slot records; this method performs their equivalent native
    // initialization and closes the settling boundary.
    static void AfterRebuild(const AfterRebuildContext& context);

private:
    static constexpr std::uint32_t SettleFrames = 60;
    static constexpr std::uint32_t RequestRetryFrames = 300;

    static void ResetScores(game::State& state, MatchEnd& match_end) noexcept;

    inline static std::string requested_{};
    inline static std::uint32_t requested_frame_ = 0;
    inline static std::uint16_t loaded_match_ = 0;
    inline static std::uint32_t loaded_frame_ = 0;
};

} // namespace fruityprime::net
