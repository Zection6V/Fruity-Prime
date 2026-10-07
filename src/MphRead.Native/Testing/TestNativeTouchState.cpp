// EU1.1 02029778 (touch producer) and 02021C28's touch branches, pinned.
#include "../Entities/Players/MorphBallBoostStateMachine.hpp"
#include "../Entities/Players/MorphBallTouchRules.hpp"
#include "../Mods/Input/DsTouchSurface.hpp"
#include "../Mods/Input/MouseMotionTouchSource.hpp"
#include "../Mods/Input/NativeTouchClock.hpp"
#include "../Mods/Input/NativeTouchState.hpp"
#include "../Mods/Input/NativeTouchSample.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
#include <stdexcept>
#include <utility>
#include <tuple>

using namespace MphRead::Mods::Input;
namespace MorphBallTouchRules = MphRead::Entities::MorphBallTouchRules;
using MorphBallTouchRules::BoostBranch;

namespace
{
    void Expect(bool value, const char* reason)
    {
        if (!value) throw std::runtime_error(reason);
    }

    bool Near(float a, float b) { return std::fabs(a - b) <= 1e-6F; }

    BoostBranch Arbitrate(bool boosting, bool canTouchBoost, const NativeTouchState& touch)
    {
        return MorphBallTouchRules::Arbitrate(boosting, canTouchBoost, touch.Continued, touch.Delta4X, touch.Delta4Y);
    }

    void Producer()
    {
        NativeTouchState s{};
        s.Update(false, 0, 0);
        s.Update(true, 100, 100);
        Expect(s.Down && !s.Continued && s.Delta4X == 0 && s.Delta4Y == 0, "first touch frame is not continued");
        for (int x : {110, 120, 130, 140}) s.Update(true, x, 100);
        Expect(s.Continued && s.Delta4X == 40 && s.Delta4Y == 0, "4-sample SUM, not average");
        s.Update(true, 150, 100);
        Expect(s.Delta4X == 40, "history keeps four samples");
        s.Update(true, 150, 100);
        Expect(s.Delta4X == 30, "oldest sample drops out");
        s.Update(false, 0, 0);
        Expect(!s.Down && !s.Continued && s.Delta4X == 0 && s.Delta4Y == 0, "release clears sums");
        for (auto v : s.DeltaHistoryX) Expect(v == 0, "release clears X history");
        // A new gesture starts without the old one's history.
        s.Update(true, 0, 0);
        s.Update(true, 255, 191);
        Expect(s.Delta4X == 255 && s.Delta4Y == 191, "full-width edge delta");
        s.Update(true, 0, 0);
        Expect(s.Delta4X == 0 && s.Delta4Y == 0, "+255 then -255");
        Expect(Sum4AsS16({127, 127, 127, 127}) == 508, "+127 x4");
        Expect(Sum4AsS16({-128, -128, -128, -128}) == -512, "-128 x4");
        Expect(Sum4AsS16({32767, 1, 0, 0}) == -32768, "s16 wrap as strh stores it");
        Expect(Sum4AsS16({255, 255, 255, 255}) == 1020, "+255 x4 stays in s16");
        // Clamp to the DS surface.
        s.Clear();
        s.Update(true, 400, -5);
        Expect(s.X == 255 && s.Y == 0, "DS coordinate clamp");
    }

    void Adapter()
    {
        Expect(DsTouchSurface::ToX(0.5F, 0.5F, 0.25F) == 0 && DsTouchSurface::ToX(0.75F, 0.5F, 0.25F) == 255, "zone X endpoints");
        Expect(DsTouchSurface::ToY(0.0F, 0.0F, 1.0F) == 0 && DsTouchSurface::ToY(1.0F, 0.0F, 1.0F) == 191, "zone Y endpoints");
        Expect(DsTouchSurface::ToX(0.5F, 0.0F, 1.0F) == 128, "X midpoint rounds half away");
        Expect(DsTouchSurface::ToX(2.0F, 0.0F, 1.0F) == 255 && DsTouchSurface::ToX(-1.0F, 0.0F, 1.0F) == 0, "outside the zone clamps");
        Expect(DsTouchSurface::ToX(0.5F, 0.0F, 0.0F) == 0, "degenerate zone");
    }

