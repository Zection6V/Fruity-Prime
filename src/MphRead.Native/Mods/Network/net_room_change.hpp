#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace fruityprime::game {
struct State;
}

namespace fruityprime::gameplay {
class Session;
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
        bool active = false;
        bool in_room_transition = false;
        std::string_view current_room;
        const MatchStatePacket* server_match = nullptr;
        std::uint32_t net_frame = 0;
        game::State* game_state = nullptr;
        void (*set_fade)() noexcept = nullptr;
        NetLog* log = nullptr;
    };

    struct RebuildContext {
        game::State* game_state = nullptr;
        gameplay::Session* session = nullptr;
        const RosterPacket* roster = nullptr;
        int local_slot = -1;
        std::uint8_t local_hunter = 0;
        int local_recolor = 0;
        SlotManager* slot_manager = nullptr;
        DamageBridge* damage = nullptr;
        MatchEnd* match_end = nullptr;
        NetLog* log = nullptr;
    };

    struct AfterRebuildContext {
        std::uint32_t net_frame = 0;
        NetPlayerBridge* player_bridge = nullptr;
        NetLog* log = nullptr;
    };

    static void Reset() noexcept;

    // Cheap enough to call from every network update.  This is the native
    // Scene argument boundary for NetRoomChange.Sync(Scene scene).
    static void Sync(const SyncContext& context) noexcept;

    [[nodiscard]] static bool Settling(std::uint32_t net_frame) noexcept;
    [[nodiscard]] static int RoomPlayerCount(bool active) noexcept;
    [[nodiscard]] static bool Rebuilding(bool active) noexcept;

    // RoomEntity.LoadRoom calls this after PlayerEntity.Construct and before
    // the new room's normal fixed-step processing begins.
    [[nodiscard]] static players::PlayerEntity* RebuildPlayers(
        const RebuildContext& context);

    // RoomEntity.LoadRoom calls this after the room transition has inserted
    // the main player.  Native gameplay::Session already owns the other
    // active slot records; this method performs their equivalent native
    // initialization and closes the settling boundary.
    static void AfterRebuild(const AfterRebuildContext& context) noexcept;

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
