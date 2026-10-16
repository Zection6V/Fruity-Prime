#include "Mods/Network/Testing/Combat/VhoscytheCombatCheck.hpp"

#include "Mods/Network/Gameplay/Claims/Hit/NetHitClaims.hpp"
#include "Mods/Network/Gameplay/Combat/NetDamage.hpp"
#include "Mods/Network/Gameplay/Combat/NetUnlagged.hpp"
#include "Mods/Network/Gameplay/Player/NetPlayerLifecycle.hpp"
#include "Mods/Network/Server/Match/ServerSim.hpp"
#include "Mods/Network/Session/NetSession.hpp"
#include "Mods/Network/Session/RemotePlayerChannel.hpp"
#include "Mods/Network/Testing/NetHitClaimsFixture.hpp"
#include "Mods/Network/Testing/ServerSimFixture.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "GameState.hpp"
#include "NativeRuntime/System/Console.hpp"
#include "NativeRuntime/System/ExceptionText.hpp"

#include <array>
#include <cstdlib>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace MphRead::Mods::Network
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using namespace Entities;
    using OpenTK::Mathematics::Vector3;

    namespace
    {
        constexpr std::int32_t Owner = 0;
        constexpr std::int32_t Target = 1;

        // What the owner's machine sends at one of its frames: in play, in
        // alt form, the alt attack held or not, and a press `pressAge`
        // frames before this one (or none).
        IntentPacket OwnerIntent(PlayerEntity& owner, std::uint32_t frame, bool held, std::int32_t pressAge)
        {
            IntentPacket intent{};
            intent.MatchId = NetSession::CurrentMatchId();
            intent.AuthorityEpoch = NetSession::AuthorityEpoch();
            intent.SlotGeneration = NetPlayerLifecycle::Generation(Owner);
            intent.LifeId = NetPlayerLifecycle::Get(Owner);
            intent.Frame = frame;
            intent.AckFrame = NetSession::NetFrame();
            intent.Aim = Vector3(0.0F, 0.0F, 1.0F);
            intent.Position = owner.Position;
            intent.WeaponSelect = 255;
            intent.AmmoUa = 400;
            intent.AmmoMissiles = 50;
            intent.Buttons = IntentButtons::InPlayState | IntentButtons::AltFormState
                | (held ? IntentButtons::AltAttack : IntentButtons::None);
            intent.Presses = std::make_shared<std::vector<std::uint32_t>>(IntentPacket::PressHistory);
            if (pressAge >= 0 && pressAge < IntentPacket::PressHistory)
            {
                (*intent.Presses)[static_cast<std::size_t>(pressAge)] = static_cast<std::uint32_t>(IntentButtons::AltAttack);
            }
            return intent;
        }

        void Deliver(const IntentPacket& intent)
        {
            RemotePlayers().StoreIntent(Owner, intent, NetSession::NetFrame());
        }

        void ReceiveClaim(const HitClaimPacket& claim)
        {
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(1 + HitClaimPacket::Size));
            bytes[0] = 1;
            claim.Write(std::span<std::uint8_t>(bytes).subspan(1));
            NetHitClaims::Receive(Owner, bytes);
        }
    }

    std::int32_t VhoscytheCombatCheck::Run(const std::string& room)
    {
        std::string reason;
        if (!ServerSim::Available(reason)) { Runtime::ConsoleWriteLine("VHOSCYTHE FAIL " + reason); return 1; }
        ServerSim sim;
        if (!sim.Start(room, GameMode::Battle, 2, [](std::span<const std::uint8_t>) {}, [] {},
                MakeNetSessionServerSimulationPort())) return 1;
        const bool shooterHits = NetHitClaims::ShooterHits();
        int result = 1, checks = 0;
        const auto check = [&checks](bool ok, const std::string& name)
        {
            if (!ok) throw std::runtime_error(name);
            ++checks;
            Runtime::ConsoleWriteLine("VHOSCYTHE PASS " + name);
        };
        try
        {
            MatchStatePacket match;
            match.MatchId = 1; match.AuthorityEpoch = 1; match.RoomKey = room;
            match.Mode = static_cast<std::uint8_t>(GameMode::Battle);
            NetSession::ApplyMatchState(match, false);
            auto roster = RosterPacket::Create();
            roster.MatchId = 1; roster.AuthorityEpoch = 1; roster.Revision = 1; roster.Count = 2;
            const std::array hunters{Hunter::Noxus, Hunter::Samus};
            for (std::uint8_t slot = 0; slot < 2; ++slot)
            {
                (*roster.Slots)[slot] = slot; (*roster.Generations)[slot] = 1;
                (*roster.Hunters)[slot] = static_cast<std::uint8_t>(hunters[slot]);
                (*roster.Names)[slot] = "VHOSCYTHE" + std::to_string(slot);
            }
            NetSession::ApplyRoster(roster);
            for (int i = 0; i < 120; ++i) sim.Step();
            auto& noxus = *PlayerEntity::Players()[Owner];
            auto& victim = *PlayerEntity::Players()[Target];
            check(noxus.ModIsInPlay() && victim.ModIsInPlay() && noxus.Hunter() == Hunter::Noxus
                && sim.StepFailures() == 0, "headless authority spawns Noxus and Samus as remote humans");

            const std::uint32_t startup = static_cast<std::uint32_t>(noxus._values.AltAttackStartup * 2);
            const auto active = [&] { return TestFlag(noxus.Flags2(), PlayerFlags2::AltAttack); };
            // Park the victim out of reach until the hit cases put it in contact.
            const Vector3 home = noxus.Position;
            const auto park = [&](float distance)
            {
                victim.ModForceForm(true);
                victim.Position = home + Vector3(distance, 0, 0);
                victim._spawnInvulnTimer = 0;
                victim._damageInvulnTimer = 0;
                victim.SetHealth(99);
            };
            park(30.0F);
            std::uint32_t ownerFrame = 10000;
            const auto idle = [&](int steps)
            {
                for (int i = 0; i < steps; ++i)
                {
                    Deliver(OwnerIntent(noxus, ++ownerFrame, false, -1));
                    sim.Step();
                }
            };
            noxus.ModForceForm(true);
            idle(30);
            check(noxus.IsAltForm() && !noxus.IsMorphing() && !active() && noxus.ModAltAttackTime() == 0,
                "the owner's relayed alt form is settled on the authority");

            // Holds the attack from owner frame `press`, delivering each owner
            // frame only when `deliver(i)` says so (i counted from the press),
            // and the first delivery `late` frames after the press. Returns
            // the owner frame the authority's copy first went active at, or 0.
            const auto hold = [&](std::uint32_t frames, std::uint32_t late,
                const std::function<bool(std::uint32_t)>& deliver, std::vector<std::uint16_t>* counts)
            {
                const std::uint32_t press = ownerFrame + 1;
                std::uint32_t activeAt = 0;
                for (std::uint32_t i = 0; i < frames; ++i)
                {
                    ownerFrame = press + i;
                    if (i >= late && (i == late || deliver(i)))
                    {
                        // The press stays in the history for eight frames.
                        const std::int32_t age = static_cast<std::int32_t>(i);
                        Deliver(OwnerIntent(noxus, ownerFrame, true, age < IntentPacket::PressHistory ? age : -1));
                    }
                    sim.Step();
                    if (counts != nullptr) counts->push_back(noxus.ModAltAttackTime());
                    if (activeAt == 0 && active()) activeAt = ownerFrame;
                }
                return activeAt;
            };
            const auto release = [&]
            {
                Deliver(OwnerIntent(noxus, ++ownerFrame, false, -1));
                sim.Step();
            };

            // 1. A press that arrives three frames late, then every other frame.
            std::vector<std::uint16_t> counts;
            const std::uint32_t pressA = ownerFrame + 1;
            const std::uint32_t activeA = hold(startup + 6, 3, [](std::uint32_t i) { return i % 2 == 0; }, &counts);
            bool steady = true;
            for (std::size_t i = 5; i < counts.size() && counts[i] < startup; ++i)
            {
                steady &= counts[i] == counts[i - 1] + 1;
            }
            // The owner's own count after its frame press+i is i+1.
            check(counts.size() > 4 && counts[4] == 5,
                "a late press is counted from the owner's press frame, not its arrival (count "
                    + std::to_string(counts.size() > 4 ? counts[4] : 0) + " of the owner's 5)");
            check(steady, "between relayed intents the count still advances one owner frame a step");
            check(activeA == pressA + startup - 1,
                "Vhoscythe goes active on the owner's own startup frame (frame "
                    + std::to_string(activeA - pressA) + " after the press; the owner's is "
                    + std::to_string(startup - 1) + ")");
            release();
            check(!active() && noxus.ModAltAttackTime() == 0, "the relayed release ends the attack");
            idle(30);

            // 2. Held for exactly the startup, the press late and the release
            //    on time: the hold that used to come out short of the threshold.
            const std::uint32_t activeB = hold(startup, 3, [](std::uint32_t) { return true; }, nullptr);
            check(activeB != 0, "a release right at the threshold still spins on the authority");
            release();
            check(!active(), "and ends on the release");
            idle(30);
            // One frame shorter never spun for the owner, so it must not here.
            const std::uint32_t activeShort = hold(startup - 1, 0, [](std::uint32_t) { return true; }, nullptr);
            release();
            check(activeShort == 0 && !active(),
                "a hold one frame short of the threshold does not spin, as on the owner's screen");
            idle(30);

            // 3. A packet gap while held is not a release.
            hold(startup + 2, 0, [](std::uint32_t) { return true; }, nullptr);
            check(active(), "precondition: active before the gap");
            for (int i = 0; i < 30; ++i) { ++ownerFrame; sim.Step(); }
            check(active() && noxus.Controls().AltAttack().IsDown(),
                "thirty frames without an intent keep the attack held");
            release();
            idle(30);

            // 4. The authority resolving the hit itself (bots, -servershots).
            NetHitClaims::ShooterHits(false);
            park(1.0F);
            const std::int32_t damage = noxus._values.AltAttackDamage;
            bool hit = false;
            const std::uint32_t pressD = ownerFrame + 1;
            for (std::uint32_t i = 0; i < startup + 10 && !hit; ++i)
            {
                ownerFrame = pressD + i;
                victim.Position = home + Vector3(1.0F, 0, 0);
                Deliver(OwnerIntent(noxus, ownerFrame, true, i < static_cast<std::uint32_t>(IntentPacket::PressHistory)
                    ? static_cast<std::int32_t>(i) : -1));
                sim.Step();
                hit = victim.Health() < 99;
            }
            check(hit && victim.Health() == 99 - damage,
                "an active Vhoscythe in contact deals its damage on the authority ("
                    + std::to_string(99 - victim.Health()) + " of " + std::to_string(damage) + ")");
            check(!active(), "the hit ends the attack, as on the cartridge");
            release();
            park(30.0F);
            idle(30);

            // 5. The owner's machine resolved it: the authority's own copy
            //    stands aside and the owner's claim is judged and applied.
            NetHitClaims::ShooterHits(true);
            check(NetDamage::Suppress(victim, &noxus, DamageFlags::NoSfx | DamageFlags::NoDmgInvuln),
                "with shooter hits on, the authority defers a human's Vhoscythe to the owner's claim");
            NetHitClaims::Reset();
            park(1.0F);
            NetHitClaims::Tick();
            NetUnlagged::Record(NetSession::NetFrame() - 2);
            NetUnlagged::Record(NetSession::NetFrame() - 1);
            NetUnlagged::Record(NetSession::NetFrame());
            HitClaimPacket claim{};
            claim.MatchId = NetSession::CurrentMatchId();
            claim.AuthorityEpoch = NetSession::AuthorityEpoch();
            claim.ShooterGeneration = NetPlayerLifecycle::Generation(Owner);
            claim.ShooterLifeId = NetPlayerLifecycle::Get(Owner);
            claim.VictimSlot = static_cast<std::uint8_t>(Target);
            claim.VictimGeneration = NetPlayerLifecycle::Generation(Target);
            claim.VictimLifeId = NetPlayerLifecycle::Get(Target);
            claim.ClaimId = 1;
            claim.AckFrame = NetSession::NetFrame() - 1;
            claim.LaunchFrame = claim.AckFrame;
            claim.Damage = static_cast<std::uint16_t>(damage);
            claim.Beam = HitClaimPacket::NoBeam;
            claim.HitPoint = victim.Position;
            HitClaimPacket far = claim;
            far.HitPoint = victim.Position + Vector3(NetHitClaims::MeleeRadius + 1.0F, 0, 0);
            const std::uint8_t farVerdict = NetHitClaimsFixture::Judge(Owner, far);
            check(farVerdict == HitVerdictPacket::ResultGeometry,
                "a melee claim beyond reach of where the authority had the victim is refused ("
                    + HitVerdictPacket::Describe(farVerdict) + ")");
            const std::uint8_t verdict = NetHitClaimsFixture::Judge(Owner, claim);
            check(verdict == HitVerdictPacket::ResultApplied,
                "the owner's Vhoscythe claim is accepted (" + HitVerdictPacket::Describe(verdict) + ")");
            ReceiveClaim(claim);
            for (std::int32_t i = 0; i < NetHitClaims::MaxGraceFrames + 2; ++i)
            {
                NetSession::Update(NetSession::NetFrame() / 60.0);
                NetHitClaims::Tick();
            }
            check(victim.Health() == 99 - damage && NetHitClaims::AppliedHere() == 1,
                "and lands on the authority (" + std::to_string(99 - victim.Health()) + " damage)");
            Runtime::ConsoleWriteLine("VHOSCYTHE PASS " + std::to_string(checks) + " checks");
            result = 0;
        }
        catch (...) { Runtime::ConsoleErrorWriteLine("VHOSCYTHE FAIL " + Runtime::ExceptionToString(std::current_exception())); }
        NetHitClaims::ShooterHits(shooterHits);
        sim.Stop();
        return result;
    }
}