    void Mouse()
    {
        // One Tick is one native touch tick; AddMotion is one 60 Hz step.
        NativeTouchState t{};
        MouseMotionTouchSource m{};
        m.Tick(t);
        Expect(!t.Down, "a resting mouse is no contact");
        m.AddMotion(40, 0);
        m.Tick(t);
        Expect(t.Down && !t.Continued && t.Delta4X == 0, "first moving tick is the touch-down frame");
        for (int i = 0; i < 4; ++i)
        {
            m.AddMotion(20, 0);
            m.AddMotion(20, 0);
            m.Tick(t);
        }
        Expect(t.Continued && t.Delta4X == 40, "both steps' motion reach the tick: 40 px is 10 DS units, summed over four");
        m.AddMotion(2, 0);
        m.Tick(t);
        m.AddMotion(2, 0);
        m.Tick(t);
        Expect(t.DeltaHistoryX[3] == 1, "fractions carry");
        for (int i = 0; i < MouseMotionTouchSource::IdleTicksBeforeLift - 1; ++i) m.Tick(t);
        Expect(t.Down, "a polling gap does not lift the stylus");
        m.Tick(t);
        Expect(!t.Down && t.Delta4X == 0, "resting lifts it");
        // A flick: 400 px a tick is 100 DS units.
        m.AddMotion(400, 0);
        m.Tick(t);
        m.AddMotion(400, 0);
        m.Tick(t);
        Expect(Arbitrate(false, true, t) == BoostBranch::TouchBoost, "a mouse flick fires");
        m.AddMotion(std::numeric_limits<float>::infinity(), 0);
        m.Tick(t);
        Expect(t.Down && t.DeltaHistoryX[3] == 0, "non-finite motion is dropped");
        NativeTouchState big{};
        big.UpdateRelative(true, 0, 0);
        big.UpdateRelative(true, 5000, -5000);
        Expect(big.Delta4X == 255 && big.Delta4Y == -191, "relative deltas clamp to a DS stroke");
    }

    void Clock()
    {
        NativeTouchClock clock{};
        Expect(clock.Advance() && !clock.Advance() && clock.Advance() && !clock.Advance(),
            "the producer ticks on every other 60 Hz step, starting with the first");
        (void)clock.Advance();
        clock.Reset();
        Expect(clock.Advance(), "a reset starts on a tick");
    }

