#include "../Mods/Render/AnimationBlend.hpp"
#include "../Mods/Render/AnimationClock.hpp"
#include "../Mods/Render/PreviewAnimation.hpp"
#include "../Mods/Render/SceneAnimation.hpp"
#include "../Entities/Players/PlayerEntity.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Read.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <numbers>
#include <stdexcept>

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
                Require(shown.AnimInfo->ModTick.Next == expectedNext.AnimInfo->NodeFrame(),
                    "lookahead disagrees with original state machine");
            }
            const bool moving = shown.AnimInfo->ModTick.Next != shown.AnimInfo->NodeFrame();
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
        (*every.AnimInfo->Frame)[1] = every.AnimInfo->ModTick.Frame;
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
        Require(inst.AnimInfo->NodeFrame() == 3 && inst.AnimInfo->ModTick.Next == 0, "loop lookahead");
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
                integer.clear();
                for (const auto& node : *model.Nodes) integer.push_back(node->Animation);
                {
                    const AnimationBlend::DrawScope draw;
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
            }
            Require(changed, "hunter has no intermediate poses");
            std::cout << "PASS real hunter 144 Hz poses and restore: " << static_cast<int>(hunter) << '\n';
        }
    }
}

int main(int argc, char** argv)
{
    const char* stage = "preview clock";
    try
    {
        PreviewClockChecks();
        stage = "simulation clock";
        SimulationClockChecks();
        stage = "poses";
        PoseChecks();
        stage = "hunter assets";
        if (argc > 1) AssetChecks(argv[1]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << stage << ": " << error.what() << '\n';
        return 1;
    }
}
