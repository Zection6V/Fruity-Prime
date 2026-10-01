#include "../NativeRuntime/Rhi/FrameContext.hpp"
#include "../NativeRuntime/Rhi/Swapchain.hpp"

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

    void TestPresentationFailuresStayTyped()
    {
        const BackendError lost(GraphicsBackend::Vulkan, BackendErrorKind::DeviceLost, -4, "device lost");
        Expect(PresentationFailure(lost) == PresentationStatus::DeviceLost, "device loss status");
        Expect(lost.Backend() == GraphicsBackend::Vulkan && lost.NativeCode() == -4,
            "native diagnostic information is preserved");
        const BackendError surface(GraphicsBackend::OpenGl, BackendErrorKind::SurfaceLost, 0, "surface lost");
        Expect(PresentationFailure(surface) == PresentationStatus::SurfaceLost, "surface loss status");
        bool propagated = false;
        try
        {
            (void)PresentationFailure(BackendError(GraphicsBackend::Vulkan,
                BackendErrorKind::OutOfMemory, -2, "allocation failed"));
        }
        catch (const BackendError& error) { propagated = error.Kind() == BackendErrorKind::OutOfMemory; }
        Expect(propagated, "allocation errors must not masquerade as surface unavailability");
    }

    void TestSubmissionCompletionIsIndependentOfFrames()
    {
        SubmissionProgress progress;
        RetirementQueue<int> queue;
        std::vector<int> destroyed;
        const auto destroy = [&destroyed](int value) { destroyed.push_back(value); };
        // A failed native submit does not consume a serial.
        Expect(progress.Next() == SubmissionSerial{1} && progress.Next() == SubmissionSerial{1},
            "only accepted submissions advance progress");
        for (int submit = 1; submit <= 4; ++submit) progress.Submitted(progress.Next());
        queue.Retire(40, SubmissionSerial{4});
        queue.Retire(20, SubmissionSerial{2});
        progress.Complete({2});
        queue.Collect(progress.Completed(), destroy);
        Expect(destroyed == std::vector<int>{20} && queue.Size() == 1,
            "collection uses actual completion even with unordered retirement");
        progress.Complete({1});
        Expect(progress.Completed() == SubmissionSerial{2}, "stale observations cannot regress completion");
        bool rejected = false;
        try { progress.Complete({5}); } catch (const std::logic_error&) { rejected = true; }
        Expect(rejected && progress.Completed() == SubmissionSerial{2}, "future completion rejected");
        rejected = false;
        try { progress.Submitted({6}); } catch (const std::logic_error&) { rejected = true; }
        Expect(rejected && progress.Submitted() == SubmissionSerial{4}, "submission gaps rejected");
        progress.Complete({4});
        queue.Collect(progress.Completed(), destroy);
        Expect(destroyed == std::vector<int>{20, 40} && queue.Size() == 0,
            "last use survives until its submission completes");
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
        TestPresentationFailuresStayTyped();
        TestSubmissionCompletionIsIndependentOfFrames();
        std::cout << "RhiLifetime tests passed.\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "RhiLifetime test failure: " << ex.what() << '\n';
        return 1;
    }
}