    // A stylus moving at a steady speed, sampled the way EU1.1 does (one
    // producer tick at 30 Hz) against Fruity's clock (60 Hz steps, a tick on
    // every other one). The rolling SUM and boost tick match; two simulation
    // steps together must add exactly one native roll impulse.
    void Cadence()
    {
        constexpr int nativeTicks = 15; // 0.5 s
        const auto positionAt = [](double seconds, double speed) {
            return static_cast<std::int32_t>(std::lround(20.0 + speed * seconds));
        };
        for (const double speed : {600.0, 700.0, 1350.0})
        {
            NativeTouchState reference{};
            int referenceBoostTick = -1;
            std::vector<std::int16_t> referenceSums{};
            for (int tick = 0; tick < nativeTicks; ++tick)
            {
                reference.Update(true, positionAt(tick / 30.0, speed), 96);
                referenceSums.push_back(reference.Delta4X);
                if (referenceBoostTick < 0 && Arbitrate(false, true, reference) == BoostBranch::TouchBoost)
                {
                    referenceBoostTick = tick;
                }
            }

            NativeTouchSample sample{};
            NativeTouchState& fruity = sample.State();
            MorphBallTouchRules::PlanarDelta integrated{};
            int fruityBoostStep = -1;
            int tick = 0;
            for (int step = 0; step < nativeTicks * 2; ++step)
            {
                if (sample.AdvanceLocal())
                {
                    fruity.Update(true, positionAt(step / 60.0, speed), 96);
                    Expect(fruity.Delta4X == referenceSums[static_cast<std::size_t>(tick)],
                        "the rolling SUM matches the 30 Hz reference tick for tick");
                    ++tick;
                }
                if (fruityBoostStep < 0 && Arbitrate(false, true, fruity) == BoostBranch::TouchBoost)
                {
                    fruityBoostStep = step;
                }
                const auto roll = MorphBallTouchRules::TouchRollStep(fruity.Delta4X, fruity.Delta4Y,
                    MorphBallTouchRules::TouchRollPerDsPixel, 1, 0, 0, 1, sample.RollShare());
                integrated.X += roll.X;
                integrated.Z += roll.Z;
                const auto native = MorphBallTouchRules::TouchRoll(referenceSums[static_cast<std::size_t>(step / 2)], 0,
                    MorphBallTouchRules::TouchRollPerDsPixel, 1, 0, 0, 1);
                if ((step & 1) != 0)
                {
                    Expect(Near(integrated.X, native.X) && Near(integrated.Z, native.Z),
                        "two simulation roll contributions equal one native tick impulse");
                    integrated = {};
                }
            }
            // 675 DS units/s is the native boundary: 90 over four 30 Hz deltas.
            if (speed < 675.0)
            {
                Expect(referenceBoostTick < 0 && fruityBoostStep < 0, "a native non-trigger gesture does not boost");
            }
            else
            {
                Expect(referenceBoostTick >= 0 && fruityBoostStep >= 0, "a native trigger gesture boosts");
                const double referenceSeconds = referenceBoostTick / 30.0;
                const double fruitySeconds = fruityBoostStep / 60.0;
                Expect(std::fabs(referenceSeconds - fruitySeconds) <= 1.0 / 60.0 + 1e-9,
                    "the boost fires within one 60 Hz step of the native time");
            }
        }
    }

    void Reported()
    {
        NativeTouchState owner{};
        owner.Update(true, 10, 10);
        for (int x : {40, 70, 100, 130}) owner.Update(true, x, 10);
        NativeTouchState remote{};
        remote.Assign(owner.Report());
        Expect(remote.Report() == owner.Report(), "the authority reads what the owner's producer read");
        Expect(Arbitrate(false, true, remote) == Arbitrate(false, true, owner), "and takes the same branch");
        remote.Assign({false, true, 50, 50});
        Expect(!remote.Down && !remote.Continued && remote.Delta4X == 0, "no contact carries no delta");
        remote.Assign({true, false, 50, 50});
        Expect(remote.Down && !remote.Continued && remote.Delta4X == 0, "a first contact carries no delta");
    }

    void Roll()
    {
        const auto one = MorphBallTouchRules::TouchRoll(0, 1, MorphBallTouchRules::TouchRollPerDsPixel, 1, 0, 0, 1);
        Expect(Near(std::hypot(one.X, one.Z), 5.0F / 4096.0F), "touch roll is 5/4096 per DS pixel, not 5");
        const auto forty = MorphBallTouchRules::TouchRoll(0, 40, MorphBallTouchRules::TouchRollPerDsPixel, 1, 0, 0, 1);
        Expect(Near(forty.X, -0.048828125F) && Near(forty.Z, 0.0F), "Delta4Y=40 along -forward");
        const auto side = MorphBallTouchRules::TouchRoll(-8, 0, MorphBallTouchRules::TouchRollPerDsPixel, 1, 0, 0, 1);
        Expect(Near(side.Z, 40.0F / 4096.0F) && Near(side.X, 0.0F), "Delta4X along -side");
        const auto locked = MorphBallTouchRules::TouchRoll(0, 40, MorphBallTouchRules::TouchRollPerDsPixel * 0.0F, 1, 0, 0, 1);
        Expect(locked.X == 0.0F && locked.Z == 0.0F, "JumpPadSlideFactor 0 removes it");
    }

