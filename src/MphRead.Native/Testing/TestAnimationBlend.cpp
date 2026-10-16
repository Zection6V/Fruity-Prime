#include "../Mods/Render/AnimationBlend.hpp"
#include "../Mods/Render/AnimationClock.hpp"
#include "../Mods/Render/PreviewAnimation.hpp"
#include "../Mods/Render/SceneAnimation.hpp"
#include "../Entities/Players/PlayerEntity.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Read.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <unordered_map>

using namespace MphRead;
using namespace MphRead::Mods::Render;
using OpenTK::Mathematics::Matrix4;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    bool Near(float a, float b)
    {
        return std::abs(a - b) < 0.0001F;
    }

    template<class T> auto Empty()
    {
        return std::make_shared<const std::vector<T>>();
    }

    // NodeAnimation is plain bytes with const members; fill it as a reader does.
    template<class T, class V> void Put(T& raw, std::size_t offset, V value)
    {
        std::memcpy(reinterpret_cast<std::byte*>(&raw) + offset, &value, sizeof(value));
    }

    std::shared_ptr<Model> Fixture(std::shared_ptr<const std::vector<RawNode>> nodes = Empty<RawNode>())
    {
        return std::make_shared<Model>("blend-test", false, Header{},
            std::move(nodes), Empty<RawMesh>(), Empty<RawMaterial>(), Empty<DisplayList>(),
            Empty<std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>>(),
            std::make_shared<AnimationResults>(), Empty<Matrix4>(),
            Empty<std::shared_ptr<Recolor>>(), Empty<std::int32_t>(), Empty<Vector3Fx>(),
            Empty<Vector3Fx>(), Empty<std::int32_t>(), Empty<Fixed>());
    }

    // One root node "n" whose X translation runs 0, 10, 20, 30 and whose X
    // rotation runs 3.1, -3.1, 0, 0 over four frames.
    struct Rig
    {
        std::shared_ptr<Model> Model;
        std::shared_ptr<NodeAnimationGroup> Group;
    };

    Rig MakeRig()
    {
        // RawNode marshals its name, so it is read from file bytes as the game does
        // (offset 0 reads as "none"): the name, then parent, child and next as int16.
        constexpr std::size_t at = 4;
        std::array<std::uint8_t, at + 0xF0> bytes{};
        bytes[at] = 'n';
        for (const std::size_t field : {64U, 66U, 68U}) bytes[at + field] = bytes[at + field + 1] = 0xFF;
        auto model = Fixture(Read::DoOffsets<RawNode>(bytes, static_cast<std::uint32_t>(at), 1U));

        NodeAnimation anim{};
        const auto lut = [&](std::size_t blend, std::size_t length, std::size_t index, std::uint16_t count, std::uint16_t at)
        {
            Put(anim, blend, std::uint8_t{1});
            Put(anim, length, count);
            Put(anim, index, at);
        };
        for (const auto [blend, length, index] : std::array<std::array<std::size_t, 3>, 3>{{
                 {offsetof(NodeAnimation, ScaleBlendX), offsetof(NodeAnimation, ScaleLutLengthX), offsetof(NodeAnimation, ScaleLutIndexX)},
                 {offsetof(NodeAnimation, ScaleBlendY), offsetof(NodeAnimation, ScaleLutLengthY), offsetof(NodeAnimation, ScaleLutIndexY)},
                 {offsetof(NodeAnimation, ScaleBlendZ), offsetof(NodeAnimation, ScaleLutLengthZ), offsetof(NodeAnimation, ScaleLutIndexZ)}}})
            lut(blend, length, index, 1, 0);
        lut(offsetof(NodeAnimation, RotateBlendX), offsetof(NodeAnimation, RotateLutLengthX), offsetof(NodeAnimation, RotateLutIndexX), 4, 0);
        lut(offsetof(NodeAnimation, RotateBlendY), offsetof(NodeAnimation, RotateLutLengthY), offsetof(NodeAnimation, RotateLutIndexY), 1, 4);
        lut(offsetof(NodeAnimation, RotateBlendZ), offsetof(NodeAnimation, RotateLutLengthZ), offsetof(NodeAnimation, RotateLutIndexZ), 1, 4);
        lut(offsetof(NodeAnimation, TranslateBlendX), offsetof(NodeAnimation, TranslateLutLengthX), offsetof(NodeAnimation, TranslateLutIndexX), 4, 0);
        lut(offsetof(NodeAnimation, TranslateBlendY), offsetof(NodeAnimation, TranslateLutLengthY), offsetof(NodeAnimation, TranslateLutIndexY), 1, 4);
        lut(offsetof(NodeAnimation, TranslateBlendZ), offsetof(NodeAnimation, TranslateLutLengthZ), offsetof(NodeAnimation, TranslateLutIndexZ), 1, 4);

        auto scales = std::make_shared<const std::vector<float>>(std::initializer_list<float>{1});
        auto rotations = std::make_shared<const std::vector<float>>(
            std::initializer_list<float>{3.1F, -3.1F, 0, 0, 0});
        auto translations = std::make_shared<const std::vector<float>>(
            std::initializer_list<float>{0, 10, 20, 30, 0});
        auto dictionary = std::make_shared<NodeAnimationDictionary>();
        dictionary->emplace("n", anim);
        auto group = std::make_shared<NodeAnimationGroup>(RawNodeAnimationGroup{},
            scales, rotations, translations, dictionary);
        return {model, group};
    }

    void Configure(ModelInstance& inst, AnimFlags flags, std::shared_ptr<NodeAnimationGroup> group = nullptr,
        std::int32_t frameCount = 8)
    {
        auto& info = *inst.AnimInfo;
        info.Node->Slot = 1;
        info.Node->Group = std::move(group);
        for (std::size_t i = 0; i < 2; ++i)
        {
            (*info.FrameCount)[i] = frameCount;
            (*info.Frame)[i] = (flags & AnimFlags::Reverse) != AnimFlags::None ? frameCount - 1 : 0;
            (*info.Flags)[i] = flags;
            (*info.Step)[i] = static_cast<int>(i + 1);
        }
    }

    void CompareState(const ModelInstance& a, const ModelInstance& b)
    {
        for (std::size_t i = 0; i < 2; ++i)
        {
            Require((*a.AnimInfo->Frame)[i] == (*b.AnimInfo->Frame)[i], "integer frame drift");
            Require((*a.AnimInfo->Flags)[i] == (*b.AnimInfo->Flags)[i], "state transition drift");
        }
    }

    float TranslationX(const Model& model)
    {
        return model.Nodes->at(0)->Animation.M41;
    }

    void Pose(Model& model, const std::shared_ptr<AnimationInfo>& info)
    {
        model.AnimateNodes(0, false, OpenTK::Mathematics::IdentityMatrix(),
            OpenTK::Mathematics::Vector3(1, 1, 1), info);
    }

    void FormFrameOverrideChecks()
    {
        const auto rig = MakeRig();
        ModelInstance instance(rig.Model);
        Configure(instance, AnimFlags::NoLoop | AnimFlags::Paused, rig.Group, 4);
        instance.AnimInfo->Node->Slot = 0;
        (*instance.AnimInfo->Frame)[0] = 1;
        const auto flags = (*instance.AnimInfo->Flags)[0];
        AnimationBlend::Rate(AnimationBlend::DisplayRate);
        {
            const AnimationBlend::DrawScope blend;
            AnimationBlend::FrameOverride frame;
            frame.Set(*instance.AnimInfo, 2.375F);
            Pose(*rig.Model, instance.AnimInfo);
            Require(Near(TranslationX(*rig.Model), 23.75F), "event frame blends the actual native curve continuously");
            Require(Near(AnimationBlend::FrameShown(*instance.AnimInfo), 2.375F), "draw uses the exact fractional frame");
            Require((*instance.AnimInfo->Frame)[0] == 1, "draw never changes the simulation frame");
            Require(Near(AnimationBlend::FrameShown(*instance.AnimInfo, 1),
                static_cast<float>((*instance.AnimInfo->Frame)[1])), "override does not leak into the other animation slot");
        }
        Require((*instance.AnimInfo->Frame)[0] == 1 && (*instance.AnimInfo->Flags)[0] == flags
            && instance.AnimInfo->ModTick.DrawFrame < 0, "draw restores integer frame and keeps gameplay flags");
        Require(Near(TranslationX(*rig.Model), 10), "draw restores the simulation pose rather than the overridden integer pose");
        try
        {
            AnimationBlend::FrameOverride frame;
            frame.Set(*instance.AnimInfo, 2.5F);
            throw std::runtime_error("draw interrupted");
        }
        catch (const std::runtime_error&) {}
        Require((*instance.AnimInfo->Frame)[0] == 1 && instance.AnimInfo->ModTick.DrawFrame < 0,
            "an exception restores the simulation frame");
        AnimationBlend::Rate(AnimationBlend::DsRate);
        {
            AnimationBlend::FrameOverride frame;
            frame.Set(*instance.AnimInfo, 1.9F);
            Require(Near(instance.AnimInfo->ModTick.DrawFrame, 1), "30 Hz setting retains integer poses");
        }
        AnimationBlend::Rate(AnimationBlend::DisplayRate);
        std::cout << "PASS authoritative form frame overrides\n";
    }

    void PreviewClockChecks()
    {
        const auto model = Fixture();
        const auto group = MakeRig().Group;
        for (const auto flags : {AnimFlags::None, AnimFlags::Reverse, AnimFlags::PingPong,
                 AnimFlags::PingPong | AnimFlags::Reverse, AnimFlags::NoLoop,
                 AnimFlags::NoLoop | AnimFlags::Reverse, AnimFlags::Paused, AnimFlags::Ended})
        for (const int hz : {24, 30, 60, 144, 240, 540})
        {
            ModelInstance shown(model), reference(model);
            Configure(shown, flags, group); Configure(reference, flags, group);
            PreviewAnimation clock;
            int referenceTicks = 0;
            for (int frame = 1; frame <= hz * 4; ++frame)
            {
                clock.Advance(shown, 1.0 / hz);
                const int expectedTicks = frame * 30 / hz;
                while (referenceTicks < expectedTicks)
                {
                    reference.UpdateAnimFrames();
                    ++referenceTicks;
                }
                CompareState(shown, reference);
                if (referenceTicks == 0) continue;
                ModelInstance expectedNext(model);
                Configure(expectedNext, flags, group);
                for (int i = 0; i <= referenceTicks; ++i) expectedNext.UpdateAnimFrames();
                Require(shown.AnimInfo->ModTick.Next[1] == expectedNext.AnimInfo->NodeFrame(),
                    "lookahead disagrees with original state machine");
            }
            const bool moving = shown.AnimInfo->ModTick.Next[1] != shown.AnimInfo->NodeFrame();
            Require(AnimationBlend::Fraction(*shown.AnimInfo) < 0.00001F, "clock rate drift");
            clock.Advance(shown, 1.0 / 120);
            Require(Near(AnimationBlend::Fraction(*shown.AnimInfo), moving ? 0.25F : 0.0F),
                "missing fractional phase");
            clock.Reset();
            Require(AnimationBlend::Fraction(*shown.AnimInfo) == 0, "reset retained phase");
        }
        std::cout << "PASS preview: original transitions at 24-540 Hz, both slots, lookahead and reset\n";
    }

    void SimulationClockChecks()
    {
        const auto model = Fixture();
        const auto group = MakeRig().Group;
        AnimationClock clock;
        ModelInstance gated(model), every(model), outside(model);
        Configure(gated, AnimFlags::None, group); Configure(every, AnimFlags::None, group);
        Configure(outside, AnimFlags::None, group);
        for (int step = 1; step <= 7; ++step)
        {
            clock.Step();
            const AnimationClock::TickScope scope(clock);
            if (step % 2 == 0) gated.UpdateAnimFrames();
            every.UpdateAnimFrames();
        }
        outside.UpdateAnimFrames();
        outside.UpdateAnimFrames();
        // Step 7: the 30 Hz instance last ticked at step 6, two steps apart.
        clock.Phase(0.5);
        Require(Near(AnimationBlend::Fraction(*gated.AnimInfo), 0.75F), "30 Hz cadence fraction");
        Require(Near(AnimationBlend::Fraction(*every.AnimInfo), 0.5F), "60 Hz cadence fraction");
        Require(AnimationBlend::Fraction(*outside.AnimInfo) == 0, "tick without a clock blended");
        // A slot the simulation changed after the tick is not blended.
        (*gated.AnimInfo->Frame)[1] = 0;
        Require(AnimationBlend::Fraction(*gated.AnimInfo) == 0, "stale lookahead blended");
        // Past due holds the next frame rather than overshooting it.
        for (int step = 0; step < 5; ++step) clock.Step();
        (*every.AnimInfo->Frame)[1] = every.AnimInfo->ModTick.Frame[1];
        Require(Near(AnimationBlend::Fraction(*every.AnimInfo), 1.0F), "overdue tick overshoot");
        // A draw-side tick runs once per simulation step however often it is drawn.
        ModelInstance drawn(model);
        Configure(drawn, AnimFlags::None, group);
        (*drawn.AnimInfo->Step)[1] = 1;
        for (int step = 0; step < 3; ++step)
        {
            const SceneAnimation::Step simulation;
            for (int draw = 0; draw < 4; ++draw) SceneAnimation::TickFromDraw(drawn);
        }
        Require(drawn.AnimInfo->NodeFrame() == 3, "draw-side tick ran per draw");
        Require(drawn.AnimInfo->ModTick.Clock == &AnimationClock::Simulation()
            && drawn.AnimInfo->ModTick.Interval == 1, "draw-side tick off the simulation clock");
        // A draw-side timer advances by the steps since it last ran, however often drawn.
        const int key = 0;
        int advanced = SceneAnimation::StepsForDraw(&key);
        for (int step = 0; step < 7; ++step)
        {
            const SceneAnimation::Step simulation;
            for (int draw = 0; draw < (step % 3) + 1; ++draw) advanced += SceneAnimation::StepsForDraw(&key);
        }
        Require(advanced == 1 + 7, "draw-side timer ran per draw");
        { const SceneAnimation::Step a; const SceneAnimation::Step b; const SceneAnimation::Step c; }
        Require(SceneAnimation::StepsForDraw(&key) == 3, "draw-side timer dropped steps at a low draw rate");
        std::cout << "PASS simulation: 30 Hz and 60 Hz cadences, clockless ticks, stale and overdue state, draw-side ticks and timers\n";
    }

    void PoseChecks()
    {
        Rig rig = MakeRig();
        Model& model = *rig.Model;
        ModelInstance inst(rig.Model);
        Configure(inst, AnimFlags::None, rig.Group, 4);
        (*inst.AnimInfo->Step)[1] = 1;
        AnimationClock clock;
        const auto tick = [&]
        {
            clock.Step();
            const AnimationClock::TickScope scope(clock);
            inst.UpdateAnimFrames();
        };
        tick(); tick();
        Require(inst.AnimInfo->NodeFrame() == 2, "fixture cadence");
        clock.Phase(0.25);

        Pose(model, inst.AnimInfo);
        Require(TranslationX(model) == 20, "blended outside a draw");
        // Settings -> Animation, on these frames a simulation step apart: a
        // rate no faster than the frames draws them as they are, a faster one
        // in its own steps between them, the display's at any point.
        const auto posed = [&](std::int32_t rate, double phase)
        {
            AnimationBlend::Rate(rate);
            clock.Phase(phase);
            const AnimationBlend::DrawScope draw;
            Pose(model, inst.AnimInfo);
            return TranslationX(model);
        };
        Require(posed(AnimationBlend::DsRate, 0.75) == 20, "30 Hz setting blended");
        Require(posed(60, 0.75) == 20, "a rate equal to the frames' blended");
        Require(posed(120, 0.25) == 20 && Near(posed(120, 0.75), 25), "120 Hz is half-frame steps");
        Require(Near(posed(AnimationBlend::DisplayRate, 0.75), 27.5F), "display rate blends continuously");
        AnimationBlend::Rate(AnimationBlend::DisplayRate);
        clock.Phase(0.25);
        Require(AnimationBlend::ParseRate("display") == AnimationBlend::DisplayRate
            && AnimationBlend::ParseRate("144") == 144 && AnimationBlend::ParseRate("30hz") == 30
            && !AnimationBlend::ParseRate("20") && !AnimationBlend::ParseRate("fast")
            && AnimationBlend::RateName(AnimationBlend::DisplayRate) == "display"
            && AnimationBlend::RateName(240) == "240", "rate names round-trip");
        {
            const AnimationBlend::DrawScope draw;
            Pose(model, inst.AnimInfo);
            Require(Near(TranslationX(model), 22.5F), "fractional translation");
            {
                const AnimationBlend::Suppress suppress;
                Pose(model, inst.AnimInfo);
                Require(TranslationX(model) == 20, "suppressed draw blended");
            }
            Pose(model, inst.AnimInfo);
            const auto other = Fixture();
            Require(other->InterpolateAnimation(rig.Group->Translations, 0, 2, 1, 4, 4) == 20,
                "sampling leaked past its pass");
        }
        Require(TranslationX(model) == 20, "blended pose leaked into the simulation");

        {
            const AnimationBlend::DrawScope draw;
            Pose(model, inst.AnimInfo);
            model.Nodes->at(0)->Animation.M41 = 99;
        }
        Require(TranslationX(model) == 99, "restore clobbered a later write");

        // Loop seam: frame 3 goes to 0, and rotation takes the short way.
        tick();
        Require(inst.AnimInfo->NodeFrame() == 3 && inst.AnimInfo->ModTick.Next[1] == 0, "loop lookahead");
        clock.Phase(0.5);
        {
            const AnimationBlend::DrawScope draw;
            Pose(model, inst.AnimInfo);
            Require(Near(TranslationX(model), 15), "loop seam interpolation");
        }
        tick();
        clock.Phase(0.25);
        {
            const AnimationBlend::DrawScope draw;
            Pose(model, inst.AnimInfo);
            const float angle = 3.1F + (2 * std::numbers::pi_v<float> - 6.2F) * 0.25F;
            const Matrix4& m = model.Nodes->at(0)->Animation;
            Require(Near(m.M22, std::cos(angle)) && Near(std::abs(m.M23), std::abs(std::sin(angle))),
                "rotation long arc");
        }
        Require(Near(model.Nodes->at(0)->Animation.M22, std::cos(3.1F)), "rotation leaked");
        std::cout << "PASS poses: draw-only blending, restore, suppress, loop seam, shortest rotation\n";
    }

    void RotationResetChecks()
    {
        const auto rig = MakeRig();
        constexpr float QuarterTurn = std::numbers::pi_v<float> / 2;
        for (const bool continuous : {false, true})
        for (const bool reverse : {false, true})
        {
            const auto angles = continuous ? std::vector<float>{0, QuarterTurn, 2 * QuarterTurn, 3 * QuarterTurn, 0}
                : std::vector<float>{0, -.1F, -.2F, -.3F, 0};
            RawNodeAnimationGroup raw{};
            Put(raw, offsetof(RawNodeAnimationGroup, FrameCount), std::uint32_t{4});
            const auto group = std::make_shared<NodeAnimationGroup>(raw, rig.Group->Scales,
                std::make_shared<const std::vector<float>>(angles), rig.Group->Translations, rig.Group->Animations);
            ModelInstance instance(rig.Model);
            Configure(instance, reverse ? AnimFlags::Reverse : AnimFlags::None, group, 4);
            (*instance.AnimInfo->Step)[1] = 1;
            (*instance.AnimInfo->Frame)[1] = reverse ? 2 : 1;
            AnimationClock clock(1.0 / 30);
            for (int i = 0; i < 2; ++i)
            {
                clock.Step();
                const AnimationClock::TickScope scope(clock);
                instance.UpdateAnimFrames();
            }
            const int frame = reverse ? 0 : 3;
            Require(instance.AnimInfo->NodeFrame() == frame, "rotation fixture missed loop reset");
            for (const double phase : {0.0, .25, .5, .75, 1.0})
            {
                clock.Phase(phase);
                Pose(*rig.Model, instance.AnimInfo);
                const auto integer = rig.Model->Nodes->at(0)->Animation;
                {
                    const AnimationBlend::DrawScope draw;
                    Pose(*rig.Model, instance.AnimInfo);
                    const float expected = angles[frame]
                        + (continuous ? (reverse ? -QuarterTurn : QuarterTurn) * static_cast<float>(phase) : 0);
                    const auto& shown = rig.Model->Nodes->at(0)->Animation;
                    Require(Near(shown.M22, std::cos(expected)) && Near(shown.M23, std::sin(expected)),
                        "authored rotation reset or continuous full-turn seam was interpolated incorrectly");
                }
                Require(std::memcmp(&rig.Model->Nodes->at(0)->Animation, &integer, sizeof(Matrix4)) == 0,
                    "rotation reset draw pose leaked");
            }
        }
        std::cout << "PASS rotation resets: preserve discontinuities, blend continuous full turns, forward/reverse and restore\n";
    }

    // TexcoordAnimation marshals its name too; S translation runs 0, 10, 20, 30.
    TexcoordAnimation MakeTexcoordAnimation(bool sharedTranslation = false)
    {
        constexpr std::size_t at = 4;
        std::array<std::uint8_t, at + 0x40> bytes{};
        const auto put16 = [&](std::size_t field, std::uint16_t value)
        {
            bytes[at + field] = static_cast<std::uint8_t>(value);
            bytes[at + field + 1] = static_cast<std::uint8_t>(value >> 8);
        };
        bytes[at] = 't';
        bytes[at + 32] = bytes[at + 33] = 1;               // scale blend S, T
        put16(34, 1); put16(36, 1); put16(38, 0); put16(40, 0);
        bytes[at + 42] = 1;                                // rotate blend
        put16(44, 1); put16(46, 0);
        bytes[at + 48] = bytes[at + 49] = 1;               // translate blend S, T
        put16(50, 4); put16(52, sharedTranslation ? 4 : 1);
        put16(54, 0); put16(56, sharedTranslation ? 0 : 4);
        return Read::DoOffset<TexcoordAnimation>(bytes, static_cast<std::uint32_t>(at));
    }

    std::shared_ptr<Material> MakeMaterial(RepeatMode s, RepeatMode t,
        TexgenMode texgen = TexgenMode::None)
    {
        constexpr std::size_t at = 4;
        std::array<std::uint8_t, at + 0x84> bytes{};
        bytes[at] = 't';
        bytes[at + 0x48] = static_cast<std::uint8_t>(s);
        bytes[at + 0x49] = static_cast<std::uint8_t>(t);
        bytes[at + 0x5C] = static_cast<std::uint8_t>(texgen);
        return std::make_shared<Material>(Read::DoOffset<RawMaterial>(bytes, static_cast<std::uint32_t>(at)));
    }

    void TexcoordLoopChecks()
    {
        const auto model = Fixture();
        const auto animation = MakeTexcoordAnimation(true);
        RawTexcoordAnimationGroup raw{};
        Put(raw, offsetof(RawTexcoordAnimationGroup, FrameCount), std::uint32_t{4});
        const auto group = std::make_shared<TexcoordAnimationGroup>(raw,
            std::make_shared<const std::vector<float>>(std::initializer_list<float>{1}),
            std::make_shared<const std::vector<float>>(std::initializer_list<float>{0}),
            std::make_shared<const std::vector<float>>(std::initializer_list<float>{0, .25F, .5F, .75F}),
            std::make_shared<TexcoordAnimationDictionary>());

        for (const auto flags : {AnimFlags::None, AnimFlags::Reverse, AnimFlags::PingPong, AnimFlags::NoLoop})
        {
            ModelInstance inst(model);
            Configure(inst, flags, nullptr, 4);
            (*inst.AnimInfo->Step)[1] = 1;
            inst.AnimInfo->Texcoord->Slot = 1;
            inst.AnimInfo->Texcoord->Group = group;
            AnimationClock clock;
            for (int i = 0; i < 3; ++i)
            {
                clock.Step();
                const AnimationClock::TickScope scope(clock);
                inst.UpdateAnimFrames();
            }
            const auto frame = inst.AnimInfo->TexcoordFrame();
            const auto next = inst.AnimInfo->ModTick.Next[1];
            const auto savedFlags = (*inst.AnimInfo->Flags)[1];
            const bool wraps = flags == AnimFlags::None || flags == AnimFlags::Reverse;
            for (const auto modes : {std::array{RepeatMode::Repeat, RepeatMode::Clamp},
                     std::array{RepeatMode::Clamp, RepeatMode::Mirror},
                     std::array{RepeatMode::Mirror, RepeatMode::Repeat}})
            for (const auto texgen : {TexgenMode::None, TexgenMode::Normal})
            {
                const auto material = MakeMaterial(modes[0], modes[1], texgen);
                for (const float phase : {0.0F, .25F, .5F, .75F, 1.0F})
                {
                    clock.Phase(phase);
                    const auto integer = AnimationBlend::Texcoords(*model, inst.AnimInfo, *material, animation);
                    Require(Near(integer.M41, frame * .25F), "UV blended outside a draw");
                    const AnimationBlend::DrawScope draw;
                    const auto shown = AnimationBlend::Texcoords(*model, inst.AnimInfo, *material, animation);
                    for (std::size_t axis = 0; axis < 2; ++axis)
                    {
                        float delta = (next - frame) * .25F;
                        if (wraps && texgen != TexgenMode::Normal && modes[axis] == RepeatMode::Repeat)
                            delta = flags == AnimFlags::Reverse ? -.25F : .25F;
                        const float expected = frame * .25F + delta * phase;
                        Require(Near(axis == 0 ? shown.M41 : shown.M42, expected),
                            "UV loop swept backward or used the other axis's repeat mode");
                    }
                    const AnimationBlend::Suppress suppress;
                    const auto suppressed = AnimationBlend::Texcoords(*model, inst.AnimInfo, *material, animation);
                    Require(Near(suppressed.M41, integer.M41), "suppressed UV loop blended");
                }
            }
            Require(inst.AnimInfo->TexcoordFrame() == frame && (*inst.AnimInfo->Flags)[1] == savedFlags,
                "UV loop changed the simulation state");
        }
        // Mirror needs a two-tile period: 1.75 -> 0 advances to 2, not back to 0.
        const auto mirroredGroup = std::make_shared<TexcoordAnimationGroup>(raw, group->Scales, group->Rotations,
            std::make_shared<const std::vector<float>>(std::initializer_list<float>{0, .5F, 1, 1.75F}),
            group->Animations);
        ModelInstance inst(model);
        Configure(inst, AnimFlags::None, nullptr, 4);
        (*inst.AnimInfo->Step)[1] = 1;
        inst.AnimInfo->Texcoord->Slot = 1;
        inst.AnimInfo->Texcoord->Group = mirroredGroup;
        PreviewAnimation preview;
        preview.Advance(inst, 3.5 / 30);
        const auto material = MakeMaterial(RepeatMode::Mirror, RepeatMode::Repeat);
        const AnimationBlend::DrawScope draw;
        const auto shown = AnimationBlend::Texcoords(*model, inst.AnimInfo, *material, animation);
        Require(Near(shown.M41, 1.875F) && Near(shown.M42, 1.875F), "mirror loop period");
        // An authored subcycle can reset before the animation slot wraps.
        // Overload's electric material does this every 150 of its 3001 frames.
        const auto subcycle = std::make_shared<TexcoordAnimationGroup>(raw, group->Scales, group->Rotations,
            std::make_shared<const std::vector<float>>(std::initializer_list<float>{0, -1.75F, 0, -.25F}),
            group->Animations);
        for (const auto flags : {AnimFlags::None, AnimFlags::Reverse})
        {
            ModelInstance subcycleInstance(model);
            Configure(subcycleInstance, flags, nullptr, 4);
            (*subcycleInstance.AnimInfo->Step)[1] = 1;
            (*subcycleInstance.AnimInfo->Frame)[1] = flags == AnimFlags::None ? 3 : 0;
            subcycleInstance.AnimInfo->Texcoord->Slot = 1;
            subcycleInstance.AnimInfo->Texcoord->Group = subcycle;
            PreviewAnimation subcycleClock;
            subcycleClock.Advance(subcycleInstance, 2.5 / 30);
            const auto subcycleMaterial = MakeMaterial(RepeatMode::Repeat, RepeatMode::Clamp);
            const auto before = subcycleInstance.AnimInfo->TexcoordFrame();
            const AnimationBlend::DrawScope subcycleDraw;
            const auto subcycleShown = AnimationBlend::Texcoords(*model, subcycleInstance.AnimInfo,
                *subcycleMaterial, animation);
            Require(Near(subcycleShown.M41, flags == AnimFlags::None ? -1.875F : .125F),
                "interior UV subcycle swept through repeated tiles");
            Require(Near(subcycleShown.M42, -.875F), "interior UV subcycle changed clamped axis");
            Require(subcycleInstance.AnimInfo->TexcoordFrame() == before, "interior UV subcycle changed frame");
        }
        // A closed ramp's duplicate last key must not add a still interval.
        const auto closed = std::make_shared<TexcoordAnimationGroup>(raw, group->Scales, group->Rotations,
            std::make_shared<const std::vector<float>>(std::initializer_list<float>{0, 1.0F / 3, 2.0F / 3, 1}),
            group->Animations);
        for (const auto flags : {AnimFlags::None, AnimFlags::Reverse, AnimFlags::NoLoop,
                 AnimFlags::NoLoop | AnimFlags::Reverse, AnimFlags::PingPong, AnimFlags::PingPong | AnimFlags::Reverse})
        {
            ModelInstance loop(model);
            Configure(loop, flags, nullptr, 4);
            (*loop.AnimInfo->Step)[1] = 1;
            const bool reverse = (flags & AnimFlags::Reverse) != AnimFlags::None;
            const bool looping = flags == AnimFlags::None || flags == AnimFlags::Reverse;
            (*loop.AnimInfo->Frame)[1] = reverse ? 1 : 2;
            loop.AnimInfo->Texcoord->Slot = 1;
            loop.AnimInfo->Texcoord->Group = closed;
            AnimationClock clock(1.0 / 30);
            const auto tick = [&] { clock.Step(); const AnimationClock::TickScope scope(clock); loop.UpdateAnimFrames(); };
            tick(); tick();
            const auto repeated = MakeMaterial(RepeatMode::Repeat, RepeatMode::Clamp);
            for (int i = 0; i < 8; ++i)
            {
                const auto frame = loop.AnimInfo->TexcoordFrame();
                const auto savedFlags = (*loop.AnimInfo->Flags)[1];
                for (const float phase : {0.0F, .25F, .5F, .75F, 1.0F})
                {
                    clock.Phase(phase);
                    const auto shown = AnimationBlend::Texcoords(*model, loop.AnimInfo, *repeated, animation);
                    const float time = reverse ? frame + 1 - phase : frame + phase;
                    const float integer = closed->Translations->at(static_cast<std::size_t>(frame));
                    const float next = closed->Translations->at(static_cast<std::size_t>(loop.AnimInfo->ModTick.Next[1]));
                    const float authored = integer + (next - integer) * phase;
                    Require(Near(shown.M41, looping ? time / 4 : authored),
                        "closed UV loop paused or changed a no-loop/ping-pong transition");
                    Require(Near(shown.M42, integer + (next - integer) * phase), "closed loop retimed a clamped axis");
                    Require(loop.AnimInfo->TexcoordFrame() == frame && (*loop.AnimInfo->Flags)[1] == savedFlags,
                        "closed loop changed simulation frames or flags");
                }
                tick();
            }
            clock.Phase(.5);
            AnimationBlend::Rate(AnimationBlend::DsRate);
            const auto legacy = AnimationBlend::Texcoords(*model, loop.AnimInfo, *repeated, animation);
            AnimationBlend::Rate(AnimationBlend::DisplayRate);
            Require(Near(legacy.M41, closed->Translations->at(static_cast<std::size_t>(loop.AnimInfo->TexcoordFrame()))),
                "30 Hz setting retimed a closed UV loop");
        }
        const auto quantized = std::make_shared<TexcoordAnimationGroup>(raw, group->Scales, group->Rotations,
            std::make_shared<const std::vector<float>>(std::initializer_list<float>{0, -2.0F / 3, -4.0F / 3, -2 + 1.0F / 4096}),
            group->Animations);
        ModelInstance quantizedLoop(model);
        Configure(quantizedLoop, AnimFlags::None, nullptr, 4);
        quantizedLoop.AnimInfo->Texcoord->Slot = 1;
        quantizedLoop.AnimInfo->Texcoord->Group = quantized;
        (*quantizedLoop.AnimInfo->Step)[1] = 1;
        AnimationClock quantizedClock(1.0 / 30);
        for (int i = 0; i < 3; ++i)
        {
            quantizedClock.Step();
            const AnimationClock::TickScope scope(quantizedClock);
            quantizedLoop.UpdateAnimFrames();
        }
        // A picture immediately before the tick, without advancing the slot.
        quantizedClock.Phase(1);
        const auto mirror = MakeMaterial(RepeatMode::Mirror, RepeatMode::Clamp);
        const auto closedEnd = AnimationBlend::Texcoords(*model, quantizedLoop.AnimInfo, *mirror, animation);
        Require(Near(closedEnd.M41, -2) && quantizedLoop.AnimInfo->TexcoordFrame() == 3,
            "quantized mirror loop did not close without changing its frame");
        const auto returning = std::make_shared<TexcoordAnimationGroup>(raw, group->Scales, group->Rotations,
            std::make_shared<const std::vector<float>>(std::initializer_list<float>{0, .5F, .5F, 0}),
            group->Animations);
        ModelInstance returningLoop(model);
        Configure(returningLoop, AnimFlags::None, nullptr, 4);
        returningLoop.AnimInfo->Texcoord->Slot = 1;
        returningLoop.AnimInfo->Texcoord->Group = returning;
        (*returningLoop.AnimInfo->Step)[1] = 1;
        PreviewAnimation returningClock;
        returningClock.Advance(returningLoop, 3.5 / 30);
        const auto returningMaterial = MakeMaterial(RepeatMode::Repeat, RepeatMode::Clamp);
        const auto returningEnd = AnimationBlend::Texcoords(*model, returningLoop.AnimInfo, *returningMaterial, animation);
        Require(Near(returningEnd.M41, .1875F) && Near(returningEnd.M42, 0),
            "UV cycle returning to the same offset retained its duplicate-endpoint hold");
        std::cout << "PASS UV loops: forward/reverse, repeat/mirror/clamp, shared LUT, ping-pong/end, interior resets, closed-cycle pacing, suppress and state\n";
    }

    void ChannelChecks()
    {
        Rig rig = MakeRig();
        Model& model = *rig.Model;
        const auto colors = std::make_shared<const std::vector<float>>(
            std::initializer_list<float>{0, 4, 8, 12});
        const auto materialGroup = std::make_shared<MaterialAnimationGroup>(RawMaterialAnimationGroup{},
            colors, std::make_shared<MaterialAnimationDictionary>());
        const TexcoordAnimation texcoord = MakeTexcoordAnimation();
        const auto texcoordGroup = std::make_shared<TexcoordAnimationGroup>(RawTexcoordAnimationGroup{},
            std::make_shared<const std::vector<float>>(std::initializer_list<float>{1}),
            std::make_shared<const std::vector<float>>(std::initializer_list<float>{0}),
            rig.Group->Translations, std::make_shared<TexcoordAnimationDictionary>());

        ModelInstance inst(rig.Model);
        Configure(inst, AnimFlags::None, rig.Group, 4);
        auto& info = *inst.AnimInfo;
        (*info.Step)[0] = (*info.Step)[1] = 1;
        info.Material->Slot = 0;
        info.Material->Group = materialGroup;
        info.Texcoord->Slot = 1;
        info.Texcoord->Group = texcoordGroup;
        AnimationClock clock;
        for (int i = 0; i < 2; ++i)
        {
            clock.Step();
            const AnimationClock::TickScope scope(clock);
            inst.UpdateAnimFrames();
        }
        clock.Phase(0.5);
        Require(Near(AnimationBlend::Fraction(info, AnimationChannel::Material), 0.5F)
            && Near(AnimationBlend::Fraction(info, AnimationChannel::Texcoord), 0.5F), "channel fractions");
        {
            const AnimationBlend::DrawScope draw;
            {
                const AnimationBlend::Pass material(model, AnimationChannel::Material, 0, inst.AnimInfo, [] {});
                Require(Near(model.InterpolateAnimation(colors, 0, 2, 1, 4, 4), 10), "material colour blend");
                Require(model.InterpolateAnimation(rig.Group->Translations, 0, 2, 1, 4, 4) == 20,
                    "material pass blended a node curve");
            }
            {
                const AnimationBlend::Pass pass(model, AnimationChannel::Texcoord, inst.AnimInfo);
                Require(Near(model.AnimateTexcoords(texcoordGroup, texcoord, 2).M41, 25), "texcoord blend");
            }
            Require(model.AnimateTexcoords(texcoordGroup, texcoord, 2).M41 == 20, "texcoord blended outside its pass");
        }
        // Each channel follows its own slot: a changed material slot stops only that channel.
        (*info.Frame)[0] = 0;
        Require(AnimationBlend::Fraction(info, AnimationChannel::Material) == 0
            && Near(AnimationBlend::Fraction(info, AnimationChannel::Texcoord), 0.5F), "channels share a slot check");
        std::cout << "PASS channels: material colours and texcoords blend per slot, inside their pass only\n";
    }

    void AssetChecks(const char* directory)
    {
        Paths::MphKey = "AMHP1";
        Paths::SetPath("AMHP1", directory);
        for (const auto hunter : {Hunter::Samus, Hunter::Kanden, Hunter::Spire,
                 Hunter::Trace, Hunter::Noxus, Hunter::Sylux, Hunter::Weavel})
        {
            auto instance = Read::GetModelInstance(Metadata::HunterModels.at(hunter).at(0));
            instance->SetAnimation(static_cast<int>(Entities::PlayerAnimation::Idle));
            auto& model = *instance->Model();
            PreviewAnimation clock;
            bool changed = false;
            std::vector<Matrix4> integer;
            for (int frame = 0; frame < 144 * 3; ++frame)
            {
                clock.Advance(*instance, 1.0 / 144);
                model.ComputeNodeMatrices(0);
                Pose(model, instance->AnimInfo);
                model.AnimateMaterials(instance->AnimInfo);
                integer.clear();
                for (const auto& node : *model.Nodes) integer.push_back(node->Animation);
                std::vector<std::array<float, 10>> colors;
                const auto colorsOf = [](const Material& m)
                {
                    return std::array<float, 10>{m.CurrentDiffuse.X, m.CurrentDiffuse.Y, m.CurrentDiffuse.Z,
                        m.CurrentAmbient.X, m.CurrentAmbient.Y, m.CurrentAmbient.Z,
                        m.CurrentSpecular.X, m.CurrentSpecular.Y, m.CurrentSpecular.Z, m.CurrentAlpha};
                };
                for (const auto& material : *model.Materials) colors.push_back(colorsOf(*material));
                {
                    const AnimationBlend::DrawScope draw;
                    model.AnimateMaterials(instance->AnimInfo);
                    Pose(model, instance->AnimInfo);
                    for (std::size_t i = 0; i < model.Nodes->size(); ++i)
                    {
                        const auto& matrix = model.Nodes->at(i)->Animation;
                        Require(std::isfinite(matrix.M11) && std::isfinite(matrix.M41), "invalid hunter pose");
                        if (std::memcmp(&matrix, &integer[i], sizeof(matrix)) != 0) changed = true;
                    }
                }
                for (std::size_t i = 0; i < model.Nodes->size(); ++i)
                    Require(std::memcmp(&model.Nodes->at(i)->Animation, &integer[i], sizeof(Matrix4)) == 0,
                        "hunter draw pose leaked");
                for (std::size_t i = 0; i < model.Materials->size(); ++i)
                    Require(colorsOf(*model.Materials->at(i)) == colors[i], "hunter draw colour leaked");
            }
            Require(changed, "hunter has no intermediate poses");
            std::cout << "PASS real hunter 144 Hz poses and restore: " << static_cast<int>(hunter) << '\n';
        }
    }

    void AssetFormChecks()
    {
        for (const auto hunter : {Hunter::Samus, Hunter::Kanden, Hunter::Spire,
                 Hunter::Trace, Hunter::Noxus, Hunter::Sylux, Hunter::Weavel})
        for (const auto animation : {Entities::PlayerAnimation::Morph, Entities::PlayerAnimation::Unmorph})
        {
            auto instance = Read::GetModelInstance(Metadata::HunterModels.at(hunter).at(0));
            instance->SetAnimation(static_cast<int>(animation), AnimFlags::NoLoop | AnimFlags::Paused);
            auto& model = *instance->Model();
            auto& info = *instance->AnimInfo;
            model.ComputeNodeMatrices(0);
            Pose(model, instance->AnimInfo);
            const int frame = info.NodeFrame();
            const auto flags = (*info.Flags)[0];
            std::vector<Matrix4> integer;
            for (const auto& node : *model.Nodes) integer.push_back(node->Animation);
            const int last = (*info.FrameCount)[0] - 1;
            Require(last > 1, "real form clip has no duration");
            for (const int hz : {30, 60, 120, 144, 240, 540})
            {
                const int draws = static_cast<int>(std::ceil(last * hz / 30.0));
                for (int draw = 0; draw <= draws; ++draw)
                {
                    const float shown = std::min(static_cast<float>(last), 30.0F * draw / hz);
                    {
                        const AnimationBlend::DrawScope blend;
                        AnimationBlend::FrameOverride sample;
                        sample.Set(info, shown);
                        Pose(model, instance->AnimInfo);
                        Require(std::abs(AnimationBlend::FrameShown(info) - shown) < 0.0001F,
                            "real form clip's duration changed with display rate");
                        for (const auto& node : *model.Nodes)
                            Require(std::isfinite(node->Animation.M11) && std::isfinite(node->Animation.M41),
                                "real form sample produced an invalid node pose");
                    }
                    Require(info.NodeFrame() == frame && (*info.Flags)[0] == flags,
                        "real form draw changed simulation or completed gameplay");
                    for (std::size_t i = 0; i < integer.size(); ++i)
                        Require(std::memcmp(&integer[i], &model.Nodes->at(i)->Animation, sizeof(Matrix4)) == 0,
                            "real form draw leaked a node pose into the simulation");
                }
            }
            std::cout << "PASS real form clip at 30/60/120/144/240/540 fps: hunter=" << static_cast<int>(hunter)
                << " animation=" << static_cast<int>(animation) << " frames=" << last + 1 << '\n';
        }
    }

    void AssetDoorSealChecks()
    {
        for (const auto* name : {"AlimbicDoorLock", "AlimbicMorphBallDoorLock", "ThinDoorLock"})
        for (const bool reverse : {false, true})
        {
            auto instance = Read::GetModelInstance(name);
            instance->SetAnimation(0, reverse ? AnimFlags::Reverse : AnimFlags::None);
            auto& model = *instance->Model();
            auto& info = *instance->AnimInfo;
            const auto& group = info.Node->Group;
            const int last = group->FrameCount - 1;
            (*info.Frame)[0] = reverse ? 2 : last - 2;
            AnimationClock clock(1.0 / 30);
            for (int tick = 0; tick < 2; ++tick)
            {
                clock.Step();
                const AnimationClock::TickScope scope(clock);
                instance->UpdateAnimFrames();
            }
            Require(info.NodeFrame() == (reverse ? 0 : last), "door seal fixture missed loop boundary");
            const auto flags = (*info.Flags)[0];
            int rotating = 0;
            for (const auto& [node, animation] : *group->Animations)
            {
                if (animation.RotateLutLengthZ <= 1) continue;
                ++rotating;
                const auto at = [&](int frame)
                {
                    return model.InterpolateAnimation(group->Rotations, animation.RotateLutIndexZ,
                        frame, animation.RotateBlendZ, animation.RotateLutLengthZ, group->FrameCount, true);
                };
                const float first = at(info.NodeFrame());
                const float step = std::remainder(at(last) - at(last - 1), 2 * std::numbers::pi_v<float>);
                const float reset = std::remainder(at(0) - at(last), 2 * std::numbers::pi_v<float>);
                std::cout << "DOOR SEAL " << name << " node=" << node << " reverse=" << reverse
                    << " step-deg=" << step * 180 / std::numbers::pi_v<float>
                    << " reset-deg=" << reset * 180 / std::numbers::pi_v<float> << '\n';
                for (const double phase : {0.0, .25, .5, .75, 1.0})
                {
                    clock.Phase(phase);
                    Pose(model, instance->AnimInfo);
                    const auto index = model.GetNodeIndexByName(node);
                    const auto integer = model.Nodes->at(index)->Animation;
                    const AnimationBlend::DrawScope draw;
                    {
                        const AnimationBlend::Pass pass(model, AnimationChannel::Node, instance->AnimInfo);
                        Require(Near(at(info.NodeFrame()), first), "door seal swept backwards across its authored rotation reset");
                    }
                    Pose(model, instance->AnimInfo);
                    const auto shown = model.Nodes->at(index)->Animation;
                    Require(Near(shown.M11, integer.M11) && Near(shown.M12, integer.M12),
                        "door seal production node pose swept across its reset");
                }
                Require(info.NodeFrame() == (reverse ? 0 : last) && (*info.Flags)[0] == flags,
                    "door seal sampling changed animation state");
            }
            Require(rotating == 2, "door seal rotating geometry was not tested");
            // The two animated pieces have threefold rotational symmetry.
            // Measure their displayed orientation modulo that real geometry,
            // over repeated loops, rather than merely checking finite matrices.
            std::unordered_map<int, float> expectedSteps;
            for (int frame = 0; frame < 2; ++frame)
            {
                (*info.Frame)[0] = frame;
                Pose(model, instance->AnimInfo);
                for (const auto& [node, animation] : *group->Animations)
                {
                    if (animation.RotateLutLengthZ <= 1) continue;
                    const int index = model.GetNodeIndexByName(node);
                    const auto& matrix = model.Nodes->at(index)->Animation;
                    const float angle = std::atan2(matrix.M12, matrix.M11);
                    if (frame == 0) expectedSteps[index] = angle;
                    else expectedSteps[index] = std::remainder(angle - expectedSteps[index],
                        2 * std::numbers::pi_v<float> / 3) * (reverse ? -1 : 1);
                }
            }
            for (const int hz : {30, 60, 144, 240, 540})
            {
                instance->SetAnimation(0, reverse ? AnimFlags::Reverse : AnimFlags::None);
                PreviewAnimation preview;
                preview.Advance(*instance, 2.0 / 30);
                std::unordered_map<int, float> previous;
                for (int picture = 0; picture < hz * 3; ++picture)
                {
                    preview.Advance(*instance, 1.0 / hz);
                    const AnimationBlend::DrawScope draw;
                    Pose(model, instance->AnimInfo);
                    for (const auto& [node, animation] : *group->Animations)
                    {
                        if (animation.RotateLutLengthZ <= 1) continue;
                        const int index = model.GetNodeIndexByName(node);
                        const auto& matrix = model.Nodes->at(index)->Animation;
                        const float angle = std::atan2(matrix.M12, matrix.M11);
                        if (const auto old = previous.find(index); old != previous.end())
                        {
                            const float step = std::remainder(angle - old->second,
                                2 * std::numbers::pi_v<float> / 3);
                            Require(std::abs(step) < .107F, "door seal jumped more than one authored rotation step");
                            if (step * expectedSteps[index] < -.00002F)
                            {
                                std::cerr << "door seal direction: hz=" << hz << " picture=" << picture
                                    << " node=" << node << " frame=" << info.NodeFrame() << " delta=" << step << '\n';
                                throw std::runtime_error("door seal reversed its displayed rotation during a loop");
                            }
                        }
                        previous[index] = angle;
                    }
                }
            }
            std::cout << "PASS real door seal rotation reset: " << name << " reverse=" << reverse << '\n';
        }
    }

    void AssetTexcoordChecks()
    {
        int surfaces = 0;
        int corrected = 0;
        int spans = 0;
        const auto checkLoops = [&](const std::shared_ptr<Model>& model)
        {
            int checked = 0;
            for (const auto& group : *model->AnimationGroups->Texcoord)
            {
                if (group->FrameCount < 3 || group->Count == 0) continue;
                ModelInstance instance(model);
                auto& info = *instance.AnimInfo;
                info.Texcoord->Group = group;
                info.Texcoord->Slot = 0;
                (*info.FrameCount)[0] = group->FrameCount;
                (*info.Frame)[0] = group->FrameCount - 3;
                AnimationClock clock(1.0 / 30);
                const auto tick = [&] { clock.Step(); const AnimationClock::TickScope scope(clock); instance.UpdateAnimFrames(); };
                tick(); tick();
                clock.Phase(.5);
                Require(info.TexcoordFrame() == group->FrameCount - 1 && info.ModTick.Next[0] == 0,
                    "asset fixture did not reach loop seam");
                std::unordered_map<const Material*, Matrix4> previousEnds;
                std::unordered_map<const Material*, Matrix4> previousStarts;
                // Spans each scrolling axis moves forward and back across the whole cycle.
                std::unordered_map<const Material*, std::array<std::array<int, 3>, 2>> directions;
                for (int span = 0; span <= group->FrameCount; ++span)
                {
                    const auto frame = info.TexcoordFrame();
                    const auto nextFrame = (frame + 1) % group->FrameCount;
                    for (const auto& material : *model->Materials)
                    {
                        const auto found = group->Animations->find(material->Name);
                        if (found == group->Animations->end() || material->TexgenMode == TexgenMode::Normal) continue;
                        const auto& animation = found->second;
                        const auto first = model->AnimateTexcoords(group, animation, frame);
                        const auto next = model->AnimateTexcoords(group, animation, nextFrame);
                        Matrix4 rawBlend;
                        {
                            const AnimationBlend::DrawScope draw;
                            {
                                const AnimationBlend::Pass pass(*model, AnimationChannel::Texcoord, instance.AnimInfo);
                                rawBlend = model->AnimateTexcoords(group, animation, frame);
                            }
                        }
                        const auto atPhase = [&](double phase)
                        {
                            clock.Phase(phase);
                            const AnimationBlend::DrawScope draw;
                            return AnimationBlend::Texcoords(*model, instance.AnimInfo, *material, animation);
                        };
                        const auto startPose = atPhase(0);
                        const auto endPose = atPhase(1);
                        // The picture never runs a span backwards: sampled finely,
                        // the turn keeps its direction, and so does each axis's
                        // scroll where nothing turns or scales (a turn about the
                        // centre moves the offset along an arc).
                        {
                            constexpr int Samples = 16;
                            const auto angle = [](const Matrix4& m) { return std::atan2(m.M12, m.M11); };
                            const float turn = std::remainder(angle(endPose) - angle(startPose), 2 * std::numbers::pi_v<float>);
                            // An authored reset (the Helm Room's floor) is a jump at
                            // the tick, never a sweep back through the turn.
                            {
                                const auto rawAngle = [&](std::int32_t f)
                                {
                                    const auto count = group->FrameCount;
                                    return angle(model->AnimateTexcoords(group, animation, (f % count + count) % count));
                                };
                                const auto rawStep = [&](std::int32_t f)
                                {
                                    return std::abs(std::remainder(rawAngle(f + 1) - rawAngle(f), 2 * std::numbers::pi_v<float>));
                                };
                                const bool seam = nextFrame == 0;
                                if (seam && rawStep(frame) > 2 * std::max(rawStep(frame - 1), rawStep(frame + 1)) + .001F)
                                    Require(Near(turn, 0), "real UV rotation reset swept through its turn");
                            }
                            const bool still = Near(startPose.M11, endPose.M11) && Near(startPose.M12, endPose.M12)
                                && Near(startPose.M21, endPose.M21) && Near(startPose.M22, endPose.M22);
                            Matrix4 before = startPose;
                            for (int k = 1; k <= Samples; ++k)
                            {
                                const auto pose = atPhase(static_cast<double>(k) / Samples);
                                const float turned = std::remainder(angle(pose) - angle(before), 2 * std::numbers::pi_v<float>);
                                if (turn * turned < 0 || (Near(turn, 0) && std::abs(turned) > .0002F))
                                {
                                    std::cerr << "UV turn reversed: model=" << model->Name << " material=" << material->Name
                                        << " frame=" << frame << " next=" << nextFrame << " sample=" << k
                                        << " step=" << turned << " total=" << turn << '\n';
                                    throw std::runtime_error("real UV rotation ran backwards inside a span");
                                }
                                for (std::size_t axis = 0; axis < 2 && still; ++axis)
                                {
                                    const auto mode = axis == 0 ? material->XRepeat : material->YRepeat;
                                    const float period = mode == RepeatMode::Repeat ? 1.0F
                                        : mode == RepeatMode::Mirror ? 2.0F : 0.0F;
                                    const auto wrap = [&](float d) { return period == 0 ? d : std::remainder(d, period); };
                                    const float total = wrap(axis == 0 ? endPose.M41 - startPose.M41 : endPose.M42 - startPose.M42);
                                    const float step = wrap(axis == 0 ? pose.M41 - before.M41 : pose.M42 - before.M42);
                                    if (total * step < 0 && std::abs(step) > .0002F)
                                    {
                                        std::cerr << "UV reversed: model=" << model->Name << " material=" << material->Name
                                            << " axis=" << axis << " frame=" << frame << " next=" << nextFrame
                                            << " sample=" << k << " step=" << step << " total=" << total << '\n';
                                        throw std::runtime_error("real UV scroll ran backwards inside a span");
                                    }
                                }
                                before = pose;
                            }
                        }
                        const auto shown = atPhase(.5);
                        for (std::size_t axis = 0; axis < 2; ++axis)
                        {
                            const auto mode = axis == 0 ? material->XRepeat : material->YRepeat;
                            const float period = mode == RepeatMode::Repeat ? 1.0F : mode == RepeatMode::Mirror ? 2.0F : 0.0F;
                            const float start = axis == 0 ? startPose.M41 : startPose.M42;
                            const float value = axis == 0 ? shown.M41 : shown.M42;
                            const float raw = axis == 0 ? rawBlend.M41 : rawBlend.M42;
                            if (period == 0)
                                Require(Near(value, raw), "clamped asset scroll changed");
                            else if (Near(first.M11, next.M11) && Near(first.M12, next.M12)
                                && Near(first.M21, next.M21) && Near(first.M22, next.M22))
                            {
                                Require(std::abs(std::remainder(value - start, period)) <= period * .25F + .0001F,
                                    "real map loop swept across repeated tiles");
                            }
                            if (period != 0 && Near(startPose.M11, endPose.M11) && Near(startPose.M12, endPose.M12))
                            {
                                const float moved = std::remainder((axis == 0 ? endPose.M41 : endPose.M42) - start, period);
                                if (std::abs(moved) > .0002F) ++directions[material.get()][axis][moved > 0 ? 0 : 1];
                                if (std::abs(moved) >= period / 8) ++directions[material.get()][axis][2];
                            }
                            // An authored rotation reset is kept, at the tick (the Helm Room's
                            // stars), and so is a seam that held still to jump a whole tile back.
                            if (const auto previous = previousEnds.find(material.get()); previous != previousEnds.end()
                                && Near(previous->second.M11, startPose.M11) && Near(previous->second.M12, startPose.M12)
                                && !(frame == 0 && Near(axis == 0 ? previousStarts[material.get()].M41 : previousStarts[material.get()].M42,
                                    axis == 0 ? previous->second.M41 : previous->second.M42)))
                            {
                                const float last = axis == 0 ? previous->second.M41 : previous->second.M42;
                                const float jump = period == 0 ? start - last : std::remainder(start - last, period);
                                Require(std::abs(jump) < .0002F, "real UV cycle jumped at a simulation tick");
                            }
                            if (!Near(value, raw)) ++corrected;
                        }
                        previousEnds[material.get()] = endPose;
                        previousStarts[material.get()] = startPose;
                        Require(info.TexcoordFrame() == frame && (*info.Flags)[0] == AnimFlags::None,
                            "asset loop draw changed animation state");
                        ++spans;
                        if (span == 0)
                        {
                            ++surfaces;
                            ++checked;
                        }
                    }
                    tick();
                    clock.Phase(.5);
                }
                // A steady scroll never runs back at its seam (the Helm Room's
                // scanning floor did, for a frame every 32 seconds).
                for (const auto& [material, axes] : directions)
                {
                    for (std::size_t axis = 0; axis < 2; ++axis)
                    {
                        const int fewer = std::min(axes[axis][0], axes[axis][1]);
                        // A step of an eighth of a tile or more is jitter (noise), not a scroll.
                        if (fewer > 0 && fewer <= 2 && axes[axis][2] <= 2)
                        {
                            std::cerr << "UV seam ran back: model=" << model->Name << " material=" << material->Name
                                << " axis=" << axis << " forward=" << axes[axis][0] << " back=" << axes[axis][1] << " raw:";
                            const auto& animation = group->Animations->find(material->Name)->second;
                            for (const int f : {0, 1, 2, group->FrameCount - 3, group->FrameCount - 2, group->FrameCount - 1})
                            {
                                const auto m = model->AnimateTexcoords(group, animation, f);
                                std::cerr << ' ' << f << '=' << (axis == 0 ? m.M41 : m.M42);
                            }
                            std::cerr << " repeat=" << static_cast<int>(axis == 0 ? material->XRepeat : material->YRepeat) << '\n';
                            throw std::runtime_error("real UV scroll ran back at its seam");
                        }
                    }
                }
            }
            std::cout << "PASS real UV loop surfaces: " << model->Name << " " << checked << '\n';
        };
        // Every room the game draws: the story's (the Helm Room's floor) too.
        for (const auto& room : Metadata::RoomList)
        {
            if (room->FirstHunt || room->Hybrid) continue;
            std::shared_ptr<ModelInstance> instance;
            try { instance = Read::GetRoomModelInstance(room->Name); }
            catch (const std::exception&) { continue; }
            if (instance != nullptr) checkLoops(instance->Model());
        }
        for (const auto* name : {"AlimbicLightPole", "Generic_Console", "Alimbic_Console", "ElectroField1",
                 "AlimbicDoorLock", "AlimbicMorphBallDoorLock", "AlimbicBossDoorLock", "ThinDoorLock",
                 "ForceField", "energyBeam", "Teleporter", "TeleporterMP"})
            checkLoops(Read::GetModelInstance(name)->Model());
        Require(corrected > 0, "real UV loop checks never exercised a wrap correction");
        std::cout << "PASS real UV loop total: " << surfaces << " surfaces, " << spans << " spans, "
            << corrected << " corrected axes\n";
    }
}

