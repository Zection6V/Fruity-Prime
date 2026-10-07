// The Morph Ball touch block of IntentPacket: what an owner reports so the
// authority can run the same touch roll and touch/shoulder boost branches.
#include "../Mods/Network/NetProtocol.hpp"
#include "../Mods/Input/HostTouch.hpp"
#include "../Mods/Input/TouchInputAdapter.hpp"
#include "../Entities/Players/MorphBallBoostStateMachine.hpp"
#include "../Entities/Players/MorphBallTouchRules.hpp"

#include <cmath>
#include <cstdio>
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
