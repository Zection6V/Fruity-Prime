#include "../NativeRuntime/FrameTelemetry.hpp"
#include <iostream>
#include <new>
#include <stdexcept>

using namespace MphRead::NativeRuntime::FrameTelemetry;
namespace
{
    void Expect(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
    void Check()
    {
        Expect(HasNewCounters(), "Allocation diagnostic was not linked.");
        Session session(128);
        for (unsigned i = 1; i <= 128; ++i)
        {
            Frame frame;
            Eligible(true);
            Scope scope(Phase::DrawScene);
            auto* memory = ::operator new(17);
            ::operator delete(memory);
            Count(Counter::QueueSubmits);
            Rendered();
        }
        const auto& stats = session.Statistics();
        const auto& draw = stats.Phases[static_cast<std::size_t>(Phase::DrawScene)];
        Expect(stats.Frames == 127 && stats.TimedFrames == 1 && draw.Calls == 127 && draw.TimedCalls == 1,
            "Sampling cadence or newly eligible frame accounting differs.");
        Expect(stats.NewCalls == 127 && stats.NewBytes == 127 * 17 && draw.NewCalls == 127 && draw.NewBytes == 127 * 17,
            "Application new telemetry lost counts/bytes or included a warmup frame.");
        Expect(stats.Counters[static_cast<std::size_t>(Counter::QueueSubmits)] == 127, "API counters differ.");
        {
            Frame frame; Scope parent(Phase::SceneRender);
            Scope child(Phase::DrawScene);
            auto* memory = ::operator new(64, std::align_val_t{64});
            Expect(reinterpret_cast<std::uintptr_t>(memory) % 64 == 0, "Diagnostic new lost over-alignment.");
            ::operator delete(memory, std::align_val_t{64});
            Rendered();
        }
        Expect(stats.NewCalls == 128 && draw.NewCalls == 128
            && stats.Phases[static_cast<std::size_t>(Phase::SceneRender)].NewCalls == 1,
            "Nested inclusive allocation scopes double counted the frame.");
        {
            Frame frame; Scope scope(Phase::DrawScene);
            auto* memory = ::operator new(1, std::nothrow); ::operator delete(memory);
            Eligible(false); Rendered();
        }
        Expect(stats.Frames == 128 && stats.NewCalls == 128, "Mid-frame condition change was accepted.");
        { Frame frame; Eligible(true); Rendered(); }
        { Frame frame; Scope scope(Phase::DrawScene); Count(Counter::DeviceIdle); }
        Expect(stats.Frames == 128 && stats.Counters[static_cast<std::size_t>(Counter::DeviceIdle)] == 0,
            "An unpresented frame was committed.");
        Expect(Current == &session, "Telemetry session lost its thread owner.");
    }
    void Check256()
    {
        Session session(256); Eligible(true);
        for (unsigned i = 0; i < 512; ++i)
        { Frame frame; Scope scope(Phase::Uniform); Rendered(); }
        const auto& stats = session.Statistics();
        const auto& uniforms = stats.Phases[static_cast<std::size_t>(Phase::Uniform)];
        Expect(stats.Frames == 512 && stats.TimedFrames == 2 && uniforms.Calls == 512 && uniforms.TimedCalls == 2,
            "The 1/256 cadence differs or phase scopes allocated.");
        Expect(!stats.NewCalls, "Empty measured phase scopes allocated on the heap.");
    }
    void CheckWaits()
    {
        Session session(128); Eligible(true);
        {
            Frame frame;
            Expect(HostWait([] { return 19; }) == 19, "Wait telemetry changed an API result.");
            bool called = false; HostWait([&] { called = true; });
            Expect(called, "Void wait operation was lost."); Rendered();
        }
        const auto& stats = session.Statistics();
        Expect(stats.Counters[static_cast<std::size_t>(Counter::HostWaits)] == 2 && !stats.NewCalls,
            "Wait telemetry allocated or lost a native call.");
    }
}
int main()
{
    try { Check(); Check256(); CheckWaits(); Expect(Current == nullptr, "Late scopes retained a released session.");
        std::cout << "Frame telemetry PASS; 1/128 and 1/256 timing; warmup/context/unpresented filtering; nested new counts; alignment; wait results; owner release\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