    void IntegratedRoll()
    {
        // Includes X/Y-only, diagonal, starting/stopping, lift, edge clamp,
        // a reverse stroke and the history decaying after motion stops.
        const std::vector<std::tuple<bool, int, int>> trace{
            {false, 0, 0}, {true, 20, 20}, {true, 50, 20}, {true, 50, 55},
            {true, 95, 95}, {true, 400, 500}, {true, 500, 500}, {true, -20, -30},
            {true, 0, 0}, {true, 0, 0}, {true, 0, 0}, {true, 0, 0},
            {false, 0, 0}, {true, 10, 10}, {true, 20, 20}};
        for (float slide : {0.0F, 0.35F, 1.0F})
        {
            NativeTouchState reference{};
            NativeTouchSample sample{};
            std::uint32_t sequence = 0;
            for (auto [down, x, y] : trace)
            {
                reference.Update(down, x, y);
                MorphBallTouchRules::PlanarDelta sum{};
                for (int step = 0; step < 2; ++step)
                {
                    if (sample.AdvanceLocal()) sample.State().Update(down, x, y);
                    Expect(sample.NewNativeSampleThisStep() == (step == 0), "sample boundary is exposed");
                    if (step == 0) ++sequence;
                    Expect(sample.NativeSampleSequence() == sequence, "sample identity holds across the pair");
                    const auto& touch = sample.State();
                    Expect(touch.Report() == reference.Report(), "all transient histories match the native producer");
                    const auto roll = MorphBallTouchRules::TouchRollStep(touch.Delta4X, touch.Delta4Y,
                        MorphBallTouchRules::TouchRollPerDsPixel * slide, 0.6F, 0.8F, 0.8F, -0.6F, sample.RollShare());
                    sum.X += roll.X;
                    sum.Z += roll.Z;
                }
                const float expectedX = -(reference.Delta4Y * 0.6F + reference.Delta4X * 0.8F) * (5.0F / 4096.0F) * slide;
                const float expectedZ = -(reference.Delta4Y * 0.8F - reference.Delta4X * 0.6F) * (5.0F / 4096.0F) * slide;
                Expect(Near(sum.X, expectedX) && Near(sum.Z, expectedZ), "integrated roll equals the native impulse including jump-pad scaling");
            }
            const auto before = sample.NativeSampleSequence();
            sample.Suspend();
            Expect(sample.RollShare() == 0 && !sample.State().Down, "suspending drops contact and its roll");
            Expect(sample.AdvanceLocal() && sample.NativeSampleSequence() != before, "resume never reuses a sample identity");
        }
    }

    NativeTouchState Continued(std::int16_t dx, std::int16_t dy)
    {
        NativeTouchState s{};
        s.Down = s.PreviousDown = s.Continued = true;
        s.Delta4X = dx;
        s.Delta4Y = dy;
        return s;
    }

    void Threshold()
    {
        Expect(Arbitrate(false, true, Continued(90, 0)) == BoostBranch::SkipShoulder, "8100 is not > 8100");
        Expect(Arbitrate(false, true, Continued(91, 0)) == BoostBranch::TouchBoost, "8281 fires");
        Expect(Arbitrate(false, true, Continued(54, 72)) == BoostBranch::SkipShoulder, "54,72 is 8100");
        Expect(Arbitrate(false, true, Continued(55, 72)) == BoostBranch::TouchBoost, "55,72 fires");
        Expect(Arbitrate(true, true, Continued(91, 0)) == BoostBranch::Shoulder, "boosting -> R path");
        Expect(Arbitrate(false, false, Continued(91, 0)) == BoostBranch::Shoulder, "disarmed -> R path");
        NativeTouchState first = Continued(91, 0);
        first.Continued = false;
        Expect(Arbitrate(false, true, first) == BoostBranch::Shoulder, "first frame -> R path");
        Expect(Arbitrate(false, true, Continued(0, 0)) == BoostBranch::SkipShoulder,
            "armed continued small touch skips R (02023668 -> 02023A24)");
    }

