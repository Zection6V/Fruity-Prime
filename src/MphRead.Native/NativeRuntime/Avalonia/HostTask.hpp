#pragma once

// Task, as the launcher's views use it: something that is pending, then either
// done or faulted, with continuations that run on the thread that draws.
//
// Background work runs on a thread of its own and settles its task by posting
// to the dispatcher, so a continuation never runs off the UI thread -- which
// is stricter than the managed default and is what the views assume.

#include "../Gui/Host.hpp"

#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia
{
    class HostTask final : public std::enable_shared_from_this<HostTask>
    {
    public:
        enum class State : std::uint8_t
        {
            Pending,
            Done,
            Faulted
        };

        [[nodiscard]] static std::shared_ptr<HostTask> Completed();
        [[nodiscard]] static std::shared_ptr<HostTask> Faulted(std::exception_ptr error);
        [[nodiscard]] static std::shared_ptr<HostTask> Pending();
        // Task.Run: the body on a thread of its own, settled on the UI thread.
        [[nodiscard]] static std::shared_ptr<HostTask> Run(std::function<void()> body);

        [[nodiscard]] State Status() const;
        [[nodiscard]] std::exception_ptr Error() const;

        // False when it had already settled, as TrySetResult is.
        bool Complete();
        bool Fail(std::exception_ptr error);

        // Runs once this one has settled, on the thread that draws.
        void Then(std::function<void(State, std::exception_ptr)> continuation);

    private:
        void Settle(State state, std::exception_ptr error);

        mutable std::mutex _lock;
        State _state = State::Pending;
        std::exception_ptr _error;
        std::vector<std::function<void(State, std::exception_ptr)>> _continuations;
    };
}
