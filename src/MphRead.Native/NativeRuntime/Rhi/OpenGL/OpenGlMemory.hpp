#pragma once

#include "../MemoryBudget.hpp"
#include "../Resources.hpp"
#include <optional>

namespace MphRead::NativeRuntime::Rhi::OpenGL
{
    // Context-thread policy boundary. GL exposes no portable heap budget;
    // NVX is optional, and tracked object sizes are only backing estimates.
    class OpenGlMemory final
    {
    public:
        OpenGlMemory(bool nvx, std::int32_t (*query)(std::int32_t)) : _nvx(nvx), _query(query) {}
        [[nodiscard]] MemoryBudgetSnapshot Snapshot(std::uint64_t reserved) const;
        [[nodiscard]] MemoryTelemetry Telemetry() const noexcept { return _telemetry; }
        void Admit(std::uint64_t bytes, std::uint64_t reserved);
        // Query optional driver budgets once at the first admission in a
        // render frame; conservatively charge every later allocation/orphan.
        // Cold/outside-frame admission and explicit snapshots stay live.
        void BeginFrame() noexcept { _frame = true; _admission.reset(); _admittedBytes = 0; }
        void EndFrame() noexcept { _frame = false; _admission.reset(); _admittedBytes = 0; }
        void CheckNativeResult(std::int32_t error, const char* operation);
        void SetBudgetCeilingForCheck(std::uint64_t bytes) noexcept { _checkCeiling = bytes; _admission.reset(); }
        void Close() noexcept { EndFrame(); _query = nullptr; }
    private:
        bool _nvx;
        std::int32_t (*_query)(std::int32_t);
        MemoryTelemetry _telemetry{};
        std::uint64_t _checkCeiling = 0;
        bool _frame = false;
        std::optional<MemoryBudgetSnapshot> _admission;
        std::uint64_t _admittedBytes = 0;
    };
    // Conservative GL storage estimate, including RGB/depth padding; no CPU
    // texture copy, mip/layer support or fragmentation guarantee is implied.
    [[nodiscard]] std::uint64_t TextureStorageEstimate(TextureFormat format, std::uint32_t width, std::uint32_t height);
}
