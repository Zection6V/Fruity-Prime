#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

namespace MphRead::NativeRuntime::FrameTelemetry
{
    enum class Phase : std::uint8_t
    { Admission, Events, Simulation, Traversal, SceneRender, DrawScene, Uniform,
      Material, Descriptor, Commands, Submit, Acquire, Present, Ui, Count };
    enum class Counter : std::uint8_t
    { UniformSlots, UniformNames, DescriptorLookup, DescriptorHit, DescriptorMiss,
      PersistentLookup, VectorGrowth, GlIntegerQueries, GlUniformLocations,
      GlUniformWrites, VaoBinds, VaoSuppressed, GlCurrentProgramQueries, GlMemoryQueries, NativeDescriptorCalls, NativeDescriptorSets,
      FrameDescriptors, DrawDescriptors, MaterialDescriptors, PassDescriptors,
      HostWaits, HostWaitNanoseconds, QueueSubmits, EventPumps, DeviceIdle, Count };
    struct PhaseTotals final
    { std::uint64_t Calls = 0, TimedCalls = 0, Nanoseconds = 0, NewCalls = 0, NewBytes = 0; };
    struct Totals final
    {
        std::uint64_t Frames = 0, TimedFrames = 0, NewCalls = 0, NewBytes = 0;
        std::array<PhaseTotals, static_cast<std::size_t>(Phase::Count)> Phases{};
        std::array<std::uint64_t, static_cast<std::size_t>(Counter::Count)> Counters{};
    };
    class Session;
    inline thread_local Session* Current = nullptr;
    [[nodiscard]] inline Session* Active() noexcept
    {
#if defined(FRUITY_PERF_TELEMETRY)
        return Current;
#else
        return nullptr;
#endif
    }
    // False in ordinary builds. The optional diagnostic build intercepts only
    // application C++ new; malloc, foreign DLLs and driver internals are outside
    // that evidence. Native allocation API calls are counted separately.
    [[nodiscard]] bool HasNewCounters() noexcept;

    class Session final
    {
    public:
        explicit Session(std::uint32_t interval, std::string output = {});
        ~Session();
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        void BeginFrame() noexcept;
        void EndFrame() noexcept;
        void Eligible(bool value) noexcept { _eligible = value; }
        void Rendered() noexcept { _rendered = true; }
        [[nodiscard]] const Totals& Statistics() const noexcept { return _total; }
        [[nodiscard]] bool Collecting() const noexcept { return _frameOpen && _eligibleAtStart && _eligible; }
        [[nodiscard]] bool Timing() const noexcept { return Collecting() && _timing; }
        void Count(Counter counter, std::uint64_t amount = 1) noexcept
        { if (Collecting()) _frame.Counters[static_cast<std::size_t>(counter)] += amount; }
        void New(std::size_t bytes) noexcept
        {
            if (!Collecting()) return;
            ++_frame.NewCalls; _frame.NewBytes += bytes;
            for (std::size_t i = 0; i < _depth; ++i)
            { auto& phase = _frame.Phases[static_cast<std::size_t>(_stack[i])]; ++phase.NewCalls; phase.NewBytes += bytes; }
        }
        bool Enter(Phase phase) noexcept
        {
            if (!Collecting() || _depth == _stack.size()) return false;
            _stack[_depth++] = phase;
            ++_frame.Phases[static_cast<std::size_t>(phase)].Calls;
            return true;
        }
        void Leave(Phase phase, bool timed, std::uint64_t nanoseconds) noexcept
        {
            if (_depth) --_depth;
            if (timed)
            { auto& total = _frame.Phases[static_cast<std::size_t>(phase)]; ++total.TimedCalls; total.Nanoseconds += nanoseconds; }
        }
    private:
        std::uint32_t _interval;
        std::uint64_t _sequence = 0;
        std::string _output;
        bool _frameOpen = false, _eligible = false, _eligibleAtStart = false, _rendered = false, _timing = false;
        std::array<Phase, 32> _stack{};
        std::size_t _depth = 0;
        Totals _frame{}, _total{};
    };
    [[nodiscard]] std::unique_ptr<Session> Create(const std::string& fpsPath);
    inline void Count(Counter counter, std::uint64_t amount = 1) noexcept
    { if (const auto session = Active()) session->Count(counter, amount); }
    inline void New(std::size_t bytes) noexcept { if (const auto session = Active()) session->New(bytes); }
    inline void Eligible(bool value) noexcept { if (const auto session = Active()) session->Eligible(value); }
    inline void Rendered() noexcept { if (const auto session = Active()) session->Rendered(); }
    template<class Action> auto HostWait(Action action) -> decltype(action())
    {
        const auto session = Active();
        const auto collecting = session && session->Collecting();
        const auto start = collecting ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        Count(Counter::HostWaits);
        const auto record = [&] {
            if (collecting) Count(Counter::HostWaitNanoseconds,
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
        };
        if constexpr (std::is_void_v<decltype(action())>) { action(); record(); }
        else { const auto result = action(); record(); return result; }
    }

    class Frame final
    {
    public:
        Frame() noexcept : _session(Active()) { if (_session) _session->BeginFrame(); }
        ~Frame() { if (_session) _session->EndFrame(); }
        Frame(const Frame&) = delete;
        Frame& operator=(const Frame&) = delete;
    private:
        Session* _session;
    };
    class Scope final
    {
    public:
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        explicit Scope(Phase phase) noexcept : _phase(phase), _session(Active())
        {
            if (!_session || !_session->Enter(phase)) { _session = nullptr; return; }
            _timed = _session->Timing();
            if (_timed) _start = std::chrono::steady_clock::now();
        }
        ~Scope()
        {
            if (!_session) return;
            const auto elapsed = _timed ? std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - _start).count() : 0;
            _session->Leave(_phase, _timed, elapsed);
        }
    private:
        Phase _phase;
        Session* _session;
        bool _timed = false;
        std::chrono::steady_clock::time_point _start{};
    };
}
