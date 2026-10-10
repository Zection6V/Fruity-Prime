// The Morph Ball touch block of IntentPacket: what an owner reports so the
// authority can run the same touch roll and touch/shoulder boost branches.
#include "../Mods/Network/NetProtocol.hpp"
#include "../Mods/Network/LocalShotLog.hpp"
#include "../Mods/Network/RemoteShotQueue.hpp"
#include "../Mods/Network/ReplayedPressOrder.hpp"
#include "../Mods/Input/HostTouch.hpp"
#include "../Mods/Input/TouchInputAdapter.hpp"
#include "../Entities/Players/MorphBallBoostStateMachine.hpp"
#include "../Entities/Players/MorphBallTouchRules.hpp"

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <vector>

using MphRead::Mods::Network::IntentPacket;
using namespace MphRead::Mods::Input;
namespace Boost = MphRead::Entities::MorphBallBoostStateMachine;
namespace Rules = MphRead::Entities::MorphBallTouchRules;

namespace
{
    void Expect(bool value, const char* reason)
    {
        if (!value) throw std::runtime_error(reason);
    }

    bool Near(float a, float b) { return std::fabs(a - b) <= 1e-6F; }

    using ShotEvent = IntentPacket::ShotEvent;
    using MphRead::BeamType;
    using MphRead::Mods::Network::LocalShotLog;
    using MphRead::Mods::Network::RemoteShotQueue;
    using MphRead::Mods::Network::ReplayedPressOrder;

    IntentPacket WithShots(std::uint32_t frame, std::initializer_list<ShotEvent> events)
    {
        IntentPacket packet{};
        packet.Frame = frame;
        for (const ShotEvent& event : events)
        {
            packet.ShotHistory[packet.ShotHistoryLength++] = event;
        }
        return packet;
    }

    std::uint8_t Id(BeamType beam) { return static_cast<std::uint8_t>(beam); }

    // Protocol 20 on the wire: the ray, this frame's shot, and the history.
    void ShotEventWire()
    {
        IntentPacket shot{};
        shot.ShotHistory[0] = {41, 100, Id(BeamType::Missile)};
        shot.ShotHistory[1] = {42, 104, Id(BeamType::OmegaCannon)};
        shot.ShotHistoryLength = 2;
        std::vector<std::uint8_t> bytes(IntentPacket::ShotFullSize);
        shot.Write(bytes);
        const IntentPacket noRay = IntentPacket::Read(bytes);
        Expect(!noRay.HasShot && noRay.ShotSequence == 0 && noRay.ShotWeaponId == IntentPacket::NoWeapon,
            "a frame that fired nothing carries no shot of its own");
        Expect(noRay.ShotHistoryLength == 2 && noRay.ShotHistory[1].Sequence == 42
            && noRay.ShotHistory[1].Frame == 104 && noRay.ShotHistory[1].WeaponId == Id(BeamType::OmegaCannon),
            "but still carries the history");

        shot.HasShot = true;
        shot.ShotOrigin = OpenTK::Mathematics::Vector3(1, 2, 3);
        shot.ShotDirection = OpenTK::Mathematics::Vector3(0, 0, -1);
        shot.ShotSequence = 42;
        shot.ShotWeaponId = Id(BeamType::OmegaCannon);
        shot.Write(bytes);
        const IntentPacket withRay = IntentPacket::Read(bytes);
        Expect(withRay.HasShot && Near(withRay.ShotOrigin.Y, 2.0F) && Near(withRay.ShotDirection.Z, -1.0F)
            && withRay.ShotSequence == 42 && withRay.ShotWeaponId == Id(BeamType::OmegaCannon),
            "the ray, sequence and weapon of this frame's shot round-trip");

        // A Shock Coil frame: a ray with no event.
        IntentPacket coil{};
        coil.HasShot = true;
        coil.ShotOrigin = OpenTK::Mathematics::Vector3(1, 2, 3);
        coil.ShotDirection = OpenTK::Mathematics::Vector3(1, 0, 0);
        coil.Write(bytes);
        const IntentPacket coilRead = IntentPacket::Read(bytes);
        Expect(coilRead.HasShot && coilRead.ShotSequence == 0, "continuous fire keeps its ray without a sequence");

        bytes[static_cast<std::size_t>(IntentPacket::FullSize + IntentPacket::ShotSize)] = 0xFF;
        Expect(IntentPacket::Read(bytes).ShotHistoryLength == IntentPacket::ShotHistoryCount,
            "a corrupt history count is clamped");
        Expect(IntentPacket::Read(std::span(bytes).first(static_cast<std::size_t>(IntentPacket::FullSize))).ShotHistoryLength == 0,
            "an intent without the block has no history");
    }

