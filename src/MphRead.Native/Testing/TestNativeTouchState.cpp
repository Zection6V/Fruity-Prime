// EU1.1 02029778 (touch producer) and 02021C28's touch branches, pinned.
#include "../Entities/Players/MorphBallBoostStateMachine.hpp"
#include "../Entities/Players/MorphBallTouchRules.hpp"
#include "../Mods/Input/DsTouchSurface.hpp"
#include "../Mods/Input/MouseMotionTouchSource.hpp"
#include "../Mods/Input/NativeTouchClock.hpp"
#include "../Mods/Input/NativeTouchState.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
#include <stdexcept>
#include <utility>

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
        clock.Advance();
        clock.Reset();
        Expect(clock.Advance(), "a reset starts on a tick");
    }

    // A stylus moving at a steady speed, sampled the way EU1.1 does (one
    // producer tick at 30 Hz) against Fruity's clock (60 Hz steps, a tick on
    // every other one). The rolling SUM, the boost tick and the roll a frame
    // gets must be the native ones.
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

            NativeTouchState fruity{};
            NativeTouchClock clock{};
            int fruityBoostStep = -1;
            int tick = 0;
            for (int step = 0; step < nativeTicks * 2; ++step)
            {
                if (clock.Advance())
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
                // The roll a 60 Hz frame adds is the native tick's roll: both
                // substeps of a tick read the same SUM.
                const auto roll = MorphBallTouchRules::TouchRoll(fruity.Delta4X, fruity.Delta4Y,
                    MorphBallTouchRules::TouchRollPerDsPixel, 1, 0, 0, 1);
                const auto native = MorphBallTouchRules::TouchRoll(referenceSums[static_cast<std::size_t>(step / 2)], 0,
                    MorphBallTouchRules::TouchRollPerDsPixel, 1, 0, 0, 1);
                Expect(Near(roll.X, native.X) && Near(roll.Z, native.Z), "a frame's touch roll is its native tick's");
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
        Threshold();
        Impulse();
        Clock();
        Cadence();
        Reported();
        StateMachine();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "NativeTouchState: %s\n", e.what());
        return 1;
    }
    std::puts("NativeTouchState: ok");
    return 0;
}
