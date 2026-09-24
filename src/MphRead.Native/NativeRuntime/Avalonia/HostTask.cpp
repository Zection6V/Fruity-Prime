#include "HostTask.hpp"

#include <thread>
#include <utility>

namespace MphRead::NativeRuntime::Avalonia
{
    std::shared_ptr<HostTask> HostTask::Completed()
    {
        auto task = std::make_shared<HostTask>();
        task->_state = State::Done;
        return task;
    }

    std::shared_ptr<HostTask> HostTask::Faulted(std::exception_ptr error)
    {
        auto task = std::make_shared<HostTask>();
        task->_state = State::Faulted;
        task->_error = std::move(error);
        return task;
    }

    std::shared_ptr<HostTask> HostTask::Pending()
    {
        return std::make_shared<HostTask>();
    }

    std::shared_ptr<HostTask> HostTask::Run(std::function<void()> body)
    {
        auto task = std::make_shared<HostTask>();
        std::thread(
            [task, body = std::move(body)]()
            {
                try
                {
                    if (body)
                    {
                        body();
                    }
                }
                catch (...)
                {
                    task->Fail(std::current_exception());
                    return;
                }
                task->Complete();
            })
            .detach();
        return task;
    }

    HostTask::State HostTask::Status() const
    {
        const std::lock_guard<std::mutex> guard(_lock);
        return _state;
    }

    std::exception_ptr HostTask::Error() const
    {
        const std::lock_guard<std::mutex> guard(_lock);
        return _error;
    }

    bool HostTask::Complete()
    {
        {
            const std::lock_guard<std::mutex> guard(_lock);
            if (_state != State::Pending)
            {
                return false;
            }
        }
        Settle(State::Done, nullptr);
        return true;
    }

    bool HostTask::Fail(std::exception_ptr error)
    {
        {
            const std::lock_guard<std::mutex> guard(_lock);
            if (_state != State::Pending)
            {
                return false;
            }
        }
        Settle(State::Faulted, std::move(error));
        return true;
    }

    void HostTask::Settle(State state, std::exception_ptr error)
    {
        std::vector<std::function<void(State, std::exception_ptr)>> continuations;
        {
            const std::lock_guard<std::mutex> guard(_lock);
            if (_state != State::Pending)
            {
                return;
            }
            _state = state;
            _error = std::move(error);
            continuations.swap(_continuations);
        }
        const std::exception_ptr settled = Error();
        for (auto& continuation : continuations)
        {
            Gui::Dispatcher::Instance().Post(
                [continuation = std::move(continuation), state, settled]()
                { continuation(state, settled); });
        }
    }

    void HostTask::Then(std::function<void(State, std::exception_ptr)> continuation)
    {
        if (!continuation)
        {
            return;
        }
        State state = State::Pending;
        std::exception_ptr error;
        {
            const std::lock_guard<std::mutex> guard(_lock);
            if (_state == State::Pending)
            {
                _continuations.push_back(std::move(continuation));
                return;
            }
            state = _state;
            error = _error;
        }
        Gui::Dispatcher::Instance().Post(
            [continuation = std::move(continuation), state, error]()
            { continuation(state, error); });
    }
}