    void Impulse()
    {
        const float max = 0.5F;
        for (auto [dx, dy] : {std::pair{91, 0}, std::pair{0, -200}, std::pair{55, 72}, std::pair{-300, 300}})
        {
            const auto i = MorphBallTouchRules::TouchBoostImpulse(dx, dy, max, 1, 0, 0, 1);
            Expect(Near(std::hypot(i.X, i.Z), max), "full BoostSpeedMax for any qualifying swipe");
        }
        const auto up = MorphBallTouchRules::TouchBoostImpulse(0, -100, max, 0.6F, 0.8F, 0.8F, -0.6F);
        Expect(Near(up.X, 0.3F) && Near(up.Z, 0.4F), "drag up is camera forward");
        const auto right = MorphBallTouchRules::TouchBoostImpulse(100, 0, max, 0.6F, 0.8F, 0.8F, -0.6F);
        Expect(Near(right.X, -0.4F) && Near(right.Z, 0.3F), "drag right is -camera side");
    }

    namespace Boost = MphRead::Entities::MorphBallBoostStateMachine;

    constexpr Boost::ChargeLimits Limits{4, 20, false};

    Boost::Inputs In(const NativeTouchState& touch, bool shoulder)
    {
        return {touch.Down, touch.Continued, touch.Delta4X, touch.Delta4Y, shoulder};
    }

    void SamplePairBoost()
    {
        Boost::SampleLatch latch{};
        Boost::State state{false, true, 7};
        auto advance = [&](const NativeTouchState& touch, bool held, std::uint64_t sample) {
            return Boost::AdvanceSample(state, In(touch, held), Limits, latch, sample);
        };
        auto r = advance(Continued(120, 0), true, 1);
        Expect(r.Boost == Boost::Fired::TouchBoost && state.Charge == 7, "charged SPACE + swipe fires once and preserves charge");
        r = advance(Continued(120, 0), true, 1);
        Expect(r.Branch == BoostBranch::TouchBoost && r.Boost == Boost::Fired::None && state.Charge == 7,
            "the sibling cannot charge SPACE half a native tick early");
        r = advance(Continued(120, 0), false, 1);
        Expect(r.Branch == BoostBranch::TouchBoost && r.Boost == Boost::Fired::None && state.Charge == 7,
            "a release or repeated report in the same sample cannot fire Shoulder");
        r = advance(Continued(120, 0), true, 2);
        Expect(r.Branch == BoostBranch::Shoulder && state.Charge == 8, "the next native sample resumes Shoulder");
        r = advance(Continued(120, 0), false, 2);
        Expect(r.Boost == Boost::Fired::ShoulderBoost && r.ChargeSpent == 8 && state.Charge == 0,
            "hold -> touch boost -> hold -> Shoulder release remains available");

        state = {false, true, 7};
        latch = {};
        (void)advance(Continued(1, 1), true, 3);
        // An external flag change must not change the already selected path.
        state.Boosting = true;
        r = advance(Continued(1, 1), false, 3);
        Expect(r.Branch == BoostBranch::SkipShoulder && r.Boost == Boost::Fired::None && state.Charge == 7,
            "small continued touch blocks both charge and release for the pair");
        state.CanTouchBoost = false;
        r = advance(NativeTouchState{}, true, 4);
        Expect(state.CanTouchBoost && state.Charge == 8, "the next no-contact sample re-arms and charges");
        (void)advance(NativeTouchState{}, true, 4);
        Expect(state.Charge == 9, "Shoulder-only charge still advances each 60 Hz step");
        state.Boosting = false;
        r = advance(Continued(120, 0), false, 5);
        Expect(r.Boost == Boost::Fired::TouchBoost && state.Charge == 9, "a re-armed next gesture can boost again");
        r = Boost::AdvanceSample(state, In(Continued(120, 0), false), {4, 20, true}, latch, 5);
        Expect(r.Boost == Boost::Fired::None && state.Charge == 9, "FullBoostCharge cannot bypass the touch latch");
        r = Boost::AdvanceSample(state, In(NativeTouchState{}, false), {4, 20, true}, latch, 6);
        Expect(r.Boost == Boost::Fired::ShoulderBoost && r.ChargeSpent == 20 && state.Charge == 0,
            "FullBoostCharge applies when the next sample reaches Shoulder");
    }

