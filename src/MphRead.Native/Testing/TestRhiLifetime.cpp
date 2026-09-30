#include "../NativeRuntime/Rhi/FrameContext.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Phase 10's lifetime contract, checked without a device: a retired object
// survives until the frame it was last used in is complete, a re-used object
// can be taken back, and a wait for idle destroys everything.
namespace
{
    using namespace MphRead::NativeRuntime::Rhi;

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

    void TestRetiredObjectsWaitForTheirFrame()
    {
        RetirementQueue<int> queue;
        std::vector<int> destroyed;
        const auto destroy = [&destroyed](int value) { destroyed.push_back(value); };
        queue.Retire(1, 5);
        queue.Retire(2, 6);
        queue.Retire(3, 7);
        Expect(queue.Collect(4, destroy) == 0 && destroyed.empty(),
            "nothing is destroyed before its frame completes");
        Expect(queue.Collect(6, destroy) == 2, "frames 5 and 6 complete");
        Expect(destroyed == std::vector<int>{1, 2}, "in the order they were retired");
        Expect(queue.Size() == 1, "frame 7's object is still waiting");
        Expect(queue.Collect(7, destroy) == 1 && queue.Size() == 0, "and goes when frame 7 completes");
    }

    void TestFramesInFlightSlots()
    {
        // BeginFrame for frame N may reuse the slot of frame N - FramesInFlight.
        static_assert(FramesInFlight == 2, "the plan's two frames in flight");
        for (std::uint64_t frame = 1; frame < 10; ++frame)
        {
            const auto slot = static_cast<std::uint32_t>(frame % FramesInFlight);
            const auto earlier = static_cast<std::uint32_t>((frame - FramesInFlight + 10) % FramesInFlight);
            Expect(slot == earlier, "a slot is shared by frames FramesInFlight apart");
        }
    }

    void TestCancelledObjectsAreNotDestroyed()
    {
        RetirementQueue<int> queue;
        queue.Retire(10, 1);
        queue.Retire(11, 1);
        queue.Retire(10, 2);
        Expect(queue.Cancel([](int value) { return value == 10; }) == 2, "both entries for 10 are taken back");
        std::vector<int> destroyed;
        (void)queue.CollectAll([&destroyed](int value) { destroyed.push_back(value); });
        Expect(destroyed == std::vector<int>{11}, "only 11 is destroyed");
    }

    void TestIdleDestroysEverything()
    {
        RetirementQueue<int> queue;
        for (int i = 0; i < 100; ++i)
        {
            queue.Retire(i, static_cast<std::uint64_t>(1000 + i));
        }
        std::size_t count = 0;
        Expect(queue.CollectAll([&count](int) { ++count; }) == 100 && count == 100 && queue.Size() == 0,
            "waiting for idle destroys every retired object");
    }
}

int main()
{
    try
    {
        TestRetiredObjectsWaitForTheirFrame();
        TestFramesInFlightSlots();
        TestCancelledObjectsAreNotDestroyed();
        TestIdleDestroysEverything();
        std::cout << "RhiLifetime tests passed.\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "RhiLifetime test failure: " << ex.what() << '\n';
        return 1;
    }
}