    // The receiver: ShotWeaponId decides the weapon, once per sequence.
    void ShotEventReceiver()
    {
        RemoteShotQueue shots;
        Expect(!shots.Next().has_value(), "nothing pending on a new life");

        // The Omega shot, then the switch to the Power Beam arriving first:
        // the switch is WeaponSelect's business, the shot keeps its weapon.
        IntentPacket switched = WithShots(205, {{1, 200, Id(BeamType::OmegaCannon)}});
        switched.WeaponSelect = Id(BeamType::PowerBeam);
        shots.Receive(switched);
        Expect(shots.Next() == BeamType::OmegaCannon, "the shot fires the weapon it left, whatever is held now");

        // The same event again, in a later intent and in an older one.
        shots.Receive(WithShots(206, {{1, 200, Id(BeamType::OmegaCannon)}}));
        shots.Receive(WithShots(201, {{1, 200, Id(BeamType::OmegaCannon)}}));
        shots.Consume();
        Expect(!shots.Next().has_value(), "one event fires once however often it arrives");

        // Rapid fire: each shot keeps its own weapon, in firing order.
        shots.Receive(WithShots(230, {
            {2, 220, Id(BeamType::Missile)}, {3, 224, Id(BeamType::PowerBeam)}, {4, 228, Id(BeamType::Missile)}}));
        Expect(shots.Next() == BeamType::Missile, "first shot: Missile");
        shots.Consume();
        Expect(shots.Next() == BeamType::PowerBeam, "second shot: Power Beam");
        shots.Consume();
        Expect(shots.Next() == BeamType::Missile, "third shot: Missile");
        shots.Consume();

        // A lost intent: the next one still carries the shot.
        shots.Receive(WithShots(242, {{4, 228, Id(BeamType::Missile)}, {5, 240, Id(BeamType::Imperialist)}}));
        Expect(shots.Next() == BeamType::Imperialist, "a shot whose intent was lost arrives with the next");
        shots.Consume();

        // Out of order: the newer intent first, then an older one.
        shots.Receive(WithShots(260, {{7, 258, Id(BeamType::Judicator)}}));
        shots.Receive(WithShots(250, {{6, 248, Id(BeamType::Magmaul)}}));
        Expect(shots.Next() == BeamType::Judicator, "a sequence behind one already queued is not queued again");
        shots.Consume();

        // An event whose trigger never fired here goes stale rather than
        // lending an old weapon to a later shot.
        shots.Receive(WithShots(300, {{8, 300, Id(BeamType::OmegaCannon)}}));
        shots.Receive(WithShots(340, {}));
        Expect(!shots.Next().has_value(), "a stale event is dropped");

        // Death and respawn: a new life starts empty and takes the sender's
        // sequence wherever it is.
        shots.Receive(WithShots(400, {{9, 399, Id(BeamType::ShockCoil)}}));
        shots.Reset();
        Expect(!shots.Next().has_value(), "a new life holds none of the old life's shots");
        shots.Receive(WithShots(410, {{10, 409, Id(BeamType::VoltDriver)}}));
        Expect(shots.Next() == BeamType::VoltDriver, "and takes the new life's");
        shots.Reset();

        // Nonsense from the wire is refused.
        shots.Receive(WithShots(500, {{11, 500, 0x7F}, {0, 500, Id(BeamType::Missile)}}));
        Expect(!shots.Next().has_value(), "an unknown weapon or a zero sequence is ignored");
    }

    // The owner's side: a sequence per shot, the last few in every intent.
    void ShotEventSender()
    {
        LocalShotLog log;
        for (std::uint32_t i = 0; i < 6; ++i)
        {
            log.Record(100 + i, i % 2 == 0 ? BeamType::Missile : BeamType::OmegaCannon);
        }
        IntentPacket intent{};
        intent.HasShot = true;
        log.Fill(intent, 105);
        Expect(intent.ShotHistoryLength == IntentPacket::ShotHistoryCount
            && intent.ShotHistory[0].Sequence == 3 && intent.ShotHistory[3].Sequence == 6,
            "the history keeps the newest shots, oldest first");
        Expect(intent.ShotSequence == 6 && intent.ShotWeaponId == Id(BeamType::OmegaCannon),
            "this frame's ray names its shot");
        IntentPacket later{};
        later.HasShot = true;
        log.Fill(later, 110);
        Expect(later.ShotSequence == 0 && later.ShotHistoryLength == IntentPacket::ShotHistoryCount,
            "a later frame's ray that made no event names none, the history still rides");
        log.Reset();
        log.Record(200, BeamType::PowerBeam);
        IntentPacket respawned{};
        log.Fill(respawned, 200);
        Expect(respawned.ShotHistoryLength == 1 && respawned.ShotHistory[0].Sequence == 7,
            "a new life empties the history but never reuses a sequence");
    }

