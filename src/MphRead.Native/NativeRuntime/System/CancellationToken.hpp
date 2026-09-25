#pragma once

// System.Threading.CancellationTokenSource and CancellationToken: a flag one
// side sets and the other reads, shared between copies of the token.

#include <atomic>
#include <memory>

namespace MphRead::NativeRuntime
{
    class CancellationToken final
    {
    public:
        // default(CancellationToken) / CancellationToken.None: never cancelled.
        CancellationToken() noexcept = default;

        [[nodiscard]] bool IsCancellationRequested() const noexcept
        {
            return _state != nullptr && _state->load(std::memory_order_acquire);
        }
        [[nodiscard]] bool CanBeCanceled() const noexcept { return _state != nullptr; }

    private:
        friend class CancellationTokenSource;
        explicit CancellationToken(std::shared_ptr<std::atomic<bool>> state) noexcept : _state(std::move(state)) {}

        std::shared_ptr<std::atomic<bool>> _state{};
    };

    class CancellationTokenSource final
    {
    public:
        CancellationTokenSource() : _state(std::make_shared<std::atomic<bool>>(false)) {}

        [[nodiscard]] CancellationToken Token() const noexcept { return CancellationToken(_state); }
        [[nodiscard]] bool IsCancellationRequested() const noexcept { return _state->load(std::memory_order_acquire); }
        void Cancel() noexcept { _state->store(true, std::memory_order_release); }

    private:
        std::shared_ptr<std::atomic<bool>> _state;
    };
}
