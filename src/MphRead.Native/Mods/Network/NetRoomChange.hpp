#pragma once

#include "NetProtocol.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace MphRead
{
    enum class Hunter : std::uint8_t;
    class Scene;

    namespace Entities
    {
        enum class LoadFlags : std::uint8_t;
        class PlayerEntity;
    }
}

namespace MphRead::Mods::Network
{
    namespace Detail
    {
        // Pair-local runtime closure for owners that have not reached Native yet.
        // These are mechanical dependency adapters only and must not add policy.
        [[nodiscard]] bool NetRoomChangeSessionActive();
        [[nodiscard]] std::uint32_t NetRoomChangeSessionNetFrame();
        [[nodiscard]] std::optional<MatchStatePacket> NetRoomChangeSessionServerMatch();
        [[nodiscard]] std::int32_t NetRoomChangeSessionLocalSlot();
        [[nodiscard]] Hunter NetRoomChangeSessionSlotHunter(std::int32_t slot);
        [[nodiscard]] std::int32_t NetRoomChangeSessionSlotOccupiedLength();
        [[nodiscard]] bool NetRoomChangeSessionSlotOccupied(std::int32_t slot);

        [[nodiscard]] bool NetRoomChangeSceneHasRoom(const Scene& scene);
        [[nodiscard]] std::int32_t NetRoomChangeSceneRoomId(const Scene& scene);
        [[nodiscard]] bool NetRoomChangeGameStateInRoomTransition();
        void NetRoomChangeSetTransitionRoomId(std::int32_t roomId);
        void NetRoomChangeSceneSetFadeOutBlackLoadRoom(
            Scene& scene, float length, bool overwrite);

        void NetRoomChangeSetPlayerCameraNodeRefNone(Entities::PlayerEntity& player);
        void NetRoomChangeSetPlayerBotLevel(
            Entities::PlayerEntity& player, std::int32_t value);
        [[nodiscard]] std::string NetRoomChangeFormatLoadFlags(
            Entities::LoadFlags flags);

        void NetRoomChangeSlotManagerReset();
        void NetRoomChangePlayerBridgeNoteRoomChanged();
        void NetRoomChangeSceneInsertPlayer(
            Scene& scene, const std::shared_ptr<Entities::PlayerEntity>& player);
        void NetRoomChangeSceneInitPlayer(
            Scene& scene, const std::shared_ptr<Entities::PlayerEntity>& player);
        void NetRoomChangeSceneInitHalfturret(
            Scene& scene, const std::shared_ptr<Entities::PlayerEntity>& player);

        [[nodiscard]] bool NetRoomChangeGameStateMultiplayer();
        void NetRoomChangeSetPoints(std::int32_t index, std::int32_t value);
        void NetRoomChangeSetTeamPoints(std::int32_t index, std::int32_t value);
        void NetRoomChangeSetKills(std::int32_t index, std::int32_t value);
        void NetRoomChangeSetTeamKills(std::int32_t index, std::int32_t value);
        void NetRoomChangeSetDeaths(std::int32_t index, std::int32_t value);
        void NetRoomChangeSetTeamDeaths(std::int32_t index, std::int32_t value);
        void NetRoomChangeSetStandings(std::int32_t index, std::int32_t value);
        void NetRoomChangeSetTeamStandings(std::int32_t index, std::int32_t value);
        void NetRoomChangeSetDamageCount(std::int32_t index, std::int32_t value);
        void NetRoomChangeSetKillStreak(std::int32_t index, std::int32_t value);
        void NetRoomChangeGameStateResetMatchProgress();

        [[nodiscard]] std::string NetRoomChangeFormatCurrentCultureInt32(
            std::int32_t value);
        [[nodiscard]] std::string NetRoomChangeFormatCurrentCultureUInt16(
            std::uint16_t value);
        void NetRoomChangeConsoleWriteLine(const std::string& value);
    }

    class NetRoomChange final
    {
    public:
        NetRoomChange() = delete;
        NetRoomChange(const NetRoomChange&) = delete;
        NetRoomChange(NetRoomChange&&) = delete;
        NetRoomChange& operator=(const NetRoomChange&) = delete;
        NetRoomChange& operator=(NetRoomChange&&) = delete;

        [[nodiscard]] static bool Settling();
        [[nodiscard]] static std::int32_t RoomPlayerCount();
        [[nodiscard]] static bool Rebuilding();

        static void Reset();
        static void Sync(Scene& scene);
        [[nodiscard]] static std::shared_ptr<Entities::PlayerEntity> RebuildPlayers(
            Scene& scene, Hunter hunter, std::int32_t recolor);
        static void AfterRebuild(Scene& scene);

    private:
        static constexpr std::uint32_t SettleFrames = 60U;

        static void ReloadIntroCamSeq(Scene& scene);
        static void ResetScores();

        static std::string _requested;
        static std::uint32_t _requestedFrame;
        static std::uint16_t _loadedMatch;
        static std::uint32_t _loadedFrame;
    };
}