    // A morph replayed in the same intent as a trigger waits behind the shot.
    void PressOrder()
    {
        ReplayedPressOrder order;
        Expect(order.MorphPressed(true, false, true), "a morph alone goes through");
        Expect(!order.MorphPressed(true, true, true), "a morph with an unspent shot waits");
        Expect(order.MorphPressed(false, false, false), "and goes through the next frame");
        Expect(!order.MorphPressed(false, false, false), "once");
        Expect(order.MorphPressed(true, true, false), "with no shot pending, nothing is held");
    }

    IntentPacket Transport(const NativeTouchState::Reported& report)
    {
        IntentPacket packet{};
        packet.SlotGeneration = 2;
        packet.LifeId = 3;
        packet.SetTouchReport(report); // same conversion CaptureIntent uses
        std::vector<std::uint8_t> bytes(IntentPacket::FullSize);
        packet.Write(bytes);
        return IntentPacket::Read(bytes);
    }

    void Receive(TouchInputAdapter& remote, const IntentPacket& packet)
    {
        // The conversion and adapter called by ApplyIntent/ModSetReportedTouch.
        remote.BeginStep();
        remote.ApplyReported(packet.TouchReport(), packet.SlotGeneration, packet.LifeId);
    }

    Boost::Result Advance(const TouchInputAdapter& input, Boost::State& state,
        Boost::SampleLatch& latch, bool held)
    {
        const auto& touch = input.State();
        return Boost::AdvanceSample(state, {touch.Down, touch.Continued, touch.Delta4X, touch.Delta4Y, held},
            {4, 20, false}, latch, input.Sample().Identity());
    }

    Rules::PlanarDelta Roll(const TouchInputAdapter& input)
    {
        const auto& touch = input.State();
        return Rules::TouchRollStep(touch.Delta4X, touch.Delta4Y, Rules::TouchRollPerDsPixel,
            0.6F, 0.8F, 0.8F, -0.6F, input.Sample().RollShare());
    }

    void OwnerAuthorityParity()
    {
        TouchInputAdapter owner{}, authority{};
        Boost::State local{false, true, 7}, remote = local;
        Boost::SampleLatch localLatch{}, remoteLatch{};
        int localBoosts = 0;
        int remoteBoosts = 0;
        // SPACE charge -> swipe -> keep holding -> release; lift and re-arm.
        const std::vector<std::tuple<bool, int, int, bool>> trace{
            {true, 10, 10, true}, {true, 120, 10, true}, {true, 125, 15, true},
            {true, 130, 15, false}, {false, 0, 0, true}, {true, 10, 10, true},
            {true, 115, 10, false}};
        NativeTouchState reference{};
        for (auto [down, x, y, held] : trace)
        {
            reference.Update(down, x, y);
            Rules::PlanarDelta localSum{}, remoteSum{};
            for (int step = 0; step < 2; ++step)
            {
                HostTouch::Publish(down, x, y);
                owner.Step({}); // real host producer/adapter, not a toy clock
                const auto packet = Transport(owner.Sample().Report());
                Receive(authority, packet);
                Expect(authority.State().Report() == owner.State().Report(), "authority sees the owner's native state");
                Expect(authority.Sample().NewNativeSampleThisStep() == owner.Sample().NewNativeSampleThisStep(),
                    "wire sample boundaries match the owner on both substeps");
                auto a = Advance(owner, local, localLatch, held);
                auto b = Advance(authority, remote, remoteLatch, held);
                Expect(a.Branch == b.Branch && a.Boost == b.Boost && a.ChargeSpent == b.ChargeSpent,
                    "owner and authority produce the same branch/event sequence");
                Expect(local.Boosting == remote.Boosting && local.CanTouchBoost == remote.CanTouchBoost && local.Charge == remote.Charge,
                    "owner and authority preserve the same boost state and charge");
                localBoosts += a.Boost == Boost::Fired::TouchBoost;
                remoteBoosts += b.Boost == Boost::Fired::TouchBoost;
                const auto lr = Roll(owner);
                const auto rr = Roll(authority);
                localSum.X += lr.X; localSum.Z += lr.Z;
                remoteSum.X += rr.X; remoteSum.Z += rr.Z;
            }
            const auto native = Rules::TouchRoll(reference.Delta4X, reference.Delta4Y,
                Rules::TouchRollPerDsPixel, 0.6F, 0.8F, 0.8F, -0.6F);
            Expect(Near(localSum.X, native.X) && Near(localSum.Z, native.Z)
                && Near(remoteSum.X, native.X) && Near(remoteSum.Z, native.Z),
                "both owner and authority integrate one native roll per sample pair");
            if (!down) local.Boosting = remote.Boosting = false; // cooldown ended before next gesture
        }
        Expect(localBoosts == 2 && remoteBoosts == 2, "both gestures fire one TouchBoost each on each machine");
        HostTouch::Withdraw();
    }

