#include "../Mods/Render/PreviewAnimation.hpp"
#include "../Entities/Players/PlayerEntity.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Read.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

using namespace MphRead;
using namespace MphRead::Mods::Render;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    template<class T> auto Empty()
    {
        return std::make_shared<const std::vector<T>>();
    }

    std::shared_ptr<Model> Fixture()
    {
        return std::make_shared<Model>("preview-test", false, Header{},
            Empty<RawNode>(), Empty<RawMesh>(), Empty<RawMaterial>(), Empty<DisplayList>(),
            Empty<std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>>(),
            std::make_shared<AnimationResults>(), Empty<OpenTK::Mathematics::Matrix4>(),
            Empty<std::shared_ptr<Recolor>>(), Empty<std::int32_t>(), Empty<Vector3Fx>(),
            Empty<Vector3Fx>(), Empty<std::int32_t>(), Empty<Fixed>());
    }

    void Configure(ModelInstance& inst, AnimFlags flags)
    {
        auto& info = *inst.AnimInfo;
        info.Node->Slot = 1;
        for (std::size_t i = 0; i < 2; ++i)
        {
            (*info.FrameCount)[i] = 8;
            (*info.Frame)[i] = (flags & AnimFlags::Reverse) != AnimFlags::None ? 7 : 0;
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

    void ClockChecks(const std::shared_ptr<Model>& model)
    {
        for (const auto flags : {AnimFlags::None, AnimFlags::Reverse, AnimFlags::PingPong,
                 AnimFlags::PingPong | AnimFlags::Reverse, AnimFlags::NoLoop,
                 AnimFlags::NoLoop | AnimFlags::Reverse, AnimFlags::Paused, AnimFlags::Ended})
        for (const int hz : {24, 30, 60, 144, 240, 540})
        {
            ModelInstance shown(model), reference(model);
            Configure(shown, flags); Configure(reference, flags);
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
                ModelInstance expectedNext(model);
                Configure(expectedNext, flags);
                for (int i = 0; i <= referenceTicks; ++i) expectedNext.UpdateAnimFrames();
                Require(PreviewAnimation::NextNodeFrame(shown) == expectedNext.AnimInfo->NodeFrame(),
                    "lookahead disagrees with original state machine");
                CompareState(shown, reference);
            }
            Require(clock.Fraction() < 0.00001F, "clock rate drift");
            clock.Advance(shown, 1.0 / 120);
            Require(std::abs(clock.Fraction() - 0.25F) < 0.00001F, "missing fractional phase");
            clock.Reset();
            Require(clock.Fraction() == 0, "reset retained phase");
        }
        std::cout << "PASS original transitions at 24/30/60/144/240/540 Hz, both slots, lookahead and reset\n";
    }

    void CurveChecks(const std::shared_ptr<Model>& model)
    {
        const auto values = std::make_shared<const std::vector<float>>(
            std::initializer_list<float>{0, 10, 20, 30});
        const auto angles = std::make_shared<const std::vector<float>>(
            std::initializer_list<float>{3.1F, -3.1F, 0, 0});
        const auto group = std::make_shared<NodeAnimationGroup>(RawNodeAnimationGroup{},
            values, angles, values, std::make_shared<const NodeAnimationDictionary>());
        const auto sample = [&](const auto& curve, int frame, bool rotation = false)
        { return model->InterpolateAnimation(curve, 0, frame, 1, 4, 4, rotation); };
        {
            const PreviewNodeInterpolation scope(*model, group.get(), 0, 1, 0.25F);
            Require(std::abs(sample(values, 0) - 2.5F) < 0.00001F, "fractional translation");
            Require(sample(values, 1) == 10, "unrelated frame was interpolated");
            Require(std::abs(sample(angles, 0, true) - (3.1F +
                (2 * std::numbers::pi_v<float> - 6.2F) * 0.25F)) < 0.00001F, "rotation long arc");
            const auto materialCurve = std::make_shared<const std::vector<float>>(*values);
            Require(sample(materialCurve, 0) == 0, "material curve changed");
            const auto other = Fixture();
            Require(other->InterpolateAnimation(values, 0, 0, 1, 4, 4) == 0, "other model changed");
            try
            {
                const PreviewNodeInterpolation nested(*model, group.get(), 3, 0, 0.5F);
                Require(std::abs(sample(values, 3) - 15) < 0.00001F, "loop seam interpolation");
                (void)model->InterpolateAnimation(values, 100, 3, 1, 4, 4);
            }
            catch (const std::out_of_range&) {}
            Require(std::abs(sample(values, 0) - 2.5F) < 0.00001F, "exception leaked sampling scope");
        }
        Require(sample(values, 0) == 0, "preview interpolation leaked into normal sampling");
        // Compressed LUT: native frames 0,2 and the explicit tail 3.
        const auto compressed = std::make_shared<const std::vector<float>>(
            std::initializer_list<float>{0, 20, 30});
        const auto compressedGroup = std::make_shared<NodeAnimationGroup>(RawNodeAnimationGroup{},
            compressed, angles, values, std::make_shared<const NodeAnimationDictionary>());
        const PreviewNodeInterpolation scope(*model, compressedGroup.get(), 1, 2, 0.5F);
        Require(model->InterpolateAnimation(compressed, 0, 1, 2, 3, 4) == 15,
            "compressed LUT decoder changed");
        std::cout << "PASS fractional curves, shortest rotation, loop seam, compressed LUT and draw isolation\n";
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
            const auto& info = *instance->AnimInfo;
            const auto group = info.Node->Group;
            Require(group && group->FrameCount > 0, "missing hunter idle nodes");
            PreviewAnimation clock;
            bool changed = false;
            std::vector<OpenTK::Mathematics::Matrix4> previous;
            for (int frame = 0; frame < 144 * 3; ++frame)
            {
                clock.Advance(*instance, 1.0 / 144);
                model.ComputeNodeMatrices(0);
                const PreviewNodeInterpolation scope(model, group.get(), info.NodeFrame(),
                    PreviewAnimation::NextNodeFrame(*instance), clock.Fraction());
                model.AnimateNodes(0, true, OpenTK::Mathematics::IdentityMatrix(), model.Scale, instance->AnimInfo);
                model.UpdateMatrixStack();
                for (std::size_t i = 0; i < model.Nodes->size(); ++i)
                {
                    const auto& matrix = model.Nodes->at(i)->Animation;
                    Require(std::isfinite(matrix.M11) && std::isfinite(matrix.M41), "invalid hunter pose");
                    if (!previous.empty() && clock.Fraction() > 0.1F
                        && std::abs(matrix.M11 - previous[i].M11) > 1e-7F) changed = true;
                }
                previous.clear();
                for (const auto& node : *model.Nodes) previous.push_back(node->Animation);
            }
            Require(changed, "hunter has no intermediate poses");
            std::cout << "PASS real hunter 144 Hz pose evaluation: " << static_cast<int>(hunter) << '\n';
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        const auto model = Fixture();
        ClockChecks(model);
        CurveChecks(model);
        if (argc > 1) AssetChecks(argv[1]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
