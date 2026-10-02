#include "../NativeRuntime/Rhi/FrameContext.hpp"
#include "../NativeRuntime/Rhi/Swapchain.hpp"
#include "../NativeRuntime/Rhi/OpenGL/OpenGlFrameScheduler.hpp"
#include <unordered_set>

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Actual completion tokens control retirement; frame slots are independent.
// Fake native dispatch tests fence failures, timeout and completion ordering.
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

    void TestRetiredObjectsWaitForCompletion()
    {
        RetirementQueue<int> queue;
        std::vector<int> destroyed;
        const auto destroy = [&destroyed](int value) { destroyed.push_back(value); };
        queue.Retire(1, {5});
        queue.Retire(2, {6});
        queue.Retire(3, {7});
        Expect(queue.Collect({4}, destroy) == 0 && destroyed.empty(),
            "nothing is destroyed before its submission completes");
        Expect(queue.Collect({6}, destroy) == 2, "submissions 5 and 6 complete");
        Expect(destroyed == std::vector<int>{1, 2}, "in the order they were retired");
        Expect(queue.Size() == 1, "submission 7's object is still waiting");
        Expect(queue.Collect({7}, destroy) == 1 && queue.Size() == 0, "and goes when submission 7 completes");
    }

    struct FakeGlStream final
    {
        using Scheduler = OpenGL::OpenGlFrameScheduler;
        std::uintptr_t Accepted = 0, Completed = 0;
        std::unordered_set<std::uintptr_t> Live;
        unsigned Finishes = 0, Flushes = 0, BlockingWaits = 0;
        bool Refuse = false, WaitFails = false, TimeoutOnce = false;
        unsigned NativeError = 0x0502;
        Scheduler::Dispatch Dispatch(bool sync = true)
        {
            return {this,
                [](void* context) -> void* {
                    auto& state = *static_cast<FakeGlStream*>(context);
                    if (state.Refuse) return nullptr;
                    state.Live.insert(++state.Accepted);
                    return reinterpret_cast<void*>(state.Accepted);
                },
                [](void* context, void* fence, bool flush, std::uint64_t timeout) {
                    auto& state = *static_cast<FakeGlStream*>(context);
                    const auto token = reinterpret_cast<std::uintptr_t>(fence);
                    Expect(state.Live.contains(token), "wait uses a live native fence");
                    if (state.WaitFails) return Scheduler::WaitStatus::Failed;
                    if (token <= state.Completed) return Scheduler::WaitStatus::AlreadySignaled;
                    if (!timeout) return Scheduler::WaitStatus::Timeout;
                    Expect(flush, "a blocking wait submits buffered commands");
                    ++state.BlockingWaits;
                    if (state.TimeoutOnce) { state.TimeoutOnce = false; return Scheduler::WaitStatus::Timeout; }
                    state.Completed = token;
                    return Scheduler::WaitStatus::Satisfied;
                },
                [](void* context, void* fence) {
                    Expect(static_cast<FakeGlStream*>(context)->Live.erase(reinterpret_cast<std::uintptr_t>(fence)) == 1,
                        "native fence deleted exactly once");
                },
                [](void* context) { ++static_cast<FakeGlStream*>(context)->Flushes; },
                [](void* context) { auto& state = *static_cast<FakeGlStream*>(context); ++state.Finishes; state.Completed = state.Accepted; },
                [](void* context) { return static_cast<FakeGlStream*>(context)->NativeError; }, sync};
        }
    };

    void TestGlNativeSubmissionProof()
    {
        FakeGlStream stream;
        FakeGlStream::Scheduler scheduler(stream.Dispatch());
        RetirementQueue<int> queue;
        std::vector<int> destroyed;
        const auto destroy = [&destroyed](int value) { destroyed.push_back(value); };
        const auto first = scheduler.Submit();
        const auto second = scheduler.Submit(true);
        queue.Retire(1, first); queue.Retire(2, second);
        queue.Collect(scheduler.Poll(), destroy);
        Expect(destroyed.empty() && stream.Flushes == 1 && scheduler.Completed() == SubmissionSerial{},
            "accepted or flushed work is not proof of completion");
        stream.Refuse = true; stream.NativeError = 0x0505;
        bool failed = false;
        try { (void)scheduler.Submit(); }
        catch (const BackendError& error) { failed = error.Kind() == BackendErrorKind::OutOfMemory && error.NativeCode() == 0x0505; }
        Expect(failed && scheduler.Submitted() == second && stream.Live.size() == 2,
            "failed fence insertion consumes no serial and keeps existing fences");
        stream.Refuse = false;
        stream.Completed = 1;
        queue.Collect(scheduler.Poll(), destroy);
        Expect(destroyed == std::vector<int>{1} && queue.Size() == 1 && stream.Live.size() == 1,
            "only actual completed native work is collected");
        const auto third = scheduler.Submit();
        Expect(third == SubmissionSerial{3}, "retry has no submission gap");
        stream.TimeoutOnce = true;
        scheduler.Wait(third);
        queue.Collect(scheduler.Completed(), destroy);
        Expect(destroyed == std::vector<int>{1, 2} && stream.Live.empty()
            && stream.BlockingWaits == 2 && scheduler.DeviceWideWaits() == 0,
            "slot wait retries timeout without finishing the whole device");
        const auto fourth = scheduler.Submit();
        stream.WaitFails = true; stream.NativeError = 0x0502; failed = false;
        try { (void)scheduler.Poll(); }
        catch (const BackendError& error) { failed = error.NativeCode() == 0x0502; }
        Expect(failed && scheduler.Completed() == third && stream.Live.size() == 1,
            "WAIT_FAILED must not establish completion or discard a live fence");
        scheduler.Finish();
        Expect(scheduler.Completed() == fourth && scheduler.DeviceWideWaits() == 1 && stream.Live.empty(),
            "explicit idle establishes final completion and drains native fences");
    }

    void TestLegacyGlCompletionFallback()
    {
        FakeGlStream stream;
        FakeGlStream::Scheduler scheduler(stream.Dispatch(false));
        const auto token = scheduler.Submit();
        Expect(token == SubmissionSerial{1} && scheduler.Completed() == token
            && stream.Finishes == 1 && stream.Accepted == 0,
            "unsupported legacy sync uses real synchronous completion, never a frame counter");
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
        queue.Retire(10, {1});
        queue.Retire(11, {1});
        queue.Retire(10, {2});
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
            queue.Retire(i, {static_cast<std::uint64_t>(1000 + i)});
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
        TestRetiredObjectsWaitForCompletion();
        TestFramesInFlightSlots();
        TestGlNativeSubmissionProof();
        TestLegacyGlCompletionFallback();
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
