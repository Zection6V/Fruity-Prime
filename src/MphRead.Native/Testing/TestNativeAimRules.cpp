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
    for (unsigned counter = 0; counter < 256; ++counter)
    {
        InputSlot slot; slot.Bytes[0xE] = static_cast<std::uint8_t>(counter);
        slot.Produce(1, true, 0);
        Check(slot.Read(0xA) == ((counter >= 4 && counter <= 7) ? 1 : 0), "filtered press4..7 exhaustive");
        Check(slot.Read(0xC) == ((counter >= 3 && counter <= 7) ? 1 : 0), "filtered press3..7 exhaustive");
        Check(slot.Bytes[0xE] == 0, "pressed key resets");
        for (unsigned other = 1; other < 12; ++other) Check(slot.Bytes[0xE + other] == 255, "other11 keys invalidate");
        const auto original = slot;
        auto shadow = slot; shadow.Clear();
        Check(slot.Bytes == original.Bytes, "shadow clear does not clear global");
    }
    InputSlot slot;
    slot.Produce(1, false, 0); Check(slot.Read(0) == 0, "InputSlot OR gate closed");
    slot.Produce(1, false, 1); Check(slot.Read(4) == 1, "84E opens InputSlot producer");
    slot.Produce(0, true, 0); Check(slot.Read(8) == 1, "release edge");
    slot.Produce(0, true, 0); slot.Produce(0, true, 0); slot.Produce(0, true, 0);
    slot.Produce(1, true, 0); Check(slot.Read(0xA) == 1 && slot.Read(0xC) == 1, "repress timeline");
    slot.Clear(); slot.Produce(3, true, 0);
    Check(slot.Bytes[0xE] == 255 && slot.Bytes[0xF] == 0, "simultaneous press scan order");
    for (unsigned selectors = 0; selectors < 8; ++selectors)
        for (unsigned offset : {0U, 4U, 8U, 0xAU})
            for (unsigned touch = 0; touch < 64; ++touch)
                for (unsigned fallback = 0; fallback < 2; ++fallback)
                {
                    slot.Clear(); slot.Write(offset, 1); slot.Bytes[0x34] = static_cast<std::uint8_t>(touch);
                    const auto spec = 1U | ((selectors & 1) ? 0x40000U : 0U) | ((selectors & 2) ? 0x100000U : 0U)
                        | ((selectors & 4) ? 0x80000U : 0U) | (fallback ? 0x10000U : 0U);
                    const unsigned selected = (selectors & 1) ? 4 : (selectors & 2) ? 8 : (selectors & 4) ? 0xA : 0;
                    const unsigned bit = (selectors & 1) ? 2 : (selectors & 2) ? 4 : (selectors & 4) ? 5 : 0;
                    Check(TestAction(slot, spec) == (selected == offset || (fallback && (touch & (1U << bit)))), "ActionSpec priority/fallback");
                }
    Control control;
    DualState dual;
    slot.Clear(); slot.Write(0, 15);
    Check(dual.X == 0 && dual.Y == 0, "old axes before first producer");
    dual.Produce(slot, control, Form::Biped);
    Check(near(dual.X, -3.2F) && near(dual.Y, 3.2F), "opposite key priority differs by axis");
    control.Flag84E = 1;
    const auto old = dual;
    Check(!dual.Produce(slot, control, Form::Biped) && dual.X == old.X && dual.Y == old.Y, "84E only skips next producer");
    dual.Produce(slot, control, Form::FreeCamera);
    Check(dual.X != old.X && dual.Y != old.Y, "FreeCamera separate same-step producer");
    control.Flag84E = 0; slot.Clear();
    Check(dual.Produce(slot, control, Form::Biped), "conditional AutoPitch");
    control.Flags |= 0x40;
    Check(!dual.Produce(slot, control, Form::Biped), "0x40 inhibits extra Pitch");
    control.Flags &= ~0x40; slot.Write(0, 16);
    Check(!dual.Produce(slot, control, Form::Biped), "ActionSpec3A4 inhibits extra Pitch");
    slot.Clear(); control.AutoPitchTimer = 1;
    Check(!dual.Produce(slot, control, Form::Biped), "unsigned timer gate");
    NativeTouchSample touch;
    unsigned ticks = 0;
    for (unsigned step = 0; step < 60; ++step) ticks += touch.AdvanceLocal();
    Check(ticks == 30, "native clock isolated from rendering");
    std::cout << "NativeAimRules PASS " << checks << " synthetic checks; ROM runtime NOT_RUN\n";
}
