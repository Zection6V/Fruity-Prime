// EU1.1 02029778 (touch producer) and 02021C28's touch branches, pinned.
#include "../Entities/Players/MorphBallTouchRules.hpp"
#include "../Mods/Input/DsTouchSurface.hpp"
#include "../Mods/Input/MouseMotionTouchSource.hpp"
#include "../Mods/Input/NativeTouchState.hpp"

#include <cmath>
#include <cstdio>
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
        NativeTouchState t{};
        MouseMotionTouchSource m{};
        m.Step(0, 0, t);
        Expect(!t.Down, "a resting mouse is no contact");
        m.Step(40, 0, t);
        Expect(t.Down && !t.Continued && t.Delta4X == 0, "first moving step is the touch-down frame");
        for (int i = 0; i < 4; ++i) m.Step(40, 0, t);
        Expect(t.Continued && t.Delta4X == 40, "40 px a step at 0.25 is 10 DS units, summed over four");
        m.Step(2, 0, t);
        m.Step(2, 0, t);
        Expect(t.DeltaHistoryX[3] == 1, "fractions carry");
        for (int i = 0; i < MouseMotionTouchSource::IdleStepsBeforeLift - 1; ++i) m.Step(0, 0, t);
        Expect(t.Down, "a polling gap does not lift the stylus");
        m.Step(0, 0, t);
        Expect(!t.Down && t.Delta4X == 0, "resting lifts it");
        // A flick: 400 px a step is 100 DS units; the boost needs the SUM of
        // four over 90 on a continued contact.
        m.Step(400, 0, t);
        m.Step(400, 0, t);
        Expect(Arbitrate(false, true, t) == BoostBranch::TouchBoost, "a mouse flick fires");
        NativeTouchState big{};
        big.UpdateRelative(true, 0, 0);
        big.UpdateRelative(true, 5000, -5000);
        Expect(big.Delta4X == 255 && big.Delta4Y == -191, "relative deltas clamp to a DS stroke");
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

    // The 02023624 state machine around R, the way ProcessAlt drives it.
    struct Machine
    {
        bool Boosting = false;
        bool CanTouchBoost = true;
        int Charge = 0;
        int ChargeMax = 20;
        int Fired = 0;
        int ShoulderFired = 0;

        void Step(const NativeTouchState& touch, bool rHeld)
        {
            if (!touch.Down) CanTouchBoost = true;
            switch (Arbitrate(Boosting, CanTouchBoost, touch))
            {
            case BoostBranch::TouchBoost:
                ++Fired;
                Boosting = true;
                CanTouchBoost = false;
                break;
            case BoostBranch::SkipShoulder:
                break;
            case BoostBranch::Shoulder:
                if (rHeld)
                {
                    if (Charge < ChargeMax) ++Charge;
                }
                else
                {
                    if (Charge > 2)
                    {
                        ++ShoulderFired;
                        Boosting = true;
                    }
                    Charge = 0;
                }
                break;
            }
        }
    };

    void Chain()
    {
        NativeTouchState none{};
        // Small continued touch: R charge does not move and is not released.
        Machine m{};
        m.Charge = 7;
        m.Step(Continued(0, 0), true);
        Expect(m.Charge == 7, "small continued touch skips the R increment");
        m.Step(Continued(0, 0), false);
        Expect(m.Charge == 7 && m.ShoulderFired == 0, "small continued touch skips the R release");

        // R held -> touch boost -> R held -> R release.
        Machine c{};
        c.Charge = 7;
        c.Step(Continued(120, 0), true);
        Expect(c.Fired == 1 && c.Charge == 7 && c.Boosting && !c.CanTouchBoost, "touch boost leaves R charge alone");
        c.Step(Continued(120, 0), true);
        Expect(c.Charge == 8, "next frame R path runs while held");
        c.Step(none, true);
        Expect(c.CanTouchBoost && c.Charge == 9, "release re-arms");
        c.Step(none, false);
        Expect(c.ShoulderFired == 1 && c.Charge == 0, "R release boosts after a touch boost");

        // Disarmed continued touch falls through to R.
        Machine d{};
        d.CanTouchBoost = false;
        d.Step(Continued(0, 0), true);
        Expect(d.Charge == 1, "continued touch while disarmed -> R path");
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
        Chain();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "NativeTouchState: %s\n", e.what());
        return 1;
    }
    std::puts("NativeTouchState: ok");
    return 0;
}