namespace
{
    // The morph ball's roll on the Animation setting's grid.
    void ShownStepFractionChecks()
    {
        using MphRead::Mods::Render::AnimationBlend;
        const std::int32_t saved = AnimationBlend::Rate();
        const auto near = [](double a, double b) { return std::fabs(a - b) < 1e-9; };
        AnimationBlend::Rate(AnimationBlend::DisplayRate);
        if (!near(AnimationBlend::ShownStepFraction(7, 0.4), 0.4)) throw std::runtime_error("display rate shows the picture's instant");
        AnimationBlend::Rate(30);
        if (!near(AnimationBlend::ShownStepFraction(8, 0.0), 0.0) || !near(AnimationBlend::ShownStepFraction(8, 0.7), 0.0))
            throw std::runtime_error("30 Hz shows the even step's pose through it");
        if (!near(AnimationBlend::ShownStepFraction(9, 0.0), -1.0) || !near(AnimationBlend::ShownStepFraction(9, 0.6), -1.0))
            throw std::runtime_error("30 Hz holds the previous step's pose through the odd step");
        AnimationBlend::Rate(60);
        if (!near(AnimationBlend::ShownStepFraction(9, 0.9), 0.0)) throw std::runtime_error("60 Hz shows the step's pose");
        AnimationBlend::Rate(120);
        if (!near(AnimationBlend::ShownStepFraction(9, 0.4), 0.0) || !near(AnimationBlend::ShownStepFraction(9, 0.6), 0.5))
            throw std::runtime_error("120 Hz shows half-step poses");
        AnimationBlend::Rate(saved);
    }
}

int main(int argc, char** argv)
{
    ShownStepFractionChecks();
    const char* stage = "preview clock";
    try
    {
        PreviewClockChecks();
        stage = "authoritative form frame";
        FormFrameOverrideChecks();
        stage = "simulation clock";
        SimulationClockChecks();
        stage = "poses";
        PoseChecks();
        stage = "rotation resets";
        RotationResetChecks();
        stage = "channels";
        ChannelChecks();
        stage = "texcoord loops";
        TexcoordLoopChecks();
        stage = "hunter assets";
        if (argc > 1)
        {
            AssetChecks(argv[1]);
            stage = "real hunter form clips";
            AssetFormChecks();
            stage = "sealed door rotation";
            AssetDoorSealChecks();
            stage = "map and prop texcoords";
            AssetTexcoordChecks();
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << stage << ": " << error.what() << '\n';
        return 1;
    }
}
