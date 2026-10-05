#include "CpuSampling.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#if defined(_WIN32) && defined(_M_X64)
#include <Windows.h>
#include <DbgHelp.h>
#include <array>
#include <atomic>
#include <fstream>
#include <map>
#include <thread>
#include <vector>
#endif
namespace MphRead::Mods::Diagnostics
{
#if defined(_WIN32) && defined(_M_X64)
    struct CpuSampling::State final
    {
        struct Sample { std::array<DWORD64, 48> addresses{}; std::size_t count = 0; };
        std::vector<Sample> samples{16384};
        std::atomic<bool> active{false};
        std::size_t count = 0;
        HANDLE mainThread = nullptr, stop = nullptr;
        DWORD64 stackLow = 0, stackHigh = 0;
        std::string path;
        std::thread worker;
        ~State()
        {
            active.store(false, std::memory_order_release);
            if (stop) SetEvent(stop);
            if (worker.joinable()) worker.join();
            if (mainThread) CloseHandle(mainThread);
            if (stop) CloseHandle(stop);
        }
        void Capture() noexcept
        {
            if (SuspendThread(mainThread) == DWORD(-1)) return;
            // No heap allocation, symbol loading or logging while the owner is suspended.
            struct Resume final { HANDLE thread; ~Resume() { ResumeThread(thread); } } resume{mainThread};
            CONTEXT context{}; context.ContextFlags = CONTEXT_FULL;
            if (!GetThreadContext(mainThread, &context)) return;
            auto& sample = samples[count];
            while (sample.count < sample.addresses.size() && context.Rip
                && context.Rsp >= stackLow && context.Rsp <= stackHigh - sizeof(DWORD64))
            {
                sample.addresses[sample.count++] = context.Rip;
                DWORD64 imageBase = 0;
                const auto function = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
                const auto previousStack = context.Rsp;
                if (function)
                {
                    PVOID handlerData = nullptr; DWORD64 establisher = 0;
                    RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function,
                        &context, &handlerData, &establisher, nullptr);
                }
                else
                {
                    context.Rip = *reinterpret_cast<const DWORD64*>(context.Rsp);
                    context.Rsp += sizeof(DWORD64);
                }
                if (context.Rsp <= previousStack) break;
            }
            if (sample.count) ++count;
        }
        void Report()
        {
            active.store(false, std::memory_order_release); SetEvent(stop); worker.join();
            // DbgHelp is single-threaded; resolve only after the worker has stopped.
            const HANDLE process = GetCurrentProcess();
            SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
            const bool symbols = SymInitialize(process, nullptr, TRUE) != FALSE;
            struct Symbols final
            { HANDLE Process; bool Loaded; ~Symbols() { if (Loaded) SymCleanup(Process); } } cleanup{process, symbols};
            std::map<DWORD64, std::pair<std::size_t, std::size_t>> counts;
            for (std::size_t i = 0; i < count; ++i)
                for (std::size_t depth = 0; depth < samples[i].count; ++depth)
                { auto& value = counts[samples[i].addresses[depth]]; ++value.second; if (!depth) ++value.first; }
            std::ofstream out(path);
            if (!out) throw std::runtime_error("Cannot open the CPU sampling report.");
            out << "samples=" << count << "; timer=1ms requested; inclusive frames may overlap\n"
                << "exclusive,inclusive,address,module,symbol\n";
            for (const auto& [address, value] : counts)
            {
                alignas(SYMBOL_INFO) std::array<char, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> storage{};
                auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage.data());
                symbol->SizeOfStruct = sizeof(SYMBOL_INFO); symbol->MaxNameLen = MAX_SYM_NAME;
                DWORD64 displacement = 0;
                IMAGEHLP_MODULE64 module{}; module.SizeOfStruct = sizeof(module);
                const bool named = symbols && SymFromAddr(process, address, &displacement, symbol);
                const bool located = symbols && SymGetModuleInfo64(process, address, &module);
                out << value.first << ',' << value.second << ",0x" << std::hex << address << std::dec
                    << ',' << (located ? module.ModuleName : "unknown") << ",\"";
                // Demangled template names contain commas; keep one CSV field.
                for (const char* character = named ? symbol->Name : "unknown"; *character; ++character)
                { if (*character == '"') out << '"'; out << *character; }
                out << '+' << displacement << "\"\n";
            }
            std::cout << "[cpu sampling] samples=" << count << "; report=" << path << '\n';
        }
    };
    std::unique_ptr<CpuSampling> CpuSampling::Create(const std::string& path)
    {
        const char* enabled = std::getenv("FRUITY_CPU_SAMPLING");
        if (!enabled || std::string(enabled) != "1") return {};
        auto state = std::make_unique<State>(); state->path = path;
        const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
        state->stackLow = reinterpret_cast<DWORD64>(tib->StackLimit);
        state->stackHigh = reinterpret_cast<DWORD64>(tib->StackBase);
        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
            &state->mainThread, THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, 0))
            throw std::runtime_error("Cannot obtain the CPU sampling thread handle.");
        state->stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!state->stop) throw std::runtime_error("Cannot create the CPU sampling stop event.");
        auto* target = state.get();
        state->worker = std::thread([target] {
            while (WaitForSingleObject(target->stop, 1) == WAIT_TIMEOUT)
                if (target->active.load(std::memory_order_acquire) && target->count < target->samples.size())
                    target->Capture();
        });
        return std::unique_ptr<CpuSampling>(new CpuSampling(std::move(state)));
    }
    void CpuSampling::SetActive(bool active) noexcept { _state->active.store(active, std::memory_order_release); }
    CpuSampling::~CpuSampling()
    {
        try { _state->Report(); }
        catch (const std::exception& error) { std::cerr << "[cpu sampling] report failed: " << error.what() << '\n'; }
    }
#else
    struct CpuSampling::State {};
    std::unique_ptr<CpuSampling> CpuSampling::Create(const std::string&) { return {}; }
    void CpuSampling::SetActive(bool) noexcept {}
    CpuSampling::~CpuSampling() = default;
#endif
    CpuSampling::CpuSampling(std::unique_ptr<State> state) : _state(std::move(state)) {}
}
