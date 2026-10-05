#include "FrameTelemetry.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <locale>
#include <stdexcept>

#if defined(FRUITY_NEW_TELEMETRY)
#include <new>
#if defined(_WIN32)
#include <malloc.h>
#endif
namespace
{
    void* AllocateNew(std::size_t size, std::size_t alignment = 0)
    {
        const auto actual = size ? size : 1;
        for (;;)
        {
            void* memory = nullptr;
            if (!alignment) memory = std::malloc(actual);
            else
            {
#if defined(_WIN32)
                memory = _aligned_malloc(actual, alignment);
#else
                if (posix_memalign(&memory, std::max(alignment, sizeof(void*)), actual)) memory = nullptr;
#endif
            }
            if (memory) { MphRead::NativeRuntime::FrameTelemetry::New(size); return memory; }
            const auto handler = std::get_new_handler();
            if (!handler) throw std::bad_alloc();
            handler();
        }
    }
    void DeleteAligned(void* memory) noexcept
    {
#if defined(_WIN32)
        _aligned_free(memory);
#else
        std::free(memory);
#endif
    }
}
void* operator new(std::size_t size) { return AllocateNew(size); }
void* operator new[](std::size_t size) { return AllocateNew(size); }
void* operator new(std::size_t size, std::align_val_t align) { return AllocateNew(size, static_cast<std::size_t>(align)); }
void* operator new[](std::size_t size, std::align_val_t align) { return AllocateNew(size, static_cast<std::size_t>(align)); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { try { return AllocateNew(size); } catch (...) { return nullptr; } }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { try { return AllocateNew(size); } catch (...) { return nullptr; } }
void* operator new(std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept { try { return AllocateNew(size, static_cast<std::size_t>(align)); } catch (...) { return nullptr; } }
void* operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept { try { return AllocateNew(size, static_cast<std::size_t>(align)); } catch (...) { return nullptr; } }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete(void* memory, std::align_val_t) noexcept { DeleteAligned(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { DeleteAligned(memory); }
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept { DeleteAligned(memory); }
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept { DeleteAligned(memory); }
void operator delete(void* memory, std::align_val_t, const std::nothrow_t&) noexcept { DeleteAligned(memory); }
void operator delete[](void* memory, std::align_val_t, const std::nothrow_t&) noexcept { DeleteAligned(memory); }
#endif

namespace MphRead::NativeRuntime::FrameTelemetry
{
    bool HasNewCounters() noexcept
    {
#if defined(FRUITY_NEW_TELEMETRY)
        return true;
#else
        return false;
#endif
    }
    Session::Session(std::uint32_t interval, std::string output) : _interval(interval), _output(std::move(output))
    {
        if ((interval != 128 && interval != 256) || Current)
            throw std::invalid_argument("Frame telemetry requires one thread owner and a 128/256-frame interval.");
        Current = this;
    }
    Session::~Session()
    {
        Current = nullptr; // File formatting/IO is outside all measured frames.
        if (_output.empty()) return;
        std::ofstream file(_output, std::ios::out | std::ios::trunc);
        file.imbue(std::locale::classic());
        constexpr std::array phases{"admission", "events", "simulation", "traversal", "scene_render", "draw_scene",
            "uniform", "material", "descriptor", "commands", "submit", "acquire", "present", "ui"};
        constexpr std::array counters{"uniform_slots", "uniform_names", "descriptor_lookup", "descriptor_hit", "descriptor_miss",
            "persistent_lookup", "vector_growth", "gl_integer_queries", "gl_uniform_locations", "gl_uniform_writes", "vao_binds",
            "vao_suppressed", "gl_current_program_queries", "gl_memory_queries",
            "native_descriptor_calls", "native_descriptor_sets", "frame_descriptor_overflow", "draw_descriptor_overflow", "material_descriptor_overflow",
            "pass_descriptor_overflow", "host_waits", "host_wait_nanoseconds", "queue_submits", "event_pumps", "device_idle"};
        static_assert(phases.size() == static_cast<std::size_t>(Phase::Count));
        static_assert(counters.size() == static_cast<std::size_t>(Counter::Count));
        file << "frames," << _total.Frames << "\ntimed_frames," << _total.TimedFrames << "\ninterval," << _interval
            << "\napplication_new_counters," << HasNewCounters() << "\nframe_new_calls," << _total.NewCalls
            << "\nframe_new_bytes," << _total.NewBytes << "\n";
        file << "phase,calls,timed_calls,inclusive_ns,application_new_calls,application_new_bytes\n";
        for (std::size_t i = 0; i < phases.size(); ++i)
        {
            const auto& total = _total.Phases[i];
            file << phases[i] << ',' << total.Calls << ',' << total.TimedCalls << ',' << total.Nanoseconds
                << ',' << total.NewCalls << ',' << total.NewBytes << '\n';
        }
        file << "counter,value\n";
        for (std::size_t i = 0; i < counters.size(); ++i) file << counters[i] << ',' << _total.Counters[i] << '\n';
    }
    void Session::BeginFrame() noexcept
    {
        _frame = {}; _depth = 0; _rendered = false; _frameOpen = true; _eligibleAtStart = _eligible;
        _timing = ++_sequence % _interval == 0;
    }
    void Session::EndFrame() noexcept
    {
        if (Collecting() && _rendered)
        {
            ++_total.Frames; if (_timing) ++_total.TimedFrames;
            _total.NewCalls += _frame.NewCalls; _total.NewBytes += _frame.NewBytes;
            for (std::size_t i = 0; i < _total.Phases.size(); ++i)
            {
                auto& total = _total.Phases[i]; const auto& frame = _frame.Phases[i];
                total.Calls += frame.Calls; total.TimedCalls += frame.TimedCalls; total.Nanoseconds += frame.Nanoseconds;
                total.NewCalls += frame.NewCalls; total.NewBytes += frame.NewBytes;
            }
            for (std::size_t i = 0; i < _total.Counters.size(); ++i) _total.Counters[i] += _frame.Counters[i];
        }
        _frameOpen = false; _depth = 0;
    }
    std::unique_ptr<Session> Create(const std::string& fpsPath)
    {
        const auto* value = std::getenv("FRUITY_PHASE_STATS");
        if (!value) return {};
#if !defined(FRUITY_PERF_TELEMETRY)
        throw std::invalid_argument("Phase telemetry needs a FRUITY_PERF_TELEMETRY diagnostic build.");
#endif
        const std::string interval(value);
        if (interval != "128" && interval != "256")
            throw std::invalid_argument("FRUITY_PHASE_STATS must be 128 or 256.");
        return std::make_unique<Session>(interval == "128" ? 128 : 256, fpsPath + ".phases.csv");
    }
}