    void PacketFaults()
    {
        TouchInputAdapter remote{};
        Boost::State state{false, true, 7};
        Boost::SampleLatch latch{};
        auto first = Transport({true, true, 120, -20, 10, false});
        Receive(remote, first);
        auto r = Advance(remote, state, latch, true);
        const auto half = Roll(remote);
        Expect(r.Boost == Boost::Fired::TouchBoost && state.Charge == 7, "first report fires TouchBoost");
        Receive(remote, first);
        r = Advance(remote, state, latch, false);
        Expect(remote.Sample().RollShare() == 0 && r.Boost == Boost::Fired::None && state.Charge == 7,
            "duplicate report adds no roll or boost and cannot release Shoulder");
        for (int heldSteps = 0; heldSteps < 8; ++heldSteps)
        {
            remote.BeginStep(); // no new intent available this simulation step
            r = Advance(remote, state, latch, true);
            Expect(remote.Sample().RollShare() == 0 && r.Boost == Boost::Fired::None && state.Charge == 7,
                "packet loss cannot reuse an impulse or change a latched touch branch");
        }
        auto second = Transport({true, true, 120, -20, 10, true});
        Receive(remote, second);
        r = Advance(remote, state, latch, false);
        const auto otherHalf = Roll(remote);
        const auto native = Rules::TouchRoll(120, -20, Rules::TouchRollPerDsPixel, 0.6F, 0.8F, 0.8F, -0.6F);
        Expect(Near(half.X + otherHalf.X, native.X) && Near(half.Z + otherHalf.Z, native.Z)
            && r.Boost == Boost::Fired::None && state.Charge == 7, "delayed sibling completes exactly one impulse and stays latched");
        for (const auto& stale : {second, first, Transport({true, true, -120, 0, 9, true})})
        {
            Receive(remote, stale);
            r = Advance(remote, state, latch, false);
            Expect(remote.Sample().RollShare() == 0 && remote.State().Delta4X == 120
                && r.Boost == Boost::Fired::None && state.Charge == 7,
                "duplicate/stale/reordered reports cannot replace or reconsume the sample");
        }
        auto next = Transport({true, true, 120, -20, 11, false});
        Receive(remote, next); // same deltas, different sample
        r = Advance(remote, state, latch, true);
        Expect(remote.Sample().NewNativeSampleThisStep() && r.Branch == Rules::BoostBranch::Shoulder && state.Charge == 8,
            "identical deltas with a new identity re-arbitrate and resume Shoulder");
        // Drop sample 11's sibling, then accept a lift and a new swipe.
        Receive(remote, Transport({false, false, 0, 0, 12, false}));
        (void)Advance(remote, state, latch, true);
        Expect(state.CanTouchBoost, "a dropped sibling does not suppress the next sample's re-arm");
        state.Boosting = false;
        // First substep lost: the second is still a new sample and may boost.
        Receive(remote, Transport({true, true, 120, 0, 13, true}));
        r = Advance(remote, state, latch, true);
        Expect(r.Boost == Boost::Fired::TouchBoost && remote.Sample().RollShare() == 0.5F,
            "losing the first substep delays a qualifying boost by at most one delivered sibling");
        Receive(remote, Transport({true, true, -120, 0, 13, false}));
        r = Advance(remote, state, latch, true);
        Expect(remote.Sample().RollShare() == 0 && remote.State().Delta4X == 120 && r.Boost == Boost::Fired::None,
            "a late first substep cannot rewind the sibling or fire again");

        TouchInputAdapter wrap{};
        Receive(wrap, Transport({true, true, 1, 0, std::numeric_limits<std::uint32_t>::max(), false}));
        Receive(wrap, Transport({true, true, 2, 0, 0, false}));
        Expect(wrap.Sample().NewNativeSampleThisStep() && wrap.State().Delta4X == 2, "sample sequence wraps forward");
        Receive(wrap, Transport({true, true, 1, 0, std::numeric_limits<std::uint32_t>::max(), true}));
        Expect(wrap.Sample().RollShare() == 0 && wrap.State().Delta4X == 2, "pre-wrap sample is stale");
        auto respawn = Transport({true, true, 120, 0, 0, false});
        respawn.LifeId = 4;
        Receive(remote, respawn);
        state = {false, true, 7};
        r = Advance(remote, state, latch, true);
        Expect(remote.Sample().NewNativeSampleThisStep() && r.Boost == Boost::Fired::TouchBoost,
            "a new life gets a new identity even if the sequence resets");
        respawn.SlotGeneration = 3;
        Receive(remote, respawn);
        Expect(remote.Sample().NewNativeSampleThisStep(), "a reused slot starts a new sample stream");
    }
}

