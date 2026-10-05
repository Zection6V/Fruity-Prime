#pragma once
#include <memory>
#include <string>
namespace MphRead::Mods::Diagnostics
{
    // Opt-in diagnostic only. Sampling and symbol work never run on a normal FPS check.
    class CpuSampling final
    {
    public:
        static std::unique_ptr<CpuSampling> Create(const std::string& reportPath);
        ~CpuSampling();
        void SetActive(bool active) noexcept;
    private:
        struct State;
        explicit CpuSampling(std::unique_ptr<State> state);
        std::unique_ptr<State> _state;
    };
}