    // The production state machine PlayerEntity::ProcessBoost runs.
    void StateMachine()
    {
        const NativeTouchState none{};

        // Small continued touch: the charge neither grows nor is released.
        Boost::State small{false, true, 7};
        Expect(Boost::Advance(small, In(Continued(0, 0), true), Limits).Boost == Boost::Fired::None
            && small.Charge == 7, "small continued touch freezes the charge while R is held");
        Expect(Boost::Advance(small, In(Continued(0, 0), false), Limits).Boost == Boost::Fired::None
            && small.Charge == 7, "small continued touch skips the R release");

        // R held -> touch boost -> R held -> R release.
        Boost::State chain{false, true, 7};
        Boost::Result r = Boost::Advance(chain, In(Continued(120, 0), true), Limits);
        Expect(r.Boost == Boost::Fired::TouchBoost && chain.Charge == 7 && chain.Boosting && !chain.CanTouchBoost,
            "touch boost fires and leaves the charge alone");
        r = Boost::Advance(chain, In(Continued(120, 0), true), Limits);
        Expect(r.Boost == Boost::Fired::None && chain.Charge == 8, "a repeated contact does not boost again; R charges");
        r = Boost::Advance(chain, In(none, true), Limits);
        Expect(chain.CanTouchBoost && chain.Charge == 9, "release re-arms");
        r = Boost::Advance(chain, In(none, false), Limits);
        Expect(r.Boost == Boost::Fired::ShoulderBoost && r.ChargeSpent == 9 && chain.Charge == 0,
            "R release boosts after a touch boost and spends the charge");

        // A release below the minimum spends the charge without boosting.
        Boost::State weak{false, true, 4};
        r = Boost::Advance(weak, In(none, false), Limits);
        Expect(r.Boost == Boost::Fired::None && weak.Charge == 0 && !weak.Boosting, "min is strict");
        Boost::State full{false, true, 5};
        r = Boost::Advance(full, In(none, false), {4, 20, true});
        Expect(r.Boost == Boost::Fired::ShoulderBoost && r.ChargeSpent == 20, "FullBoostCharge spends the max");

        // Charge stops at the max.
        Boost::State cap{false, true, 20};
        (void)Boost::Advance(cap, In(none, true), Limits);
        Expect(cap.Charge == 20, "charge stops at the max");

        // Disarmed continued touch falls through to R.
        Boost::State disarmed{false, false, 0};
        (void)Boost::Advance(disarmed, In(Continued(0, 0), true), Limits);
        Expect(disarmed.Charge == 1, "continued touch while disarmed -> R path");
        // So does a contact while boosting.
        Boost::State boosting{true, true, 0};
        r = Boost::Advance(boosting, In(Continued(200, 0), true), Limits);
        Expect(r.Boost == Boost::Fired::None && boosting.Charge == 1, "boosting -> R path");

        // Strength: the touch boost is full, the shoulder boost proportional.
        const Boost::BoostValues values{0.2F, 0.6F, 1.5F, 30};
        const Boost::Strength touch = Boost::TouchBoostStrength(values);
        Expect(Near(touch.Speed, 0.6F) && Near(touch.Cap, 1.5F) && touch.Damage == 30, "touch boost is full strength");
        const Boost::Strength half = Boost::ShoulderBoostStrength(values, 10, 20);
        Expect(Near(half.Speed, 0.4F) && Near(half.Cap, 0.75F) && half.Damage == 15, "shoulder boost scales with charge");
        const Boost::Strength max = Boost::ShoulderBoostStrength(values, 20, 20);
        Expect(Near(max.Speed, 0.6F) && Near(max.Cap, 1.5F) && max.Damage == 30, "a full charge is full strength");
    }
}

int main()
{
    try
    {
        Producer();
        Adapter();
        Mouse();
        Roll();
        IntegratedRoll();
        Threshold();
        Impulse();
        Clock();
        Cadence();
        Reported();
        StateMachine();
        SamplePairBoost();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "NativeTouchState: %s\n", e.what());
        return 1;
    }
    std::puts("NativeTouchState: ok");
    return 0;
}
