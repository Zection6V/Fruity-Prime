#include "../Mods/Input/NativeDualAim.hpp"
#include "../Mods/Input/NativeTouchSample.hpp"
#include <cstdlib>
#include <iostream>
#include <cmath>
#include "../Mods/Input/AimNumericPolicy.hpp"
#include <string_view>

using namespace MphRead::Mods::Input;
using namespace NativeAim;
namespace
{
    int checks = 0;
    void Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
    }
}
int main()
{
    const auto near = [](float actual, float expected) { return std::fabs(actual - expected) < 0.00001F; };
    Check(near(Floor(8, Form::Biped, false), 3.2F), "Biped ideal 40 percent floor");
    Check(near(Floor(8, Form::AltStrafe, false), 3.2F) && near(Floor(8, Form::AltStrafe, true), -3.2F), "Alt ideal signed floor");
    Check(near(ProduceAxis(0, 8, 1, Form::Biped), 3.2F), "first press");
    Check(near(ProduceAxis(3.2F, 8, 1, Form::Biped), 3.28F), "held 1 percent acceleration");
    Check(near(ProduceAxis(3.28F, 8, -1, Form::Biped), -3.2F), "direction reversal");
    Check(ProduceAxis(8, 8, 1, Form::Biped) == 8, "maximum clamp");
    auto half = ProduceAxis(3.2F, 8, 1, Form::Biped, true);
    half = ProduceAxis(half, 8, 1, Form::Biped, true);
    Check(near(half, ProduceAxis(3.2F, 8, 1, Form::Biped)), "DirectPC two acceleration substeps preserve native duration");
    half = ProduceAxis(3.2F, 8, 0, Form::Biped, true);
    half = ProduceAxis(half, 8, 0, Form::Biped, true);
    Check(near(half, ProduceAxis(3.2F, 8, 0, Form::Biped)), "DirectPC two decay substeps preserve native duration");
    Check(near(NativeZoomSensitivity(39, 39, 0.1F), 1), "affine zoom unity");
    Check(near(NativeZoomSensitivity(20, 39, 0.025F), 0.525F), "affine zoom retains fractional precision");
    Check(near(AimEnvelopeCos * AimEnvelopeCos + AimEnvelopeSin * AimEnvelopeSin, 1), "ideal 15 degree envelope is unit length");
    Control control;
    DualState dual;
    AimButtons all; all.Left = all.Right = all.Up = all.Down = true;
    Check(dual.X == 0 && dual.Y == 0, "old axes before first producer");
    dual.Produce(all, control, Form::Biped);
    Check(near(dual.X, -3.2F) && near(dual.Y, 3.2F), "opposite key priority differs by axis");
    control.Flag84E = 1;
    const auto old = dual;
    Check(!dual.Produce(all, control, Form::Biped) && dual.X == old.X && dual.Y == old.Y, "84E only skips next producer");
    dual.Produce(all, control, Form::FreeCamera);
    Check(dual.X != old.X && dual.Y != old.Y, "FreeCamera separate same-step producer");
    control.Flag84E = 0;
    const AimButtons none;
    Check(dual.Produce(none, control, Form::Biped), "conditional AutoPitch");
    control.Flags |= 0x40;
    Check(!dual.Produce(none, control, Form::Biped), "0x40 inhibits extra Pitch");
    control.Flags &= ~0x40;
    AimButtons aim; aim.Aim = true;
    Check(!dual.Produce(aim, control, Form::Biped), "Aim action inhibits extra Pitch");
    control.AutoPitchTimer = 1;
    Check(!dual.Produce(none, control, Form::Biped), "unsigned timer gate");
    control.AutoPitchTimer = 0;
    // Every gate of the conditional (non-0x08) configuration, exhaustively.
    for (unsigned flags = 0; flags < 8; ++flags)
        for (unsigned held = 0; held < 8; ++held)
        {
            Control gated; gated.Flags = static_cast<std::uint16_t>(0x20 | ((flags & 1) ? 2 : 0) | ((flags & 2) ? 4 : 0) | ((flags & 4) ? 8 : 0));
            AimButtons b; b.Left = b.Up = true;
            b.Enable = (held & 1) != 0; b.Aim = (held & 2) != 0; b.TouchDown = (held & 4) != 0;
            const bool unconditional = (flags & 4) != 0;
            const bool horizontal = unconditional || (!((flags & 2) && !b.Enable) && !((flags & 1) && b.TouchDown) && !b.Aim);
            const bool vertical = unconditional || b.Enable;
            DualState d; d.Produce(b, gated, Form::Biped);
            Check((d.X != 0) == horizontal, "horizontal gate: Enable/TouchDown/Aim");
            Check((d.Y != 0) == vertical, "vertical gate: unconditional or Enable");
        }
    AimButtons gatedTouch; gatedTouch.Left = gatedTouch.TouchDown = true;
    Control touchFlag; touchFlag.Flags = 0x22;
    DualState d; d.Produce(gatedTouch, touchFlag, Form::FreeCamera);
    Check(d.X != 0, "FreeCamera ignores every gate");
    NativeTouchSample touch;
    unsigned ticks = 0;
    for (unsigned step = 0; step < 60; ++step) ticks += touch.AdvanceLocal();
    Check(ticks == 30, "native clock isolated from rendering");
    std::cout << "NativeAimRules PASS " << checks << " synthetic checks; ROM runtime NOT_RUN\n";
}
