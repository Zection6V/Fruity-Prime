#include "FormPresentationCheck.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Mods/Network/Gameplay/Player/NetPlayerBridge.hpp"
#include "Mods/Network/Gameplay/Player/NetPlayerLifecycle.hpp"
#include "Mods/Network/Gameplay/Replication/PlayerStateSnapshotCapture.hpp"
#include "Mods/Network/Gameplay/Replication/NetSmoothing.hpp"
#include "Mods/Network/Bots/NetBotInput.hpp"
#include "Mods/Network/Server/Match/ServerSim.hpp"
#include "Mods/Network/Testing/NetSessionFixture.hpp"
#include "NativeRuntime/System/Console.hpp"
#include <array>
#include <stdexcept>

namespace MphRead::Mods::Diagnostics
{
    std::int32_t FormPresentationCheck::Run(const std::string& room)
    {
        using namespace Network;
        using namespace Entities;
        ServerSim sim;
        if (!sim.Start(room, GameMode::Battle, 8, [](std::span<const std::uint8_t>) {}, [] {},
            MakeNetSessionServerSimulationPort())) return 1;
        int failures = 0, checks = 0;
        const auto check = [&](bool ok, const std::string& message)
        {
            ++checks;
            failures += !ok;
            NativeRuntime::ConsoleWriteLine(std::string("FORM PRESENTATION ") + (ok ? "PASS " : "FAIL ") + message);
        };
        try
        {
            MatchStatePacket match;
            match.MatchId = 1; match.AuthorityEpoch = 1; match.RoomKey = room;
            match.Mode = static_cast<std::uint8_t>(GameMode::Battle);
            NetSession::ApplyMatchState(match, false);
            const std::array hunters{Hunter::Samus, Hunter::Kanden, Hunter::Spire,
                Hunter::Noxus, Hunter::Trace, Hunter::Sylux, Hunter::Weavel};
            auto roster = RosterPacket::Create();
            roster.MatchId = 1; roster.AuthorityEpoch = 1; roster.Revision = 1; roster.Count = 7;
            for (std::uint8_t i = 0; i < hunters.size(); ++i)
            {
                (*roster.Slots)[i] = i; (*roster.Generations)[i] = 1;
                (*roster.Hunters)[i] = static_cast<std::uint8_t>(hunters[i]);
                (*roster.Names)[i] = "FORM" + std::to_string(i);
            }
            NetSession::ApplyRoster(roster);
            for (int i = 0; i < 120; ++i) sim.Step();
            NetSessionFixture::SetRole(NetRole::Client);
            NetSessionFixture::RevokeAuthority();
            NetSessionFixture::SetLocalSlot(7);
            std::uint32_t frame = 1000;
            for (std::size_t slot = 0; slot < hunters.size(); ++slot)
            {
                auto& player = *PlayerEntity::Players()[slot];
                const auto name = ToString(player.Hunter());
                check(player.Hunter() == hunters[slot] && player.ModIsInPlay(), name + " loaded real hunter");
                player.SetIsBot(false);
                auto baseline = PlayerStateSnapshotCapture::Capture(player, static_cast<int>(slot),
                    NetPlayerLifecycle::Generation(static_cast<int>(slot)), NetPlayerLifecycle::Get(static_cast<int>(slot)));
                baseline.Flags &= ~PlayerState::FlagAltForm;
                baseline.WeavelFlags = 0;
                NetSmoothing::Reset();
                NetPlayerBridge::ApplyState(player, baseline, false);
                NetSessionFixture::SetLocalSlot(static_cast<int>(slot));
                player.ModForceForm(false);
                player.Controls().ClearAll();
                NetBotInput::BeginInput(player);
                NetBotInput::RequestFormSwitch(player);
                NetSessionFixture::SetFrame(frame++);
                NetPlayerBridge::RecordPresses(player);
                const auto intent = NetPlayerBridge::CaptureIntent(player);
                check((intent.Buttons & IntentButtons::Morph) != IntentButtons::None
                    && intent.Presses && ((*intent.Presses)[0] & static_cast<std::uint32_t>(IntentButtons::Morph)) != 0,
                    name + " AI morph enters intent and redundant press history before simulation");
                check(!player.IsMorphing(), name + " AI request waits for common simulation input");
                player._frozenTimer = 0;
                player.ProcessInput();
                check(player.IsMorphing(), name + " common input starts native morph");
                NetBotInput::BeginInput(player);
                check(!player.Controls().Morph().IsPressed() && !player.Controls().Morph().IsDown(),
                    name + " AI morph edge clears on next input");
                NetSessionFixture::SetLocalSlot(7);
                // Offline AI and server bots: the game's own request. The AI
                // holds its morph button for several frames; none of them may
                // force the switch back while the morph is under way.
                player.ModForceForm(false);
                player._frozenTimer = 0;
                NetBotInput::RequestFormSwitch(player);
                const bool started = player.IsMorphing();
                // Part way through the clip, as a few steps would leave it.
                auto& clipFrame = (*player._bipedModel2->AnimInfo->Frame)[0];
                clipFrame = 5;
                for (int held = 0; held < 6; ++held) NetBotInput::RequestFormSwitch(player);
                check(started && player.IsMorphing() && !player.IsUnmorphing() && clipFrame == 5,
                    name + " a held native AI morph request does not restart the morph (clip frame "
                        + std::to_string(clipFrame) + " of 5)");
                player.ModForceForm(false);
                player._frozenTimer = 0; player._altAttackCooldown = 0;
                player._field6D0 = false;
                player._flags1 &= ~PlayerFlags1::Standing;
                player.ModForceForm(true);
                player.Controls().ClearAll();
                for (int step = 0; step < 100; ++step)
                {
                    ++frame;
                    player._prevPosition = player.Position;
                    player._prevSpeed = player.Speed();
                    player._volume = CollisionVolume::Move(player._volumeUnxf, player.Position);
                    player.Controls().AltAttack().SetIsPressed(step == 0);
                    player.Controls().AltAttack().SetIsDown(true);
                    player.AdvanceNativeWeaponTimers();
                    player.ProcessInput();
                    if (TestFlag(player.Flags2(), PlayerFlags2::AltAttack)) break;
                }
                const bool hasMelee = hunters[slot] == Hunter::Spire || hunters[slot] == Hunter::Noxus
                    || hunters[slot] == Hunter::Trace || hunters[slot] == Hunter::Weavel;
                if (hasMelee) check(TestFlag(player.Flags2(), PlayerFlags2::AltAttack), name + " real alt attack starts");
                auto state = PlayerStateSnapshotCapture::Capture(player, static_cast<int>(slot),
                    NetPlayerLifecycle::Generation(static_cast<int>(slot)), NetPlayerLifecycle::Get(static_cast<int>(slot)));
                state.Flags &= ~PlayerState::FlagAltForm;
                state.WeavelFlags = 0;
                NetSmoothing::Reset();
                NetSmoothing::Record(frame, std::span<const PlayerState>(&state, 1));
                for (int step = 0; step < 3; ++step) NetSmoothing::Tick();
                ReplicatedFormState sample;
                check(NetSmoothing::SampleForm(static_cast<int>(slot), sample) && !sample.AltForm,
                    name + " buffered biped reaches presentation clock");
                NetPlayerBridge::ApplyState(player, state, false);
                check(!player.IsAltForm() && !player.IsMorphing() && !player.IsUnmorphing(), name + " snapshot selects biped draw branch");
                check(!TestFlag(player.Flags2(), PlayerFlags2::AltAttack) && player._altAttackTime == 0
                    && !TestFlag(player.Flags1(), PlayerFlags1::Boosting), name + " biped has no active alt attack");
                // Re-applying the same stable snapshot must stay clean, without replaying an action.
                NetPlayerBridge::ApplyState(player, state, false);
                check(!TestFlag(player.Flags2(), PlayerFlags2::AltAttack), name + " repeated biped is clean");
            }
        }
        catch (const std::exception& error)
        {
            NativeRuntime::ConsoleErrorWriteLine(std::string("FORM PRESENTATION FAIL ") + error.what());
            ++failures;
        }
        sim.Stop(); NetSession::Stop();
        NativeRuntime::ConsoleWriteLine("FORM PRESENTATION " + std::to_string(checks) + " checks, "
            + std::to_string(failures) + " failures");
        return failures == 0 ? 0 : 1;
    }
}