int main()
{
    try
    {
        IntentPacket touch{};
        touch.ChargeLevel = 7;
        touch.BoostDamage = 30;
        touch.SetTouchReport({true, true, -300, 91, 0xFEDCBA98U, true});
        std::vector<std::uint8_t> bytes(IntentPacket::FullSize);
        touch.Write(bytes);
        const IntentPacket full = IntentPacket::Read(bytes);
        Expect(full.HasState && full.ChargeLevel == 7 && full.BoostDamage == 30, "the state block survives");
        Expect(full.HasTouch() && full.TouchFlags == touch.TouchFlags
            && full.TouchDelta4X == -300 && full.TouchDelta4Y == 91, "the touch block round-trips, signed");
        Expect(full.HasTouchSample() && full.TouchReport() == touch.TouchReport(), "32-bit sample identity and sibling bit round-trip");

        // Protocol 15's touch payload and partial sequence tails retain touch
        // but must never claim to contain a complete sample identity.
        for (int size = IntentPacket::Size + IntentPacket::StateSize + IntentPacket::LegacyTouchSize;
            size < IntentPacket::FullSize; ++size)
        {
            const auto legacy = IntentPacket::Read(std::span(bytes).first(static_cast<std::size_t>(size)));
            Expect(legacy.HasTouch() && !legacy.HasTouchSample() && legacy.TouchDelta4X == -300,
                "legacy or truncated identity keeps signed touch without inventing a sequence");
        }

        std::vector<std::uint8_t> stateOnly(IntentPacket::Size + IntentPacket::StateSize, 0xFF);
        touch.Write(stateOnly);
        Expect(IntentPacket::Read(stateOnly).ChargeLevel == 7 && !IntentPacket::Read(stateOnly).HasTouch(),
            "writing a state-only intent omits touch flags");

        // What a demo recorded before the touch block holds: the state block
        // with its fourth byte zero, and nothing after it.
        bytes.resize(static_cast<std::size_t>(IntentPacket::Size + IntentPacket::StateSize));
        bytes[static_cast<std::size_t>(IntentPacket::Size + 3)] = 0;
        const IntentPacket older = IntentPacket::Read(bytes);
        Expect(older.HasState && older.ChargeLevel == 7, "a state-only payload still carries the state");
        Expect(!older.HasTouch() && older.TouchFlags == 0 && older.TouchDelta4X == 0 && older.TouchDelta4Y == 0,
            "and reads as no contact");

        bytes.resize(static_cast<std::size_t>(IntentPacket::Size));
        const IntentPacket bare = IntentPacket::Read(bytes);
        Expect(!bare.HasState && !bare.HasTouch(), "a bare intent has neither block");
        ShotEventWire();
        ShotEventReceiver();
        ShotEventSender();
        PressOrder();
        OwnerAuthorityParity();
        PacketFaults();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "IntentTouchPayload: %s\n", e.what());
        return 1;
    }
    std::puts("IntentTouchPayload: ok");
    return 0;
}
