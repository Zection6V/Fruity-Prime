#include "../RendererGpuMesh.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using namespace MphRead;

    [[noreturn]] void Fail(std::string_view message)
    {
        throw std::runtime_error(std::string(message));
    }

    void Expect(bool value, std::string_view message)
    {
        if (!value)
        {
            Fail(message);
        }
    }

    class FakeGpuMesh final : public GpuMeshResource
    {
    public:
        FakeGpuMesh(std::int32_t& destroyed, std::int32_t& drawn)
            : _destroyed(destroyed), _drawn(drawn)
        {
        }

        ~FakeGpuMesh() override
        {
            ++_destroyed;
        }

        void Draw() override
        {
            ++_drawn;
        }

    private:
        std::int32_t& _destroyed;
        std::int32_t& _drawn;
    };

    void TestDrawPlanPreservesRanges()
    {
        RendererGeometry geometry{};
        geometry.Indices.resize(18);
        geometry.Ranges = {
            {ScenePrimitiveTopology::Triangles, 0, 3},
            {ScenePrimitiveTopology::Quads, 3, 4},
            {ScenePrimitiveTopology::TriangleStrip, 7, 5},
            {ScenePrimitiveTopology::QuadStrip, 12, 6}
        };

        const GpuMeshDrawPlan plan = BuildGpuMeshDrawPlan(geometry);
        Expect(plan.IndexCount == 18, "draw plan index count");
        Expect(plan.Ranges.size() == 4, "draw plan range count");
        Expect(plan.Ranges[0].Topology == ScenePrimitiveTopology::Triangles,
            "triangle topology");
        Expect(plan.Ranges[1].Topology == ScenePrimitiveTopology::Quads,
            "quad topology");
        Expect(plan.Ranges[2].Topology == ScenePrimitiveTopology::TriangleStrip,
            "triangle strip topology");
        Expect(plan.Ranges[3].Topology == ScenePrimitiveTopology::QuadStrip,
            "quad strip topology");
        Expect(plan.Ranges[0].FirstIndex == 0 && plan.Ranges[0].IndexCount == 3
            && plan.Ranges[0].IndexByteOffset == 0, "range zero");
        Expect(plan.Ranges[1].FirstIndex == 3 && plan.Ranges[1].IndexCount == 4
            && plan.Ranges[1].IndexByteOffset == 3 * sizeof(std::uint32_t), "range one");
        Expect(plan.Ranges[2].FirstIndex == 7 && plan.Ranges[2].IndexCount == 5
            && plan.Ranges[2].IndexByteOffset == 7 * sizeof(std::uint32_t), "range two");
        Expect(plan.Ranges[3].FirstIndex == 12 && plan.Ranges[3].IndexCount == 6
            && plan.Ranges[3].IndexByteOffset == 12 * sizeof(std::uint32_t), "range three");
    }

    void TestCacheIdentityAndLifetime()
    {
        GpuMeshCache cache{};
        auto modelA = std::make_shared<std::int32_t>(1);
        auto modelB = std::make_shared<std::int32_t>(2);
        std::shared_ptr<const void> lifetimeA = modelA;
        std::shared_ptr<const void> lifetimeB = modelB;
        std::int32_t destroyed = 0;
        std::int32_t drawn = 0;
        std::int32_t factoryCalls = 0;

        auto make = [&]()
        {
            ++factoryCalls;
            return std::make_shared<FakeGpuMesh>(destroyed, drawn);
        };

        std::weak_ptr<GpuMeshResource> firstWeak;
        {
            const auto first = cache.GetOrCreate(lifetimeA, 7, make);
            firstWeak = first;
            const auto again = cache.GetOrCreate(lifetimeA, 7, make);
            Expect(first == again, "same live model and geometry identity must hit cache");
            Expect(factoryCalls == 1, "cache hit must not rebuild resource");
            first->Draw();
            Expect(drawn == 1, "cached resource draw");
        }

        {
            const auto secondMesh = cache.GetOrCreate(lifetimeA, 8, make);
            const auto secondModel = cache.GetOrCreate(lifetimeB, 7, make);
            Expect(secondMesh != cache.Find(modelA.get(), 7),
                "different geometry identity must not alias");
            Expect(secondModel != cache.Find(modelA.get(), 7),
                "different model identity must not alias");
            Expect(cache.Size() == 3, "three cache identities");
        }

        cache.EraseModel(modelA.get());
        Expect(cache.Size() == 1, "erase model removes all of its mesh entries");
        Expect(firstWeak.expired(), "erased model releases its GPU resource");
        Expect(destroyed == 2, "model erase destroys each owned resource once");

        modelB.reset();
        Expect(cache.Size() == 1, "separate lifetime token keeps model identity live");
        lifetimeB.reset();
        cache.PruneExpired();
        Expect(cache.Size() == 0, "expired model is pruned");
        Expect(destroyed == 3, "expired model releases resource once");

        auto modelC = std::make_shared<std::int32_t>(3);
        std::shared_ptr<const void> lifetimeC = modelC;
        (void)cache.GetOrCreate(lifetimeC, 1, make);
        Expect(cache.Size() == 1, "cache refill");
        cache.Clear();
        Expect(cache.Size() == 0, "cache clear");
        Expect(destroyed == 4, "clear releases resource exactly once");
    }
}

int main()
{
    try
    {
        TestDrawPlanPreservesRanges();
        TestCacheIdentityAndLifetime();
        std::cout << "RendererGpuMesh tests passed.\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "RendererGpuMesh test failure: " << ex.what() << '\n';
        return 1;
    }
}
