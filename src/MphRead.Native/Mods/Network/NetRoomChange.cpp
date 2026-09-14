#include "NetRoomChange.hpp"

#include "NetDamage.hpp"
#include "NetHitPrediction.hpp"
#include "NetLaunch.hpp"
#include "NetLog.hpp"
#include "NetMatchEnd.hpp"
#include "NetPlayerSetup.hpp"
#include "../../Entities/CamSeq/CameraSequence.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Metadata/Metadata.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace MphRead::Mods::Network
{
    std::string NetRoomChange::_requested{};
    std::uint32_t NetRoomChange::_requestedFrame = 0;
    std::uint16_t NetRoomChange::_loadedMatch = 0;
    std::uint32_t NetRoomChange::_loadedFrame = 0;

    bool NetRoomChange::Settling()
    {
        return _loadedFrame != 0
            && Detail::NetRoomChangeSessionNetFrame() - _loadedFrame < SettleFrames;
    }

    std::int32_t NetRoomChange::RoomPlayerCount()
    {
        return Detail::NetRoomChangeSessionActive() ? NetLaunch::RoomPlayerCount : 0;
    }

    bool NetRoomChange::Rebuilding()
    {
        return Detail::NetRoomChangeSessionActive();
    }

    void NetRoomChange::Reset()
    {
        _requested = "";
        _requestedFrame = 0;
        _loadedMatch = 0;
        _loadedFrame = 0;
    }

    void NetRoomChange::Sync(Scene& scene)
    {
        if (!Detail::NetRoomChangeSessionActive()
            || !Detail::NetRoomChangeSceneHasRoom(scene)
            || Detail::NetRoomChangeGameStateInRoomTransition())
        {
            return;
        }
        std::optional<MatchStatePacket> state
            = Detail::NetRoomChangeSessionServerMatch();
        std::string wanted;
        if (state.has_value() && state->RoomKey.has_value())
        {
            wanted = state->RoomKey.value();
        }
        if (wanted.length() == 0)
        {
            return;
        }
        const std::uint16_t match = state.value().MatchId;
        const RoomMetadata* currentMeta = Metadata::GetRoomById(
            Detail::NetRoomChangeSceneRoomId(scene), true);
        const std::string current = currentMeta == nullptr
            ? std::string{}
            : currentMeta->Name;
        if (current == wanted && (_loadedMatch == 0 || _loadedMatch == match))
        {
            _loadedMatch = match;
            _requested = "";
            return;
        }
        if (_requested == wanted && _loadedMatch == match
            && Detail::NetRoomChangeSessionNetFrame() - _requestedFrame < 300U)
        {
            return;
        }
        auto [meta, ignored] = Metadata::GetRoomByName(wanted);
        static_cast<void>(ignored);
        if (meta == nullptr)
        {
            Detail::NetRoomChangeConsoleWriteLine(
                "[net] server switched to \"" + wanted
                + "\", which this build does not know");
            NetLog::Event("unknown server map \"" + wanted + "\"");
            _requested = wanted;
            _requestedFrame = Detail::NetRoomChangeSessionNetFrame();
            return;
        }
        _requested = wanted;
        _requestedFrame = Detail::NetRoomChangeSessionNetFrame();
        _loadedMatch = match;
        Detail::NetRoomChangeConsoleWriteLine(current == wanted
            ? "[net] server started a new match on " + wanted + "; loading it"
            : "[net] server rotated to " + wanted + "; loading it");
        NetLog::Event("loading " + wanted + " for match "
            + Detail::NetRoomChangeFormatCurrentCultureUInt16(match));
        Detail::NetRoomChangeSetTransitionRoomId(meta->Id);
        Detail::NetRoomChangeSceneSetFadeOutBlackLoadRoom(
            scene, 10.0F / 30.0F, true);
    }

    std::shared_ptr<Entities::PlayerEntity> NetRoomChange::RebuildPlayers(
        Scene& scene, Hunter hunter, std::int32_t recolor)
    {
        static_cast<void>(scene);
        const std::int32_t localSlot
            = std::max(Detail::NetRoomChangeSessionLocalSlot(), 0);
        for (std::int32_t slot = 0; slot < Entities::PlayerEntity::MaxPlayers(); ++slot)
        {
            const Hunter slotHunter = slot == localSlot
                ? hunter
                : Detail::NetRoomChangeSessionSlotHunter(slot);
            std::shared_ptr<Entities::PlayerEntity> created
                = Entities::PlayerEntity::Create(
                    slotHunter, slot == localSlot ? recolor : 0);
            if (created == nullptr)
            {
                continue;
            }
            created->SetLoadFlags(created->LoadFlags() | Entities::LoadFlags::SlotActive);
            created->SetLoadFlags(created->LoadFlags() | Entities::LoadFlags::Active);
            created->SetLoadFlags(created->LoadFlags() | Entities::LoadFlags::Initial);
            created->NodeRef = Formats::Culling::NodeRef::None;
            Detail::NetRoomChangeSetPlayerCameraNodeRefNone(*created);
            created->SetIsBot(false);
            Detail::NetRoomChangeSetPlayerBotLevel(*created, 0);
            const bool occupied = slot == localSlot
                || (slot < Detail::NetRoomChangeSessionSlotOccupiedLength()
                    && Detail::NetRoomChangeSessionSlotOccupied(slot));
            if (!occupied)
            {
                created->SetLoadFlags(
                    created->LoadFlags() & ~Entities::LoadFlags::Active);
            }
        }
        Entities::PlayerEntity::SetPlayerCount(1);
        Entities::PlayerEntity::SetMainPlayerIndex(localSlot);
        Detail::NetRoomChangeSlotManagerReset();
        NetPlayerSetup::Reset();
        NetDamage::ResetForRoomChange();
        NetHitPrediction::ForgetPending();
        ResetScores();
        Detail::NetRoomChangeConsoleWriteLine(
            "[net] player slots rebuilt for the new room, main player = slot "
            + Detail::NetRoomChangeFormatCurrentCultureInt32(localSlot));
        return Entities::PlayerEntity::Players().at(
            static_cast<std::size_t>(localSlot));
    }

    void NetRoomChange::AfterRebuild(Scene& scene)
    {
        _loadedFrame = std::max(Detail::NetRoomChangeSessionNetFrame(), 1U);
        Detail::NetRoomChangePlayerBridgeNoteRoomChanged();
        NetLaunch::DisableCheatsForMatch();
        ReloadIntroCamSeq(scene);
        for (std::int32_t slot = 0;
            slot < static_cast<std::int32_t>(
                Entities::PlayerEntity::Players().size()); ++slot)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players()[static_cast<std::size_t>(slot)];
            if (slot == Entities::PlayerEntity::MainPlayerIndex())
            {
                continue;
            }
            if ((player->LoadFlags() & Entities::LoadFlags::SlotActive)
                == Entities::LoadFlags::None)
            {
                NetLog::Event("slot "
                    + Detail::NetRoomChangeFormatCurrentCultureInt32(slot)
                    + " skipped on rebuild: flags="
                    + Detail::NetRoomChangeFormatLoadFlags(player->LoadFlags()));
                continue;
            }
            Detail::NetRoomChangeSceneInsertPlayer(scene, player);
            player->Initialize();
            Detail::NetRoomChangeSceneInitPlayer(scene, player);
            Detail::NetRoomChangeSceneInitHalfturret(scene, player);
            NetLog::Event("slot "
                + Detail::NetRoomChangeFormatCurrentCultureInt32(slot)
                + " re-inserted into the new room");
        }
    }

    void NetRoomChange::ReloadIntroCamSeq(Scene& scene)
    {
        Formats::CameraSequence::Current(nullptr);
        Formats::CameraSequence::Intro(nullptr);
        if (!Detail::NetRoomChangeGameStateMultiplayer()
            || Entities::PlayerEntity::PlayerCount() == 0)
        {
            return;
        }
        const std::int32_t seqId = Detail::NetRoomChangeSceneRoomId(scene) - 93 + 172;
        if (seqId < 172 || seqId >= 199)
        {
            return;
        }
        try
        {
            std::shared_ptr<Formats::CameraSequence> intro
                = Formats::CameraSequence::Load(seqId, &scene);
            intro->Initialize();
            intro->Flags(static_cast<Formats::CamSeqFlags>(
                static_cast<std::int32_t>(intro->Flags())
                | static_cast<std::int32_t>(Formats::CamSeqFlags::Loop)));
            Formats::CameraSequence::Intro(intro.get());
        }
        catch (const std::exception& ex)
        {
            const std::int32_t consoleRoomId
                = Detail::NetRoomChangeSceneRoomId(scene);
            const std::string consoleMessage = ex.what();
            Detail::NetRoomChangeConsoleWriteLine(
                "[net] no intro camera for room "
                + Detail::NetRoomChangeFormatCurrentCultureInt32(consoleRoomId)
                + ": " + consoleMessage);
            const std::int32_t logRoomId
                = Detail::NetRoomChangeSceneRoomId(scene);
            const std::string logMessage = ex.what();
            NetLog::Event(
                "no intro camera for room "
                + Detail::NetRoomChangeFormatCurrentCultureInt32(logRoomId)
                + ": " + logMessage);
        }
    }

    void NetRoomChange::ResetScores()
    {
        for (std::int32_t i = 0; i < Entities::PlayerEntity::SlotCapacity; ++i)
        {
            Detail::NetRoomChangeSetPoints(i, 0);
            Detail::NetRoomChangeSetTeamPoints(i, 0);
            Detail::NetRoomChangeSetKills(i, 0);
            Detail::NetRoomChangeSetTeamKills(i, 0);
            Detail::NetRoomChangeSetDeaths(i, 0);
            Detail::NetRoomChangeSetTeamDeaths(i, 0);
            Detail::NetRoomChangeSetStandings(i, 0);
            Detail::NetRoomChangeSetTeamStandings(i, 0);
            Detail::NetRoomChangeSetDamageCount(i, 0);
            Detail::NetRoomChangeSetKillStreak(i, 0);
        }
        Detail::NetRoomChangeGameStateResetMatchProgress();
        NetMatchEnd::Reset();
    }
}
